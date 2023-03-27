// Copyright Nezametdinov E. Ildus 2023.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_CAEA089AEB44440FA6700327664226E8
#define H_CAEA089AEB44440FA6700327664226E8

#include "ipc.hh"

#include <string>
#include <string_view>

namespace rose {

////////////////////////////////////////////////////////////////////////////////
// Unix pipe endpoint.
////////////////////////////////////////////////////////////////////////////////

struct pipe_endpoint {
    ////////////////////////////////////////////////////////////////////////////
    // Construction/destruction.
    ////////////////////////////////////////////////////////////////////////////

    pipe_endpoint(int fd);
    ~pipe_endpoint();

    pipe_endpoint(pipe_endpoint const&) = delete;
    pipe_endpoint(pipe_endpoint&&) = delete;

    ////////////////////////////////////////////////////////////////////////////
    // Assignment operators.
    ////////////////////////////////////////////////////////////////////////////

    auto
    operator=(pipe_endpoint const&) = delete;

    auto
    operator=(pipe_endpoint&&) = delete;

    ////////////////////////////////////////////////////////////////////////////
    // Conversion operator.
    ////////////////////////////////////////////////////////////////////////////

    operator int() const noexcept {
        return this->fd_;
    }

private:
    int fd_;
};

////////////////////////////////////////////////////////////////////////////////
// Execution result.
////////////////////////////////////////////////////////////////////////////////

enum struct execution_result { failure, success };

////////////////////////////////////////////////////////////////////////////////
// Execution interface.
////////////////////////////////////////////////////////////////////////////////

auto
execute(ipc_client& client, std::u8string_view string) -> execution_result;

auto
execute(pipe_endpoint const& pipe, std::u8string_view string)
    -> execution_result;

auto
run_executor_process() -> pipe_endpoint;

} // namespace rose

#endif // H_CAEA089AEB44440FA6700327664226E8