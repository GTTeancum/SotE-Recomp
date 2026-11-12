#include "cutscene_files.hpp"
#include "graphics_menu.hpp"
#include "diagnostic_progress.hpp"

#include "controls_menu.hpp"
#include "control_bindings.hpp"
#include "menu_skin.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "recomp.h"

namespace sote::graphics_menu {
namespace {

constexpr uint16_t n64_a = 0x8000;
constexpr uint16_t n64_b = 0x4000;
constexpr uint16_t n64_start = 0x1000;
constexpr uint16_t n64_z = 0x2000;
constexpr uint16_t n64_du = 0x0800;
constexpr uint16_t n64_dd = 0x0400;
constexpr uint16_t n64_dl = 0x0200;
constexpr uint16_t n64_dr = 0x0100;
constexpr uint16_t n64_l = 0x0020;
constexpr uint16_t n64_r = 0x0010;
constexpr uint16_t menu_buttons =
    n64_a | n64_b | n64_start | n64_z | n64_du | n64_dd | n64_dl |
    n64_dr | n64_l | n64_r;

constexpr uint32_t ui_string_table_pointer = 0x8013CE30U;
constexpr uint32_t ui_positions = 0x80111110U;
constexpr uint32_t ui_colors = 0x80111250U;
constexpr uint32_t ui_flags = 0x80111390U;

constexpr uint32_t normal_color = 0x408040FFU;
constexpr uint32_t selected_color = 0xC8FFC8FFU;
constexpr uint32_t selected_value_color = 0xD060E8FFU;

enum class TopCategory : int {
    Graphics,
    Controls,
    Cheats,
};

int cheats_item_count();

enum class MenuTransition {
    None,
    ReturnToMain,
    WaitMain,
    ActivateReturn,
    FocusResume,
    ActivateResume,
    WaitTitleOptions,
    WaitTitle,
    WaitProfiles,
    WaitPause,
};

constexpr int graphics_submenu_item_count = 12;

int submenu_item_count(TopCategory category) {
    if (category == TopCategory::Cheats) return cheats_item_count();
    return category == TopCategory::Controls
        ? 5 + sote::controls_menu::tuning_menu_row_count()
        : graphics_submenu_item_count;
}

struct MenuState {
    std::mutex mutex;
    bool initialized = false;
    bool adjusting = false;
    int front_end_mode = -1;
    bool profile_dialog = false;
    bool cancel_profile_dialog = false;
    bool title_options = false;
    bool submenu_bindings = false;
    bool title_start_release = false;
    int title_selection = 0;
    int profile_action_direction = 1;
    int profile_action_index = -1;
    int requested_profile_action = -1;
    int requested_pause_focus = -1;
    int requested_options_focus = -1;
    bool cancel_pause_quit = false;
    int requested_title_mode = -1;
    bool reset_options_focus = false;
    bool options_visible = false;
    bool return_to_pause = false;
    bool graphics_focus = false;
    bool submenu_active = false;
    TopCategory category = TopCategory::Graphics;
    int original_selection = 52;
    int submenu_selection = 1;
    uint16_t previous_navigation = 0;
    uint16_t repeat_direction = 0;
    std::chrono::steady_clock::time_point repeat_at{};
    MenuTransition transition = MenuTransition::None;
    std::chrono::steady_clock::time_point transition_started{};
    std::chrono::steady_clock::time_point phase_started{};
    Settings applied{};
    Settings editing{};
    bool renderer_request = false;
    bool window_request = false;
    std::filesystem::path persistence_path;
    std::chrono::steady_clock::time_point last_options_seen{};
    int display_width = 1920;
    int display_height = 1080;
};

MenuState state;
bool pc_movies_available = false;
std::chrono::steady_clock::time_point availability_checked{};
void refresh_cutscene_availability_locked(bool force = false) {
    const auto now = std::chrono::steady_clock::now();
    if (force || now - availability_checked >= std::chrono::seconds(1)) {
        availability_checked = now;
        pc_movies_available = sote::pc_cutscene_files_present(state.persistence_path.parent_path());
    }
    if (!pc_movies_available) state.applied.pc_cutscenes = state.editing.pc_cutscenes = false;
}

gpr guest_address(uint32_t address) {
    return static_cast<gpr>(static_cast<int32_t>(address));
}

uint32_t read_word(uint8_t* rdram, uint32_t address) {
    return MEM_W(0, guest_address(address));
}

uint16_t read_half(uint8_t* rdram, uint32_t address) {
    return MEM_HU(0, guest_address(address));
}

uint8_t read_byte(uint8_t* rdram, uint32_t address) {
    return MEM_BU(0, guest_address(address));
}

void write_half(uint8_t* rdram, uint32_t address, int16_t value) {
    MEM_H(0, guest_address(address)) = static_cast<uint16_t>(value);
}

void write_byte(uint8_t* rdram, uint32_t address, uint8_t value) {
    MEM_B(0, guest_address(address)) = value;
}

uint32_t row_buffer(uint8_t* rdram, int row) {
    const uint32_t table = read_word(rdram, ui_string_table_pointer);
    if (table < 0x80000000U || table >= 0x80800000U) {
        return 0;
    }
    return read_word(
        rdram,
        table + static_cast<uint32_t>(row * 4));
}

std::string read_row(uint8_t* rdram, int row) {
    const uint32_t buffer = row_buffer(rdram, row);
    if (buffer < 0x80000000U || buffer >= 0x80800000U) {
        return {};
    }
    std::string result;
    result.reserve(64);
    for (uint32_t offset = 0; offset < 63; ++offset) {
        const uint8_t character =
            read_byte(rdram, buffer + offset);
        if (character == 0) {
            break;
        }
        result.push_back(static_cast<char>(character));
    }
    return result;
}

void set_row_position(uint8_t* rdram, int row, int x, int y) {
    const uint32_t address =
        ui_positions + static_cast<uint32_t>(row * 4);
    write_half(rdram, address, static_cast<int16_t>(x));
    write_half(rdram, address + 2, static_cast<int16_t>(y));
}

void set_row_color(uint8_t* rdram, int row, uint32_t rgba) {
    const uint32_t address =
        ui_colors + static_cast<uint32_t>(row * 4);
    write_byte(rdram, address, static_cast<uint8_t>(rgba >> 24));
    write_byte(rdram, address + 1, static_cast<uint8_t>(rgba >> 16));
    write_byte(rdram, address + 2, static_cast<uint8_t>(rgba >> 8));
    write_byte(rdram, address + 3, static_cast<uint8_t>(rgba));
}

void set_row(
    uint8_t* rdram,
    int row,
    std::string_view text,
    int x,
    int y,
    uint32_t color) {
    const uint32_t buffer = row_buffer(rdram, row);
    if (buffer < 0x80000000U || buffer >= 0x80800000U) {
        return;
    }
    const size_t length = std::min<size_t>(text.size(), 63);
    for (size_t i = 0; i < length; ++i) {
        write_byte(
            rdram,
            buffer + static_cast<uint32_t>(i),
            static_cast<uint8_t>(text[i]));
    }
    write_byte(
        rdram,
        buffer + static_cast<uint32_t>(length),
        0);
    set_row_position(rdram, row, x, y);
    set_row_color(rdram, row, color);
    write_byte(
        rdram,
        ui_flags + static_cast<uint32_t>(row),
        0);
}

void hide_row(uint8_t* rdram, int row) {
    set_row_position(rdram, row, -1000, -1000);
}

bool row_is_selected(uint8_t* rdram, int row) {
    const uint32_t address =
        ui_colors + static_cast<uint32_t>(row * 4);
    return read_byte(rdram, address) >= 0xA0 &&
        read_byte(rdram, address + 1) >= 0xD0 &&
        read_byte(rdram, address + 2) >= 0xA0;
}

int detect_original_selection(uint8_t* rdram) {
    for (const int row : {53, 55, 57, 59, 61, 63}) {
        if (row_is_selected(rdram, row)) {
            return row;
        }
    }
    return 52;
}

const char* resolution_name(ResolutionPreset preset) {
    switch (preset) {
        case ResolutionPreset::Native:
            return "~sNative";
        case ResolutionPreset::Scale2x:
            return "~s2x";
        case ResolutionPreset::Scale4x:
            return "~s4x";
        case ResolutionPreset::Scale8x:
            return "~s8x";
        case ResolutionPreset::WindowScale:
        default:
            return "~sScale to Fit";
    }
}

std::string output_resolution_name(const Settings& settings) {
    char value[32]{};
    std::snprintf(
        value,
        sizeof(value),
        "~s%d x %d",
        settings.output_width,
        settings.output_height);
    return value;
}

const char* antialiasing_name(AntialiasingPreset preset) {
    switch (preset) {
        case AntialiasingPreset::Off:
            return "~sOFF";
        case AntialiasingPreset::MSAA2x:
            return "~s2x MSAA";
        case AntialiasingPreset::MSAA4x:
            return "~s4x MSAA";
        case AntialiasingPreset::MSAA8x:
        default:
            return "~s8x MSAA";
    }
}

template <typename Enum>
Enum cycle_enum(Enum value, int count, int direction) {
    const int current = static_cast<int>(value);
    return static_cast<Enum>(
        (current + direction + count) % count);
}

std::vector<std::pair<int, int>> output_resolutions_locked() {
    constexpr std::array<std::pair<int, int>, 5> common{{
        {1280, 720},
        {1600, 900},
        {1920, 1080},
        {2560, 1440},
        {3840, 2160},
    }};
    std::vector<std::pair<int, int>> resolutions;
    for (const auto& resolution : common) {
        if (resolution.first <= state.display_width &&
            resolution.second <= state.display_height) {
            resolutions.push_back(resolution);
        }
    }
    const std::pair desktop{
        state.display_width,
        state.display_height};
    if (std::find(
            resolutions.begin(),
            resolutions.end(),
            desktop) == resolutions.end()) {
        resolutions.push_back(desktop);
    }
    const std::pair current{
        state.editing.output_width,
        state.editing.output_height};
    if (std::find(
            resolutions.begin(),
            resolutions.end(),
            current) == resolutions.end()) {
        resolutions.push_back(current);
    }
    std::sort(resolutions.begin(), resolutions.end());
    resolutions.erase(
        std::unique(resolutions.begin(), resolutions.end()),
        resolutions.end());
    return resolutions;
}

void adjust_output_resolution_locked(int direction) {
    const auto resolutions = output_resolutions_locked();
    if (resolutions.empty()) {
        return;
    }
    const std::pair current{
        state.editing.output_width,
        state.editing.output_height};
    auto current_it =
        std::find(resolutions.begin(), resolutions.end(), current);
    int index = current_it == resolutions.end()
        ? 0
        : static_cast<int>(
              std::distance(resolutions.begin(), current_it));
    index =
        (index + direction + static_cast<int>(resolutions.size())) %
        static_cast<int>(resolutions.size());
    state.editing.output_width = resolutions[index].first;
    state.editing.output_height = resolutions[index].second;
}

int read_persisted_integer(
    const std::string& contents,
    std::string_view field,
    int fallback) {
    const size_t field_position = contents.find(field);
    if (field_position == std::string::npos) {
        return fallback;
    }
    const size_t colon = contents.find(':', field_position + field.size());
    if (colon == std::string::npos) {
        return fallback;
    }
    const char* begin = contents.c_str() + colon + 1;
    char* end = nullptr;
    const long parsed = std::strtol(begin, &end, 10);
    if (end == begin || parsed <= 0 || parsed > 16384) {
        return fallback;
    }
    return static_cast<int>(parsed);
}

bool read_persisted_bool(
    const std::string& contents,
    std::string_view field,
    bool fallback) {
    const size_t field_position = contents.find(field);
    if (field_position == std::string::npos) return fallback;
    const size_t colon = contents.find(':', field_position + field.size());
    if (colon == std::string::npos) return fallback;
    size_t value = colon + 1;
    while (value < contents.size() &&
           std::isspace(static_cast<unsigned char>(contents[value]))) {
        ++value;
    }
    if (contents.compare(value, 4, "true") == 0) return true;
    if (contents.compare(value, 5, "false") == 0) return false;
    return fallback;
}

void persist_window_mode_locked() {
    if (state.persistence_path.empty()) {
        return;
    }
    std::ofstream output{
        state.persistence_path,
        std::ios_base::out | std::ios_base::trunc};
    if (!output.is_open()) {
        return;
    }
    output << "{\n  \"borderless\": "
           << (state.applied.borderless ? "true" : "false")
           << ",\n  \"pcCutscenes\": "
           << (state.applied.pc_cutscenes ? "true" : "false")
           << ",\n  \"outputWidth\": "
           << state.applied.output_width
           << ",\n  \"outputHeight\": "
           << state.applied.output_height
           << ",\n  \"compatibilityMode\": "
           << (state.applied.compatibility ? "true" : "false")
           << ",\n  \"hdTextures\": "
           << (state.applied.hd_textures ? "true" : "false")
           << "\n}\n";
}

void queue_apply_locked() {
    std::printf("[sote][menu-apply] cutscenes %s -> %s compatibility %d -> %d\n",
        state.applied.pc_cutscenes ? "PC" : "N64", state.editing.pc_cutscenes ? "PC" : "N64",
        state.applied.compatibility, state.editing.compatibility);
    std::fflush(stdout);
    refresh_cutscene_availability_locked(true);
    state.applied = state.editing;
    state.renderer_request = true;
    state.window_request = true;
    persist_window_mode_locked();
    std::printf(
        "[sote][graphics-menu] apply output=%dx%d "
        "render_scale=%d widescreen=%d antialiasing=%d "
        "borderless=%d\n",
        state.applied.output_width,
        state.applied.output_height,
        static_cast<int>(state.applied.resolution),
        state.applied.widescreen ? 1 : 0,
        static_cast<int>(state.applied.antialiasing),
        state.applied.borderless ? 1 : 0);
    std::fflush(stdout);
}


// ---------------------------------------------------------------------------
// Cheats page (after Controls). Each row edits an INI the runtime already
// re-reads about once a second: SOTE_CHEATS.INI for cheats and rapid fire,
// CONTROLS_MODERN.INI for aim assist. No Apply step is needed.
// ---------------------------------------------------------------------------
std::filesystem::path menu_data_directory;

std::string ini_trim(const std::string& text) {
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::vector<std::string> ini_read_lines(const std::filesystem::path& path) {
    std::vector<std::string> lines;
    std::ifstream input{path};
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

std::string ini_get(const std::filesystem::path& path, const std::string& key,
                    const std::string& fallback) {
    for (const std::string& line : ini_read_lines(path)) {
        const std::string trimmed = ini_trim(line);
        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') continue;
        const size_t equals = trimmed.find('=');
        if (equals == std::string::npos) continue;
        if (ini_trim(trimmed.substr(0, equals)) == key) {
            std::string value = trimmed.substr(equals + 1);
            const size_t comment = value.find(';');
            if (comment != std::string::npos) value.resize(comment);
            return ini_trim(value);
        }
    }
    return fallback;
}

void ini_set(const std::filesystem::path& path, const std::string& key,
             const std::string& value) {
    std::vector<std::string> lines = ini_read_lines(path);
    bool replaced = false;
    for (std::string& line : lines) {
        const std::string trimmed = ini_trim(line);
        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') continue;
        const size_t equals = trimmed.find('=');
        if (equals != std::string::npos &&
            ini_trim(trimmed.substr(0, equals)) == key) {
            line = key + " = " + value;
            replaced = true;
            break;
        }
    }
    if (!replaced) lines.push_back(key + " = " + value);
    std::ofstream output{path, std::ios::trunc};
    for (const std::string& line : lines) output << line << "\n";
}

struct CheatToggle { const char* key; const char* label; };
constexpr CheatToggle cheat_toggles[] = {
    {"infinite_health", "Infinite Health"},
    {"infinite_lives", "Infinite Lives"},
    {"infinite_ammo", "Infinite Ammo"},
    {"infinite_missiles", "Infinite Missiles"},
    {"infinite_vehicle_health", "Vehicle Health"},
    {"rapid_fire", "Rapid Fire"},
};
constexpr int cheat_toggle_count =
    static_cast<int>(sizeof(cheat_toggles) / sizeof(cheat_toggles[0]));
// Rows: 0 Return, 1..toggles, then Fire Rate, Aim Assist, Assist Width.
constexpr int fire_rate_row = cheat_toggle_count + 1;
constexpr int aim_assist_row = cheat_toggle_count + 2;
constexpr int assist_width_row = cheat_toggle_count + 3;
constexpr int fire_rate_default = 4;

int cheats_item_count() { return assist_width_row + 1; }

struct CheatsCache {
    bool toggles[cheat_toggle_count] = {};
    int fire_rate = fire_rate_default;
    int assist_level = 0;   // 0-10, tenths of on_foot_magnetism_strength
    int assist_width = 2;   // degrees, 1-8
};
CheatsCache cheats_cache;

std::filesystem::path cheats_ini() { return menu_data_directory / "SOTE_CHEATS.INI"; }
std::filesystem::path controls_ini() { return menu_data_directory / "CONTROLS_MODERN.INI"; }

void load_cheats_cache() {
    for (int i = 0; i < cheat_toggle_count; ++i) {
        const std::string value = ini_get(cheats_ini(), cheat_toggles[i].key, "0");
        cheats_cache.toggles[i] = value == "1" || value == "true" || value == "on";
    }
    cheats_cache.fire_rate = std::clamp(std::atoi(ini_get(cheats_ini(), "rapid_fire_rate",
        std::to_string(fire_rate_default)).c_str()), 1, 10);
    const float strength = static_cast<float>(std::atof(
        ini_get(controls_ini(), "on_foot_magnetism_strength", "0").c_str()));
    cheats_cache.assist_level = std::clamp(static_cast<int>(strength * 10.0f + 0.5f), 0, 10);
    cheats_cache.assist_width = std::clamp(std::atoi(
        ini_get(controls_ini(), "on_foot_magnetism_cone_degrees", "2").c_str()), 1, 8);
}

int wrap(int value, int low, int high) {
    if (value > high) return low;
    if (value < low) return high;
    return value;
}

void adjust_cheat_row(int selection, int direction) {
    if (selection >= 1 && selection <= cheat_toggle_count) {
        bool& toggle = cheats_cache.toggles[selection - 1];
        toggle = !toggle;
        ini_set(cheats_ini(), cheat_toggles[selection - 1].key, toggle ? "1" : "0");
    } else if (selection == fire_rate_row) {
        cheats_cache.fire_rate = wrap(cheats_cache.fire_rate + direction, 1, 10);
        ini_set(cheats_ini(), "rapid_fire_rate", std::to_string(cheats_cache.fire_rate));
    } else if (selection == aim_assist_row) {
        const int next = cheats_cache.assist_level + direction;
        cheats_cache.assist_level = state.title_options ? wrap(next, 0, 10) : std::clamp(next, 0, 10);
        char text[16];
        std::snprintf(text, sizeof(text), "%.1f", cheats_cache.assist_level / 10.0);
        ini_set(controls_ini(), "on_foot_magnetism_strength", text);
    } else if (selection == assist_width_row) {
        const int next = cheats_cache.assist_width + direction;
        cheats_cache.assist_width = state.title_options ? wrap(next, 1, 8) : std::clamp(next, 1, 8);
        ini_set(controls_ini(), "on_foot_magnetism_cone_degrees",
                std::to_string(cheats_cache.assist_width));
    }
}

std::string cheat_row_label(int selection) {
    if (selection >= 1 && selection <= cheat_toggle_count) return cheat_toggles[selection - 1].label;
    if (selection == fire_rate_row) return "Fire Rate";
    if (selection == aim_assist_row) return "Aim Assist";
    return "Assist Width";
}

std::string cheat_row_value(int selection) {
    if (selection >= 1 && selection <= cheat_toggle_count) {
        return cheats_cache.toggles[selection - 1] ? "On" : "Off";
    }
    if (selection == fire_rate_row) {
        return std::to_string(cheats_cache.fire_rate) + " per sec" +
            (cheats_cache.fire_rate == fire_rate_default ? " (Default)" : "");
    }
    if (selection == aim_assist_row) {
        return cheats_cache.assist_level == 0
            ? std::string{"Off"} : std::to_string(cheats_cache.assist_level) + " / 10";
    }
    return std::to_string(cheats_cache.assist_width) + " deg";
}

constexpr const char* cheats_header = "Options   Graphics   Controls   < Cheats >";

void adjust_selected_setting_locked(int direction) {
    if (state.category == TopCategory::Cheats) {
        adjust_cheat_row(state.submenu_selection, direction);
        return;
    }
    if (state.category == TopCategory::Controls) {
        if (state.submenu_selection == 1) {
            sote::controls_menu::cycle_scheme(
                sote::controls_menu::SchemeSlot::OnFoot, direction);
        } else if (state.submenu_selection == 2) {
            sote::controls_menu::cycle_scheme(
                sote::controls_menu::SchemeSlot::Bike, direction);
        } else {
            const int tuning_index = state.submenu_selection - 3;
            if (tuning_index >= 0 &&
                tuning_index < sote::controls_menu::tuning_menu_row_count()) {
                sote::controls_menu::adjust_tuning_menu_row(
                    tuning_index,
                    direction, state.title_options);
            }
        }
        return;
    }
    switch (state.submenu_selection) {
        case 1:
            adjust_output_resolution_locked(direction);
            break;
        case 2:
            state.editing.resolution = cycle_enum(
                state.editing.resolution,
                5,
                direction);
            break;
        case 3:
            state.editing.widescreen =
                !state.editing.widescreen;
            break;
        case 4:
            if (state.editing.compatibility) break;
            state.editing.antialiasing = cycle_enum(
                state.editing.antialiasing,
                4,
                direction);
            break;
        case 5:
            state.editing.borderless =
                !state.editing.borderless;
            break;
        case 6:
            refresh_cutscene_availability_locked(true);
            if (pc_movies_available) state.editing.pc_cutscenes =
                !state.editing.pc_cutscenes;
            break;
        case 8:
            state.editing.compatibility = !state.editing.compatibility;
            break;
        case 9:
            state.editing.hd_textures = !state.editing.hd_textures;
            break;
        case 7:
            // Applies immediately; it only changes how menus are drawn.
            sote::menu_skin::set_menu_font_original(
                !sote::menu_skin::menu_font_original());
            break;
        default:
            break;
    }
}

void draw_controls_submenu(uint8_t* rdram) {
    for (int row = 52; row < 80; ++row) {
        hide_row(rdram, row);
    }

    auto label_color = [](int selection) {
        return state.submenu_selection == selection
            ? selected_color
            : normal_color;
    };
    auto value_color = [](int selection) {
        return state.submenu_selection == selection
            ? selected_value_color
            : normal_color;
    };

    set_row(
        rdram,
        52,
        "~s~cOptions   Graphics   < Controls >   Cheats",
        160,
        50,
        normal_color);
    auto scheme_name = [](sote::controls_menu::SchemeSlot slot) {
        return sote::controls_menu::current_scheme(slot) ==
                sote::controls_menu::ControlScheme::Modern
            ? "~sModern"
            : "~sClassic";
    };

    constexpr int label_x = 52;
    constexpr int value_x = 166;
    constexpr int first_y = 66;
    constexpr int row_step = 12;

    set_row(rdram, 53, "~sOn Foot", label_x, first_y, label_color(1));
    set_row(
        rdram,
        54,
        scheme_name(sote::controls_menu::SchemeSlot::OnFoot),
        value_x,
        first_y,
        value_color(1));
    set_row(
        rdram,
        55,
        "~sSpeeder Bike",
        label_x,
        first_y + row_step,
        label_color(2));
    set_row(
        rdram,
        56,
        scheme_name(sote::controls_menu::SchemeSlot::Bike),
        value_x,
        first_y + row_step,
        value_color(2));

    const int tuning_count = sote::controls_menu::tuning_menu_row_count();
    for (int index = 0; index < tuning_count; ++index) {
        sote::controls_menu::TuningMenuRow row{};
        if (!sote::controls_menu::tuning_menu_row(index, row)) {
            continue;
        }
        const int selection = index + 3;
        const int text_row = 57 + index * 2;
        const int y = first_y + (index + 2) * row_step;
        char label[48];
        char value[64];
        std::snprintf(label, sizeof(label), "~s%s", row.label);
        std::snprintf(
            value,
            sizeof(value),
            "~s%s %s",
            row.slider,
            row.value);
        set_row(rdram, text_row, label, label_x, y, label_color(selection));
        set_row(
            rdram,
            text_row + 1,
            value,
            value_x,
            y,
            value_color(selection));
    }

    const int apply_selection = 3 + tuning_count;
    set_row(
        rdram,
        75,
        "~s~cReturn to previous menu",
        105,
        220,
        label_color(0));
    set_row(
        rdram,
        76,
        "~s~cApply",
        220,
        220,
        label_color(apply_selection));
}

void draw_cheats_submenu(uint8_t* rdram) {
    for (int row = 52; row < 80; ++row) {
        hide_row(rdram, row);
    }
    auto color = [](int selection, bool value) {
        if (state.submenu_selection != selection) return normal_color;
        return value ? selected_value_color : selected_color;
    };
    set_row(rdram, 52, std::string{"~s~c"} + cheats_header, 160, 50, normal_color);
    for (int selection = 1; selection < cheats_item_count(); ++selection) {
        const int text_row = 53 + (selection - 1) * 2;
        const int y = 66 + (selection - 1) * 14;
        set_row(rdram, text_row, "~s" + cheat_row_label(selection), 60, y, color(selection, false));
        set_row(rdram, text_row + 1, "~s" + cheat_row_value(selection), 176, y, color(selection, true));
    }
    set_row(rdram, 75, "~s~cReturn to previous menu", 160, 220, color(0, false));
}

void draw_graphics_submenu(uint8_t* rdram) {
    refresh_cutscene_availability_locked();
    for (int row = 52; row < 80; ++row) {
        hide_row(rdram, row);
    }

    auto label_color = [](int selection) {
        return state.submenu_selection == selection
            ? selected_color
            : normal_color;
    };
    auto value_color = [](int selection) {
        return state.submenu_selection == selection
            ? selected_value_color
            : normal_color;
    };

    set_row(
        rdram,
        52,
        "~s~cOptions   < Graphics >   Controls   Cheats",
        160,
        50,
        normal_color);
    set_row(
        rdram,
        53,
        "~sResolution",
        72,
        70,
        label_color(1));
    set_row(
        rdram,
        54,
        output_resolution_name(state.editing),
        160,
        70,
        value_color(1));
    set_row(
        rdram,
        55,
        "~sRender Scale",
        72,
        86,
        label_color(2));
    set_row(
        rdram,
        56,
        resolution_name(state.editing.resolution),
        160,
        86,
        value_color(2));
    set_row(
        rdram,
        57,
        "~sAspect Ratio",
        72,
        102,
        label_color(3));
    set_row(
        rdram,
        58,
        state.editing.widescreen
            ? "~sWidescreen"
            : "~sOriginal 4:3",
        160,
        102,
        value_color(3));
    set_row(
        rdram,
        59,
        "~sAntialiasing",
        72,
        118,
        label_color(4));
    set_row(
        rdram,
        60,
        antialiasing_name(state.editing.compatibility ? AntialiasingPreset::Off : state.editing.antialiasing),
        160,
        118,
        value_color(4));
    set_row(
        rdram,
        61,
        "~sDisplay Mode",
        72,
        134,
        label_color(5));
    set_row(
        rdram,
        62,
        state.editing.borderless
            ? "~sBorderless"
            : "~sWindowed",
        160,
        134,
        value_color(5));
    set_row(
        rdram,
        63,
        "~sCutscenes",
        72,
        150,
        label_color(6));
    set_row(
        rdram,
        65,
        state.editing.pc_cutscenes ? "~sPC" : "~sN64",
        160,
        150,
        value_color(6));
    if (!pc_movies_available) {
        set_row(rdram,63,"~sCutscenes",72,150,0x697388FF);
        set_row(rdram,65,"~sN64",160,150,0x697388FF);
    }
    set_row(rdram, 69, "~sFont", 72, 166, label_color(7));
    set_row(rdram, 70,
        sote::menu_skin::menu_font_original() ? "~sOriginal" : "~sModern",
        160, 166, value_color(7));
    set_row(rdram,71,"~sCompatibility Mode",42,181,label_color(8));
    set_row(rdram,72,state.editing.compatibility ? "~sOn" : "~sOff",200,181,value_color(8));
    set_row(rdram,73,"~sHD Textures",72,194,label_color(9));
    set_row(rdram,74,state.editing.hd_textures ? "~sOn" : "~sOff",200,194,value_color(9));
    set_row(
        rdram,
        67,
        "~s~cApply",
        110,
        208,
        label_color(10));
    set_row(
        rdram,
        68,
        "~s~cReset",
        210,
        208,
        label_color(11));
    set_row(
        rdram,
        64,
        "~s~cReturn to previous menu",
        160,
        220,
        label_color(0));
}

void set_transition_locked(MenuTransition next) {
    std::printf("[sote][menu] transition %d -> %d category=%d selection=%d options=%d\n",
        int(state.transition), int(next), int(state.category), state.submenu_selection,
        state.options_visible);
    std::fflush(stdout);
    state.transition = next;
    state.phase_started = std::chrono::steady_clock::now();
}

void return_to_main_locked() {
    state.adjusting = false;
    state.requested_options_focus = -1;
    state.editing = state.applied;
    state.transition_started = std::chrono::steady_clock::now();
    if (state.title_options) {
        state.title_selection = 1;
        state.requested_title_mode = 0;
        set_transition_locked(MenuTransition::WaitTitle);
    } else {
        set_transition_locked(MenuTransition::ReturnToMain);
    }
}

void switch_page_locked(int direction) {
    state.requested_options_focus = -1;
    const int page = state.submenu_active ? 1 + static_cast<int>(state.category) : 0;
    const int next = (page + direction + 4) % 4;
    state.submenu_active = next != 0;
    state.reset_options_focus = next == 0;
    if (next != 0) state.category = static_cast<TopCategory>(next - 1);
    if (next == 3) load_cheats_cache();
    state.submenu_selection = 1;
}

} // namespace

void initialize(const std::filesystem::path& data_directory) {
    std::lock_guard lock{state.mutex};
    menu_data_directory = data_directory;
    state.applied = Settings{};
    state.editing = state.applied;
    state.options_visible = false;
    state.submenu_active = false;
    state.transition = MenuTransition::None;
    state.previous_navigation = 0;
    state.adjusting = false;
    state.front_end_mode = -1;
    state.repeat_direction = 0;
    state.profile_dialog = state.cancel_profile_dialog = false;
    state.title_options = state.submenu_bindings = false;
    state.profile_action_index = state.requested_profile_action = -1;
    state.requested_pause_focus = -1;
    state.requested_options_focus = -1;
    state.cancel_pause_quit = false;
    state.title_start_release = false;
    state.title_selection = 0;
    state.profile_action_direction = 1;
    state.requested_title_mode = -1;
    state.reset_options_focus = false;
    state.persistence_path = data_directory / "sote_options.json";
    std::ifstream input{state.persistence_path};
    if (input.is_open()) {
        const std::string contents{
            std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}};
        state.applied.borderless = read_persisted_bool(
            contents, "\"borderless\"", state.applied.borderless);
        state.applied.compatibility = read_persisted_bool(contents, "\"compatibilityMode\"", false);
        state.applied.hd_textures = read_persisted_bool(contents, "\"hdTextures\"", true);
        state.applied.pc_cutscenes = read_persisted_bool(
            contents, "\"pcCutscenes\"", state.applied.pc_cutscenes);
        state.applied.output_width = read_persisted_integer(
            contents,
            "\"outputWidth\"",
            state.applied.output_width);
        state.applied.output_height = read_persisted_integer(
            contents,
            "\"outputHeight\"",
            state.applied.output_height);
        state.editing.borderless = state.applied.borderless;
        state.editing.pc_cutscenes = state.applied.pc_cutscenes;
        state.editing.output_width = state.applied.output_width;
        state.editing.output_height = state.applied.output_height;
    }
    state.editing.compatibility = state.applied.compatibility;
    state.editing.hd_textures = state.applied.hd_textures;
    refresh_cutscene_availability_locked(true);
    state.window_request = true;
    state.initialized = true;
}

void sync_display_resolution(int width, int height) {
    std::lock_guard lock{state.mutex};
    if (width > 0 && height > 0) {
        state.display_width = width;
        state.display_height = height;
    }
}

void sync_from_renderer(const Settings& settings) {
    std::lock_guard lock{state.mutex};
    const bool saved_compatibility = state.applied.compatibility;
    const bool saved_hd_textures = state.applied.hd_textures;
    const bool saved_borderless = state.applied.borderless;
    const bool saved_pc_cutscenes = state.applied.pc_cutscenes;
    const int saved_output_width = state.applied.output_width;
    const int saved_output_height = state.applied.output_height;
    state.applied = settings;
    state.applied.compatibility = saved_compatibility;
    state.applied.hd_textures = saved_hd_textures;
    state.applied.borderless = saved_borderless;
    state.applied.pc_cutscenes = saved_pc_cutscenes;
    state.applied.output_width = saved_output_width;
    state.applied.output_height = saved_output_height;
    state.editing = state.applied;
}

void sync_window_mode(bool borderless) {
    std::lock_guard lock{state.mutex};
    state.applied.borderless = borderless;
    state.editing.borderless = borderless;
    persist_window_mode_locked();
}

bool compatibility_enabled() {
    std::lock_guard lock{state.mutex};
    return state.applied.compatibility;
}

bool hd_textures_enabled() {
    std::lock_guard lock{state.mutex};
    return state.applied.hd_textures;
}

bool pc_cutscenes_enabled() {
    std::lock_guard lock{state.mutex};
    refresh_cutscene_availability_locked();
    return state.applied.pc_cutscenes;
}

bool take_renderer_request(Settings& settings) {
    std::lock_guard lock{state.mutex};
    if (!state.renderer_request) {
        return false;
    }
    settings = state.applied;
    state.renderer_request = false;
    return true;
}

bool take_window_request(Settings& settings) {
    std::lock_guard lock{state.mutex};
    if (!state.window_request) {
        return false;
    }
    settings = state.applied;
    state.window_request = false;
    return true;
}

bool front_end_picker_active() {
    std::lock_guard lock{state.mutex};
    return state.front_end_mode == 2 || state.front_end_mode == 3;
}

void filter_input(uint16_t* buttons, float* x, float* y) {
    sote::diagnostics::Phase phase(sote::diagnostics::input_phase);
    ++sote::diagnostics::inputs;
    const auto view = sote::menu_skin::latest();
    phase.set(2);
    std::lock_guard lock{state.mutex};
    phase.set(3);
    const auto now = std::chrono::steady_clock::now();

    if (state.options_visible &&
        now - state.last_options_seen > std::chrono::seconds{3}) {
        state.options_visible = false;
        state.graphics_focus = false;
        state.submenu_active = false;
        state.adjusting = false;
    }
    uint16_t navigation = *buttons & menu_buttons;
    // Enter changes from native Start in Pause to A in Options. Track both
    // as one confirmation so opening a page cannot also enter adjustment.
    if (navigation & n64_start)
        navigation = (navigation & ~n64_start) | n64_a;
    if (*y > 0.55f) navigation |= n64_du;
    if (*y < -0.55f) navigation |= n64_dd;
    if (*x < -0.55f) navigation |= n64_dl;
    if (*x > 0.55f) navigation |= n64_dr;
    uint16_t pressed =
        navigation & static_cast<uint16_t>(~state.previous_navigation);
    state.previous_navigation = navigation;
    // Repeat directions only. Confirm and Back always remain single presses.
    const uint16_t repeat_mask = state.adjusting
        ? (n64_dl | n64_dr | n64_l | n64_r) : (n64_du | n64_dd);
    const uint16_t direction = (state.options_visible ||
        (view && view->screen == sote::menu_skin::Screen::Pause)) && !state.submenu_bindings
        ? navigation & repeat_mask : 0;
    if (direction != state.repeat_direction) {
        state.repeat_direction = direction;
        state.repeat_at = now + std::chrono::milliseconds{400};
    } else if (direction && !(direction & (direction - 1)) && now >= state.repeat_at) {
        pressed |= direction;
        state.repeat_at = now + std::chrono::milliseconds{100};
    }
    if (pressed && view && (state.options_visible || state.submenu_active)) {
        std::printf("[sote][menu-input] buttons=%04X screen=%d native_selection=%d category=%d selection=%d submenu=%d transition=%d cutscenes_applied=%s cutscenes_pending=%s\n",
            pressed, int(view->screen), view->focused_setting, int(state.category),
            state.submenu_selection, state.submenu_active, int(state.transition),
            state.applied.pc_cutscenes ? "PC" : "N64", state.editing.pc_cutscenes ? "PC" : "N64");
        std::fflush(stdout);
    }

    auto consume_menu_input = [&] {
        *buttons &= static_cast<uint16_t>(~menu_buttons);
        *x = 0.0f;
        *y = 0.0f;
    };

    if (state.transition != MenuTransition::None) {
        consume_menu_input();
        if (now - state.transition_started > std::chrono::seconds{5}) {
            std::fprintf(stderr, "[sote][menu] Page transition timed out: phase=%d category=%d selection=%d options=%d submenu=%d view=%d. Releasing menu input.\n",
                int(state.transition), int(state.category), state.submenu_selection,
                state.options_visible, state.submenu_active,
                view ? int(view->screen) : -1);
            state.transition = MenuTransition::None;
            state.submenu_active = false;
            return;
        }
        switch (state.transition) {
            case MenuTransition::WaitTitleOptions:
                if (state.options_visible && !(navigation & (n64_a | n64_start)))
                    state.transition = MenuTransition::None;
                return;
            case MenuTransition::WaitTitle:
                if (view && view->screen == sote::menu_skin::Screen::Title && navigation == 0)
                    state.transition = MenuTransition::None;
                return;
            case MenuTransition::WaitProfiles:
                if (state.requested_title_mode < 0 && !state.cancel_profile_dialog &&
                    state.front_end_mode == 3 && !state.profile_dialog && navigation == 0)
                    state.transition = MenuTransition::None;
                return;
            case MenuTransition::WaitPause:
                if (!state.cancel_pause_quit && view &&
                    view->screen == sote::menu_skin::Screen::Pause &&
                    !view->pause_quit_confirmation && navigation == 0)
                    state.transition = MenuTransition::None;
                return;
            case MenuTransition::ReturnToMain:
                // The guest capture selects Return directly before we confirm it.
                if (state.original_selection == 52)
                    set_transition_locked(MenuTransition::ActivateReturn);
                return;
            case MenuTransition::WaitMain:
                if (view && view->screen == sote::menu_skin::Screen::Pause && navigation == 0)
                    state.transition = MenuTransition::None;
                return;
            case MenuTransition::FocusResume:
                return;
            case MenuTransition::ActivateResume:
                if (view && view->screen == sote::menu_skin::Screen::Pause) {
                    *buttons |= n64_a;
                } else if (navigation == 0) {
                    state.transition = MenuTransition::None;
                }
                return;
            case MenuTransition::ActivateReturn:
                *buttons |= n64_a;
                if (now - state.phase_started > std::chrono::milliseconds{80}) {
                    set_transition_locked(MenuTransition::WaitMain);
                }
                return;
            default:
                return;
        }
    }

    using Screen = sote::menu_skin::Screen;
    if (view && view->screen == Screen::Pause && view->pause_quit_confirmation &&
        view->focused_setting == 2 && (pressed & n64_a)) {
        // Let the native handler finish the level exit, then select our title
        // instead of its old level-card destination. Consume confirmation across
        // that handoff so it cannot immediately start the selected level again.
        state.title_selection = 0;
        state.requested_title_mode = 0;
        state.transition_started = now;
        set_transition_locked(MenuTransition::WaitTitle);
        consume_menu_input();
        *buttons |= n64_a;
        return;
    }
    if (view && view->screen == Screen::Pause && ((pressed & n64_b) ||
        (view->pause_quit_confirmation && view->focused_setting == 1 && (pressed & n64_a)))) {
        // The retail Pause handler treats B as confirmation of the focused row.
        state.transition_started = now;
        if (view->pause_quit_confirmation) {
            state.cancel_pause_quit = true;
            set_transition_locked(MenuTransition::WaitPause);
        } else {
            set_transition_locked(MenuTransition::FocusResume);
        }
        consume_menu_input();
        return;
    }
    if (view && view->screen == Screen::Pause) {
        if (pressed & (n64_du | n64_dd)) {
            const int first = view->pause_quit_confirmation ? 1 : 0;
            const int count = 3 - first;
            const int delta = (pressed & n64_dd) ? 1 : -1;
            state.requested_pause_focus = first +
                (view->focused_setting - first + delta + count) % count;
        }
        // Native analog smoothing can miss short taps. Apply the discrete
        // selection on the guest thread and prevent a second native movement.
        *buttons &= static_cast<uint16_t>(~(n64_du | n64_dd | n64_dl | n64_dr | n64_b));
        *x = *y = 0;
        return;
    }
    if (!(navigation & (n64_a | n64_start))) state.title_start_release = false;
    if (view && view->screen == Screen::Title) {
        consume_menu_input();
        if (pressed & (n64_du | n64_dl | n64_l)) state.title_selection = 0;
        if (pressed & (n64_dd | n64_dr | n64_r)) state.title_selection = 1;
        if (pressed & (n64_a | n64_start)) {
            if (state.title_selection == 0) {
                *buttons |= n64_start;
                state.title_start_release = true;
            } else {
                state.title_options = true;
                state.editing = state.applied;
                state.requested_title_mode = 4;
                state.transition_started = now;
                set_transition_locked(MenuTransition::WaitTitleOptions);
            }
        }
        return;
    }
    if (state.front_end_mode == 2 || state.front_end_mode == 3) {
        if (state.title_start_release) {
            consume_menu_input();
            return;
        }
        if (pressed & n64_b) {
            if (state.front_end_mode == 2) {
                state.requested_title_mode = 3;
                state.transition_started = now;
                set_transition_locked(MenuTransition::WaitProfiles);
            } else if (state.profile_dialog) {
                state.cancel_profile_dialog = true;
                state.transition_started = now;
                set_transition_locked(MenuTransition::WaitProfiles);
            } else {
                state.title_selection = 0;
                state.requested_title_mode = 0;
                state.transition_started = now;
                set_transition_locked(MenuTransition::WaitTitle);
            }
            consume_menu_input();
            return;
        }
        // Back belongs to the host navigation above. The guest can reuse the
        // same B tap as confirmation on the screen we have just returned to.
        *buttons &= static_cast<uint16_t>(~n64_b);
        if (state.front_end_mode == 3) {
            // The save picker and its dialogs have native digital directions.
            // Keep those immediate; a short synthetic stick movement may never
            // cross the retail menu's accumulated analog threshold.
            *buttons |= navigation & (n64_du | n64_dd);
            *y = 0;
        }
        if (state.front_end_mode == 3 && !state.profile_dialog) {
            const bool left = (pressed & (n64_dl | n64_l)) != 0;
            const bool right = (pressed & (n64_dr | n64_r)) != 0;
            if (left != right) {
                state.profile_action_direction = right ? 1 : -1;
                state.requested_profile_action = state.profile_action_index < 0
                    ? (right ? 1 : 2) : (state.profile_action_index == 1 ? 2 : 1);
            }
            // Rename/Clear use the same horizontal controls as Options. The
            // retail picker requires a separate L/R press to reach this row.
            if (navigation & (n64_dl | n64_dr | n64_l | n64_r)) {
                consume_menu_input();
                return;
            }
        }
        return;
    }
    if (!state.options_visible) return;

    if (!view || (view->screen != sote::menu_skin::Screen::Options &&
                  view->screen != sote::menu_skin::Screen::Graphics &&
                  view->screen != sote::menu_skin::Screen::Schemes &&
                  !(view->screen == sote::menu_skin::Screen::Controls && !view->binding_editing))) return;

    *buttons &= static_cast<uint16_t>(~n64_b);

    if (state.submenu_bindings) {
        consume_menu_input();
        return;
    }
    const bool confirm = (pressed & (n64_a | n64_start)) != 0;
    const bool left = (pressed & (n64_dl | n64_l)) != 0;
    const bool right = (pressed & (n64_dr | n64_r)) != 0;
    const int horizontal = int(right) - int(left);
    if (state.adjusting) {
        consume_menu_input();
        if (confirm || (pressed & n64_b)) {
            state.adjusting = false;
        } else if (horizontal != 0) {
            if (state.submenu_active)
                adjust_selected_setting_locked(horizontal);
            else
                *x = horizontal * (80.0f / 127.0f);
        }
        return;
    }
    *buttons &= static_cast<uint16_t>(~(n64_dl | n64_dr | n64_l | n64_r));
    *x = 0.0f;
    if (horizontal != 0) {
        switch_page_locked(horizontal);
        consume_menu_input();
        return;
    }

    if ((pressed & n64_b) ||
        ((pressed & (n64_a | n64_start)) &&
            (state.submenu_active ? state.submenu_selection == 0 : state.original_selection == 52))) {
        return_to_main_locked();
        consume_menu_input();
        return;
    }

    if (state.submenu_active) {
        consume_menu_input();
        const int item_count = submenu_item_count(state.category);
        if ((pressed & n64_du) != 0) {
            state.submenu_selection =
                (state.submenu_selection + item_count - 1) % item_count;
        } else if ((pressed & n64_dd) != 0) {
            state.submenu_selection =
                (state.submenu_selection + 1) % item_count;
        }
        if (confirm) {
            if (state.category == TopCategory::Cheats) {
                state.adjusting = true;
            } else if (state.category == TopCategory::Controls) {
                if (state.submenu_selection ==
                           3 + sote::controls_menu::tuning_menu_row_count()) {
                    sote::controls_menu::persist();
                    sote::controls_menu::persist_tuning();
                } else if (state.submenu_selection ==
                           4 + sote::controls_menu::tuning_menu_row_count()) {
                    // Keep the Controls page and its selection underneath the
                    // editor for both origins, so Back returns to Edit Bindings.
                    sote::control_bindings::open_editor();
                    state.submenu_bindings = sote::control_bindings::editor_view().editing;
                } else {
                    state.adjusting = true;
                }
            } else if (state.submenu_selection <= 9) {
                state.adjusting = true;
            } else if (state.submenu_selection == 10) {
                queue_apply_locked();
            } else {
                state.editing = Settings{};
            }
        }
        return;
    }

    if (pressed & (n64_du | n64_dd)) {
        const int count = state.title_options ? 6 : 7;
        const int selection = state.original_selection == 52 ? 0 :
            (state.original_selection - 51) / 2;
        const int delta = (pressed & n64_dd) ? 1 : -1;
        state.requested_options_focus = (selection + delta + count) % count;
    }
    *buttons &= static_cast<uint16_t>(~(n64_du | n64_dd));
    *y = 0;
    if (state.original_selection != 63) {
        *buttons &= static_cast<uint16_t>(~(n64_a | n64_start));
        if (confirm) {
            state.adjusting = true;
            consume_menu_input();
        }
    }

}


void observe_native_options(int selection) {
    std::lock_guard lock{state.mutex};
    state.options_visible = true;
    state.last_options_seen = std::chrono::steady_clock::now();
    state.original_selection = selection == 0 ? 52 : 51 + selection * 2;
}

void observe_other_screen(bool pause) {
    std::lock_guard lock{state.mutex};
    state.adjusting = false;
    state.requested_options_focus = -1;
    state.return_to_pause = pause;
    state.title_options = state.submenu_bindings = false;
    state.options_visible = false;
    state.submenu_active = false;
    if (!pause && (state.transition == MenuTransition::ReturnToMain ||
        state.transition == MenuTransition::ActivateReturn ||
        state.transition == MenuTransition::WaitMain)) {
        set_transition_locked(MenuTransition::None);
    }
}

void synchronize_guest_focus(uint8_t* rdram) {
    std::lock_guard lock{state.mutex};
    if (sote::menu_skin::native_pause_active(rdram, 0x800000)) {
        if (state.cancel_pause_quit) {
            write_half(rdram, 0x800D094C, 0);
            write_half(rdram, 0x800D094E, 0);
            state.requested_pause_focus = 2;
            state.cancel_pause_quit = false;
        }
        if (state.requested_pause_focus >= 0) {
            write_half(rdram, 0x800D0944, state.requested_pause_focus);
            state.requested_pause_focus = -1;
        }
    }
    if (sote::menu_skin::native_options_active(rdram, 0x800000)) {
        if (state.transition == MenuTransition::ReturnToMain) {
            write_half(rdram, 0x800DD5E0, 0);
        } else if (state.reset_options_focus) {
            write_half(rdram, 0x800DD5E0, 1);
            state.reset_options_focus = false;
        } else if (state.requested_options_focus >= 0) {
            write_half(rdram, 0x800DD5E0, state.requested_options_focus);
            state.requested_options_focus = -1;
        }
    } else if (state.transition == MenuTransition::FocusResume &&
               sote::menu_skin::native_pause_active(rdram, 0x800000)) {
        write_half(rdram, 0x800D0944, 0);
        set_transition_locked(MenuTransition::ActivateResume);
    } else if (state.return_to_pause &&
               (state.transition == MenuTransition::ActivateReturn ||
                state.transition == MenuTransition::WaitMain) &&
               sote::menu_skin::native_pause_active(rdram, 0x800000)) {
        // Native Return resets the Pause selection to Resume.
        write_half(rdram, 0x800D0944, 1);
        state.transition = MenuTransition::WaitMain;
    }
}

void decorate_title_snapshot(sote::menu_skin::Snapshot& view) {
    std::lock_guard lock{state.mutex};
    if (state.transition != MenuTransition::WaitTitleOptions) {
        state.title_options = state.submenu_bindings = false;
        state.options_visible = state.submenu_active = state.return_to_pause = false;
    }
    view.focused_setting = state.title_selection;
}

void decorate_native_snapshot(sote::menu_skin::Snapshot& view) {
    using namespace sote::menu_skin;
    std::lock_guard lock{state.mutex};
    refresh_cutscene_availability_locked();
    view.pc_cutscenes_available = pc_movies_available;
    view.compatibility = state.editing.compatibility;
    view.pause_background = state.return_to_pause;
    view.title_options = state.title_options;
    view.adjusting = state.adjusting;
    if (state.submenu_bindings) {
        if (sote::control_bindings::editor_view().editing) {
            view.screen = Screen::Controls;
            return;
        }
        state.submenu_bindings = false;
    }
    auto put = [&](int row, std::string text, int x, int y, int selection = -1) {
        const bool focus = selection >= 0 && selection == state.submenu_selection;
        view.rows[row] = {x, y, focus ? selected_color : normal_color, plain_text(text)};
    };
    if (!state.submenu_active) {
        if (!state.title_options) view.rows[63].text = "Control Bindings";
        view.rows[52].text = "Return to previous menu";
        constexpr const char* help[] = {"Return to the previous screen.",
            "Show or hide on-screen gameplay information.", "Choose the seeker camera behavior.",
            "Adjust sound effects volume.", "Adjust music volume.",
            "Choose the sound channel arrangement.", "Enter / A: open keyboard, mouse and gamepad bindings."};
        if (view.focused_setting>=0 && view.focused_setting<7) view.option_help=help[view.focused_setting];
        return;
    }
    view.focused_setting = state.submenu_selection;
    view.rows = {};
    if (state.category == TopCategory::Cheats) {
        view.screen = Screen::Schemes;
        constexpr const char* help[] = {"Return to previous menu.",
            "Restore nonlethal damage. Lethal damage still allows respawn.",
            "Prevent life loss and maintain at least three spare lives.",
            "Keep acquired ammunition from decreasing.", "Keep acquired vehicle missiles from decreasing.",
            "Replenish Hoth, train and Outrider health after nonlethal damage.",
            "Pulse held fire in Modern on-foot controls.", "Choose fire pulses per second. Weapon cooldowns still apply.",
            "Adjust how strongly Modern aim follows nearby targets.", "Adjust the angle in which aim assistance finds targets."};
        view.option_help=help[std::clamp(state.submenu_selection,0,9)];
        put(52, std::string{state.title_options ? "" : "Main   "} + cheats_header, 160, 50);
        for (int selection = 1; selection < cheats_item_count(); ++selection) {
            const int text_row = 53 + (selection - 1) * 2;
            const int y = 66 + (selection - 1) * 14;
            put(text_row, cheat_row_label(selection), 60, y, selection);
            put(text_row + 1, cheat_row_value(selection), 176, y, selection);
        }
        put(75, "Return to previous menu", 160, 220, 0);
        return;
    }
    if (state.category == TopCategory::Controls) {
        view.screen = Screen::Schemes;
        if(state.submenu_selection==0) view.option_help="Return to previous menu.";
        else if(state.submenu_selection==1) view.option_help="Choose Modern aiming or Classic PC controls on foot.";
        else if(state.submenu_selection==2) view.option_help="Choose Modern triggers or Classic controls for the speeder bike.";
        else {
            constexpr const char* tuning_help[] = {
                "Ignore small movement-stick input to prevent drift.",
                "Scale movement-stick input. Higher values reach full speed sooner.",
                "Ignore small aiming-stick input to prevent camera drift.",
                "Scale aiming-stick input. Higher values increase aiming response.",
                "Choose how far a trigger must move before it responds.",
                "Scale trigger input. Higher values reach full input sooner.",
                "Set Modern horizontal aiming speed in degrees per second.",
                "Set Modern vertical aiming speed in degrees per second.",
                "Adjust mouse movement sensitivity for Modern aiming."};
            const int index = state.submenu_selection - 3;
            view.option_help = index >= 0 && index < 9 ? tuning_help[index] :
                state.submenu_selection == 4 + controls_menu::tuning_menu_row_count() ?
                "Change keyboard, mouse-button and gamepad bindings for every gameplay context." :
                "Save control schemes and tuning settings.";
        }
        put(52, state.title_options ? "Options   Graphics   < Controls >   Cheats" :
            "Main   Options   Graphics   < Controls >   Cheats", 160, 50);
        put(53, "On Foot", 52, 66, 1);
        put(54, controls_menu::current_scheme(controls_menu::SchemeSlot::OnFoot) == controls_menu::ControlScheme::Modern ? "Modern" : "Classic", 166, 66, 1);
        put(55, "Speeder Bike", 52, 78, 2);
        put(56, controls_menu::current_scheme(controls_menu::SchemeSlot::Bike) == controls_menu::ControlScheme::Modern ? "Modern" : "Classic", 166, 78, 2);
        const int tuning_count = controls_menu::tuning_menu_row_count();
        for (int index = 0; index < tuning_count; ++index) {
            controls_menu::TuningMenuRow row{};
            if (!controls_menu::tuning_menu_row(index, row)) continue;
            const int selection = index + 3;
            const int text_row = 57 + index * 2;
            const int y = 66 + (index + 2) * 12;
            put(text_row, row.label, 52, y, selection);
            put(text_row + 1, std::string{row.slider} + " " + row.value, 166, y, selection);
        }
        put(75, "Return to previous menu", 105, 220, 0);
        put(76, "Apply", 220, 220, 3 + tuning_count);
        put(77, "Edit Bindings", 270, 220, 4 + tuning_count);
    } else {
        view.screen = Screen::Graphics;
        constexpr const char* help[] = {"Return to previous menu without applying pending changes.",
            "Choose the game window's output resolution.", "Higher render scales sharpen 3D graphics and use more GPU power.",
            "Choose original 4:3 proportions or a widescreen view.", "Smooth jagged edges. Higher settings use more GPU power.",
            "Choose a bordered window or borderless display.", "Choose original N64 scenes or PC video cutscenes.",
            "Choose the original game font or modern text. Changes save immediately.",
            "Conservative graphics settings that may reduce flickering.\nApply, then restart to finish changing color precision.",
            "Apply without restarting. Loading texture packs may briefly pause the game.",
            "Save and apply graphics settings. Compatibility color changes need a restart.",
            "Restore default graphics settings. Select Apply to save."};
        view.option_help=help[std::clamp(state.submenu_selection,0,11)];
        if(state.submenu_selection==6 && !pc_movies_available)
            view.option_help="PC cutscenes require SAN files in Sdata beside the executable.";
        if(state.submenu_selection==4 && state.editing.compatibility)
            view.option_help="Compatibility Mode disables antialiasing. Turn it off to restore your setting.";
        put(52, state.title_options ? "Options   < Graphics >   Controls   Cheats" :
            "Main   Options   < Graphics >   Controls   Cheats", 160, 50);
        put(53, "Resolution", 72, 70, 1); put(54, output_resolution_name(state.editing), 160, 70, 1);
        put(55, "Render Scale", 72, 86, 2); put(56, resolution_name(state.editing.resolution), 160, 86, 2);
        put(57, "Aspect Ratio", 72, 102, 3); put(58, state.editing.widescreen ? "Widescreen" : "Original 4:3", 160, 102, 3);
        put(59, "Antialiasing", 72, 118, 4); put(60, antialiasing_name(state.editing.compatibility ? AntialiasingPreset::Off : state.editing.antialiasing), 160, 118, 4);
        put(61, "Display Mode", 72, 134, 5); put(62, state.editing.borderless ? "Borderless" : "Windowed", 160, 134, 5);
        put(63, "Cutscenes", 72, 150, 6); put(65, state.editing.pc_cutscenes ? "PC" : "N64", 160, 150, 6);
        put(69, "Font", 72, 166, 7);
        put(70, sote::menu_skin::menu_font_original() ? "Original" : "Modern", 160, 166, 7);
        put(71,"Compatibility Mode",42,181,8); put(72,state.editing.compatibility ? "On" : "Off",200,181,8);
        put(73,"HD Textures",72,194,9); put(74,state.editing.hd_textures ? "On" : "Off",200,194,9);
        put(67, "Apply", 110, 208, 10); put(68, "Reset", 210, 208, 11);
        put(64, "Return to previous menu", 160, 220, 0);
    }
}

void update_guest_menu(uint8_t* rdram) {
    std::lock_guard lock{state.mutex};
    const int previous_mode = state.front_end_mode;
    state.front_end_mode = -1;
    // func_8003B480 uses mode 0 for the title, 3 for profiles and 4
    // for front-end options. Apply requests on the guest thread.
    if (sote::menu_skin::native_text_enabled() && read_half(rdram, 0x8013CE0E) == 2) {
        // Host-handled navigation never reaches the retail inactivity logic.
        // Keep Options/editor activity from accumulating an attract-mode timeout
        // that would immediately kick in when returning to the title.
        if (state.title_options || state.previous_navigation != 0) {
            write_half(rdram, 0x800DD740, 0);
            write_half(rdram, 0x800DD742, 0);
        }
        const uint32_t front_end_mode = static_cast<int16_t>(read_half(rdram, 0x800DD744)) >= 0
            ? 0x800DD744 : 0x800DD748;
        if (state.title_options && state.reset_options_focus && read_half(rdram, front_end_mode) == 4) {
            write_half(rdram, 0x800DD5E0, 1);
            state.reset_options_focus = false;
        } else if (state.title_options && state.requested_options_focus >= 0 && read_half(rdram, front_end_mode) == 4) {
            write_half(rdram, 0x800DD5E0, state.requested_options_focus);
            state.requested_options_focus = -1;
        }
        if (state.requested_title_mode >= 0) {
            write_half(rdram, front_end_mode, static_cast<int16_t>(state.requested_title_mode));
            write_half(rdram, 0x800DD5E0, 1);
            state.requested_title_mode = -1;
        }
        if (!state.title_options && read_half(rdram, front_end_mode) == 4)
            write_half(rdram, front_end_mode, 3);
        state.front_end_mode = read_half(rdram, front_end_mode);
        if (state.front_end_mode == 3) {
            if (previous_mode == 0 || previous_mode == 2) {
                write_half(rdram, 0x800DD750, -1);
                write_half(rdram, 0x800DD754, 0);
            }
            if (state.cancel_profile_dialog) {
                write_half(rdram, 0x800DD754, 0);
                write_half(rdram, 0x800DD758, 0);
                write_half(rdram, 0x800DD75A, 0);
                // Clear uses footer index 3 as its confirmation state.
                if (read_half(rdram, 0x800DD750) == 3)
                    write_half(rdram, 0x800DD750, 2);
                state.cancel_profile_dialog = false;
            }
            state.profile_dialog = read_word(rdram, 0x800DD758) != 0 ||
                read_half(rdram, 0x800DD754) != 0 || read_half(rdram, 0x800DD750) == 3;
            if (state.requested_profile_action >= 0 && !state.profile_dialog) {
                write_half(rdram, 0x800DD750, state.requested_profile_action);
                state.requested_profile_action = -1;
            }
            // This is the footer action index. -1 is the normal save-row focus.
            if (read_half(rdram, 0x800DD750) == 0 && !state.profile_dialog)
                write_half(rdram, 0x800DD750, state.profile_action_direction < 0 ? 2 : 1);
            state.profile_action_index = static_cast<int16_t>(read_half(rdram, 0x800DD750));
            if (sote::menu_skin::plain_text(read_row(rdram, 76)) == "Options")
                write_half(rdram, ui_positions + 76 * 4, -1000);
        }
        if (std::getenv("SOTE_TRACE_PROFILE")) {
            static int last_mode = -1, last_cursor = -2, last_action = -1, last_slot = -1;
            const int cursor = static_cast<int16_t>(read_half(rdram, 0x800DD750));
            const int action = read_half(rdram, 0x800DD754);
            const int slot = read_byte(rdram, 0x8018BBFD);
            if (last_mode != state.front_end_mode || last_cursor != cursor ||
                last_action != action || last_slot != slot) {
                std::printf("[sote][picker] mode=%d cursor=%d action=%d slot=%d release=%d\n",
                    state.front_end_mode, cursor, action, slot, state.title_start_release);
                std::fflush(stdout);
                last_mode = state.front_end_mode;
                last_cursor = cursor;
                last_action = action;
                last_slot = slot;
            }
        }
        if (read_half(rdram, front_end_mode) == 0) {
            for (int row = 0; row < 80; ++row) {
                if (sote::menu_skin::plain_text(read_row(rdram, row)) == "Press Start")
                    write_half(rdram, ui_positions + row * 4, -1000);
            }
        }
    }
    const bool native_options =
        read_row(rdram, 53).find("Overlay Displays") !=
            std::string::npos &&
        static_cast<int16_t>(
            read_half(
                rdram,
                ui_positions + static_cast<uint32_t>(53 * 4))) == 90;

    if (std::getenv("SOTE_TRACE_MENU") != nullptr) {
        // Find which row actually carries the Options screen's marker text,
        // rather than assuming it is row 53.
        // Dump every populated row so the screen's actual layout is visible
        // instead of assumed. Keyed on the row set so each distinct screen
        // reports once.
        // Only dump the target screen. Keying on colors makes every pulsing
        // title-screen frame a new state and floods the log.
        std::string signature;
        bool is_target = false;
        for (int row = 0; row < 80; ++row) {
            const std::string text = read_row(rdram, row);
            if (text.find("Overlay Displays") != std::string::npos) {
                is_target = true;
            }
            signature += text;
        }
        static std::string reported_signature;
        if (is_target && signature != reported_signature) {
            reported_signature = signature;
            std::printf("[sote][menu] --- screen (native_options=%d) ---\n",
                        native_options ? 1 : 0);
            for (int row = 0; row < 80; ++row) {
                const std::string text = read_row(rdram, row);
                if (text.empty()) {
                    continue;
                }
                std::printf(
                    "[sote][menu]   row %2d x=%4d color=%08X '%s'\n",
                    row,
                    static_cast<int>(static_cast<int16_t>(read_half(
                        rdram,
                        ui_positions + static_cast<uint32_t>(row * 4)))),
                    read_word(
                        rdram,
                        ui_colors + static_cast<uint32_t>(row * 4)),
                    text.c_str());
            }
            std::fflush(stdout);
        }
    }

    if (!native_options) {
        return;
    }

    state.options_visible = true;
    state.last_options_seen = std::chrono::steady_clock::now();
    if (!state.graphics_focus && !state.submenu_active) {
        state.original_selection =
            detect_original_selection(rdram);
    }
    if (state.submenu_active) {
        if (state.category == TopCategory::Cheats) {
            draw_cheats_submenu(rdram);
        } else if (state.category == TopCategory::Controls) {
            draw_controls_submenu(rdram);
        } else {
            draw_graphics_submenu(rdram);
        }
    }
}

} // namespace sote::graphics_menu

extern "C" void sote_update_graphics_menu(uint8_t* rdram) {
    sote::graphics_menu::update_guest_menu(rdram);
}
