#include "menu_skin.hpp"
#include "controls_menu.hpp"
#include "graphics_menu.hpp"
#include "control_bindings.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace sote::menu_skin {
void configure_menu_fonts(const std::filesystem::path& root);
namespace {
std::mutex snapshot_mutex;
std::shared_ptr<const Snapshot> published;
std::atomic<bool> enabled{true};
std::atomic<bool> renderer_available{false};
std::atomic<int> scroll_offset{0};
uint64_t sequence = 0, previous_hash = 0;
Screen previous_screen = Screen::Native;
uint64_t fingerprint(const Snapshot& s) {
    uint64_t h = 14695981039346656037ULL;
    auto byte = [&](unsigned v) { h = (h ^ (v & 255)) * 1099511628211ULL; };
    auto string = [&](const std::string& v) { for (unsigned char ch : v) byte(ch); byte(0); };
    byte(unsigned(s.screen));
    for (const auto& row : s.rows) {
        string(row.text); byte(row.x); byte(unsigned(row.x) >> 8); byte(row.y);
        // Native selected colors pulse continuously. Retain focus, not the
        // random color/alpha modulation, so static menus are uploaded once.
        byte(((row.color >> 16) & 255) >= 160);
        byte(((row.color >> 24) & 255) >= 160 && ((row.color >> 16) & 255) >= 208 && ((row.color >> 8) & 255) >= 160);
    }
    for (const auto& b : s.on_foot) { string(b.action); string(b.pad); string(b.keyboard); }
    for (const auto& b : s.bike) { string(b.action); string(b.pad); string(b.keyboard); }
    for (uint8_t v : s.thumbnail.rgba) byte(v);
    string(s.preset);
    byte(s.rebinding); byte(s.binding_editing); byte(unsigned(s.binding_group)); byte(unsigned(s.binding_row));
    byte(unsigned(s.binding_column)); byte(unsigned(s.binding_phase));
    string(s.binding_title); string(s.binding_detail); string(s.binding_hint); string(s.binding_status);
    for (const auto& group : s.native_controls) for (const auto& b : group) { string(b.action); string(b.pad); string(b.keyboard); }
    byte(unsigned(s.controls_scroll)); byte(unsigned(s.controls_scroll) >> 8);
    return h;
}
void append_legend(std::vector<Binding>& to, controls_menu::SchemeSlot slot) {
    const controls_menu::LegendEntry* entries[16]{};
    const int n = controls_menu::scheme_legend(slot, entries, 16);
    for (int i = 0; i < n && i < 16; ++i) if (entries[i])
        to.push_back({plain_text(entries[i]->action), plain_text(entries[i]->pad), plain_text(entries[i]->key)});
}
}
void initialize(const std::filesystem::path& runtime_directory) {
    const char* disabled = std::getenv("SOTE_DISABLE_MENU_REVAMP");
    enabled.store(!(disabled && disabled[0] && disabled[0] != '0'));
    configure_menu_fonts(runtime_directory / "Sdata" / "UI");
    std::lock_guard lock(snapshot_mutex);
    published.reset(); sequence = 0; previous_hash = 0; previous_screen = Screen::Native;
    std::printf("[sote][menu] Live menu revamp %s; default font: original.\n", enabled.load() ? "enabled" : "disabled");
}

void set_renderer_available(bool available) { renderer_available.store(available); }
namespace {
void publish_snapshot(Snapshot snapshot) {
    auto next = std::make_shared<Snapshot>(std::move(snapshot));
    if (next->screen == Screen::Schemes || next->screen == Screen::Controls) {
        append_legend(next->on_foot, controls_menu::SchemeSlot::OnFoot);
        append_legend(next->bike, controls_menu::SchemeSlot::Bike);
        if (next->screen == Screen::Controls) {
            if (controls_menu::current_scheme(controls_menu::SchemeSlot::OnFoot) == controls_menu::ControlScheme::Modern) {
                for (auto& b : next->native_controls[0]) {
                    if (b.action == "Fire") b.pad = "RT";
                    else if (b.action == "Jump" || b.action == "Thrust") b.pad = "A";
                    else if (b.action == "Aim/Look") b.pad = "RS";
                    else if (b.action == "Strafe") b.pad = "LS";
                    else if (b.action == "Activate") b.pad = "X";
                    else if (b.action == "Duck") b.pad = "B";
                    else if (b.action == "Jetpack") b.pad = "Y";
                    else if (b.action == "Weapon") b.pad = "LB / RB";
                    else if (b.action == "Camera") b.pad = "D-Up";
                }
            }
            if (controls_menu::current_scheme(controls_menu::SchemeSlot::Bike) == controls_menu::ControlScheme::Modern)
                for (auto& b : next->native_controls[3]) {
                    if (b.action == "Accelerate") b.pad = "RT";
                    else if (b.action == "Brakes") b.pad = "LT";
                }
        }
    }
    next->captured = std::chrono::steady_clock::now();
    std::lock_guard lock(snapshot_mutex);
    if (next->screen != previous_screen) {
        scroll_offset.store(0);
        if (std::getenv("SOTE_TRACE_MENU_REVAMP")) std::printf("[sote][menu] screen=%s\n", screen_name(next->screen));
        previous_screen = next->screen;
    }
    next->controls_scroll = scroll_offset.load();
    if (renderer_available.load()) control_bindings::decorate(*next);
    const uint64_t hash = fingerprint(*next);
    if (hash != previous_hash) { ++sequence; previous_hash = hash; }
    next->serial = sequence;
    published = std::move(next);
}
void failed_capture() {
    std::lock_guard lock(snapshot_mutex); published.reset();
    static bool reported = false;
    if (!reported) std::fprintf(stderr, "[sote][menu] Capture failed; retaining native menus.\n");
    reported = true;
}
}
void capture(const uint8_t* rdram) {
    if (!enabled.load(std::memory_order_relaxed)) return;
    try {
        // Options/control diagrams are direct-drawn later by func_8001FC90;
        // unrelated cached HUD rows must not clear that frame's host snapshot.
        if (native_options_active(rdram, 0x800000)) return;
        publish_snapshot(read_guest(rdram, 0x800000));
    } catch (...) { failed_capture(); }
}
void capture_native_options(const uint8_t* rdram) {
    if (!enabled.load(std::memory_order_relaxed) || !native_options_active(rdram, 0x800000)) return;
    try {
        auto snapshot = read_native_options(rdram, 0x800000);
        if (snapshot.screen == Screen::Controls)
            control_bindings::observe_menu(rdram, 0x800000);
        if (snapshot.screen != Screen::Native && renderer_available.load()) {
            graphics_menu::observe_native_options(snapshot.focused_setting);
            graphics_menu::decorate_native_snapshot(snapshot);
        }
        publish_snapshot(std::move(snapshot));
    } catch (...) { failed_capture(); }
}
std::shared_ptr<const Snapshot> latest() {
    std::lock_guard lock(snapshot_mutex);
    if (!enabled.load() || !published || published->screen == Screen::Native ||
        std::chrono::steady_clock::now() - published->captured > std::chrono::milliseconds(150)) return {};
    return published;
}
void scroll_controls(int lines) {
    const auto s = latest();
    if (s && s->screen == Screen::Controls) {
        if (s->rebinding) { control_bindings::scroll(lines); return; }
        int count = 0;
        for (const auto& group : s->native_controls) count += int(group.size()) + 2;
        scroll_offset.store(std::clamp(scroll_offset.load() + lines, 0, std::max(0, count - 10)));
    }
}
bool controls_visible() {
    const auto s = latest();
    return s && s->screen == Screen::Controls;
}
} // namespace sote::menu_skin
extern "C" void sote_capture_menu(uint8_t* rdram) { sote::menu_skin::capture(rdram); }

extern "C" void sote_capture_native_options(uint8_t* rdram) { sote::menu_skin::capture_native_options(rdram); }
