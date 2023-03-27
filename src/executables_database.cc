// Copyright Nezametdinov E. Ildus 2023.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "executables_database.hh"

#include <concepts>
#include <cstdlib>
#include <filesystem>
#include <type_traits>

namespace rose {

////////////////////////////////////////////////////////////////////////////////
// String manipulation utility functions.
////////////////////////////////////////////////////////////////////////////////

auto
append_escaped(std::u8string_view input, std::u8string& result)
    -> std::u8string& {
    for(auto c : input) {
        if(c == u8' ') {
            result.push_back(u8'\\');
        }

        result.push_back(c);
    }

    return result;
}

auto
append_escaped_path(std::u8string_view path, std::u8string& result)
    -> std::u8string& {
    if(path.empty()) {
        return result;
    }

    if(append_escaped(path, result).back() != u8'/') {
        result.push_back(u8'/');
    }

    return result;
}

auto
split_command_string(std::u8string_view input, std::u8string& command)
    -> std::u8string_view {
    // Obtain the command from the input with escape characters removed.
    auto offset = std::size_t{};
    for(auto is_escaped = (command.clear(), false); auto c : input) {
        if(++offset; !is_escaped && (c == u8'\\')) {
            is_escaped = true;
            continue;
        }

        if(!is_escaped && (c == u8' ')) {
            break;
        }

        is_escaped = (command.push_back(c), false);
    }

    // Return string view of arguments.
    return input.substr(offset);
}

////////////////////////////////////////////////////////////////////////////////
// Database of executable files. Construction/destruction implementation.
////////////////////////////////////////////////////////////////////////////////

executables_database::executables_database() {
    this->initialize();
}

////////////////////////////////////////////////////////////////////////////////
// Database of executable files. Initialization interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
executables_database::initialize() {
    using std::filesystem::perms;

    // Define permissions mask of executable files.
    static constexpr auto exec_mask =
        (perms::owner_exec | perms::group_exec | perms::others_exec);

    // Clear existing database.
    this->files_.clear();
    this->paths_.clear();

    // Do nothing else if the PATH environment variable is not set.
    if(std::getenv("PATH") == nullptr) {
        return;
    }

    // Iterate through each path.
    auto paths = std::string_view{std::getenv("PATH")};
    while(!(paths.empty())) {
        // Obtain the next directory path.
        auto i = paths.find(':');
        auto directory_path = paths.substr(0, i);

        // Update the remaining paths.
        if(i != paths.npos) {
            paths.remove_prefix(i + 1);
        } else {
            paths.remove_prefix(paths.size());
        }

        try {
            auto directory = std::filesystem::directory_iterator{
                std::filesystem::path{directory_path}};

            // Iterate through all files in the directory.
            for(auto const& entry : directory) {
                // Obtain permissions of the current file.
                auto file_permissions = entry.status().permissions();

                // Check permissions.
                if((file_permissions & exec_mask) != perms::none) {
                    // If the file is executable, then obtain its path.
                    auto const& path = entry.path();

                    // Add its full path to the database.
                    this->paths_.insert(path.u8string());

                    // Add its name to the database.
                    if(!(this->files_.contains(path.filename().u8string()))) {
                        this->files_[path.filename().u8string()] =
                            path.parent_path().u8string();
                    }
                }
            }
        } catch(...) {
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Database of executable files. Lookup interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
executables_database::autocomplete( //
    std::u8string_view input, std::u8string& result) {
    // If the input is empty, then there is nothing to complete.
    if(result.clear(); input.empty()) {
        return;
    }

    // Define a function which preforms lookup.
    auto perform_lookup = [&](auto const& map) {
        // Define a function which obtains string from iterator.
        auto string = [](auto i) -> std::u8string const& {
            if constexpr( //
                std::same_as<std::remove_cvref_t<decltype(*i)>,
                             std::u8string>) {
                return (*i);
            } else {
                return (*i).first;
            }
        };

        // Obtain the command string from the input.
        split_command_string(input, this->buffer_);

        // Set initial sequence.
        result = this->buffer_;

        // Append characters to the sequence.
        for(auto i = map.lower_bound(result); i != map.end();
            i = map.lower_bound(result)) {
            // If the current suggestion does not start with the current
            // sequence, then the sequence is complete.
            if(!(string(i).starts_with(result))) {
                break;
            }

            // If the current suggestion equals the current sequence, then
            // the sequence is complete.
            if(string(i) == result) {
                break;
            }

            // Obtain the next character from the current suggestion.
            auto const next_character = string(i)[result.size()];

            // Modify the sequence so that it contains the character which
            // is strictly greater than the next character from current
            // suggestion.
            result.push_back(next_character + 1);

            // Find a suggestion which is strictly greater than current
            // suggestion, and is guaranteed to differ in the next
            // character.
            auto j = map.lower_bound(result);

            // Update the sequence.
            result.pop_back();
            if((j == map.end()) || !(string(j).starts_with(result))) {
                result.push_back(next_character);
            } else if(string(i)[result.size()] == string(j)[result.size()]) {
                result.push_back(next_character);
            } else {
                break;
            }
        }

        // Remove prefix from resulting sequence.
        result.erase(0, this->buffer_.size());

        // Clear temporary buffer.
        this->buffer_.clear();

        // Add escape characters.
        result = append_escaped(result, this->buffer_);
    };

    // Lookup suggestions based on input.
    if(input.starts_with(u8'/')) {
        perform_lookup(this->paths_);
    } else {
        perform_lookup(this->files_);
    }
}

void
executables_database::lookup_suggestions( //
    std::u8string_view input, std::u8string& result) {
    // If the input is empty, then there are no suggestions.
    if(input.empty()) {
        return;
    }

    // Split the command string.
    auto arguments = split_command_string(input, this->buffer_);

    // Add the command string.
    this->append_command_string_(this->buffer_, arguments, result);

    // If arguments string is not empty, then do nothing else.
    if(!(arguments.empty())) {
        return;
    }

    // Add delimiter.
    result.append(u8" *");

    // Define the maximum number of suggestions.
    static constexpr auto n_suggestions_max = 5;

    // Lookup suggestions based on input.
    if(auto i = 0; input.starts_with(u8'/')) {
        for(auto suggestion = this->paths_.lower_bound(this->buffer_);
            suggestion != this->paths_.end(); ++suggestion) {
            if(!(suggestion->starts_with(this->buffer_))) {
                break;
            }

            if(*suggestion != this->buffer_) {
                auto suffix = std::u8string_view{*suggestion}.substr(
                    this->buffer_.size());

                append_escaped(suffix, result.append(u8" \xE2\x80\xA6"));
            } else {
                continue;
            }

            if(++i == n_suggestions_max) {
                break;
            }
        }
    } else {
        for(auto suggestion = this->files_.lower_bound(this->buffer_);
            suggestion != this->files_.end(); ++suggestion) {
            if(!(suggestion->first.starts_with(this->buffer_))) {
                break;
            }

            append_escaped_path(suggestion->second, result.append(u8" "));
            if(suggestion->first != this->buffer_) {
                auto suffix = std::u8string_view{suggestion->first}.substr(
                    this->buffer_.size());

                append_escaped(suffix, result.append(u8"\xE2\x80\xA6"));
            } else {
                append_escaped(this->buffer_, result);
            }

            if(++i == n_suggestions_max) {
                break;
            }
        }
    }
}

void
executables_database::modify_command_string( //
    std::u8string_view input, std::u8string& result) {
    // Clear the resulting string.
    result.clear();

    // Split the command string.
    auto arguments = split_command_string(input, this->buffer_);

    // Construct modified command string.
    this->append_command_string_(this->buffer_, arguments, result);
}

////////////////////////////////////////////////////////////////////////////////
// Database of executable files. Utility functions implementation.
////////////////////////////////////////////////////////////////////////////////

void
executables_database::append_command_string_( //
    std::u8string_view command, std::u8string_view arguments,
    std::u8string& result) {
    if(command.starts_with(u8'/')) {
        append_escaped(command, result);
    } else {
        if(auto i = this->files_.find(command); i != this->files_.end()) {
            append_escaped(i->first, append_escaped_path(i->second, result));
        } else {
            append_escaped(command, result);
        }
    }

    if(!(arguments.empty())) {
        result.append(u8" ").append(arguments);
    }
}

} // namespace rose
