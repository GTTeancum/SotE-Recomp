#include "graphics_menu.hpp"

#include "controls_menu.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
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
constexpr uint16_t n64_du = 0x0800;
constexpr uint16_t n64_dd = 0x0400;
constexpr uint16_t n64_dl = 0x0200;
constexpr uint16_t n64_dr = 0x0100;
constexpr uint16_t menu_buttons =
    n64_a | n64_b | n64_start | n64_du | n64_dd | n64_dl | n64_dr;

constexpr uint32_t ui_string_table_pointer = 0x8013CE30U;
constexpr uint32_t ui_positions = 0x80111110U;
constexpr uint32_t ui_colors = 0x80111250U;
constexpr uint32_t ui_flags = 0x80111390U;

constexpr uint32_t normal_color = 0x408040FFU;
constexpr uint32_t selected_color = 0xC8FFC8FFU;
constexpr uint32_t selected_value_color = 0xD060E8FFU;

// The single row-63/64 entry appended below the native menu now cycles
// between two categories via Left/Right instead of us reserving separate,
// unverified rows for a second entry. Both categories share the same
// proven-safe row range (52-66) for their submenu content, since only one
// submenu is ever drawn at a time.
enum class TopCategory : int {
    Graphics,
    Controls,
};

constexpr int graphics_submenu_item_count = 8;
// 0 = Return, 1 = On Foot, 2 = Speeder Bike, 3 = Apply.
constexpr int controls_submenu_item_count = 4;

int submenu_item_count(TopCategory category) {
    return category == TopCategory::Controls
        ? controls_submenu_item_count
        : graphics_submenu_item_count;
}

struct MenuState {
    std::mutex mutex;
    bool initialized = false;
    bool options_visible = false;
    bool graphics_focus = false;
    bool submenu_active = false;
    TopCategory category = TopCategory::Graphics;
    int original_selection = 52;
    int submenu_selection = 0;
    uint16_t previous_navigation = 0;
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
    for (const int row : {53, 55, 57, 59, 61}) {
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
           << ",\n  \"outputWidth\": "
           << state.applied.output_width
           << ",\n  \"outputHeight\": "
           << state.applied.output_height
           << "\n}\n";
}

void queue_apply_locked() {
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

void adjust_selected_setting_locked(int direction) {
    if (state.category == TopCategory::Controls) {
        if (state.submenu_selection == 1) {
            sote::controls_menu::cycle_scheme(
                sote::controls_menu::SchemeSlot::OnFoot, direction);
        } else if (state.submenu_selection == 2) {
            sote::controls_menu::cycle_scheme(
                sote::controls_menu::SchemeSlot::Bike, direction);
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
            state.editing.antialiasing = cycle_enum(
                state.editing.antialiasing,
                4,
                direction);
            break;
        case 5:
            state.editing.borderless =
                !state.editing.borderless;
            break;
        default:
            break;
    }
}

void draw_parent_entry(uint8_t* rdram) {
    const bool focused = state.graphics_focus;
    if (focused) {
        set_row_color(rdram, 61, normal_color);
        set_row_color(rdram, 62, normal_color);
    }
    // There is only one injected row, so nothing otherwise tells the player
    // that Left/Right swaps which category it opens. Show the markers only
    // while focused, to avoid cluttering the row at rest.
    const bool controls = state.category == TopCategory::Controls;
    const char* label = focused
        ? (controls ? "~s< Controls >" : "~s< Graphics >")
        : (controls ? "~sControls" : "~sGraphics");
    set_row(
        rdram,
        63,
        label,
        90,
        150,
        focused ? selected_color : normal_color);
    set_row(
        rdram,
        64,
        "~sOptions",
        200,
        150,
        focused ? selected_value_color : normal_color);
}

void draw_controls_submenu(uint8_t* rdram) {
    // Rows 50 and 51 are claimed too: the legend needs a separate row per
    // field to align its columns, and 52-79 alone is one row short.
    for (int row = 50; row < 80; ++row) {
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
        "~s~cControls Options",
        160,
        50,
        normal_color);
    // These labels are wider than any Graphics label, so their values need a
    // column further right than the 160 those rows use or the two run
    // together.
    constexpr int scheme_value_x = 184;
    auto scheme_name = [](sote::controls_menu::SchemeSlot slot) {
        return sote::controls_menu::current_scheme(slot) ==
                sote::controls_menu::ControlScheme::Modern
            ? "~sModern"
            : "~sClassic";
    };

    set_row(rdram, 53, "~sOn Foot", 72, 84, label_color(1));
    set_row(
        rdram,
        54,
        scheme_name(sote::controls_menu::SchemeSlot::OnFoot),
        scheme_value_x,
        84,
        value_color(1));
    set_row(rdram, 55, "~sSpeeder Bike", 72, 110, label_color(2));
    set_row(
        rdram,
        56,
        scheme_name(sote::controls_menu::SchemeSlot::Bike),
        scheme_value_x,
        110,
        value_color(2));

    // Both actions share one line so neither falls outside the panel.
    set_row(rdram, 57, "~s~cApply", 116, 140, label_color(3));
    set_row(rdram, 58, "~s~cReturn", 208, 140, label_color(0));

    // The legend goes in the open screen below the panel, where there is
    // room for a readable two-column list instead of a cramped one.
    const sote::controls_menu::SchemeSlot highlighted =
        state.submenu_selection == 2
            ? sote::controls_menu::SchemeSlot::Bike
            : sote::controls_menu::SchemeSlot::OnFoot;
    // One composed line per action keeps a ten-entry list inside both the
    // 80-row string table and the 240-unit framebuffer. Two columns of five,
    // each "Action  Pad / Key".
    constexpr int legend_capacity = 12;
    constexpr int legend_rows = 6;
    const sote::controls_menu::LegendEntry* legend[legend_capacity] = {};
    const int legend_count = sote::controls_menu::scheme_legend(
        highlighted, legend, legend_capacity);
    // The guest font is proportional, so padding the action with spaces
    // cannot line the bindings up. Each field gets its own row at a fixed x
    // instead, which needs two rows per entry.
    static constexpr int row_pool[] = {
        50, 51, 59, 60, 61, 62, 63, 64, 65, 66, 67,
        68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
    };
    constexpr int row_pool_size =
        static_cast<int>(sizeof(row_pool) / sizeof(row_pool[0]));
    int pool_index = 0;
    for (int entry = 0; entry < legend_count; ++entry) {
        if (pool_index + 2 > row_pool_size) {
            break;
        }
        const int column = entry / legend_rows;
        const int row = entry % legend_rows;
        const int action_x = column == 0 ? 6 : 152;
        const int binding_x = column == 0 ? 64 : 208;
        const int y = 166 + row * 11;
        char binding[32];
        std::snprintf(
            binding,
            sizeof(binding),
            "~s%s / %s",
            legend[entry]->pad,
            legend[entry]->key);
        char action[24];
        std::snprintf(action, sizeof(action), "~s%s", legend[entry]->action);
        set_row(rdram, row_pool[pool_index++], action, action_x, y,
                normal_color);
        set_row(rdram, row_pool[pool_index++], binding, binding_x, y,
                normal_color);
    }
}

void draw_graphics_submenu(uint8_t* rdram) {
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
        "~s~cGraphics Options",
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
        antialiasing_name(state.editing.antialiasing),
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
        "~s~cApply",
        110,
        151,
        label_color(6));
    set_row(
        rdram,
        66,
        "~s~cReset",
        210,
        151,
        label_color(7));
    set_row(
        rdram,
        64,
        "~s~cReturn to Game Options",
        160,
        170,
        label_color(0));
}

} // namespace

void initialize(const std::filesystem::path& data_directory) {
    std::lock_guard lock{state.mutex};
    state.persistence_path = data_directory / "sote_options.json";
    std::ifstream input{state.persistence_path};
    if (input.is_open()) {
        const std::string contents{
            std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}};
        state.applied.borderless =
            contents.find("\"borderless\": true") != std::string::npos;
        state.applied.output_width = read_persisted_integer(
            contents,
            "\"outputWidth\"",
            state.applied.output_width);
        state.applied.output_height = read_persisted_integer(
            contents,
            "\"outputHeight\"",
            state.applied.output_height);
        state.editing.borderless = state.applied.borderless;
        state.editing.output_width = state.applied.output_width;
        state.editing.output_height = state.applied.output_height;
    }
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
    const bool saved_borderless = state.applied.borderless;
    const int saved_output_width = state.applied.output_width;
    const int saved_output_height = state.applied.output_height;
    state.applied = settings;
    state.applied.borderless = saved_borderless;
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

void filter_input(uint16_t* buttons, float* x, float* y) {
    std::lock_guard lock{state.mutex};

    if (state.options_visible &&
        std::chrono::steady_clock::now() - state.last_options_seen >
            std::chrono::milliseconds{150}) {
        state.options_visible = false;
        state.graphics_focus = false;
        state.submenu_active = false;
    }

    uint16_t navigation = *buttons & menu_buttons;
    if (*y > 0.55f) navigation |= n64_du;
    if (*y < -0.55f) navigation |= n64_dd;
    if (*x < -0.55f) navigation |= n64_dl;
    if (*x > 0.55f) navigation |= n64_dr;
    const uint16_t pressed =
        navigation & static_cast<uint16_t>(~state.previous_navigation);
    state.previous_navigation = navigation;

    if (!state.options_visible) {
        return;
    }

    auto consume_menu_input = [&] {
        *buttons &= static_cast<uint16_t>(~menu_buttons);
        *x = 0.0f;
        *y = 0.0f;
    };

    if (state.submenu_active) {
        consume_menu_input();
        if ((pressed & n64_b) != 0) {
            state.submenu_active = false;
            state.graphics_focus = true;
            state.editing = state.applied;
            return;
        }
        const int item_count = submenu_item_count(state.category);
        if ((pressed & n64_du) != 0) {
            state.submenu_selection =
                (state.submenu_selection + item_count - 1) % item_count;
        } else if ((pressed & n64_dd) != 0) {
            state.submenu_selection =
                (state.submenu_selection + 1) % item_count;
        }
        if ((pressed & n64_dl) != 0) {
            adjust_selected_setting_locked(-1);
        } else if ((pressed & n64_dr) != 0) {
            adjust_selected_setting_locked(1);
        }
        if ((pressed & (n64_a | n64_start)) != 0) {
            if (state.submenu_selection == 0) {
                state.submenu_active = false;
                state.graphics_focus = true;
                state.editing = state.applied;
            } else if (state.category == TopCategory::Controls) {
                if (state.submenu_selection == 1) {
                    sote::controls_menu::cycle_scheme(
                        sote::controls_menu::SchemeSlot::OnFoot, 1);
                } else if (state.submenu_selection == 2) {
                    sote::controls_menu::cycle_scheme(
                        sote::controls_menu::SchemeSlot::Bike, 1);
                } else {
                    sote::controls_menu::persist();
                }
            } else if (state.submenu_selection <= 5) {
                adjust_selected_setting_locked(1);
            } else if (state.submenu_selection == 6) {
                queue_apply_locked();
            } else {
                state.editing = Settings{};
            }
        }
        return;
    }

    if (state.graphics_focus) {
        if ((pressed & n64_du) != 0) {
            state.graphics_focus = false;
            consume_menu_input();
            return;
        }
        if ((pressed & n64_dd) != 0) {
            state.graphics_focus = false;
            return;
        }
        if ((pressed & (n64_dl | n64_dr)) != 0) {
            state.category = state.category == TopCategory::Graphics
                ? TopCategory::Controls
                : TopCategory::Graphics;
            consume_menu_input();
            return;
        }
        if ((pressed & (n64_a | n64_start)) != 0) {
            state.submenu_active = true;
            state.submenu_selection = 0;
            state.editing = state.applied;
            consume_menu_input();
            return;
        }
        if ((pressed & n64_b) != 0) {
            state.graphics_focus = false;
            return;
        }
        consume_menu_input();
        return;
    }

    if (state.original_selection == 61 &&
        (pressed & n64_dd) != 0) {
        state.graphics_focus = true;
        consume_menu_input();
    } else if (state.original_selection == 52 &&
               (pressed & n64_du) != 0) {
        state.graphics_focus = true;
        consume_menu_input();
    }
}

void update_guest_menu(uint8_t* rdram) {
    std::lock_guard lock{state.mutex};
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
        if (state.category == TopCategory::Controls) {
            draw_controls_submenu(rdram);
        } else {
            draw_graphics_submenu(rdram);
        }
    } else {
        draw_parent_entry(rdram);
    }
}

} // namespace sote::graphics_menu

extern "C" void sote_update_graphics_menu(uint8_t* rdram) {
    sote::graphics_menu::update_guest_menu(rdram);
}
