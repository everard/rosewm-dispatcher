// Copyright Nezametdinov E. Ildus 2023.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "buffer.hh"
#include "execution.hh"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

namespace rose {

////////////////////////////////////////////////////////////////////////////////
// Command initialization utility function.
////////////////////////////////////////////////////////////////////////////////

auto
initialize_command(std::u8string_view string) {
    // Initialize an empty command.
    struct {
        buffer<64 * 1024, char8_t> storage;
        size_t size;
    } command = {};

    // Write the command to the storage.
    for(auto is_escaped = false; auto c : string) {
        if(command.size == (command.storage.size() - 1)) {
            command.size = 0;
            return command;
        }

        if(!is_escaped && (c == u8'\\')) {
            is_escaped = true;
            continue;
        }

        if(c == u8' ') {
            if(is_escaped) {
                command.storage[command.size++] = c;
            } else {
                command.storage[command.size++] = '\0';
            }
        } else {
            command.storage[command.size++] = c;
        }

        is_escaped = false;
    }

    if(command.size != 0) {
        if(command.storage[command.size - 1] != '\0') {
            command.size++;
        }
    }

    // Return the command.
    return command;
}

////////////////////////////////////////////////////////////////////////////////
// Unix pipe endpoint. Construction/destruction implementation.
////////////////////////////////////////////////////////////////////////////////

pipe_endpoint::pipe_endpoint(int fd) : fd_{fd} {
}

pipe_endpoint::~pipe_endpoint() {
    if(this->fd_ != -1) {
        close(this->fd_);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Execution interface implementation.
////////////////////////////////////////////////////////////////////////////////

auto
execute(ipc_client& client, std::u8string_view string) -> execution_result {
    if(auto command = initialize_command(string); command.size != 0) {
        if(request_execution(
               client, std::span{command.storage}.first(command.size))) {
            return execution_result::success;
        }
    }

    return execution_result::failure;
}

auto
execute(pipe_endpoint const& pipe, std::u8string_view string)
    -> execution_result {
    if(auto command = initialize_command(string); command.size != 0) {
        // Define a function which writes to the pipe.
        auto write_pipe = [&pipe](auto const& range) -> bool {
            // Write data.
            auto n = ssize_t{};
            while((n = write(pipe, range.data(), range.size())) == -1) {
                if(errno != EINTR) {
                    return false;
                }
            }

            // Write succeeded.
            return true;
        };

        // Write packet to the pipe.
        auto size = int_to_buffer(static_cast<std::uint16_t>(command.size));
        if(write_pipe(size)) {
            if(write_pipe(std::span{command.storage}.first(command.size))) {
                return execution_result::success;
            }
        }
    }

    return execution_result::failure;
}

auto
run_executor_process() -> pipe_endpoint {
    // Define an array of pipe file descriptors.
    int pipe_fds[2] = {};

    // Create a pipe.
    if(pipe(pipe_fds) == -1) {
        return -1;
    }

    // Fork.
    switch(fork()) {
        case -1:
            close(pipe_fds[0]);
            close(pipe_fds[1]);
            return -1;

        case 0: {
            // Fork: This section defines executor process.

            // Ignore SIGPIPE.
            signal(SIGPIPE, SIG_IGN);

            // Close the file descriptor of the write end of the pipe.
            close(pipe_fds[1]);

            // Initialize storage for packets.
            auto packet_storage = buffer<64 * 1024, char>{};

            // Define a function which reads from the pipe.
            auto read_pipe = [pipe_fd = pipe_fds[0]](auto& range) -> bool {
                // Read data.
                auto n = ssize_t{};
                while((n = read(pipe_fd, range.data(), range.size())) == -1) {
                    if(errno != EINTR) {
                        return false;
                    }
                }

                // If the pipe has been closed, then read failed.
                if(n == 0) {
                    return false;
                }

                // Read succeeded.
                return true;
            };

            // Read the pipe and execute the commands.
            for(auto packet_size = size_t{}; true;) {
                // Read packet size.
                using u16_storage = buffer<integer_size<std::uint16_t>>;
                if(auto size_storage = u16_storage{}; read_pipe(size_storage)) {
                    packet_size = buffer_to_int<std::uint16_t>(size_storage);
                } else {
                    break;
                }

                // Obtain command storage.
                auto command = std::span{packet_storage}.first(packet_size);
                if(command.empty()) {
                    continue;
                }

                // Read the command.
                if(!read_pipe(command)) {
                    break;
                }

                // Discard invalid command.
                if(command.back() != '\0') {
                    continue;
                }

                // Execute the command. Fork for the first time.
                if(auto pid = fork(); pid == 0) {
                    // Fork: the following code is only executed in the child
                    // process.

                    // Close the file descriptor of the read end of the pipe.
                    close(pipe_fds[0]);

                    // Initialize storage for arguments.
                    char* argument_storage[256] = {};

                    // Initialize arguments.
                    auto arguments = std::span{
                        argument_storage, std::size(argument_storage) - 1};

                    for(auto offset = size_t{}; auto& argument : arguments) {
                        offset += //
                            std::strlen(argument =
                                            packet_storage.data() + offset) +
                            1;

                        if(offset == packet_size) {
                            break;
                        }
                    }

                    // Reset signal handlers.
                    signal(SIGALRM, SIG_DFL);
                    signal(SIGCHLD, SIG_DFL);
                    signal(SIGPIPE, SIG_DFL);
                    signal(SIGQUIT, SIG_DFL);
                    signal(SIGTERM, SIG_DFL);

                    signal(SIGHUP, SIG_DFL);
                    signal(SIGINT, SIG_DFL);

                    // Fork for the second time.
                    if(fork() == 0) {
                        // Fork: the following code is only executed in the
                        // child process.

                        // Set SID.
                        setsid();

                        // Execute the command.
                        execvp(arguments[0], arguments.data());
                    }

                    // Exit the program.
                    std::exit(EXIT_SUCCESS);
                } else if(pid != -1) {
                    // Wait for child process.
                    while(waitpid(pid, nullptr, 0) == -1) {
                        if(errno != EINTR) {
                            break;
                        }
                    }
                }
            }

            // Close the file descriptor of the read end of the pipe.
            close(pipe_fds[0]);

            // Exit the program.
            std::exit(EXIT_SUCCESS);

            // Note: This point will never be reached.
            break;
        }

        default:
            break;
    }

    // Close the file descriptor of the read end of the pipe.
    close(pipe_fds[0]);

    // Return write end of the pipe.
    return pipe_fds[1];
}

} // namespace rose
