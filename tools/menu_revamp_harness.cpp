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
    std::filesystem::remove(scratch / "sote_options.json");
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

    press(0x0010);
    sote_capture_native_options(memory.bytes.data());
    check(latest_screen_is(sote::menu_skin::Screen::Graphics),
        "R shoulder moves Options to Graphics");
    check(
        latest_snapshot().rows[52].text.find("< Graphics >") != std::string::npos,
        "Graphics page title is selected");

    for (int index = 0; index < 6; ++index) press(0x0400);
    sote_capture_native_options(memory.bytes.data());
    check(latest_snapshot().rows[65].text == "PC FMV",
        "Graphics starts with PC FMV cutscenes");
    press(0x0100);
    sote_capture_native_options(memory.bytes.data());
    check(latest_snapshot().rows[65].text == "Original N64",
        "Cutscene mode changes through the menu");
    check(sote::graphics_menu::pc_cutscenes_enabled(),
        "Cutscene mode waits for Apply");
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
    pump_transition(memory, options_focused, native_options);
    check(!native_options && latest_screen_is(sote::menu_skin::Screen::Profiles),
        "R shoulder wraps Controls back to profiles");
    check(!options_focused,
        "return restores focus to the original profile item");

    press(0x0020);
    pump_transition(memory, options_focused, native_options);
    check(native_options && latest_screen_is(sote::menu_skin::Screen::Schemes),
        "L shoulder wraps profiles back to Controls");
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

    std::cout << "Menu revamp checks: " << checks
              << "; failures: " << failures << "\n";
    return failures == 0 ? 0 : 1;
}
