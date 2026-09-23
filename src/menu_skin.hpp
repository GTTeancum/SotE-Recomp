#pragma once

// Host-side menu presentation only. Native save state, input and menu logic
// remain authoritative. No renderer thread reads live guest memory.
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace sote::menu_skin {
enum class Screen { Native, Profiles, Summary, Options, Controls, Graphics, Schemes };
struct Row {
    int x = -1000, y = 0;
    uint32_t color = 0;
    std::string text;
    bool visible() const { return x != -1000 && !text.empty(); }
};
struct Glyph {
    int advance = 0, bearing_y = 0, bearing_x = 0;
    int x = -1, y = -1, width = 0, height = 0;
};
struct OriginalFont {
    std::array<Glyph, 95> glyphs{};
    std::array<uint8_t, 64 * 128> alpha{};
    int line_height = 13;
    bool valid = false;
};
struct Image {
    unsigned width = 0, height = 0;
    std::vector<uint8_t> rgba;
    bool valid() const { return width && height && rgba.size() == size_t(width) * height * 4; }
};
struct Binding { std::string action, pad, keyboard; };
struct Snapshot {
    Screen screen = Screen::Native;
    std::array<Row, 80> rows{};
    std::shared_ptr<const OriginalFont> original;
    Image thumbnail;
    std::vector<Binding> on_foot, bike;
    std::array<std::vector<Binding>, 5> native_controls{};
    std::string preset;
    bool rebinding = false, binding_editing = false;
    int binding_group = 0, binding_row = 0, binding_column = 0, binding_phase = 0;
    std::string binding_title, binding_detail, binding_hint, binding_status;
    int focused_setting = -1;
    int controls_scroll = 0;
    uint64_t serial = 0;
    std::chrono::steady_clock::time_point captured{};
};
// Pure, bounded decoder used by both production capture and regression tests.
std::string plain_text(const std::string& text);
Screen classify(const std::array<Row, 80>& rows);
std::shared_ptr<OriginalFont> read_original_font(const uint8_t* rdram, size_t size);
Snapshot read_guest(const uint8_t* rdram, size_t size);
Snapshot read_native_options(const uint8_t* rdram, size_t size);
bool native_options_active(const uint8_t* rdram, size_t size);
const char* screen_name(Screen screen);

void initialize(const std::filesystem::path& runtime_directory);
void set_renderer_available(bool available);
void capture(const uint8_t* rdram);
std::shared_ptr<const Snapshot> latest();
// CPU surface is 16:9 and opaque; unsupported screens return an empty image.
// All fonts default to original. Optional per-widget/role TTFs are user supplied.
Image render(const Snapshot& snapshot, unsigned width = 1280, unsigned height = 720);
void scroll_controls(int lines);
bool controls_visible();
} // namespace sote::menu_skin

extern "C" void sote_capture_menu(uint8_t* rdram);

extern "C" void sote_capture_native_options(uint8_t* rdram);
