// Copyright Nezametdinov E. Ildus 2023.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_61F1843D6BC640C4B7099A867B0DCCD9
#define H_61F1843D6BC640C4B7099A867B0DCCD9

#include <cstddef>
#include <memory>

#include <vector>
#include <span>

namespace rose {

////////////////////////////////////////////////////////////////////////////////
// Standard integer type.
////////////////////////////////////////////////////////////////////////////////

using std::size_t;

////////////////////////////////////////////////////////////////////////////////
// Text rendering context.
////////////////////////////////////////////////////////////////////////////////

namespace detail {

// Context type declaration.
// Note: Context's implementation is private.
struct text_rendering_context;

// Context deleter.
struct text_rendering_context_deleter {
    // This operator will call context's destructor and free its memory.
    void
    operator()(struct text_rendering_context* context);
};

} // namespace detail

// Note: Context is represented as a unique pointer to incomplete type.
using text_rendering_context =
    std::unique_ptr<detail::text_rendering_context,
                    detail::text_rendering_context_deleter>;

////////////////////////////////////////////////////////////////////////////////
// Text rendering context initialization parameters.
////////////////////////////////////////////////////////////////////////////////

struct text_rendering_context_parameters {
    // A range of binary font data. These data are moved into the context upon
    // initialization.
    std::span<std::vector<unsigned char>> fonts;
};

////////////////////////////////////////////////////////////////////////////////
// Render target, has 32-bit RGBA format.
////////////////////////////////////////////////////////////////////////////////

struct render_target {
    // Pointer to the region of memory which contains pixel data.
    unsigned char* pixels;

    // Width, height, and pitch (pitch is the byte size of the line of pixels).
    int w, h, pitch;
};

////////////////////////////////////////////////////////////////////////////////
// Text rendering parameters.
////////////////////////////////////////////////////////////////////////////////

struct text_rendering_parameters {
    // Font size, DPI.
    int font_size, dpi;

    // Line of text to render.
    std::span<char32_t> text_line;
};

////////////////////////////////////////////////////////////////////////////////
// Text rendering result.
////////////////////////////////////////////////////////////////////////////////

struct text_rendering_result {
    // Rectangle which defines the damaged region in the render target after
    // text rendering is complete.
    struct {
        long x, y, w, h;
    } rectangle;

    // Number of code points consumed from the text line.
    size_t n_code_points_consumed;
};

////////////////////////////////////////////////////////////////////////////////
// Text rendering context initialization interface.
////////////////////////////////////////////////////////////////////////////////

auto
initialize(struct text_rendering_context_parameters params)
    -> text_rendering_context;

////////////////////////////////////////////////////////////////////////////////
// Text rendering interface.
////////////////////////////////////////////////////////////////////////////////

auto
render(text_rendering_context const& context,
       struct text_rendering_parameters params, struct render_target target)
    -> struct text_rendering_result;

} // namespace rose

#endif // H_61F1843D6BC640C4B7099A867B0DCCD9
