// Copyright Nezametdinov E. Ildus 2023.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "text_input.hh"

namespace rose {

////////////////////////////////////////////////////////////////////////////////
// Text input. Manipulation interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
text_input::clear() {
    this->string_.clear();
    this->cursor_history_.clear();
}

void
text_input::enter(std::u8string_view string) {
    if(!(string.empty())) {
        this->cursor_history_.push_back(this->string_.size());
        this->string_.append(string);
    }
}

void
text_input::erase() {
    if(this->cursor_history_.empty()) {
        return;
    }

    if(auto i = this->cursor_history_.back(); i < this->string_.size()) {
        while(this->string_.size() != i) {
            this->string_.pop_back();
        }
    }

    this->cursor_history_.pop_back();
}

////////////////////////////////////////////////////////////////////////////////
// Text input. Conversion operator implementation.
////////////////////////////////////////////////////////////////////////////////

text_input::operator std::u8string_view() const {
    return this->string_;
}

} // namespace rose
