// Copyright Nezametdinov E. Ildus 2023.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "filesystem.hh"
#include "rendering_theme.hh"

#include <algorithm>
#include <cstdlib>

namespace rose {

////////////////////////////////////////////////////////////////////////////////
// Theme initialization interface implementation.
////////////////////////////////////////////////////////////////////////////////

auto
initialize_theme() -> struct theme {
    // Initialize default theme.
    auto theme = (struct theme){
        .font_size = 16,
        .color_scheme = {
            .panel_background = {0x26, 0x26, 0x26, 0xFF},
            .panel_foreground = {0xFF, 0xFF, 0xFF, 0xFF},
            .panel_highlight = {0x40, 0x26, 0x26, 0xFF},
            .menu_background = {0x21, 0x21, 0x21, 0xFF},
            .menu_foreground = {0xFF, 0xFF, 0xFF, 0xFF},
            .menu_highlight0 = {0x3B, 0x1E, 0x1E, 0xFF},
            .menu_highlight1 = {0x54, 0x1E, 0x1E, 0xFF},
            .surface_background0 = {0xCC, 0xCC, 0xCC, 0xFF},
            .surface_background1 = {0x99, 0x99, 0x99, 0xFF},
            .surface_resizing_background0 = {0xCC, 0xCC, 0xCC, 0x80},
            .surface_resizing_background1 = {0x99, 0x99, 0x99, 0x80},
            .surface_resizing = {0x1E, 0x1E, 0x1E, 0x80},
            .workspace_background = {0x33, 0x33, 0x33, 0xFF}}};

    // Specify configuration paths.
    std::filesystem::path paths[] = {
        std::string{std::getenv("HOME")} + "/.config/rosewm/theme",
        "/etc/rosewm/theme"};

    // Try reading theme from configuration paths.
    for(auto const& path : paths) {
        if(auto file = filesystem::file_stream{std::fopen(path.c_str(), "rb")};
           file) {
            // Read font size.
            theme.font_size = std::fgetc(file.get());
            theme.font_size = std::clamp(theme.font_size, 1, 144);

            // Skip panel data.
            std::fgetc(file.get());
            std::fgetc(file.get());

#define read_color_(color)                                          \
    if(fread(color.rgba, sizeof(color.rgba), 1, file.get()) != 1) { \
        return theme;                                               \
    }

            // Read color scheme.
            read_color_(theme.color_scheme.panel_background);
            read_color_(theme.color_scheme.panel_foreground);
            read_color_(theme.color_scheme.panel_highlight);
            read_color_(theme.color_scheme.menu_background);
            read_color_(theme.color_scheme.menu_foreground);
            read_color_(theme.color_scheme.menu_highlight0);
            read_color_(theme.color_scheme.menu_highlight1);
            read_color_(theme.color_scheme.surface_background0);
            read_color_(theme.color_scheme.surface_background1);
            read_color_(theme.color_scheme.surface_resizing_background0);
            read_color_(theme.color_scheme.surface_resizing_background1);
            read_color_(theme.color_scheme.surface_resizing);
            read_color_(theme.color_scheme.workspace_background);

#undef read_color_
        }
    }

    return theme;
}

} // namespace rose
