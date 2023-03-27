// Copyright Nezametdinov E. Ildus 2023.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "buffer.hh"
#include "executables_database.hh"
#include "execution.hh"
#include "filesystem.hh"
#include "ipc.hh"
#include "rendering_text.hh"
#include "rendering_theme.hh"
#include "text_input.hh"
#include "unicode.hh"

#include <cstdlib>
#include <fstream>
#include <vector>

namespace rose {

////////////////////////////////////////////////////////////////////////////////
// Window.
////////////////////////////////////////////////////////////////////////////////

namespace detail {

using window_deleter = decltype([](SDL_Window* window) {
    if(window != nullptr) {
        SDL_DestroyWindow(window);
    }
});

} // namespace detail

using window = std::unique_ptr<SDL_Window, detail::window_deleter>;

////////////////////////////////////////////////////////////////////////////////
// Renderer.
////////////////////////////////////////////////////////////////////////////////

namespace detail {

using renderer_deleter = decltype([](SDL_Renderer* renderer) {
    if(renderer != nullptr) {
        SDL_DestroyRenderer(renderer);
    }
});

} // namespace detail

using renderer = std::unique_ptr<SDL_Renderer, detail::renderer_deleter>;

////////////////////////////////////////////////////////////////////////////////
// Texture.
////////////////////////////////////////////////////////////////////////////////

namespace detail {

using texture_deleter = decltype([](SDL_Texture* texture) {
    if(texture != nullptr) {
        SDL_DestroyTexture(texture);
    }
});

} // namespace detail

using texture = std::unique_ptr<SDL_Texture, detail::texture_deleter>;

////////////////////////////////////////////////////////////////////////////////
// Font data reading utility function.
////////////////////////////////////////////////////////////////////////////////

auto
read_font_data() -> std::vector<std::vector<unsigned char>> {
    // Obtain configuration paths.
    std::filesystem::path paths[] = {
        std::string{std::getenv("HOME")} + "/.config/rosewm/fonts",
        "/etc/rosewm/fonts"};

    // Read fonts.
    auto fonts = std::vector<std::vector<unsigned char>>{};
    for(auto font_name = std::string{}; auto const& path : paths) {
        if(auto file = std::ifstream{path}; !file) {
            continue;
        } else {
            for(std::getline(file, font_name); !(file.eof());) {
                if(fonts.emplace_back(filesystem::read(font_name));
                   fonts.size() > 7) {
                    break;
                }
            }
        }

        break;
    }

    return fonts;
}

} // namespace rose

////////////////////////////////////////////////////////////////////////////////
// Program entry point.
////////////////////////////////////////////////////////////////////////////////

int
main() {
    // Check if the required environment variables are set.
    if((std::getenv("HOME") == nullptr)) {
        return EXIT_FAILURE;
    }

    // Start executor process.
    auto pipe = rose::run_executor_process();
    if(pipe == -1) {
        return EXIT_FAILURE;
    }

    // Initialize text rendering context.
    auto text_rendering_context = rose::text_rendering_context{};
    if(auto fonts = rose::read_font_data(); true) {
        text_rendering_context = rose::initialize(
            rose::text_rendering_context_parameters{.fonts = fonts});
    }

    if(!text_rendering_context) {
        return EXIT_FAILURE;
    }

    // Initialize the theme.
    auto theme = rose::initialize_theme();

    // Initialize SDL subsystems.
    if(SDL_Init(SDL_INIT_VIDEO) != 0) {
        return EXIT_FAILURE;
    }

    // Make sure SDL state is cleaned-up upon exit.
    struct exit_guard {
        ~exit_guard() {
            SDL_Quit();
        }
    } exit_guard;

    // Create a window.
    auto window = rose::window{SDL_CreateWindow(
        "dispatcher", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 100,
        100, SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN)};

    if(!window) {
        return EXIT_FAILURE;
    }

    // Initialize window flags.
    auto is_window_visible = false, is_window_updated = true;

    // Define functions for hiding and showing the window.
    auto show_window = [&]() {
        is_window_visible = is_window_updated = true;
        SDL_ShowWindow(window.get());
    };

    auto hide_window = [&is_window_visible, &window]() {
        is_window_visible = false;
        SDL_HideWindow(window.get());
    };

    // Create a renderer.
    auto renderer = rose::renderer{
        SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE)};

    if(!renderer) {
        return EXIT_FAILURE;
    }

    // Initialize an empty texture.
    struct {
        rose::texture handle;
        int w, h;
    } texture = {};

    // Initialize text input.
    auto text_input = rose::text_input{};

    // Initialize shared state.
    auto state = rose::shared_state{
        .event_idx = SDL_RegisterEvents(1), .is_program_running = true};

    // Initialize IPC client.
    auto ipc_client = rose::initialize_ipc_client(state);

    // Initialize database of executable files.
    auto database = rose::executables_database{};

    // Initialize execution flag.
    auto is_prompt_privileged = false;

    // Run event loop.
    auto string = std::u8string{};
    for(SDL_StartTextInput(); state.is_program_running;) {
        // Process events.
        if(SDL_Event event; SDL_WaitEvent(&event) != 0) {
            if(event.type == state.event_idx) {
                // Process IPC request.
                switch(static_cast<rose::request>(event.user.code)) {
                    case rose::request::prompt_normal:
                        show_window(), text_input.clear();
                        is_prompt_privileged = false;
                        break;

                    case rose::request::prompt_privileged:
                        show_window(), text_input.clear();
                        is_prompt_privileged = true;
                        break;

                    case rose::request::reload_database:
                        database.initialize();
                        break;

                    case rose::request::reload_theme:
                        theme = rose::initialize_theme();
                        break;

                    default:
                        break;
                }
            } else {
                switch(event.type) {
                    case SDL_QUIT:
                        rose::stop(ipc_client);
                        state.is_program_running = false;
                        break;

                    case SDL_WINDOWEVENT:
                        switch(event.window.event) {
                            case SDL_WINDOWEVENT_FOCUS_LOST:
                                hide_window();
                                break;

                            case SDL_WINDOWEVENT_SIZE_CHANGED:
                                is_window_updated = true;
                                break;

                            default:
                                break;
                        }

                        break;

                    case SDL_KEYDOWN:
                        switch(event.key.keysym.sym) {
                            case SDLK_ESCAPE:
                                hide_window();
                                break;

                            case SDLK_BACKSPACE:
                                text_input.erase();
                                break;

                            case SDLK_TAB:
                                database.autocomplete(text_input, string);
                                text_input.enter(string);
                                break;

                            case SDLK_RETURN:
                                database.modify_command_string(
                                    text_input, string);

                                if(is_prompt_privileged) {
                                    rose::execute(ipc_client, string);
                                } else {
                                    rose::execute(pipe, string);
                                }

                                hide_window();
                                break;

                            default:
                                break;
                        }

                        is_window_updated = true;
                        break;

                    case SDL_TEXTINPUT:
                        text_input.enter(
                            reinterpret_cast<char8_t*>(event.text.text));

                        is_window_updated = true;
                        break;

                    default:
                        break;
                }
            }
        }

        // If the window is hidden, then skip rendering.
        if(!is_window_visible) {
            continue;
        }

        // Render a frame, if needed.
        auto w = 0, h = 0, window_w = 0, window_h = 0, pitch = 0;
        for(; is_window_updated; is_window_updated = false) {
            // Obtain window size.
            SDL_GetRendererOutputSize(renderer.get(), &w, &h);
            SDL_GetWindowSize(window.get(), &window_w, &window_h);

            // Make sure the size is correct.
            if((w <= 0) || (h <= 0) || (window_w <= 0) || (window_h <= 0)) {
                break;
            }

            // Compute effective DPI.
            auto dpi = static_cast<int>((96.0 * w) / window_w);

            // Create a texture, if needed.
            if(!(texture.handle) || (texture.w != w) || (texture.h != h)) {
                texture.handle.reset(
                    SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_RGBA8888,
                                      SDL_TEXTUREACCESS_STREAMING, w, h));

                texture.w = w;
                texture.h = h;
            }

            // Render text.
            for(void* buffer = nullptr; texture.handle;) {
                // Lock the texture.
                SDL_LockTexture(texture.handle.get(), nullptr, &buffer, &pitch);
                if(buffer == nullptr) {
                    break;
                }

                // Initialize render target description.
                auto render_target = rose::render_target{
                    .pixels = static_cast<unsigned char*>(buffer),
                    .w = w,
                    .h = h,
                    .pitch = pitch};

                // Clear the texture.
                std::fill_n(render_target.pixels, h * pitch, 0);

                // Compose the text which will be rendered to the texture.
                if(string = (is_prompt_privileged ? u8"# " : u8"$ "); true) {
                    database.lookup_suggestions(text_input, string);
                }

                // Render the text.
                render(text_rendering_context,
                       {.font_size = theme.font_size,
                        .dpi = dpi,
                        .text_line = rose::convert_utf8_to_utf32(string)},
                       render_target);

                // Unlock the texture.
                SDL_UnlockTexture(texture.handle.get());
                break;
            }

            // Set background color.
            SDL_SetRenderDrawColor(renderer.get(), //
                                   theme.color_scheme.panel_background.rgba[0],
                                   theme.color_scheme.panel_background.rgba[1],
                                   theme.color_scheme.panel_background.rgba[2],
                                   theme.color_scheme.panel_background.rgba[3]);

            // Clear color.
            SDL_RenderClear(renderer.get());

            // Render the text.
            if(texture.handle) {
                SDL_SetTextureBlendMode(
                    texture.handle.get(), SDL_BLENDMODE_ADD);

                SDL_SetTextureColorMod( //
                    texture.handle.get(),
                    theme.color_scheme.panel_foreground.rgba[0],
                    theme.color_scheme.panel_foreground.rgba[1],
                    theme.color_scheme.panel_foreground.rgba[2]);

                SDL_RenderCopy(
                    renderer.get(), texture.handle.get(), nullptr, nullptr);
            }

            // Swap buffers.
            SDL_RenderPresent(renderer.get());
        }
    }

    return EXIT_SUCCESS;
}
