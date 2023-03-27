// Copyright Nezametdinov E. Ildus 2023.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "filesystem.hh"

namespace rose::filesystem {

////////////////////////////////////////////////////////////////////////////////
// File size query interface implementation.
////////////////////////////////////////////////////////////////////////////////

auto
obtain_file_size(std::filesystem::path path) -> size_t {
    auto error = std::error_code{};
    if(auto file_size = std::filesystem::file_size(path, error); !error) {
        if(file_size <= std::numeric_limits<size_t>::max()) {
            return static_cast<size_t>(file_size);
        }
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Data reading interface implementation.
////////////////////////////////////////////////////////////////////////////////

auto
read(std::filesystem::path path) -> std::vector<unsigned char> {
    if(auto file_size = obtain_file_size(path); file_size != 0) {
        if(auto file = file_stream{std::fopen(path.c_str(), "rb")}; file) {
            // Try reading data from the file.
            auto result = std::vector<unsigned char>(file_size, 0);
            if(std::fread(result.data(), file_size, 1, file.get()) == 1) {
                return result;
            }
        }
    }

    return {};
}

auto
read_string(std::filesystem::path path) -> std::string {
    if(auto file_size = obtain_file_size(path); file_size != 0) {
        if(auto file = file_stream{std::fopen(path.c_str(), "rb")}; file) {
            // Try reading string from the file.
            auto result = std::string(file_size, 0);
            if(std::fread(result.data(), file_size, 1, file.get()) == 1) {
                return result;
            }
        }
    }

    return {};
}

} // namespace rose::filesystem
