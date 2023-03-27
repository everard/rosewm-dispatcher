// Copyright Nezametdinov E. Ildus 2023.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "unicode.hh"

#include <algorithm>
#include <fribidi/fribidi.h>

namespace rose {

////////////////////////////////////////////////////////////////////////////////
// Unicode conversion utility function and type.
////////////////////////////////////////////////////////////////////////////////

enum : char32_t {
    utf8_decoding_incomplete = 0xFFFFFFFE,
    utf8_decoding_error = 0xFFFFFFFF
};

struct utf8_decoding_result {
    char32_t x;
    size_t n;
};

static auto
utf8_decode(std::u8string_view string) -> utf8_decoding_result {
    // Empty string is always incomplete.
    if(string.empty()) {
        return {.x = utf8_decoding_incomplete};
    }

    // Check if the string starts with an ASCII character.
    if(string[0] <= 0x7F) {
        return {.x = string[0], .n = 1};
    }

    // Define a function which checks if the given value is in the given range.
    auto is_in_range = [](auto x, auto a, auto b) {
        return ((x >= a) && (x <= b));
    };

    // Check for disallowed first byte values (see the Table 3-7 in the Unicode
    // Standard for details).
    if(!is_in_range(string[0], 0xC2, 0xF4)) {
        return {.x = utf8_decoding_error, .n = 1};
    }

    static const struct entry {
        char32_t high;
        unsigned char ranges[3][2], n;
    } utf8_table[51] = {
        {0x00000080, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x000000C0, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000100, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000140, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000180, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x000001C0, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000200, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000240, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000280, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x000002C0, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000300, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000340, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000380, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x000003C0, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000400, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000440, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000480, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x000004C0, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000500, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000540, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000580, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x000005C0, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000600, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000640, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000680, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x000006C0, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000700, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000740, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000780, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x000007C0, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000000, {{0xA0, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x00001000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x00002000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x00003000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x00004000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x00005000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x00006000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x00007000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x00008000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x00009000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x0000A000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x0000B000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x0000C000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x0000D000, {{0x80, 0x9F}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x0000E000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x0000F000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x00000000, {{0x90, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 3},
        {0x00040000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 3},
        {0x00080000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 3},
        {0x000C0000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 3},
        {0x00100000, {{0x80, 0x8F}, {0x80, 0xBF}, {0x80, 0xBF}}, 3}};

    // Decode the rest of the UTF-8 sequence.
    if(auto data = string.data(); true) {
        auto const r = utf8_table[(*(data++)) - 0xC2];
        auto x = r.high, shift = static_cast<char32_t>(r.n * 6);

        for(auto i = size_t{}, limit = std::min(string.size() - 1, size_t{r.n});
            i < limit; ++i) {
            if(!is_in_range(data[i], r.ranges[i][0], r.ranges[i][1])) {
                return {.x = utf8_decoding_error, .n = (i + 1)};
            }

            shift -= 6;
            x |= static_cast<char32_t>(data[i] & 0x3F) << shift;
        }

        if(shift == 0) {
            return {.x = x, .n = static_cast<size_t>(r.n + 1)};
        }
    }

    return {.x = utf8_decoding_incomplete};
}

////////////////////////////////////////////////////////////////////////////////
// String conversion interface implementation.
////////////////////////////////////////////////////////////////////////////////

auto
convert_utf8_to_utf32(std::u8string_view string) -> utf32_string_buffer {
    auto r = (struct utf32_string_buffer){};

    // Decode the given string.
    while((r.size < std::size(r.data)) && !(string.empty())) {
        // Decode the next character.
        auto d = utf8_decode(string);

        // Shrink the string.
        string.remove_prefix(d.n);

        // If the code sequence is incomplete, then break out of the cycle.
        if(d.x == utf8_decoding_incomplete) {
            break;
        }

        // Skip the error, if any.
        if(d.x == utf8_decoding_error) {
            continue;
        }

        // Save successfully decoded character.
        r.data[r.size++] = d.x;
    }

    // Add ellipsis to the end of the decoded string, if needed.
    if(!(string.empty()) && (r.size == std::size(r.data))) {
        r.data[std::size(r.data) - 1] = 0x2026;
    }

    // Apply Unicode Bidirectional Algorithm.
    if(true) {
        FriBidiChar buffer_dst[std::size(r.data)] = {};
        if(auto base_dir = static_cast<FriBidiParType>(FRIBIDI_TYPE_ON); true) {
            // Copy the UTF-32 string into the input buffer.
            FriBidiChar buffer_src[std::size(r.data)] = {};
            std::ranges::copy(std::span<char32_t const>{r}, buffer_src);

            // Run the algorithm.
            fribidi_log2vis(buffer_src, static_cast<FriBidiStrIndex>(r.size),
                            &base_dir, buffer_dst, nullptr, nullptr, nullptr);
        }

        // Copy algorithm's output back to the resulting UTF-32 string.
        std::ranges::copy(std::span{buffer_dst, r.size}, r.data);
    }

    // Return decoded string.
    return r;
}

} // namespace rose
