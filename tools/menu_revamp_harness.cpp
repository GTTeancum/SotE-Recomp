#include "controls_menu.hpp"
#include "control_bindings.hpp"
#include "graphics_menu.hpp"
#include "menu_skin.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

extern "C" void sote_native_text_begin(uint8_t*,uint32_t);
extern "C" int sote_native_text_glyph(uint8_t*,uint32_t,uint32_t,int,int);
extern "C" void sote_publish_native_text();

extern "C" int sote_is_bike_stage_active() {
    return 0;
}

namespace {

int checks = 0;
int failures = 0;

void check(bool condition, const char* name) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL " << name << "\n";
    }
}

struct Memory {
    std::vector<uint8_t> bytes = std::vector<uint8_t>(0x800000);
    uint32_t strings = 0x80220000;

    Memory() {
        word(0x800da818, 0x80210000);
        half(0x800da81c, 13);
        for (unsigned index = 0; index < 95; ++index) {
            const uint32_t address = 0x800da818 + index * 14;
            half(address + 6, index == 'A' - 32 ? 9 : 7);
            half(address + 8, 0);
            half(address + 10, 0);
            half(address + 12, 0);
            half(address + 14, 0);
            half(address + 16, 5);
            half(address + 18, 9);
        }
        for (int index = 0; index < 4096; ++index) {
            byte(0x80210000 + static_cast<uint32_t>(index), 0xff);
        }
    }

    void byte(uint32_t address, unsigned value) {
        bytes[(address & 0x7fffffU) ^ 3U] = static_cast<uint8_t>(value);
    }

    void half(uint32_t address, unsigned value) {
        byte(address, value >> 8);
        byte(address + 1, value);
    }

    void word(uint32_t address, uint32_t value) {
        half(address, value >> 16);
        half(address + 2, value);
    }

    uint32_t text(const std::string& value) {
        const uint32_t address = strings;
        for (char ch : value) {
            byte(strings++, static_cast<unsigned char>(ch));
        }
        byte(strings++, 0);
        return address;
    }

    void native_options() {
        word(0x800dd5e4, 1);
        word(0x800d0948, 0);
        half(0x800dd5e0, 0);
        word(0x800d0954, 0);
        byte(0x8018bbfd, 0);

        const char* labels[] = {
            "",
            "Overlay Displays",
            "Seeker Camera",
            "Sound Effects",
            "Music",
            "Sound Panning",
            "Controls",
        };
        word(0x800dd61c, text("Return to Main Menu"));
        for (int value = 1; value <= 8; ++value) {
            word(
                0x800dd61c + static_cast<uint32_t>(value) * 4,
                text("Value " + std::to_string(value)));
        }
        for (int row = 0; row < 7; ++row) {
            word(
                0x800dd5e8 + static_cast<uint32_t>(row) * 4,
                text(labels[row]));
            for (int value = 0; value < 9; ++value) {
                half(
                    0x800dd674 + static_cast<uint32_t>(row) * 18 +
                        static_cast<uint32_t>(value) * 2,
                    row == 0 ? 0 : value);
            }
        }
        for (int index = 0; index < 48; ++index) {
            word(
                0x800da70c + static_cast<uint32_t>(index) * 4,
                text("Action " + std::to_string(index)));
        }
        for (int preset = 0; preset < 8; ++preset) {
            for (int index = 0; index < 48; ++index) {
                half(
                    0x800e6808 + static_cast<uint32_t>(preset) * 96 +
                        static_cast<uint32_t>(index) * 2,
                    0x8000U >> preset);
            }
        }
    }

    void profiles(bool options_focused) {
        word(0x800dd5e4, 0);
        word(0x800d0948, 1);
        constexpr uint32_t table = 0x80230000;
        word(0x8013ce30, table);
        for (int row = 0; row < 80; ++row) {
            word(table + static_cast<uint32_t>(row) * 4, text(""));
            half(0x80111110 + static_cast<uint32_t>(row) * 4, 0xfc18);
        }
        auto put = [&](int row, const char* label, bool focused) {
            word(table + static_cast<uint32_t>(row) * 4, text(label));
            half(0x80111110 + static_cast<uint32_t>(row) * 4, 100);
            word(0x80111250 + static_cast<uint32_t>(row) * 4,
                focused ? 0xc8ffc8ff : 0x408040ff);
        };
        put(51, "Player A", !options_focused);
        put(52, "Player B", false);
        put(53, "Player C", false);
        put(54, "New Player 4", false);
        put(76, "Options", options_focused);
        put(77, "Rename", false);
        put(78, "Clear", false);
    }
};

void press(uint16_t buttons) {
    float x = 0.0f;
    float y = 0.0f;
    uint16_t neutral = 0;
    sote::graphics_menu::filter_input(&neutral, &x, &y);
    sote::graphics_menu::filter_input(&buttons, &x, &y);
}

void pump_transition(Memory& memory, bool& options_focused,
                     bool& native_options, int milliseconds = 500) {
    uint16_t previous = 0;
    for (int elapsed = 0; elapsed < milliseconds; elapsed += 10) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        uint16_t buttons = 0;
        float x = 0.0f, y = 0.0f;
        sote::graphics_menu::filter_input(&buttons, &x, &y);
        const uint16_t rising = buttons & static_cast<uint16_t>(~previous);
        if ((rising & 0x2000) != 0) {
            options_focused = !options_focused;
            memory.profiles(options_focused);
        } else if ((rising & 0x8000) != 0 && native_options) {
            memory.profiles(true);
            options_focused = true;
            native_options = false;
        } else if ((rising & 0x8000) != 0 && options_focused) {
            memory.native_options();
            native_options = true;
        } else if ((rising & 0x4000) != 0) {
            memory.profiles(true);
            options_focused = true;
            native_options = false;
        }
        if (native_options) sote_capture_native_options(memory.bytes.data());
        else sote_capture_menu(memory.bytes.data());
        previous = buttons;
    }
}

const sote::menu_skin::Snapshot& latest_snapshot() {
    const auto latest = sote::menu_skin::latest();
    if (!latest) {
        static const sote::menu_skin::Snapshot empty{};
        return empty;
    }
    return *latest;
}

bool latest_screen_is(sote::menu_skin::Screen screen) {
    const auto latest = sote::menu_skin::latest();
    return latest && latest->screen == screen;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        return 2;
    }

    const std::filesystem::path scratch = argv[1];
    {
        namespace bindings = sote::control_bindings;
        bindings::NativeTable ord{};
        ord.masks[12] = 0x8000; // Ord Mantell preset 6: Fire.
        ord.masks[13] = 0x4000; // Jump.
        ord.masks[14] = 0x0001; // Aim/Look.
        ord.masks[18] = 0x0008; // Strafe/Activate.
        ord.masks[19] = 0x0004; // Duck.
        ord.labels[12] = "Fire";
        ord.labels[13] = "Jump";
        ord.labels[14] = "Aim/Look";
        const auto ord_layout = bindings::make_layout(ord);
        const auto& ord_foot = ord_layout[0];
        auto route_for = [&](bindings::Token token) {
            return std::find_if(ord_foot.begin(), ord_foot.end(), [&](const auto& row) {
                return std::find(row.targets[0].begin(), row.targets[0].end(), token) !=
                    row.targets[0].end();
            });
        };
        const auto jump = route_for(bindings::key('Z'));
        const auto fire = route_for(bindings::key('X'));
        check(jump != ord_foot.end() && jump->label.find("Jump") != std::string::npos &&
            fire != ord_foot.end() && fire->label.find("Fire") != std::string::npos,
            "Classic PC Jump and Fire rows follow Ord preset 6");
        check(bindings::translate_on_foot_buttons(ord, 0x8000) == 0x4000 &&
            bindings::translate_on_foot_buttons(ord, 0x4000) == 0x8000 &&
            bindings::translate_on_foot_buttons(ord, 0x2000) == 0x0001,
            "Classic PC actions use Ord preset 6 native buttons");
        auto same_row = [](const bindings::Layout& layout, int group,
                           bindings::Token keyboard, bindings::Token pad) {
            const auto& rows = layout[group];
            return std::any_of(rows.begin(), rows.end(), [&](const auto& row) {
                return std::find(row.targets[0].begin(), row.targets[0].end(), keyboard) !=
                    row.targets[0].end() &&
                    std::find(row.targets[1].begin(), row.targets[1].end(), pad) !=
                    row.targets[1].end();
            });
        };
        check(same_row(ord_layout, 0, bindings::key('X'), bindings::button(0)) &&
            same_row(ord_layout, 0, bindings::key('Z'), bindings::button(1)) &&
            same_row(ord_layout, 0, bindings::key('C'), bindings::button(2)) &&
            same_row(ord_layout, 0, bindings::key('A'), bindings::button(3)),
            "Classic PC joystick face buttons share their on-foot keyboard actions");
        bindings::NativeTable pc_vehicles{};
        pc_vehicles.masks[24] = 0x0030;
        pc_vehicles.masks[27] = 0x8000;
        pc_vehicles.masks[28] = 0x2000;
        pc_vehicles.masks[30] = 0x4000;
        pc_vehicles.masks[31] = 0x4000;
        pc_vehicles.masks[32] = 0x8000;
        pc_vehicles.masks[36] = 0x4000;
        pc_vehicles.masks[37] = 0x2000;
        pc_vehicles.masks[39] = 0x0010;
        pc_vehicles.masks[40] = 0x8000;
        pc_vehicles.masks[42] = 0x4000;
        pc_vehicles.masks[43] = 0x2000;
        const auto pc_layout = bindings::make_layout(pc_vehicles);
        check(same_row(pc_layout, 1, bindings::key('X'), bindings::button(0)) &&
            same_row(pc_layout, 1, bindings::key('Z'), bindings::button(1)) &&
            same_row(pc_layout, 1, bindings::key(32), bindings::button(2)) &&
            same_row(pc_layout, 1, bindings::key('A'), bindings::button(3)),
            "Classic PC snowspeeder joystick actions appear beside keyboard routes");
        check(same_row(pc_layout, 2, bindings::key('X'), bindings::button(0)) &&
            same_row(pc_layout, 2, bindings::key(32), bindings::button(1)) &&
            same_row(pc_layout, 2, bindings::key(32), bindings::button(2)),
            "Classic PC turret joystick fire and missile routes share editor rows");
        check(same_row(pc_layout, 3, bindings::key('Z'), bindings::button(0)) &&
            same_row(pc_layout, 3, bindings::key('A'), bindings::button(1)),
            "Classic PC bike joystick throttle and reverse share editor rows");
        check(same_row(pc_layout, 4, bindings::key('X'), bindings::button(0)) &&
            same_row(pc_layout, 4, bindings::key('Z'), bindings::button(1)) &&
            same_row(pc_layout, 4, bindings::key(32), bindings::button(2)) &&
            same_row(pc_layout, 4, bindings::key('A'), bindings::button(3)),
            "Classic PC Outrider joystick actions appear beside keyboard routes");
        bindings::NativeTable asteroid{};
        asteroid.masks[3] = 0x1000;
        asteroid.masks[37] = 0x2016;
        asteroid.labels[37] = "Missile";
        auto skyhook = asteroid;
        skyhook.turret_skyhook = true;
        const auto asteroid_layout = bindings::make_layout(asteroid);
        const auto skyhook_layout = bindings::make_layout(skyhook);
        auto missile_row = [](const bindings::Layout& layout, bindings::Token key) {
            const auto& turret = layout[2];
            return std::find_if(turret.begin(), turret.end(), [&](const auto& action) {
                return std::find(action.targets[0].begin(), action.targets[0].end(), key) !=
                    action.targets[0].end();
            });
        };
        const auto asteroid_missile = missile_row(asteroid_layout, bindings::key(32));
        const auto skyhook_missile = missile_row(skyhook_layout, bindings::key('Z'));
        check(asteroid_missile != asteroid_layout[2].end() &&
            skyhook_missile != skyhook_layout[2].end() &&
            asteroid_missile->id == skyhook_missile->id,
            "Turret Missile retains one binding across PC stage variants");
        if (asteroid_missile != asteroid_layout[2].end()) {
            bindings::Assignments edited;
            edited.overrides[bindings::storage_key(*asteroid_missile,
                bindings::Device::Keyboard)] = bindings::key('M');
            bindings::PhysicalInput raw{};
            raw.keys['M'] = 1;
            const auto a = edited.apply(raw, asteroid_layout[2]);
            const auto s = edited.apply(raw, skyhook_layout[2]);
            check(a.keys[32] && !a.keys['Z'] && s.keys['Z'] && !s.keys[32],
                "rebound Turret Missile key uses the active stage route");
        }
    }
    std::filesystem::create_directories(scratch / "Sdata" / "UI");
    { std::ofstream san(scratch / "Sdata/L01INTRO.SAN",std::ios::binary); san.write("ANIM\0\0\0\0",8); }
    std::filesystem::remove(scratch / "sote_options.json");
    std::filesystem::remove(scratch / "SOTE_CHEATS.INI");
    sote::graphics_menu::initialize(scratch);
    sote::controls_menu::initialize(scratch);
    sote::menu_skin::initialize(scratch);
    sote::menu_skin::set_renderer_available(true);

    Memory memory;
    bool options_focused = false;
    bool native_options = false;
    memory.profiles(options_focused);
    sote_capture_menu(memory.bytes.data());
    check(latest_screen_is(sote::menu_skin::Screen::Profiles),
        "starts on the profile picker");

    press(0x0010);
    pump_transition(memory, options_focused, native_options);
    check(native_options && latest_screen_is(sote::menu_skin::Screen::Options),
        "R shoulder opens real Options from profiles");

    check(!latest_snapshot().option_help.empty(), "native Options has contextual help");
    {
        Memory cached;
        cached.profiles(false);
        constexpr uint32_t table = 0x80230000;
        cached.word(table+53*4,cached.text("Overlay Displays"));
        cached.word(table+55*4,cached.text("Seeker Camera"));
        cached.word(table+57*4,cached.text("Sound Effects"));
        cached.word(table+59*4,cached.text("Music"));
        for(int row : {53,55,57,59}) cached.half(0x80111110+row*4,90);
        cached.half(0x80111110+76*4,0xfc18); // Hide profile-only rows.
        cached.half(0x800dd5e0,3);
        auto view=sote::menu_skin::read_guest(cached.bytes.data(),cached.bytes.size());
        check(view.screen==sote::menu_skin::Screen::Options && view.focused_setting==3,
            "cached Options carries the native focused row");
        sote::graphics_menu::decorate_native_snapshot(view);
        check(view.option_help=="Adjust sound effects volume.","cached Options describes the selected row");
    }

    press(0x0010);
    sote_capture_native_options(memory.bytes.data());
    check(latest_screen_is(sote::menu_skin::Screen::Graphics),
        "R shoulder moves Options to Graphics");
    check(
        latest_snapshot().rows[52].text.find("< Graphics >") != std::string::npos,
        "Graphics page title is selected");

    for (int index = 0; index < 6; ++index) press(0x0400);
    sote_capture_native_options(memory.bytes.data());
    check(latest_snapshot().rows[65].text == "PC",
        "Graphics starts with PC FMV cutscenes");
    press(0x0100);
    sote_capture_native_options(memory.bytes.data());
    check(latest_snapshot().rows[65].text == "N64",
        "Cutscene mode changes through the menu");
    check(sote::graphics_menu::pc_cutscenes_enabled(),
        "Cutscene mode waits for Apply");
    press(0x0400);
    press(0x0400);
    press(0x0400);
    press(0x8000);
    check(!sote::graphics_menu::pc_cutscenes_enabled(),
        "Apply activates original N64 cutscenes");
    std::ifstream saved_options{scratch / "sote_options.json"};
    const std::string saved_text{
        std::istreambuf_iterator<char>{saved_options},
        std::istreambuf_iterator<char>{}};
    check(saved_text.find("\"pcCutscenes\": false") != std::string::npos,
        "Apply persists cutscene mode");

    // Apply is row 9; compatibility is row 8. Keep the normal 4x AA preference.
    check(!sote::graphics_menu::compatibility_enabled(), "compatibility defaults off");
    press(0x0800); // Up to compatibility.
    press(0x8000);
    sote_capture_native_options(memory.bytes.data());
    check(latest_snapshot().rows[72].text == "On" && latest_snapshot().rows[60].text == "OFF",
        "compatibility preview shows On and disables AA");
    check(latest_snapshot().option_help.find("restart") != std::string::npos,
        "compatibility footer explains restart");
    check(!sote::graphics_menu::compatibility_enabled(), "compatibility waits for Apply");
    for(int i=0;i<4;++i) press(0x0800);
    press(0x0100); // Attempt to edit AA while compatibility preview is on.
    sote_capture_native_options(memory.bytes.data());
    check(latest_snapshot().rows[60].text == "OFF", "AA is locked while compatibility is on");
    for(int i=0;i<5;++i) press(0x0400);
    press(0x8000);
    sote::graphics_menu::Settings requested;
    check(sote::graphics_menu::take_renderer_request(requested) && requested.compatibility &&
        requested.antialiasing == sote::graphics_menu::AntialiasingPreset::MSAA4x,
        "Apply requests compatibility without losing normal AA");
    sote::graphics_menu::initialize(scratch);
    sote::graphics_menu::Settings normal;
    sote::graphics_menu::sync_from_renderer(normal);
    check(sote::graphics_menu::compatibility_enabled(), "compatibility survives restart and renderer sync");
    // Re-enter Graphics after initialize resets navigation state.
    memory.native_options();sote_capture_native_options(memory.bytes.data());
    press(0x0010);
    for(int i=0;i<8;++i)press(0x0400);
    press(0x8000);
    sote_capture_native_options(memory.bytes.data());
    check(latest_snapshot().rows[72].text == "Off" && latest_snapshot().rows[60].text == "4x MSAA",
        "disabling compatibility restores the normal AA preview");
    press(0x0400);press(0x8000);
    check(sote::graphics_menu::take_renderer_request(requested) && !requested.compatibility &&
        requested.antialiasing == sote::graphics_menu::AntialiasingPreset::MSAA4x,
        "Apply restores normal graphics request");

    press(0x0010);
    sote_capture_native_options(memory.bytes.data());
    check(latest_screen_is(sote::menu_skin::Screen::Schemes),
        "R shoulder moves Graphics to Controls");
    check(
        latest_snapshot().rows[52].text.find("< Controls >") != std::string::npos,
        "Controls page title is selected");

    press(0x0400);
    press(0x0100);
    sote_capture_native_options(memory.bytes.data());
    check(
        latest_snapshot().rows[54].text == "Modern",
        "Controls scheme row changes through input handler");
    const auto before_slider = latest_snapshot().rows[58].text;
    press(0x0400);
    press(0x0400);
    press(0x0100);
    sote_capture_native_options(memory.bytes.data());
    check(
        latest_snapshot().rows[58].text != before_slider,
        "Controls tuning slider changes through input handler");
    check(
        latest_snapshot().rows[58].text.find("[") != std::string::npos,
        "Controls tuning row is presented as a slider");

    press(0x0010);
    sote_capture_native_options(memory.bytes.data());
    check(latest_snapshot().rows[52].text.find("< Cheats >") != std::string::npos,
        "Controls advances to Cheats");
    for (int row=1;row<=9;++row) {
        press(0x0400);
        press(0x0100);
        sote_capture_native_options(memory.bytes.data());
        check(!latest_snapshot().rows[53+(row-1)*2].text.empty(),"cheat row has a label");
    }
    std::ifstream cheats_file(scratch / "SOTE_CHEATS.INI");
    const std::string cheats_text{std::istreambuf_iterator<char>(cheats_file),{}};
    check(cheats_text.find("infinite_health = 1")!=std::string::npos &&
        cheats_text.find("rapid_fire_rate = 5")!=std::string::npos,
        "Cheats page persists toggles and rate");
    press(0x0010);
    pump_transition(memory, options_focused, native_options);
    check(!native_options && latest_screen_is(sote::menu_skin::Screen::Profiles),
        "R shoulder wraps Cheats back to profiles");
    check(!options_focused,
        "return restores focus to the original profile item");

    press(0x0020);
    pump_transition(memory, options_focused, native_options);
    check(native_options && latest_screen_is(sote::menu_skin::Screen::Schemes),
        "L shoulder wraps profiles back to Cheats");
    check(latest_snapshot().rows[52].text.find("< Cheats >")!=std::string::npos,"reverse entry selects Cheats");
    press(0x0020);
    sote_capture_native_options(memory.bytes.data());
    check(latest_snapshot().rows[52].text.find("< Controls >")!=std::string::npos,"Cheats returns to Controls");
    press(0x0020);
    sote_capture_native_options(memory.bytes.data());
    check(latest_screen_is(sote::menu_skin::Screen::Graphics),
        "L shoulder moves Controls to Graphics");
    press(0x0020);
    sote_capture_native_options(memory.bytes.data());
    check(latest_screen_is(sote::menu_skin::Screen::Options),
        "L shoulder moves Graphics to Options");
    press(0x0020);
    pump_transition(memory, options_focused, native_options);
    check(!native_options && latest_screen_is(sote::menu_skin::Screen::Profiles),
        "L shoulder returns Options to profiles");

    Memory pause_memory;
    pause_memory.word(0x800d0948, 1);
    pause_memory.half(0x800d0944, 1);
    auto paused = sote::menu_skin::read_native_pause(
        pause_memory.bytes.data(), pause_memory.bytes.size());
    check(paused.screen == sote::menu_skin::Screen::Pause &&
        paused.focused_setting == 1 && paused.rows[71].text == "Options",
        "Pause reads native focus and presents Options");
    const auto pause_image = sote::menu_skin::render(paused, 960, 540);
    check(pause_image.valid() && pause_image.rgba[3] == 0 &&
        pause_image.rgba[(size_t(280) * 960 + 480) * 4 + 3] == 255,
        "Pause overlay keeps gameplay visible outside its action panel");

    {
        std::ofstream options{scratch / "sote_options.json", std::ios::trunc};
        options << "{\"pcCutscenes\":false,\"borderless\":true}";
    }
    sote::graphics_menu::initialize(scratch);
    check(!sote::graphics_menu::pc_cutscenes_enabled(),
        "compact persisted false disables PC movies on restart");
    {
        std::ofstream options{scratch / "sote_options.json", std::ios::trunc};
        options << "{\"pcCutscenes\":true,\"borderless\":false}";
    }
    sote::graphics_menu::initialize(scratch);
    check(sote::graphics_menu::pc_cutscenes_enabled(),
        "compact persisted true re-enables PC movies on restart");

    {
        sote::control_bindings::PhysicalInput raw;
        raw.keys['W'] = raw.keys['A'] = 1;
        raw.buttons[2] = 1;
        const auto mapped = sote::control_bindings::menu_navigation(raw);
        check(mapped.keys[38] && mapped.keys[37] && !mapped.keys['W'] && !mapped.keys['A'],
            "WASD menus use the same digital path as arrow keys without analog duplication");
        check(!mapped.buttons[2] && raw.buttons[2],
            "X is excluded from native menu actions without altering binding-capture input");
    }

    check(sote::control_bindings::should_capture_mouse(true, true, false, false, false),
        "foreground gameplay requests mouse capture");
    for (int gate = 0; gate < 5; ++gate)
        check(!sote::control_bindings::should_capture_mouse(gate != 0, gate != 1,
            gate == 2, gate == 3, gate == 4),
            "capture releases outside gameplay, on focus loss, menus, movies and diagnostics");

    {
        namespace bindings = sote::control_bindings;
        bindings::NativeTable table;
        table.modern_foot = true;
        const auto modern_layout = bindings::make_layout(table);
        for (int group : {1, 2, 4}) {
            check(modern_layout[group][0].targets[0].front() == bindings::key('W') &&
                modern_layout[group][2].targets[0].front() == bindings::key('A'),
                "Modern flying sections expose WASD movement bindings");
            bindings::Assignments edited;
            edited.overrides[bindings::storage_key(modern_layout[group][0], bindings::Device::Keyboard)] = bindings::key('T');
            bindings::PhysicalInput raw;
            raw.keys['T'] = 1;
            check(edited.apply(raw, modern_layout[group]).keys['W'],
                "flying movement overrides reach the canonical WASD input path");
        }
        table.modern_foot = false;
        const auto classic_layout = bindings::make_layout(table);
        check(classic_layout[1][0].targets[0].front() == bindings::key(38),
            "Classic flying controls preserve the PC arrow-key defaults");
        for (int group : {1, 2, 4})
            check(bindings::storage_key(classic_layout[group][0], bindings::Device::Keyboard) !=
                bindings::storage_key(modern_layout[group][0], bindings::Device::Keyboard),
                "Classic and Modern flying bindings persist independently");
        for (int group : {1, 2, 4}) {
            bindings::Assignments previous_version;
            previous_version.overrides[bindings::storage_key(
                classic_layout[group][0], bindings::Device::Controller)] = bindings::axis(2,1);
            bindings::PhysicalInput raw{};
            raw.connected = true;
            raw.axes[2] = 24000;
            check(previous_version.apply(raw, modern_layout[group]).axes[1] < 0,
                "existing vehicle controller bindings survive Modern keyboard layout changes");
        }
    }

    {
        namespace bindings = sote::control_bindings;
        Memory binding_memory;
        binding_memory.native_options();
        binding_memory.half(0x800e6808 + 6, 0x1000);
        std::filesystem::remove(scratch / "Sdata" / "controls_bindings.ini");
        bindings::initialize(scratch);
        bindings::observe_menu(binding_memory.bytes.data(), binding_memory.bytes.size());
        uint64_t time = 10000;
        auto input = [&](int vk) {
            bindings::PhysicalInput raw{};
            if (vk) raw.keys[vk] = 1;
            return bindings::handle_menu(raw, true, time += 100, true);
        };
        input(0);
        check(!bindings::editor_view().editing, "Controls reference does not open editor on focus");
        input(13);
        input(0);
        check(bindings::editor_view().editing, "Enter opens keyboard binding editor");
        input(13);
        input(0);
        check(bindings::editor_view().phase == bindings::Phase::Capture,
            "Enter and release arm keyboard capture");
        input('T');
        input(0);
        const auto saved = bindings::saved_assignments();
        check(saved.size() == 1 && saved.begin()->second == bindings::key('T'),
            "keyboard editor saves reassigned movement key");
        bindings::initialize(scratch);
        check(bindings::saved_assignments() == saved,
            "keyboard assignment survives editor restart");
        bindings::observe_menu(binding_memory.bytes.data(), binding_memory.bytes.size());
        input(0);
        input(13);
        input(0);
        input(13);
        input(0);
        input(6); // VK_XBUTTON2, through the same keyboard/mouse capture column.
        input(0);
        const auto mouse_saved = bindings::saved_assignments();
        check(mouse_saved.size() == 1 && mouse_saved.begin()->second == bindings::key(6),
            "mouse button can replace a keyboard-column binding");
        bindings::initialize(scratch);
        check(bindings::saved_assignments() == mouse_saved,
            "mouse button assignment survives editor restart");
    }

    {
        namespace b=sote::control_bindings;
        b::NativeTable table; table.modern_foot=true;
        const auto layout=b::make_layout(table);
        for (const auto& row:layout[0]) if(row.id.ends_with("center_camera") ||
            row.id.ends_with("quick_turn") || row.id.ends_with("quick_turn_single")) {
            b::Assignments bindings;
            bindings.overrides[b::storage_key(row,b::Device::Keyboard)]=b::key(6);
            b::PhysicalInput raw;raw.keys[6]=1;
            check(b::value(bindings.apply(raw,layout[0]),row.targets[0].front())>0,
                "each extra action supports a mouse binding without a controller");
            raw={}; raw.connected=true; raw.buttons[7]=1;
            if(row.id.ends_with("center_camera")) check(
                b::value(b::Assignments{}.apply(raw,layout[0]),row.targets[1].front())>0,
                "Center Camera defaults to LS click");
        }
        std::filesystem::remove(scratch / "Sdata/controls_bindings.ini");
        b::initialize(scratch);
        Memory combo_memory;
        combo_memory.native_options();
        combo_memory.half(0x800e6808+6,0x1000);
        combo_memory.half(0x800e6808+18*2,0x10);
        if(sote::controls_menu::current_scheme(sote::controls_menu::SchemeSlot::OnFoot)!=
            sote::controls_menu::ControlScheme::Modern)
            sote::controls_menu::cycle_scheme(sote::controls_menu::SchemeSlot::OnFoot,1);
        b::observe_context(combo_memory.bytes.data(),combo_memory.bytes.size(),b::Context::OnFoot);
        b::PhysicalInput combo;combo.connected=true;
        b::remap(combo,false);
        combo.buttons[2]=1;
        auto out=b::remap(combo,false);
        check(out.buttons[2] && !out.buttons[b::virtual_quick_turn],"X alone keeps its normal action");
        combo.axes[1]=25000;
        out=b::remap(combo,false);
        check(!out.buttons[b::virtual_quick_turn],"adding Back after holding X does not start a turn");
        combo.buttons[2]=0;b::remap(combo,false);
        combo.buttons[2]=1;out=b::remap(combo,false);
        check(!out.buttons[2] && out.buttons[b::virtual_quick_turn],"Back then X consumes normal action for turn");
        combo.axes[1]=0;out=b::remap(combo,false);
        check(!out.buttons[2],"turn consumes X until release even when Back is released");
        combo.buttons[2]=0;b::remap(combo,false);
        combo.buttons[2]=1;out=b::remap(combo,false);
        check(out.buttons[2],"X normal action returns after release");
        const auto fonts=scratch / "Sdata/UI/fonts.ini";
        { std::ofstream out(fonts);out << "# custom\ndefault = original\ncustom.widget = myfont.ttf\noptions = original\n"; }
        sote::menu_skin::initialize(scratch);
        sote::menu_skin::set_menu_font_original(false);
        check(!sote::menu_skin::menu_font_original(),"font switches to modern");
        Memory font_memory;
        sote::menu_skin::set_renderer_available(true);
        font_memory.word(0x800db290,0x800da818);
        font_memory.word(0x80200000,0x80200010);
        font_memory.word(0x80200008,0xfa000000);
        font_memory.word(0x8020000c,0x40c08080);
        font_memory.word(0x800db288,0x40c08080);
        const auto native_string=font_memory.text("~oGreen");
        const auto original_bytes=font_memory.bytes;
        sote_native_text_begin(font_memory.bytes.data(),native_string);
        check(sote_native_text_glyph(font_memory.bytes.data(),0x80200000,'G',20,30)==1,"Modern routes native letters through UI font");
        sote_publish_native_text();
        const auto captured=sote::menu_skin::latest_native_text();
        check(captured && captured->glyphs.size()==1 && captured->glyphs[0].color==0x40c08080,"native draw preserves exact RGBA including opacity");
        sote::menu_skin::Image image{1280,720,std::vector<uint8_t>(1280*720*4)};
        sote::menu_skin::composite_native_text(image,*captured);
        unsigned pixels=0;bool colors=true;
        for(size_t i=0;i<image.rgba.size();i+=4) if(image.rgba[i+3]) {
            ++pixels;colors &= image.rgba[i]==0x40 && image.rgba[i+1]==0xc0 && image.rgba[i+2]==0x80 && image.rgba[i+3]<=0x80;
        }
        check(pixels>50 && colors,"UI glyph pixels preserve requested color and antialiased alpha");
        check(font_memory.bytes==original_bytes,"Modern does not rewrite retail font pixels or guest data");
        font_memory.half(0x800da818+12,0xffff);
        check(sote_native_text_glyph(font_memory.bytes.data(),0x80200000,' ',27,30)==1,
            "space without an atlas rectangle retains UI advance");
        sote_publish_native_text();
        check(sote::menu_skin::latest_native_text()->glyphs[0].character==' ',
            "space is included in the resolved text run");
        const auto punctuation=font_memory.text("...");
        sote_native_text_begin(font_memory.bytes.data(),punctuation);
        check(sote_native_text_glyph(font_memory.bytes.data(),0x80200000,'.',0,0)==1,
            "punctuation-only text uses UI glyphs");
        const auto border=font_memory.text("~r######");
        sote_native_text_begin(font_memory.bytes.data(),border);
        check(!sote_native_text_glyph(font_memory.bytes.data(),0x80200000,'#',0,0),"decorative border glyphs retain native rendering");
        sote::menu_skin::set_menu_font_original(true);
        sote_native_text_begin(font_memory.bytes.data(),native_string);
        check(!sote_native_text_glyph(font_memory.bytes.data(),0x80200000,'G',20,30),"Original retains native glyph rendering");
        sote::menu_skin::set_menu_font_original(true);
        check(sote::menu_skin::menu_font_original(),"font switches to original");
        std::ifstream in(fonts);const std::string text{std::istreambuf_iterator<char>(in),{}};
        check(text.find("custom.widget = myfont.ttf")!=std::string::npos,
            "font toggle preserves custom widget configuration");
    }
    {
        std::filesystem::remove(scratch / "Sdata/L01INTRO.SAN");
        { std::ofstream options(scratch / "sote_options.json"); options << "{\"pcCutscenes\":true}"; }
        sote::graphics_menu::initialize(scratch);
        check(!sote::graphics_menu::pc_cutscenes_enabled(),"missing SAN files override saved PC preference");
        Memory absent;bool focus=false,native=false;absent.profiles(focus);
        sote_capture_menu(absent.bytes.data());
        press(0x0010);pump_transition(absent,focus,native);
        press(0x0010);sote_capture_native_options(absent.bytes.data());
        for(int i=0;i<6;++i)press(0x0400);
        press(0x0100);press(0x8000);
        sote_capture_native_options(absent.bytes.data());
        check(latest_snapshot().rows[65].text=="N64" && !latest_snapshot().pc_cutscenes_available,
            "missing files lock N64 against directional and confirm input");
        press(0x0400);press(0x0400);press(0x0400);press(0x8000);
        check(!sote::graphics_menu::pc_cutscenes_enabled(),"Apply cannot bypass missing-files gate");
        press(0x0400);press(0x8000);sote_capture_native_options(absent.bytes.data());
        check(latest_snapshot().rows[65].text=="N64", "Reset cannot display unavailable PC mode");
        { std::ofstream san(scratch / "L01INTRO.SAN",std::ios::binary);san.write("ANIM\0\0\0\0",8); }
        sote::graphics_menu::initialize(scratch);
        check(!sote::graphics_menu::pc_cutscenes_enabled(),"SAN outside Sdata does not unlock PC");
        std::filesystem::remove(scratch / "L01INTRO.SAN");
        { std::ofstream san(scratch / "Sdata/L01INTRO.SAN"); san << "not a SAN file"; }
        sote::graphics_menu::initialize(scratch);
        check(!sote::graphics_menu::pc_cutscenes_enabled(),"invalid SAN signature does not unlock PC");
        std::filesystem::remove(scratch / "Sdata/L01INTRO.SAN");
    }
    {
        sote::graphics_menu::initialize(scratch);
        Memory options;
        options.native_options();
        options.half(0x800dd5e0,3);
        sote_capture_native_options(options.bytes.data());
        press(0x0020);
        uint16_t buttons=0;float x=0,y=0;
        sote::graphics_menu::filter_input(&buttons,&x,&y);
        check((buttons&0x0800) && !(buttons&0xc000),
            "return navigates toward Return without adjusting an option");
        sote::graphics_menu::observe_native_options(0);
        buttons=0;sote::graphics_menu::filter_input(&buttons,&x,&y);
        buttons=0;sote::graphics_menu::filter_input(&buttons,&x,&y);
        check(buttons==0x8000,"return activates native Return row with A");
        sote::graphics_menu::observe_other_screen();
        buttons=0x1000;sote::graphics_menu::filter_input(&buttons,&x,&y);
        check(buttons==0x1000,"arrival at Pause releases transition input immediately");
    }
    std::cout << "Menu revamp checks: " << checks
              << "; failures: " << failures << "\n";
    return failures == 0 ? 0 : 1;
}
