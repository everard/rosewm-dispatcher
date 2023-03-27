// Copyright Nezametdinov E. Ildus 2023.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_DFE3E7DAE9B04E21A2C77858193C3024
#define H_DFE3E7DAE9B04E21A2C77858193C3024

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace rose::filesystem {

////////////////////////////////////////////////////////////////////////////////
// Standard integer type.
////////////////////////////////////////////////////////////////////////////////

using std::size_t;

////////////////////////////////////////////////////////////////////////////////
// Self-closing file stream.
////////////////////////////////////////////////////////////////////////////////

namespace detail {

using file_closer = decltype([](std::FILE* file) {
    if(file != nullptr) {
        std::fclose(file);
    }
});

} // namespace detail

using file_stream = std::unique_ptr<std::FILE, detail::file_closer>;

////////////////////////////////////////////////////////////////////////////////
// File size query interface.
////////////////////////////////////////////////////////////////////////////////

auto
obtain_file_size(std::filesystem::path path) -> size_t;

////////////////////////////////////////////////////////////////////////////////
// Data reading interface.
////////////////////////////////////////////////////////////////////////////////

auto
read(std::filesystem::path path) -> std::vector<unsigned char>;

auto
read_string(std::filesystem::path path) -> std::string;

} // namespace rose::filesystem

#endif // H_DFE3E7DAE9B04E21A2C77858193C3024
