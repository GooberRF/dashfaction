#include <windows.h>
#include <patch_common/CodeInjection.h>
#include <patch_common/CallHook.h>
#include <xlog/xlog.h>
#include "../os/console.h"
#include "../rf/misc.h"
#include "../rf/file/file.h"
#include "../rf/gr/gr.h"
#include "../rf/ui.h"

const char screenshot_dir_name[] = "screenshots";
static int screenshot_path_id = -1;
static std::unique_ptr<byte* []> g_screenshot_scanlines_buf;

void game_init_screenshot_dir()
{
    auto full_path = string_format("%s\\%s", rf::root_path, screenshot_dir_name);
    if (CreateDirectoryA(full_path.c_str(), nullptr))
        xlog::info("Created screenshots directory");
    else if (GetLastError() != ERROR_ALREADY_EXISTS)
        xlog::error("Failed to create screenshots directory %lu", GetLastError());
    screenshot_path_id = rf::file_add_path(screenshot_dir_name, "", true);
}

CodeInjection game_print_screen_injection{
    0x004366E0,
    []() {
        if (screenshot_path_id == -1) {
            game_init_screenshot_dir();
        }
    },
};

CodeInjection jpeg_write_bitmap_overflow_fix1{
    0x0055A066,
    [](auto& regs) {
        g_screenshot_scanlines_buf = std::make_unique<byte* []>(rf::gr_screen.max_h);
        regs.ecx = reinterpret_cast<int32_t>(g_screenshot_scanlines_buf.get());
        regs.eip = 0x0055A06D;
    },
};

CodeInjection jpeg_write_bitmap_overflow_fix2{
    0x0055A0DF,
    [](auto& regs) {
        regs.eax = reinterpret_cast<int32_t>(g_screenshot_scanlines_buf.get());
        regs.eip = 0x0055A0E6;
    },
};

int bm_load_if_exists(const char* name, int unk, bool generate_mipmaps)
{
    if (rf::file_exists(name)) {
        return rf::bm_load(name, unk, generate_mipmaps);
    }
    else {
        return -1;
    }
}

CallHook<void(int, int, int, rf::GrMode)> game_render_cursor_gr_bitmap_hook{
    0x004354E4,
    [](int bm_handle, int x, int y, rf::GrMode mode) {
        if (rf::ui_scale_y >= 2.0f) {
            static int cursor_1_bmh = bm_load_if_exists("cursor_1.tga", -1, false);
            if (cursor_1_bmh != -1) {
                bm_handle = cursor_1_bmh;
            }
        }
        game_render_cursor_gr_bitmap_hook.call_target(bm_handle, x, y, mode);
    },
};

static ConsoleCommand2 screenshot_cmd{
    "screenshot",
    []() {
        auto game_print_screen = addr_as_ref<void(char* filename)>(0x004366E0);

        char buf[MAX_PATH];
        game_print_screen(buf);
        rf::console_printf("Screenshot saved in %s", buf);
    },
};

CodeInjection gameplay_render_frame_display_full_screen_image_injection{
    0x00432CAF,
    [](auto& regs) {
        // Change gr mode to one that uses alpha blending for Display_Fullscreen_Image event handling in
        // gameplay_render_frame function
        static rf::GrMode mode{
            rf::TEXTURE_SOURCE_WRAP,
            rf::COLOR_SOURCE_TEXTURE,
            rf::ALPHA_SOURCE_VERTEX_TIMES_TEXTURE,
            rf::ALPHA_BLEND_ALPHA,
            rf::ZBUFFER_TYPE_NONE,
            rf::FOG_ALLOWED
        };
        regs.edx = mode;
    },
};

void game_apply_patch()
{
    // Override screenshot directory
    write_mem_ptr(0x004367CA + 2, &screenshot_path_id);
    game_print_screen_injection.install();

    // Fix buffer overflow in screenshot to JPG conversion code
    jpeg_write_bitmap_overflow_fix1.install();
    jpeg_write_bitmap_overflow_fix2.install();

    // Bigger cursor bitmap support
    game_render_cursor_gr_bitmap_hook.install();

    // Support textures with alpha channel in Display_Fullscreen_Image event
    gameplay_render_frame_display_full_screen_image_injection.install();

    // Commands
    screenshot_cmd.register_cmd();
}
