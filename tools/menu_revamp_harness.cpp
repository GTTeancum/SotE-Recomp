#include "controls_menu.hpp"
#include "graphics_menu.hpp"
#include "menu_skin.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
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
};

void press(uint16_t buttons) {
    float x = 0.0f;
    float y = 0.0f;
    uint16_t neutral = 0;
    sote::graphics_menu::filter_input(&neutral, &x, &y);
    sote::graphics_menu::filter_input(&buttons, &x, &y);
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
    std::filesystem::create_directories(scratch / "Sdata" / "UI");
    sote::graphics_menu::initialize(scratch);
    sote::controls_menu::initialize(scratch);
    sote::menu_skin::initialize(scratch);
    sote::menu_skin::set_renderer_available(true);

    Memory memory;
    memory.native_options();
    sote_capture_native_options(memory.bytes.data());
    check(latest_screen_is(sote::menu_skin::Screen::Options),
        "starts on native Options page");

    press(0x0100);
    sote_capture_native_options(memory.bytes.data());
    check(latest_screen_is(sote::menu_skin::Screen::Graphics),
        "D-right moves Options to Graphics");
    check(
        latest_snapshot().rows[52].text.find("< Graphics >") != std::string::npos,
        "Graphics page title is selected");

    press(0x0010);
    sote_capture_native_options(memory.bytes.data());
    check(latest_screen_is(sote::menu_skin::Screen::Schemes),
        "R shoulder moves Graphics to Controls");
    check(
        latest_snapshot().rows[52].text.find("< Controls >") != std::string::npos,
        "Controls page title is selected");

    press(0x0010);
    sote_capture_native_options(memory.bytes.data());
    check(latest_screen_is(sote::menu_skin::Screen::Options),
        "R shoulder wraps Controls to Options");

    press(0x0020);
    sote_capture_native_options(memory.bytes.data());
    check(latest_screen_is(sote::menu_skin::Screen::Schemes),
        "L shoulder wraps Options to Controls");

    press(0x0020);
    sote_capture_native_options(memory.bytes.data());
    check(latest_screen_is(sote::menu_skin::Screen::Graphics),
        "L shoulder moves Controls to Graphics");

    press(0x0020);
    sote_capture_native_options(memory.bytes.data());
    check(latest_screen_is(sote::menu_skin::Screen::Options),
        "L shoulder moves Graphics back to Options");

    press(0x0100);
    press(0x0010);
    sote_capture_native_options(memory.bytes.data());
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

    std::cout << "Menu revamp checks: " << checks
              << "; failures: " << failures << "\n";
    return failures == 0 ? 0 : 1;
}
