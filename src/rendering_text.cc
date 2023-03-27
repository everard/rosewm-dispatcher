// Copyright Nezametdinov E. Ildus 2023.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "rendering_text.hh"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include <algorithm>
#include <utility>

namespace rose {

////////////////////////////////////////////////////////////////////////////////
// Freetype font face.
////////////////////////////////////////////////////////////////////////////////

struct freetype_font_face {
    ////////////////////////////////////////////////////////////////////////////
    // Construction/destruction.
    ////////////////////////////////////////////////////////////////////////////

    freetype_font_face(FT_Library ft, std::vector<unsigned char> font_data)
        : face{}, data{std::move(font_data)} {
        // Create a new font face.
        if(FT_New_Memory_Face( //
               ft, this->data.data(), this->data.size(), 0, &(this->face)) !=
           FT_Err_Ok) {
            goto error;
        }

        // Make sure it represents a scalable font.
        if(!FT_IS_SCALABLE(this->face)) {
            goto error;
        }

        // Initialization succeeded.
        return;

    error:
        // On error, free memory.
        if(this->face != nullptr) {
            FT_Done_Face(this->face);
        }

        this->face = nullptr;
    }

    freetype_font_face(freetype_font_face const&) = delete;
    freetype_font_face(freetype_font_face&& other)
        : face{std::exchange(other.face, nullptr)}
        , data{std::move(other.data)} {
    }

    ~freetype_font_face() {
        if(this->face != nullptr) {
            FT_Done_Face(this->face);
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // Assignment operator.
    ////////////////////////////////////////////////////////////////////////////

    auto
    operator=(freetype_font_face other) noexcept -> freetype_font_face& {
        std::swap(this->face, other.face);
        std::swap(this->data, other.data);

        return *this;
    }

    ////////////////////////////////////////////////////////////////////////////
    // Conversion operator.
    ////////////////////////////////////////////////////////////////////////////

    operator FT_Face() const noexcept {
        return this->face;
    }

    ////////////////////////////////////////////////////////////////////////////
    // Data members.
    ////////////////////////////////////////////////////////////////////////////

    FT_Face face;
    std::vector<unsigned char> data;
};

////////////////////////////////////////////////////////////////////////////////
// Text rendering context implementation details.
////////////////////////////////////////////////////////////////////////////////

namespace detail {

struct text_rendering_context {
    ////////////////////////////////////////////////////////////////////////////
    // Construction/destruction.
    ////////////////////////////////////////////////////////////////////////////

    text_rendering_context(struct text_rendering_context_parameters params)
        : ft{}, font_faces{} {
        // Make sure at least one font is supplied.
        if(params.fonts.empty()) {
            return;
        }

        // Initialize FreeType library.
        if(FT_Init_FreeType(&(this->ft)) != FT_Err_Ok) {
            goto error;
        }

        // Reserve memory for font faces.
        this->font_faces.reserve(params.fonts.size());

        // Initialize font faces.
        for(auto& font : params.fonts) {
            auto& x = this->font_faces.emplace_back(this->ft, std::move(font));
            if(x.face == nullptr) {
                goto error;
            }
        }

        // Initialization succeeded.
        return;

    error:
        // On error, free memory.
        if(font_faces.clear(); this->ft != nullptr) {
            FT_Done_FreeType(this->ft);
        }

        this->ft = nullptr;
    }

    text_rendering_context(text_rendering_context const&) = delete;
    text_rendering_context(text_rendering_context&&) = delete;

    ~text_rendering_context() {
        if(font_faces.clear(); this->ft != nullptr) {
            FT_Done_FreeType(this->ft);
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // Assignment operators.
    ////////////////////////////////////////////////////////////////////////////

    auto
    operator=(text_rendering_context const&) = delete;

    auto
    operator=(text_rendering_context&&) = delete;

    ////////////////////////////////////////////////////////////////////////////
    // Data members.
    ////////////////////////////////////////////////////////////////////////////

    FT_Library ft;
    std::vector<freetype_font_face> font_faces;
};

void
text_rendering_context_deleter::operator()(
    struct text_rendering_context* context) {
    delete context;
}

} // namespace detail

////////////////////////////////////////////////////////////////////////////////
// Bounding box computation utility functions.
////////////////////////////////////////////////////////////////////////////////

static auto
compute_bbox(FT_GlyphSlot glyph) -> FT_BBox {
    if(glyph != nullptr) {
        return {.xMin = glyph->bitmap_left,
                .yMin = glyph->bitmap_top - glyph->bitmap.rows,
                .xMax = glyph->bitmap_left + glyph->bitmap.width,
                .yMax = glyph->bitmap_top};
    }

    return {};
}

static auto
stretch_bbox(FT_BBox a, FT_BBox b, FT_Pos offset_x) -> FT_BBox {
    return {.xMin = std::min(a.xMin, b.xMin + offset_x),
            .yMin = std::min(a.yMin, b.yMin),
            .xMax = std::max(a.xMax, b.xMax + offset_x),
            .yMax = std::max(a.yMax, b.yMax)};
}

////////////////////////////////////////////////////////////////////////////////
// Glyph rendering utility function.
////////////////////////////////////////////////////////////////////////////////

static auto
render_glyph(text_rendering_context const& context, char32_t c)
    -> FT_GlyphSlot {
    // Find a font face which contains the given character's code point.
    auto const* font_face = context->font_faces.data();
    for(auto const& face : context->font_faces) {
        if(FT_Get_Char_Index(face, c) != 0) {
            font_face = &face;
            break;
        }
    }

    // Render a glyph for the given character.
    if(FT_Load_Char(*font_face, c, FT_LOAD_RENDER) != FT_Err_Ok) {
        return nullptr;
    }

    // Validate rendered glyph's format.
    if(font_face->face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
        return nullptr;
    }

    // Return the glyph slot with rendered glyph.
    return font_face->face->glyph;
}

////////////////////////////////////////////////////////////////////////////////
// Text rendering utility function and type.
////////////////////////////////////////////////////////////////////////////////

struct rendered_text_line {
    FT_Glyph glyphs[256];
    size_t n_glyphs;

    FT_BBox bbox;
    FT_Pos y_ref_min, y_ref_max;
};

static auto
render(text_rendering_context const& context, FT_Pos pen_x, FT_Pos w_max,
       struct text_rendering_parameters params) -> struct rendered_text_line {
    // Initialize empty result.
    auto result = (struct rendered_text_line){};

    // Make sure the text line is not empty.
    if(params.text_line.empty()) {
        return result;
    }

    // Limit the size of the text line.
    if(std::size(params.text_line) > std::size(result.glyphs)) {
        params.text_line = params.text_line.first(std::size(result.glyphs));
    }

    // Set font size.
    for(auto& face : context->font_faces) {
        FT_Set_Char_Size(
            face, 0, params.font_size * 64, params.dpi, params.dpi);
    }

    // Compute reference space for text line.
    if(auto glyph = render_glyph(context, 0x4D); glyph != nullptr) {
        result.y_ref_min = glyph->bitmap_top - glyph->bitmap.rows;
        result.y_ref_max = glyph->bitmap_top;
    }

    // Initialize bounding box for text line.
    result.bbox =
        FT_BBox{.xMin = 65535, .yMin = 65535, .xMax = -65535, .yMax = -65535};

    // Define rendering history (used for backtracking).
    struct {
        FT_Pos pen_x;
        FT_BBox bbox;
    } history[std::size(result.glyphs)] = {};

    // Initialize ellipsis character data.
    struct {
        FT_Glyph glyph;
        FT_BBox bbox;
        FT_Pos advance_x;
    } ellipsis = {};

    if(auto glyph = render_glyph(context, 0x2026); glyph != nullptr) {
        if(FT_Get_Glyph(glyph, &(ellipsis.glyph)) == FT_Err_Ok) {
            ellipsis.bbox = compute_bbox(glyph);
            ellipsis.advance_x = (glyph->advance.x / 64);
        }
    }

    // Render text line characters.
    for(auto i = size_t{0}; auto c : params.text_line) {
        if(auto glyph = render_glyph(context, c); glyph != nullptr) {
            // Stretch text line's bounding box.
            result.bbox = stretch_bbox(result.bbox, compute_bbox(glyph), pen_x);

            // Check text line's width.
            if((result.bbox.xMax - result.bbox.xMin) > w_max) {
                // If it exceeds horizontal bound, then start backtracking and
                // find a position which can fit the rendered ellipsis
                // character.
                for(i = 0; result.n_glyphs > 0;) {
                    // Destroy the last glyph.
                    FT_Done_Glyph(result.glyphs[i = --result.n_glyphs]);

                    // Restore pen's position.
                    pen_x = history[i].pen_x;

                    // Compute text line's bounding box.
                    result.bbox =
                        stretch_bbox(history[i].bbox, ellipsis.bbox, pen_x);

                    // If the text line fits, then break out of the cycle.
                    if((result.bbox.xMax - result.bbox.xMin) <= w_max) {
                        break;
                    }
                }

                // If such position is at the beginning, then restore pen's
                // initial position and set text line's bounding box to ellipsis
                // character's bounding box.
                if(i == 0) {
                    pen_x = history[0].pen_x;
                    result.bbox = ellipsis.bbox;
                }

                // Add rendered ellipsis glyph to the glyph buffer.
                if(ellipsis.glyph != nullptr) {
                    if(FT_Glyph_Copy(ellipsis.glyph, &(result.glyphs[i])) ==
                       FT_Err_Ok) {
                        // Compute glyph's offset.
                        reinterpret_cast<FT_BitmapGlyph>(result.glyphs[i])
                            ->left += static_cast<int>(pen_x);

                        // Increment the number of glyphs in the glyph buffer.
                        result.n_glyphs = ++i;
                    }
                }

                // And break out of the cycle.
                break;
            }

            // Add rendered glyph to the resulting text line.
            if(FT_Get_Glyph(glyph, &(result.glyphs[i])) == FT_Err_Ok) {
                // Compute glyph's offset.
                reinterpret_cast<FT_BitmapGlyph>(result.glyphs[i])->left +=
                    static_cast<int>(pen_x);

                // Save current pen position and text line's bounding box.
                history[i].pen_x = pen_x;
                history[i].bbox = result.bbox;

                // Increment the number of glyphs in the glyph buffer.
                result.n_glyphs = ++i;
            }

            // Advance pen's position.
            pen_x += (glyph->advance.x / 64);
        }
    }

    // Free memory.
    if(ellipsis.glyph != nullptr) {
        FT_Done_Glyph(ellipsis.glyph);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////
// Text rendering context initialization interface implementation.
////////////////////////////////////////////////////////////////////////////////

auto
initialize(struct text_rendering_context_parameters params)
    -> text_rendering_context {
    auto context = text_rendering_context{
        new struct detail::text_rendering_context(params)};

    if(context->ft == nullptr) {
        context.reset();
    }

    return context;
}

////////////////////////////////////////////////////////////////////////////////
// Text rendering interface implementation.
////////////////////////////////////////////////////////////////////////////////

auto
render(text_rendering_context const& context,
       struct text_rendering_parameters params, struct render_target target)
    -> struct text_rendering_result {
    // Make sure the context is valid.
    if(!context) {
        return {};
    }

    // Make sure the supplied parameters are valid.
    if((params.font_size < 1) || (params.dpi < 1)) {
        return {};
    }

    // Make sure the supplied render target is valid.
    if((target.pixels == nullptr) || (target.w <= 0) || (target.h <= 0)) {
        return {};
    }

    // Compute render target's pitch, if needed.
    target.pitch = ((target.pitch <= 0) ? (target.w * 4) : target.pitch);

    // Render the glyphs.
    auto rendered_text_line = render(context, 0, target.w, params);

    // Obtain the list of rendered glyphs.
    auto glyphs =
        std::span{rendered_text_line.glyphs, rendered_text_line.n_glyphs};

    // Copy glyphs to the render target.
    if(true) {
        // Compute baseline's offset.
        auto baseline_dx = -rendered_text_line.bbox.xMin;
        auto baseline_dy = -rendered_text_line.y_ref_min;

        // Center the baseline.
        if(true) {
            // Compute text line's reference height.
            auto h =
                (rendered_text_line.y_ref_max - rendered_text_line.y_ref_min);

            // Update the baseline.
            if(target.h > h) {
                if(rendered_text_line.y_ref_min < 0) {
                    h -= rendered_text_line.y_ref_min;
                }

                baseline_dy += (target.h - h) / 2;
            }
        }

        for(auto _ : glyphs) {
            // Obtain current glyph.
            auto glyph = reinterpret_cast<FT_BitmapGlyph>(_);

            // Compute offsets.
            auto dst_dx = glyph->left + baseline_dx;
            auto dst_dy = target.h - glyph->top - baseline_dy;

            auto src_dy = ((dst_dy < 0) ? -dst_dy : 0);
            dst_dy = ((dst_dy < 0) ? 0 : dst_dy);

            // Compute target width based on glyph bitmap's width and the amount
            // of space left in the render target.
            auto w = std::min(static_cast<FT_Pos>(glyph->bitmap.width),
                              static_cast<FT_Pos>(target.w - dst_dx));

            // Compute target height based on glyph bitmap's height and the
            // amount of space left in the render target.
            auto h = std::min(static_cast<FT_Pos>(glyph->bitmap.rows - src_dy),
                              static_cast<FT_Pos>(target.h - dst_dy));

            // Compute glyph bitmap's pitch.
            auto bitmap_pitch =
                ((glyph->bitmap.pitch < 0) ? -(glyph->bitmap.pitch)
                                           : +(glyph->bitmap.pitch));

            // Copy glyph's bitmap to the render target.
            if((w > 0) && (h > 0)) {
                auto line_src = glyph->bitmap.buffer + (bitmap_pitch * src_dy);
                for(auto j = dst_dy; j < (dst_dy + h); ++j) {
                    auto src = line_src;
                    auto dst = target.pixels + target.pitch * j + 4 * dst_dx;

                    for(auto k = 0; k != w; ++k, dst += 4) {
                        std::fill_n(dst, 4, *(src++));
                    }

                    line_src += bitmap_pitch;
                }
            }
        }
    }

    // Free memory.
    for(auto glyph : glyphs) {
        FT_Done_Glyph(glyph);
    }

    // Return the result.
    return {.rectangle = //
            {.x = rendered_text_line.bbox.xMin,
             .y = rendered_text_line.bbox.yMin,
             .w = rendered_text_line.bbox.xMax - rendered_text_line.bbox.xMin,
             .h = rendered_text_line.bbox.yMax - rendered_text_line.bbox.yMin},
            .n_code_points_consumed = glyphs.size()};
}

} // namespace rose
