#include <algorithm>
#include <cmath>
#include <cstring>
#include <patch_common/CallHook.h>
#include <patch_common/FunHook.h>
#include <patch_common/CodeInjection.h>
#include "xlog/xlog.h"
#include "../rf/gr/gr.h"
#include "../rf/hud.h"
#include "../rf/entity.h"
#include "../rf/weapon.h"
#include "../rf/gr/gr_font.h"
#include "../rf/player/player.h"
#include "../rf/multi.h"
#include "../graphics/gr.h"
#include "../main/main.h"
#include "../misc/alpine_settings.h"
#include "../misc/misc.h"
#include "../os/console.h"
#include <common/utils/string-utils.h>
#include "hud_internal.h"

float g_hud_ammo_scale = 1.0f;
bool g_displaying_custom_reticle = false;
static bool g_reticle_locked = false;
static int g_reticle_weapon_slot = -1; // weapon whose reticle is being drawn this frame

static float hud_reticle_scale()
{
    return (g_alpine_game_config.big_hud ? 2.0f : 1.0f) * g_alpine_game_config.get_reticle_scale();
}

static const ReticleConfig& reticle_active_config()
{
    if (g_reticle_weapon_slot >= 0 && g_reticle_weapon_slot < rf::num_weapon_types) {
        const char* name = rf::weapon_types[g_reticle_weapon_slot].name;
        return g_alpine_game_config.reticle_for_weapon(name ? name : "");
    }
    return g_alpine_game_config.reticle;
}

// Hollow regular n-gon (ring) centered at cx,cy (clip-relative, float) with inradius r and line thickness t.
// Built from n quads via tmapper, same as gr_rect does internally. a0 = angle of first vertex.
static void hud_draw_ngon_ring(float cx, float cy, int n, float r, float t, float a0)
{
    constexpr float pi = 3.14159265f;
    const float k = 1.0f / std::cos(pi / n); // inradius -> circumradius
    const float ro = (r + t * 0.5f) * k;
    const float ri = std::max(0.0f, r - t * 0.5f) * k;
    cx += rf::gr::screen.offset_x;
    cy += rf::gr::screen.offset_y;
    rf::gr::set_texture(-1, -1);
    for (int i = 0; i < n; ++i) {
        const float a1 = a0 + 2 * pi * i / n;
        const float a2 = a0 + 2 * pi * (i + 1) / n;
        rf::gr::Vertex v[4]{};
        rf::gr::Vertex* vp[4] = {&v[0], &v[1], &v[2], &v[3]};
        auto set = [&](int idx, float radius, float ang) {
            v[idx].sx = cx + radius * std::cos(ang);
            v[idx].sy = cy + radius * std::sin(ang);
            v[idx].sw = 1.0f;
        };
        set(0, ro, a1);
        set(1, ro, a2);
        set(2, ri, a2);
        set(3, ri, a1);
        rf::gr::tmapper(4, vp, static_cast<rf::gr::TMapperFlags>(0), rf::gr::rect_mode);
    }
}

// Vector reticle: any mix of dot, crosshair, square, circle, triangle at screen center.
// Rects for dot/cross (pixel exact), n-gon rings for the outline shapes.
static void hud_render_vector_reticle(const ReticleConfig& cfg)
{
    constexpr float pi = 3.14159265f;
    const int style = cfg.style;
    const float scale = hud_reticle_scale();
    auto px = [scale](int v) { return static_cast<int>(v * scale + 0.5f); };
    const int cx = rf::gr::clip_width() / 2;
    const int cy = rf::gr::clip_height() / 2;
    const int d = std::max(1, px(cfg.dot_size));
    const int l = std::max(1, px(cfg.cross_length));
    const int t = std::max(1, px(cfg.cross_thickness));
    const int g = px(cfg.cross_gap);
    const int o = px(cfg.outline);
    auto deg = [](int v) { return v * pi / 180.0f; };

    // shape = the style bit each primitive belongs to, used to pick which get an outline
    struct R { int shape, x, y, w, h; };
    R rects[5];
    int n = 0;
    if (style & 1) {
        rects[n++] = {1, cx - d / 2, cy - d / 2, d, d};
    }
    if (style & 2) {
        rects[n++] = {2, cx - g - l, cy - t / 2, l, t}; // left
        rects[n++] = {2, cx + g, cy - t / 2, l, t};     // right
        rects[n++] = {2, cx - t / 2, cy - g - l, t, l}; // top
        rects[n++] = {2, cx - t / 2, cy + g, t, l};     // bottom
    }

    // ring shapes: {shape, sides, inradius, line thickness, first vertex angle}
    struct Ring { int shape, sides; float r; float t; float a0; };
    Ring rings[3];
    int nr = 0;
    if (style & 4) {
        rings[nr++] = {4, 4, static_cast<float>(px(cfg.square_size)),
                       static_cast<float>(std::max(1, px(cfg.square_thickness))),
                       -0.75f * pi + deg(cfg.square_angle)};
    }
    if (style & 8) {
        const float r = static_cast<float>(px(cfg.circle_size));
        rings[nr++] = {8, std::clamp(static_cast<int>(r * 2), 24, 128), r,
                       static_cast<float>(std::max(1, px(cfg.circle_thickness))), 0.0f};
    }
    if (style & 16) {
        rings[nr++] = {16, 3, px(cfg.triangle_size) * 0.5f, // inradius = circumradius/2
                       static_cast<float>(std::max(1, px(cfg.triangle_thickness))),
                       -0.5f * pi + deg(cfg.triangle_angle)}; // 0 deg = tip up
    }

    // ring center on the pixel center of (cx, cy), matching the rect convention
    const float fcx = cx + 0.5f;
    const float fcy = cy + 0.5f;

    rf::Color clr = g_reticle_locked
        ? rf::Color::from_hex(cfg.locked_color.value_or(0xFF0000FF))
        : rf::Color::from_hex(cfg.color.value_or(0x00FF00FF));

    if (o > 0) {
        const int om = cfg.outline_shapes;
        rf::gr::set_color(rf::Color::from_hex(cfg.outline_color));
        for (int i = 0; i < n; ++i) {
            if (rects[i].shape & om) {
                rf::gr::rect(rects[i].x - o, rects[i].y - o, rects[i].w + 2 * o, rects[i].h + 2 * o);
            }
        }
        for (int i = 0; i < nr; ++i) {
            if (rings[i].shape & om) {
                hud_draw_ngon_ring(fcx, fcy, rings[i].sides, rings[i].r, rings[i].t + 2 * o, rings[i].a0);
            }
        }
    }
    rf::gr::set_color(clr);
    for (int i = 0; i < n; ++i) {
        rf::gr::rect(rects[i].x, rects[i].y, rects[i].w, rects[i].h);
    }
    for (int i = 0; i < nr; ++i) {
        hud_draw_ngon_ring(fcx, fcy, rings[i].sides, rings[i].r, rings[i].t, rings[i].a0);
    }
}

CallHook<void(int, int, int, rf::gr::Mode)> hud_render_ammo_gr_bitmap_hook{
    {
        // hud_render_ammo_clip
        0x0043A5E9u,
        0x0043A637u,
        0x0043A680u,
        // hud_render_ammo_power
        0x0043A988u,
        0x0043A9DDu,
        0x0043AA24u,
        // hud_render_ammo_no_clip
        0x0043AE80u,
        0x0043AEC3u,
        0x0043AF0Au,
    },
    [](int bm_handle, int x, int y, rf::gr::Mode mode) {
        hud_scaled_bitmap(bm_handle, x, y, g_hud_ammo_scale, mode);
    },
};

CallHook<void(int, int, int, rf::gr::Mode)> render_reticle_gr_bitmap_hook{
    {
        0x0043A499,
        0x0043A4FE,
    },
    [](int bm_handle, int x, int y, rf::gr::Mode mode) {
        const ReticleConfig& cfg = reticle_active_config();
        if (cfg.style) {
            hud_render_vector_reticle(cfg);
            return;
        }
        float scale = hud_reticle_scale();
        int clip_w = rf::gr::clip_width();
        int clip_h = rf::gr::clip_height();

        x = static_cast<int>((x - clip_w / 2.0F) * scale + clip_w / 2.0F);
        y = static_cast<int>((y - clip_h / 2.0F) * scale + clip_h / 2.0F);

        hud_scaled_bitmap(bm_handle, x, y, scale, mode);
    },
};

CallHook<void(int, int, int, int)> render_reticle_set_color_hook{
    0x0043A4D7,
    [](int r, int g, int b, int a) {
        rf::Color clr{};
        const auto& color_override = reticle_active_config().color;

        if (g_displaying_custom_reticle && !g_alpine_game_config.colorize_custom_reticles) {
            clr = {255, 255, 255, 255}; // white
        }
        else if (color_override) {
            clr = rf::Color::from_hex(*color_override);
        }
        else {
            clr = g_displaying_custom_reticle ?
                rf::Color{255, 255, 255, 255} : // white
                rf::Color{0, 255, 0, 255}; // green
        }

        render_reticle_set_color_hook.call_target(clr.red, clr.green, clr.blue, clr.alpha);
    },
};

CallHook<void(int, int, int, int)> render_reticle_locked_set_color_hook{
    0x0043A472,
    [](int r, int g, int b, int a) {
        rf::Color clr{};
        const auto& color_override = reticle_active_config().locked_color;

        if (g_displaying_custom_reticle && !g_alpine_game_config.colorize_custom_reticles) {
            clr = {255, 255, 255, 255}; // white
        }
        else if (color_override) {
            clr = rf::Color::from_hex(*color_override);
        }
        else {
            clr = g_displaying_custom_reticle ?
                rf::Color{255, 255, 255, 255} : // white
                rf::Color{255, 0, 0, 255}; // red
        }

        render_reticle_locked_set_color_hook.call_target(clr.red, clr.green, clr.blue, clr.alpha);
    },
};

CodeInjection render_reticle_check_custom_injection{
    0x0043A3B1,
    [](auto& regs) {
        int weap_slot = regs.eax;
        g_reticle_locked = false;
        g_reticle_weapon_slot = weap_slot;

        if (weap_slot >= 0) {
            bool big_reticle = hud_reticle_scale() > 1.0f; // reticle is using _1 variant
            g_displaying_custom_reticle = weapon_reticle_is_customized(weap_slot, big_reticle);

            // special case for rocket lock on reticle
            if (weap_slot == rf::rocket_launcher_weapon_type) {
                rf::Player* pp = regs.edi;
                if (rf::player_fpgun_locked_on(pp)) {
                    g_reticle_locked = true;
                    g_displaying_custom_reticle = rocket_locked_reticle_is_customized(big_reticle);
                }
            }
        }
        else {
            g_displaying_custom_reticle = false;
        }
    },
};

// A weapon with no FP mesh is confusing, render the weapon's display name
// so at least you know what gun you have out.
bool weapon_has_first_person_mesh(int weapon_type)
{
    if (weapon_type < 0 || weapon_type >= rf::num_weapon_types) {
        return true;
    }
    const char* filename = rf::weapon_types[weapon_type].first_person_vmesh_filename;
    return filename && *filename;
}

void hud_render_weapon_name_label(int weapon_type)
{
    // Hidden with the first person weapon itself.
    if (!rf::local_player || !rf::local_player->settings.render_fpgun) {
        return;
    }
    if (weapon_has_first_person_mesh(weapon_type)) {
        return;
    }
    const char* name = rf::weapon_types[weapon_type].display_name;
    if (!name || !*name) {
        name = rf::weapon_types[weapon_type].name;   // fall back to the class name
    }
    if (!name || !*name) {
        return;
    }

    const int font_id = rf::hud_text_font_num;
    const auto [text_w, text_h] = rf::gr::get_string_size(name, font_id);

    const int pad = std::max(4, static_cast<int>(6 * g_hud_ammo_scale));
    const int box_w = text_w + pad * 2;
    const int box_h = text_h + pad;
    const int margin_x = std::max(8, static_cast<int>(12 * g_hud_ammo_scale));
    const int margin_y = std::max(40, static_cast<int>(64 * g_hud_ammo_scale));
    const int box_x = rf::gr::screen_width() - box_w - margin_x;
    const int box_y = rf::gr::screen_height() - box_h - margin_y;

    rf::gr::set_color(0, 0, 0, 140);
    rf::gr::rect(box_x, box_y, box_w, box_h);
    rf::gr::set_color(rf::hud_full_color);
    hud_rect_border(box_x, box_y, box_w, box_h, 1);
    rf::gr::string(box_x + pad, box_y + pad / 2, name, font_id);
}

FunHook<void(rf::Entity*, int, int, bool)> hud_render_ammo_hook{
    0x0043A510,
    [](rf::Entity *entity, int weapon_type, int offset_y, bool is_inactive) {
        offset_y = static_cast<int>(offset_y * g_hud_ammo_scale);
        hud_render_ammo_hook.call_target(entity, weapon_type, offset_y, is_inactive);
        if (!is_inactive) {
            hud_render_weapon_name_label(weapon_type);
        }
    },
};

FunHook<void(rf::Entity*, int)> hud_render_ammo_no_clip_hook{
    0x0043ADD0,
    [](rf::Entity* entity, int weapon_type) {
        hud_render_ammo_no_clip_hook.call_target(entity, weapon_type);
        hud_render_weapon_name_label(weapon_type);
    },
};

void hud_weapons_set_big(bool is_big)
{
    rf::HudItem ammo_hud_items[] = {
        rf::hud_ammo_bar,
        rf::hud_ammo_signal,
        rf::hud_ammo_icon,
        rf::hud_ammo_in_clip_text_ul_region_coord,
        rf::hud_ammo_in_clip_text_width_and_height,
        rf::hud_ammo_in_inv_text_ul_region_coord,
        rf::hud_ammo_in_inv_text_width_and_height,
        rf::hud_ammo_bar_position_no_clip,
        rf::hud_ammo_signal_position_no_clip,
        rf::hud_ammo_icon_position_no_clip,
        rf::hud_ammo_in_inv_ul_region_coord_no_clip,
        rf::hud_ammo_in_inv_text_width_and_height_no_clip,
        rf::hud_ammo_in_clip_ul_coord,
        rf::hud_ammo_in_clip_width_and_height,
    };
    g_hud_ammo_scale = is_big ? 1.875f : 1.0f;
    for (auto item_num : ammo_hud_items) {
        rf::hud_coords[item_num] = hud_scale_coords(rf::hud_coords[item_num], g_hud_ammo_scale);
    }
    rf::hud_ammo_font = rf::gr::load_font(is_big ? "biggerfont.vf" : "bigfont.vf");
}


bool hud_weapons_is_double_ammo()
{
    if (rf::is_multi) {
        return false;
    }
    rf::Entity* entity = rf::entity_from_handle(rf::local_player->entity_handle);
    if (!entity) {
        return false;
    }
    auto weapon_type = entity->ai.current_primary_weapon;
    return weapon_type == rf::machine_pistol_weapon_type || weapon_type == rf::machine_pistol_special_weapon_type;
}

static bool is_mouse_wheel_down() {
    static bool was_mouse_3_down = false;
    static HighResTimer mouse_3_up_cool_down_timer{};
    constexpr int MOUSE_BUTTON_3 = 2;
    const bool is_mouse_3_down = rf::mouse_button_is_down(MOUSE_BUTTON_3);
    if (!is_mouse_3_down && was_mouse_3_down) {
        // Use a cool down after key up to avoid accidental
        // weapon cycle selection.
        constexpr uint64_t COOL_DOWN_MS = 64;
        mouse_3_up_cool_down_timer.set_ms(COOL_DOWN_MS);
    } else if (is_mouse_3_down) {
        mouse_3_up_cool_down_timer.invalidate();
    }
    was_mouse_3_down = is_mouse_3_down;
    return is_mouse_3_down || (mouse_3_up_cool_down_timer.valid()
        && !mouse_3_up_cool_down_timer.elapsed());
}

FunHook<void(rf::Player*, int, bool)> player_select_next_primary_hook{
    0x004A3770,
    [] (rf::Player* const player, const int a2, const bool play_sound) {
        if (!is_mouse_wheel_down() || rf::hud_render_weapon_cycle) {
            player_select_next_primary_hook.call_target(player, a2, play_sound);
        }
    },
};

FunHook<void(rf::Player*, int, bool)> player_select_prev_primary_hook{
    0x004A3BE0,
    [] (rf::Player* const player, const int a2, const bool play_sound) {
        if (!is_mouse_wheel_down() || rf::hud_render_weapon_cycle) {
            player_select_prev_primary_hook.call_target(player, a2, play_sound);
        }
    },
};

// ---- reticle console commands ----
// All ui_reticle_* / ui_color_reticle* commands edit one target: the default config, or one weapon's override.
static std::string g_reticle_edit_weapon; // canonical weapon class name, empty = default

ReticleConfig& reticle_edit_target()
{
    if (!g_reticle_edit_weapon.empty()) {
        auto it = g_alpine_game_config.weapon_reticles.find(g_reticle_edit_weapon);
        if (it != g_alpine_game_config.weapon_reticles.end()) {
            return it->second;
        }
        g_reticle_edit_weapon.clear(); // override was removed
    }
    return g_alpine_game_config.reticle;
}

std::string reticle_edit_target_label()
{
    return g_reticle_edit_weapon.empty() ? "default" : "weapon: " + g_reticle_edit_weapon;
}

static const char* reticle_shape_names[] = {"dot", "cross", "square", "circle", "triangle"}; // bit i = 1 << i

static std::string reticle_style_to_string(int style)
{
    std::string s;
    for (int i = 0; i < 5; ++i) {
        if (style & (1 << i)) {
            s += (s.empty() ? "" : "+") + std::string(reticle_shape_names[i]);
        }
    }
    return s.empty() ? "default" : s;
}

// "dot+circle", "dot,cross", "cross circle" (quoted), "both", "all", "default", or a numeric bitmask
static std::optional<int> reticle_style_from_string(std::string s)
{
    s = string_to_lower(s);
    if (s == "default" || s == "off" || s == "none") {
        return 0;
    }
    if (s == "both") {
        return 3;
    }
    if (s == "all") {
        return 31;
    }
    int style = 0;
    size_t pos = 0;
    while (pos < s.size()) {
        size_t end = s.find_first_of("+, ", pos);
        if (end == std::string::npos) {
            end = s.size();
        }
        if (end > pos) {
            std::string tok = s.substr(pos, end - pos);
            int bit = -1;
            for (int i = 0; i < 5; ++i) {
                if (tok == reticle_shape_names[i]) {
                    bit = 1 << i;
                }
            }
            if (bit < 0) {
                if (tok.find_first_not_of("0123456789") != std::string::npos) {
                    return {};
                }
                bit = std::atoi(tok.c_str());
            }
            style |= bit;
        }
        pos = end + 1;
    }
    return style;
}

static const ReticleIntField& reticle_field(const char* cmd)
{
    for (const auto& f : reticle_int_fields) {
        if (std::strcmp(f.cmd, cmd) == 0) {
            return f;
        }
    }
    return reticle_int_fields[0];
}

static void handle_reticle_int_cmd(std::optional<int> v, const ReticleIntField& f)
{
    ReticleConfig& cfg = reticle_edit_target();
    if (v) {
        reticle_set_int(cfg, f, *v);
    }
    rf::console::print("{} is {} {} ({})", f.label, cfg.*f.member, f.wrap ? "degrees" : "px", reticle_edit_target_label());
}

// "current" = held weapon, otherwise a weapon class name (case-insensitive). Returns slot or -1.
static int reticle_resolve_weapon(const std::string& s)
{
    if (string_to_lower(s) == "current") {
        rf::Entity* entity = rf::local_player ? rf::entity_from_handle(rf::local_player->entity_handle) : nullptr;
        return entity ? entity->ai.current_primary_weapon : -1;
    }
    return rf::weapon_lookup_type(s.c_str());
}

static std::string reticle_weapon_name(int slot)
{
    const char* name = (slot >= 0 && slot < rf::num_weapon_types) ? rf::weapon_types[slot].name : nullptr;
    return name ? name : "";
}

ConsoleCommand2 reticle_style_cmd{
    "ui_reticle_style",
    [](std::optional<std::string> style_opt) {
        ReticleConfig& cfg = reticle_edit_target();
        if (style_opt) {
            auto style = reticle_style_from_string(*style_opt);
            if (!style) {
                rf::console::print("Invalid style. Combine dot, cross, square, circle, triangle with '+' (e.g. dot+circle), or use default.");
                return;
            }
            reticle_set_int(cfg, reticle_field("style"), *style);
        }
        rf::console::print("Reticle style is {} ({})", reticle_style_to_string(cfg.style), reticle_edit_target_label());
    },
    "Select reticle shapes: default (weapon bitmap) or any '+'-joined mix of dot, cross, square, circle, triangle. Color: ui_color_reticle / ui_color_reticle_locked. Scale: ui_scale_reticle. Per-weapon: ui_reticle_weapon.",
    "ui_reticle_style <default|dot|cross|square|circle|triangle|a+b+c>",
};

ConsoleCommand2 reticle_outline_shapes_cmd{
    "ui_reticle_outline_shapes",
    [](std::optional<std::string> shapes_opt) {
        ReticleConfig& cfg = reticle_edit_target();
        if (shapes_opt) {
            auto mask = reticle_style_from_string(*shapes_opt);
            if (!mask) {
                rf::console::print("Invalid input. Use all, none, or a '+'-joined mix of dot, cross, square, circle, triangle.");
                return;
            }
            reticle_set_int(cfg, reticle_field("outline_shapes"), *mask);
        }
        const int m = cfg.outline_shapes;
        rf::console::print("Reticle outline applies to {} ({})", m == 31 ? "all shapes" : m == 0 ? "no shapes" : reticle_style_to_string(m), reticle_edit_target_label());
    },
    "Select which reticle shapes get the outline (thickness via ui_reticle_outline, color via ui_color_reticle_outline).",
    "ui_reticle_outline_shapes <all|none|dot|cross|square|circle|triangle|a+b+c>",
};

ConsoleCommand2 reticle_weapon_cmd{
    "ui_reticle_weapon",
    [](std::optional<std::string> arg) {
        if (arg) {
            if (string_to_lower(*arg) == "default") {
                g_reticle_edit_weapon.clear();
            }
            else {
                const std::string name = reticle_weapon_name(reticle_resolve_weapon(*arg));
                if (name.empty()) {
                    rf::console::print("Unknown weapon '{}'. Use default, current, or a weapon class name (quote names with spaces).", *arg);
                    return;
                }
                if (!g_alpine_game_config.weapon_reticles.count(name)) {
                    g_alpine_game_config.weapon_reticles[name] = g_alpine_game_config.reticle;
                    rf::console::print("Created reticle override for {} (copied from default).", name);
                }
                g_reticle_edit_weapon = name;
            }
        }
        rf::console::print("Reticle commands now edit: {}", reticle_edit_target_label());
        if (!arg) {
            if (g_alpine_game_config.weapon_reticles.empty()) {
                rf::console::print("No per-weapon reticle overrides.");
            }
            for (const auto& [name, cfg] : g_alpine_game_config.weapon_reticles) {
                rf::console::print("  {}: {}", name, reticle_style_to_string(cfg.style));
            }
        }
    },
    "Select which reticle the ui_reticle_* and ui_color_reticle* commands edit: default, current (held weapon), or a weapon class name. Creates the per-weapon override if missing. No argument lists overrides.",
    "ui_reticle_weapon [default|current|<weapon name>]",
};

ConsoleCommand2 reticle_weapon_reset_cmd{
    "ui_reticle_weapon_reset",
    [](std::optional<std::string> arg) {
        const std::string name = arg ? reticle_weapon_name(reticle_resolve_weapon(*arg)) : g_reticle_edit_weapon;
        if (name.empty()) {
            rf::console::print("Specify a weapon, or select one first with ui_reticle_weapon.");
            return;
        }
        if (g_alpine_game_config.weapon_reticles.erase(name)) {
            rf::console::print("Removed reticle override for {}; it now uses the default reticle.", name);
        }
        else {
            rf::console::print("{} has no reticle override.", name);
        }
        if (g_reticle_edit_weapon == name) {
            g_reticle_edit_weapon.clear();
        }
    },
    "Remove a per-weapon reticle override (the selected weapon, or the named one) so that weapon uses the default reticle.",
    "ui_reticle_weapon_reset [current|<weapon name>]",
};

// Copy a reticle config between targets. Source: default, current, or a weapon (its effective config,
// so a weapon without an override copies the default). Destination: default, current, or a weapon (override created).
ConsoleCommand2 reticle_weapon_copy_cmd{
    "ui_reticle_weapon_copy",
    [](std::string from, std::string to) {
        auto resolve = [](const std::string& s, std::string& name) {
            if (string_to_lower(s) == "default") {
                return true; // name stays empty = default
            }
            name = reticle_weapon_name(reticle_resolve_weapon(s));
            if (name.empty()) {
                rf::console::print("Unknown weapon '{}'. Use default, current, or a weapon class name (quote names with spaces).", s);
                return false;
            }
            return true;
        };
        std::string from_name, to_name;
        if (!resolve(from, from_name) || !resolve(to, to_name)) {
            return;
        }
        const ReticleConfig src = g_alpine_game_config.reticle_for_weapon(from_name); // copy: dest may alias
        if (to_name.empty()) {
            g_alpine_game_config.reticle = src;
        }
        else {
            g_alpine_game_config.weapon_reticles[to_name] = src;
        }
        rf::console::print("Copied reticle from {} to {}.", from_name.empty() ? "default" : from_name, to_name.empty() ? "default" : to_name);
    },
    "Copy a reticle config: from/to are default, current (held weapon), or a weapon class name. Copying to a weapon creates its override.",
    "ui_reticle_weapon_copy <from> <to>",
};

void hud_weapons_apply_patches()
{
    // Big HUD support for ammo display
    hud_render_ammo_gr_bitmap_hook.install();
    hud_render_ammo_hook.install();
    hud_render_ammo_no_clip_hook.install();

    // reticle color, scale and vector reticle
    render_reticle_gr_bitmap_hook.install();
    render_reticle_set_color_hook.install();
    render_reticle_locked_set_color_hook.install();
    render_reticle_check_custom_injection.install();

    reticle_style_cmd.register_cmd();
    reticle_outline_shapes_cmd.register_cmd();
    reticle_weapon_cmd.register_cmd();
    reticle_weapon_reset_cmd.register_cmd();
    reticle_weapon_copy_cmd.register_cmd();
    // one ui_reticle_<field> <value> command per int field (style/outline_shapes have string forms above)
    for (const auto& f : reticle_int_fields) {
        if (std::strcmp(f.cmd, "style") == 0 || std::strcmp(f.cmd, "outline_shapes") == 0) {
            continue;
        }
        const char* unit = f.wrap ? "degrees" : "px";
        auto* name = new std::string("ui_reticle_" + std::string(f.cmd));
        auto* desc = new std::string(std::string(f.label) + " in " + unit + " (edits the target chosen with ui_reticle_weapon).");
        auto* usage = new std::string(*name + " <" + unit + ">");
        auto* cmd = new ConsoleCommand2{
            name->c_str(),
            [&f](std::optional<int> v) { handle_reticle_int_cmd(v, f); },
            desc->c_str(),
            usage->c_str(),
        };
        cmd->register_cmd();
    }

    // Disable weapon cycle selection, if `Mouse 3` is pressed.
    player_select_next_primary_hook.install();
    player_select_prev_primary_hook.install();
}
