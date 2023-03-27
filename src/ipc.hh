// Copyright Nezametdinov E. Ildus 2023.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_EED61A94ACC64B23AC54791921BFB9AD
#define H_EED61A94ACC64B23AC54791921BFB9AD

#include <atomic>
#include <memory>
#include <span>
#include <SDL.h>

namespace rose {

////////////////////////////////////////////////////////////////////////////////
// Request which shall be processed by the main thread.
////////////////////////////////////////////////////////////////////////////////

enum struct request {
    prompt_normal,
    prompt_privileged,
    reload_database,
    reload_theme
};

////////////////////////////////////////////////////////////////////////////////
// Shared state.
////////////////////////////////////////////////////////////////////////////////

struct shared_state {
    // SDL user event index.
    Uint32 event_idx;

    // Flags.
    std::atomic_bool is_program_running;
};

////////////////////////////////////////////////////////////////////////////////
// IPC client.
////////////////////////////////////////////////////////////////////////////////

namespace detail {

// IPC client type declaration.
struct ipc_client;

// IPC client deleter.
struct ipc_client_deleter {
    void
    operator()(struct ipc_client* client);
};

} // namespace detail

// Note: IPC client is represented as a unique pointer to incomplete type.
using ipc_client =
    std::unique_ptr<detail::ipc_client, detail::ipc_client_deleter>;

////////////////////////////////////////////////////////////////////////////////
// IPC client initialization interface.
////////////////////////////////////////////////////////////////////////////////

auto
initialize_ipc_client(struct shared_state& state) -> ipc_client;

////////////////////////////////////////////////////////////////////////////////
// IPC client communication interface.
////////////////////////////////////////////////////////////////////////////////

void
stop(ipc_client& client);

auto
request_execution(ipc_client& client, std::span<char8_t const> command_and_args)
    -> bool;

} // namespace rose

#endif // H_EED61A94ACC64B23AC54791921BFB9AD
