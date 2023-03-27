// Copyright Nezametdinov E. Ildus 2023.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "buffer.hh"
#include "ipc.hh"

#include <asio.hpp>
#include <chrono>
#include <thread>
#include <map>

namespace rose {
using namespace std::literals::chrono_literals;

////////////////////////////////////////////////////////////////////////////////
// Asio types.
////////////////////////////////////////////////////////////////////////////////

using ipc_endpoint = asio::local::stream_protocol::endpoint;
using ipc_socket = asio::local::stream_protocol::socket;

////////////////////////////////////////////////////////////////////////////////
// A storage for IPC packets.
////////////////////////////////////////////////////////////////////////////////

using ipc_packet_storage = buffer<8192>;

////////////////////////////////////////////////////////////////////////////////
// IPC client state.
////////////////////////////////////////////////////////////////////////////////

struct ipc_client_data {
    // A reference to the shared state.
    struct shared_state& state;

    // Asio IO context.
    asio::io_context context;

    // Server connection.
    ipc_endpoint endpoint;
    ipc_socket socket{context};

    // Flags.
    std::atomic_bool is_tx_active;
};

////////////////////////////////////////////////////////////////////////////////
// Dispatcher IPC client execution logic.
////////////////////////////////////////////////////////////////////////////////

static auto
run_dispatcher_client(struct ipc_client_data& data, std::chrono::seconds dt)
    -> asio::awaitable<void> {
    // Define packet processing function.
    static auto process_packet = //
        [](struct shared_state& state, auto packet) {
            // Define IPC command type.
            using ipc_command = buffer<64>;

            // Define the map of IPC commands.
            static std::map<ipc_command, request> command_map = {
                {{0x00}, request::prompt_normal},
                {{0x01}, request::prompt_privileged},
                {{0x02}, request::reload_database}};

            // Process IPC commands from the packet.
            auto event = SDL_Event{.type = state.event_idx};
            for(auto command = ipc_command{};
                packet.size() >= command.size();) {
                // Obtain current command.
                std::ranges::copy(packet.first(command.size()), command.data());

                // Shrink the packet.
                packet = packet.subspan(command.size());

                // Process the command.
                if(command_map.contains(command)) {
                    event.user.code = static_cast<int>(command_map[command]);
                    SDL_PushEvent(&event);
                }
            }
        };

    try {
        // Wait for the specified amount of time.
        if(dt != 0s) {
            auto timer = asio::steady_timer{data.context, dt};
            co_await timer.async_wait(asio::use_awaitable);
        }

        // Connect to the server.
        data.socket = ipc_socket{data.context};
        co_await data.socket.async_connect(data.endpoint, asio::use_awaitable);

        // Select IPC protocol variant.
        if(char packet[] = {0x01, 0x00, 0x02}; true) {
            co_await async_write(
                data.socket, asio::buffer(packet), asio::use_awaitable);
        }

        // The server is ready to receive commands.
        data.is_tx_active = false;

        // Read packets.
        auto packet_storage = ipc_packet_storage{};
        for(auto packet_size = size_t{};;) {
            using u16_storage = buffer<integer_size<std::uint16_t>>;

            // Read packet size.
            auto size_storage = packet_storage.view_as<u16_storage>();
            co_await asio::async_read(
                data.socket,
                asio::buffer(size_storage.data(), size_storage.size()),
                asio::use_awaitable);

            if(packet_size = buffer_to_int<std::uint16_t>(size_storage);
               packet_size > packet_storage.size()) {
                break;
            }

            // Read packet data.
            co_await asio::async_read(
                data.socket, asio::buffer(packet_storage.data(), packet_size),
                asio::use_awaitable);

            // Process the packet.
            process_packet(
                data.state, std::span{packet_storage}.first(packet_size));
        }
    } catch(...) {
    }

    // Restart.
    if(data.state.is_program_running) {
        asio::co_spawn( //
            data.context, run_dispatcher_client(data, 5s), asio::detached);
    }
}

static auto
send_dispatcher_packet(struct ipc_client_data& data, auto packet)
    -> asio::awaitable<void> {
    // Start the transmission.
    if(data.is_tx_active.exchange(true)) {
        co_return;
    }

    try {
        // Write packet size.
        if(true) {
            auto size_storage =
                int_to_buffer(static_cast<std::uint16_t>(packet.size));

            co_await async_write( //
                data.socket,
                asio::buffer(size_storage.data(), size_storage.size()),
                asio::use_awaitable);
        }

        // Write packet data.
        co_await async_write( //
            data.socket, asio::buffer(packet.storage.data(), packet.size),
            asio::use_awaitable);

    } catch(...) {
    }

    // End the transmission.
    data.is_tx_active = false;
}

////////////////////////////////////////////////////////////////////////////////
// Status IPC client execution logic.
////////////////////////////////////////////////////////////////////////////////

auto
run_status_client(struct ipc_client_data& data, std::chrono::seconds dt)
    -> asio::awaitable<void> {
    // Define packet processing function.
    static auto process_packet = //
        [](struct shared_state& state, auto packet) {
            // Define the type of the status message which notifies of the theme
            // update.
            static constexpr auto status_type_theme = 3;

            // Define the size of the server state.
            static constexpr auto server_state_size = size_t{4};

            // Define the size of a device ID.
            static constexpr auto device_id_size = sizeof(unsigned);

            // Define status message sizes.
            static constexpr size_t data_sizes[] = {
                // rose_ipc_status_type_server_state
                server_state_size + 1,
                // rose_ipc_status_type_keyboard_keymap
                1,
                // rose_ipc_status_type_keyboard_control_scheme
                1,
                // rose_ipc_status_type_theme
                1,
                // rose_ipc_status_type_input_initialized
                device_id_size + 1,
                // rose_ipc_status_type_input_destroyed
                device_id_size + 1,
                // rose_ipc_status_type_output_initialized
                device_id_size + 1,
                // rose_ipc_status_type_output_destroyed
                device_id_size + 1};

            // Process status messages.
            while(!(packet.empty())) {
                // Process the next status message depending on its type.
                if(auto type = packet[0]; type < std::size(data_sizes)) {
                    // Request theme update, if needed.
                    if(auto event = SDL_Event{.type = state.event_idx};
                       type == status_type_theme) {
                        // Construct the SDL event.
                        event.user.code =
                            static_cast<int>(request::reload_theme);

                        // Queue the event.
                        SDL_PushEvent(&event);

                        // Do nothing else.
                        break;
                    }

                    // Shrink the packet.
                    packet = packet.subspan(
                        std::min(packet.size(), data_sizes[type]));
                } else {
                    break;
                }
            }
        };

    try {
        // Wait for the specified amount of time.
        if(dt != 0s) {
            auto timer = asio::steady_timer{data.context, dt};
            co_await timer.async_wait(asio::use_awaitable);
        }

        // Connect to the server.
        auto socket = ipc_socket{data.context};
        co_await socket.async_connect(data.endpoint, asio::use_awaitable);

        // Select IPC protocol variant.
        if(char packet[] = {0x01, 0x00, 0x03}; true) {
            co_await async_write(
                socket, asio::buffer(packet), asio::use_awaitable);
        }

        // Read packets.
        auto packet_storage = ipc_packet_storage{};
        for(auto packet_size = size_t{};;) {
            using u16_storage = buffer<integer_size<std::uint16_t>>;

            // Read packet size.
            auto size_storage = packet_storage.view_as<u16_storage>();
            co_await asio::async_read(
                socket, asio::buffer(size_storage.data(), size_storage.size()),
                asio::use_awaitable);

            if(packet_size = buffer_to_int<std::uint16_t>(size_storage);
               packet_size > packet_storage.size()) {
                break;
            }

            // Read packet data.
            co_await asio::async_read(
                socket, asio::buffer(packet_storage.data(), packet_size),
                asio::use_awaitable);

            // Process the packet.
            process_packet(
                data.state, std::span{packet_storage}.first(packet_size));
        }
    } catch(...) {
    }

    // Restart.
    if(data.state.is_program_running) {
        asio::co_spawn( //
            data.context, run_status_client(data, 5s), asio::detached);
    }
}

////////////////////////////////////////////////////////////////////////////////
// IPC client implementation details.
////////////////////////////////////////////////////////////////////////////////

namespace detail {

struct ipc_client {
    ////////////////////////////////////////////////////////////////////////////
    // Construction/destruction.
    ////////////////////////////////////////////////////////////////////////////

    ipc_client(struct shared_state& state)
        : data{.state = state}, thread{[this]() {
            // Obtain IPC endpoint address.
            if(auto string = getenv("ROSE_IPC_ENDPOINT"); string != nullptr) {
                data.endpoint = ipc_endpoint{string};
            }

            try {
                // Add work.
                asio::co_spawn(data.context, run_dispatcher_client(data, 0s),
                               asio::detached);

                asio::co_spawn(
                    data.context, run_status_client(data, 0s), asio::detached);

                // Run event loop.
                data.context.run();
            } catch(...) {
            }

            // Stop program execution.
            data.state.is_program_running = false;
            return EXIT_SUCCESS;
        }} {
    }

    ////////////////////////////////////////////////////////////////////////////
    // Data members.
    ////////////////////////////////////////////////////////////////////////////

    struct ipc_client_data data;
    std::jthread thread;
};

void
ipc_client_deleter::operator()(struct ipc_client* client) {
    delete client;
}

} // namespace detail

////////////////////////////////////////////////////////////////////////////////
// IPC client initialization interface implementation.
////////////////////////////////////////////////////////////////////////////////

auto
initialize_ipc_client(struct shared_state& state) -> ipc_client {
    return ipc_client{new detail::ipc_client{state}};
}

////////////////////////////////////////////////////////////////////////////////
// IPC client communication interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
stop(ipc_client& client) {
    client->data.context.stop();
}

auto
request_execution(ipc_client& client, std::span<char8_t const> command_and_args)
    -> bool {
    if(client->data.is_tx_active) {
        return false;
    }

    // Validate the given command and its arguments.
    if((command_and_args.size() == 0) ||
       (command_and_args.size() > (ipc_packet_storage::static_size() - 1))) {
        return false;
    }

    if(command_and_args[command_and_args.size() - 1] != '\0') {
        return false;
    }

    // Initialize the packet.
    struct {
        ipc_packet_storage storage;
        size_t size;
    } packet = {.storage = {0x03}, .size = command_and_args.size() + 1};
    std::ranges::copy(command_and_args, packet.storage.data() + 1);

    // Request packet transmission.
    asio::post(client->data.context, [&data = client->data, packet] {
        asio::co_spawn(
            data.context, send_dispatcher_packet(data, packet), asio::detached);
    });

    return true;
}

} // namespace rose
