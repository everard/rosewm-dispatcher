// Copyright Nezametdinov E. Ildus 2023.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_2F106E4061E0481D847FEC432466A848
#define H_2F106E4061E0481D847FEC432466A848

namespace rose {

////////////////////////////////////////////////////////////////////////////////
// Color.
////////////////////////////////////////////////////////////////////////////////

struct color {
    unsigned char rgba[4];
};

////////////////////////////////////////////////////////////////////////////////
// Color scheme.
////////////////////////////////////////////////////////////////////////////////

struct color_scheme {
    struct color panel_background;
    struct color panel_foreground;
    struct color panel_highlight;
    struct color menu_background;
    struct color menu_foreground;
    struct color menu_highlight0;
    struct color menu_highlight1;
    struct color surface_background0;
    struct color surface_background1;
    struct color surface_resizing_background0;
    struct color surface_resizing_background1;
    struct color surface_resizing;
    struct color workspace_background;
};

////////////////////////////////////////////////////////////////////////////////
// Theme.
////////////////////////////////////////////////////////////////////////////////

struct theme {
    int font_size;
    struct color_scheme color_scheme;
};

////////////////////////////////////////////////////////////////////////////////
// Theme initialization interface.
////////////////////////////////////////////////////////////////////////////////

auto
initialize_theme() -> struct theme;

} // namespace rose

#endif // H_2F106E4061E0481D847FEC432466A848
