// Copyright Nezametdinov E. Ildus 2023.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_DEA0AEB5543A4D818F2CCD0ABD45D42F
#define H_DEA0AEB5543A4D818F2CCD0ABD45D42F

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace rose {

////////////////////////////////////////////////////////////////////////////////
// Standard integer type.
////////////////////////////////////////////////////////////////////////////////

using std::size_t;

////////////////////////////////////////////////////////////////////////////////
// Text input.
////////////////////////////////////////////////////////////////////////////////

struct text_input {
    ////////////////////////////////////////////////////////////////////////////
    // Manipulation interface.
    ////////////////////////////////////////////////////////////////////////////

    void
    clear();

    void
    enter(std::u8string_view string);

    void
    erase();

    ////////////////////////////////////////////////////////////////////////////
    // Conversion operator.
    ////////////////////////////////////////////////////////////////////////////

    operator std::u8string_view() const;

private:
    ////////////////////////////////////////////////////////////////////////////
    // Data members.
    ////////////////////////////////////////////////////////////////////////////

    // String buffer.
    std::u8string string_;

    // The list of previous cursor positions.
    std::vector<size_t> cursor_history_;
};

} // namespace rose

#endif // H_DEA0AEB5543A4D818F2CCD0ABD45D42F