#include "menu_skin.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <string_view>

namespace sote::menu_skin {
namespace {
struct Guest {
    const uint8_t* data;
    size_t size;
    bool valid(uint32_t a, size_t n = 1) const {
        return data && a >= 0x80000000U && a < 0x80800000U &&
            n <= size && size_t(a & 0x7FFFFFU) <= size - n;
    }
    uint8_t byte(uint32_t a) const {
        const size_t i = (a & 0x7FFFFFU) ^ 3U;
        return valid(a) && i < size ? data[i] : 0;
    }
    uint16_t half(uint32_t a) const {
        // Reconstruct guest endian order, including intentionally unaligned data.
        return uint16_t((uint16_t(byte(a)) << 8) | byte(a + 1));
    }
    uint32_t word(uint32_t a) const {
        return uint32_t(half(a)) << 16 | half(a + 2);
    }
    int signed_half(uint32_t a) const { return int(int16_t(half(a))); }
    std::string string(uint32_t a) const {
        if (!valid(a)) return {};
        std::string s;
        for (size_t i = 0; i < 255 && valid(a + uint32_t(i)); ++i) {
            const char c = char(byte(a + uint32_t(i)));
            if (!c) return s;
            s.push_back(c);
        }
        return {}; // A truncated or unterminated buffer is not a menu label.
    }
};
bool is(const Row& row, std::string_view s) { return row.visible() && row.text == s; }
bool has(const Row& row, std::string_view s) {
    return row.visible() && row.text.find(s) != std::string::npos;
}
Image thumbnail(const Guest& g) {
    const uint32_t table = g.word(0x8013CE2CU);
    if (!g.valid(table, 320)) return {};
    // Rows 40..49 are the ten level thumbnails. The carousel moves adjacent
    // cards through y=-60/70/200 and changes their alpha. Select the visible
    // card nearest the native center, not the first loaded sprite (Hoth).
    int card = -1, distance = 10000;
    for (unsigned i = 40; i < 50; ++i) {
        const int x = g.signed_half(0x80111110U + i * 4);
        const int y = g.signed_half(0x80111112U + i * 4);
        if (x == -1000 || !(g.word(0x80111250U + i * 4) & 255)) continue;
        const int d = std::abs(y - 70);
        if (d < distance) { distance = d; card = int(i); }
    }
    if (card < 0) return {};
    const uint32_t s = g.word(table + unsigned(card) * 4);
    if (!g.valid(s, 40)) return {};
    const unsigned w = g.half(s + 4), h = g.half(s + 6);
    const unsigned count = g.half(s + 28);
    const uint32_t palette = g.word(s + 24), tiles = g.word(s + 36);
    if (w != 160 || h != 120 || count != 12 || g.byte(s + 32) != 2 ||
        g.byte(s + 33) != 1 || !g.valid(palette, 512) || !g.valid(tiles, count * 8)) return {};
    Image out{w, h, std::vector<uint8_t>(size_t(w) * h * 4)};
    unsigned ox = 0, oy = 0, row_height = 0;
    for (unsigned i = 0; i < count; ++i) {
        const uint32_t t = tiles + i * 8;
        const unsigned tw = g.half(t), th = g.half(t + 2);
        const uint32_t p = g.word(t + 4);
        if (!tw || tw % 8 || !th || tw > w || th > h || ox + tw > w ||
            oy + th > h || !g.valid(p, size_t(tw) * th)) return {};
        if (ox && row_height != th) return {};
        row_height = th;
        for (unsigned y = 0; y < th; ++y) for (unsigned x = 0; x < tw; ++x) {
            // These CI8 sprite tiles are pre-swapped for RDP odd scanlines.
            const unsigned index = (y * tw + x) ^ ((y & 1) ? 4U : 0U);
            const uint16_t c = g.half(palette + g.byte(p + index) * 2U);
            const size_t q = (size_t(oy + y) * w + ox + x) * 4;
            out.rgba[q] = uint8_t(((c >> 11) & 31) * 255 / 31);
            out.rgba[q + 1] = uint8_t(((c >> 6) & 31) * 255 / 31);
            out.rgba[q + 2] = uint8_t(((c >> 1) & 31) * 255 / 31);
            out.rgba[q + 3] = (c & 1) ? 255 : 0;
        }
        ox += tw;
        if (ox == w) { ox = 0; oy += row_height; }
    }
    return oy == h && ox == 0 ? out : Image{};
}
} // namespace

std::string plain_text(const std::string& raw) {
    std::string out;
    for (size_t i = 0; i < raw.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(raw[i]);
        if (c == '~' && i + 1 < raw.size()) {
            const char op = raw[++i];
            if (op == 'n') out += '\n';
            else if (op == 'f' && i + 1 < raw.size()) ++i;
            // ~s/~o font, ~c alignment, ~~ unformatted prefix and numeric color
            // commands are native formatting, not visible letters.
            continue;
        }
        if ((c >= 32 && c <= 126) || c == '\n') out += char(c);
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '\r')) out.pop_back();
    return out;
}

Screen classify(const std::array<Row, 80>& r) {
    if (r[52].text.find("Controls") != std::string::npos &&
        is(r[53], "On Foot") && is(r[55], "Speeder Bike"))
        return Screen::Schemes;
    if (has(r[52], "Graphics") && is(r[53], "Resolution")) return Screen::Graphics;
    if (has(r[52], "Player:") && has(r[55], "Difficulty Setting:") &&
        has(r[56], "Lives") && has(r[60], "Challenge Pts")) return Screen::Summary;
    if (is(r[76], "Options") && is(r[77], "Rename") && is(r[78], "Clear") &&
        r[51].visible() && r[52].visible() && r[53].visible() && r[54].visible()) {
        // Only the ordinary picker. Rename, clear-confirmation, difficulty and
        // new-player entry screens must remain native until separately styled.
        if (!r[50].text.empty()) return Screen::Native;
        if (r[67].visible() && r[67].text != "Press Start to select") return Screen::Native;
        for (int i : {55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 69, 74, 75, 79})
            if (r[i].visible()) return Screen::Native;
        return Screen::Profiles;
    }
    // Detect from labels, not level/event numbers: the same menu also appears
    // over paused gameplay. Its values and focus remain native-authoritative.
    if (is(r[53], "Overlay Displays") && is(r[55], "Seeker Camera") &&
        is(r[57], "Sound Effects") && is(r[59], "Music")) return Screen::Options;
    return Screen::Native;
}

std::shared_ptr<OriginalFont> read_original_font(const uint8_t* rdram, size_t size) {
    const Guest g{rdram, size};
    constexpr uint32_t base = 0x800DA818U;
    if (!g.valid(base, 1340)) return {};
    const uint32_t pixels = g.word(base);
    const int line = g.signed_half(base + 4);
    if (line != 13 || !g.valid(pixels, 4096)) return {};
    auto font = std::make_shared<OriginalFont>();
    font->line_height = line;
    for (unsigned c = 0; c < 95; ++c) {
        const uint32_t a = base + c * 14;
        auto& t = font->glyphs[c];
        t = {g.signed_half(a + 6), g.signed_half(a + 8), g.signed_half(a + 10),
             g.signed_half(a + 12), g.signed_half(a + 14), g.signed_half(a + 16), g.signed_half(a + 18)};
        if (t.advance < 0 || t.advance > 32 || std::abs(t.bearing_x) > 32 ||
            std::abs(t.bearing_y) > 32) return {};
        if (t.x >= 0 && (t.y < 0 || t.width < 0 || t.height < 0 ||
            t.x + t.width > 64 || t.y + t.height > 128)) return {};
    }
    if (font->glyphs['A' - 32].advance != 9 || font->glyphs['0' - 32].advance != 7) return {};
    for (unsigned i = 0; i < 64 * 128; ++i) {
        const uint8_t b = g.byte(pixels + i / 2);
        font->alpha[i] = uint8_t(((i & 1) ? (b & 15) : (b >> 4)) * 17);
    }
    font->valid = true;
    return font;
}

Snapshot read_guest(const uint8_t* rdram, size_t size) {
    Snapshot out;
    const Guest g{rdram, size};
    const uint32_t table = g.word(0x8013CE30U);
    if (!g.valid(table, 80 * 4) || !g.valid(0x80111110U, 80 * 4) ||
        !g.valid(0x80111250U, 80 * 4)) return out;
    for (unsigned i = 0; i < 80; ++i) {
        auto& row = out.rows[i];
        row.x = g.signed_half(0x80111110U + i * 4);
        row.y = g.signed_half(0x80111112U + i * 4);
        row.color = g.word(0x80111250U + i * 4);
        if (row.x != -1000) row.text = plain_text(g.string(g.word(table + i * 4)));
    }
    out.screen = classify(out.rows);
    if (out.screen != Screen::Native) {
        out.original = read_original_font(rdram, size);
        if (!out.original || !out.original->valid) out.screen = Screen::Native;
        if (out.screen == Screen::Summary) {
            out.thumbnail = thumbnail(g);
            // A broken/unknown sprite should not erase a useful native card.
            if (!out.thumbnail.valid()) out.screen = Screen::Native;
        }
    }
    return out;
}

bool native_options_active(const uint8_t* rdram, size_t size) {
    const Guest g{rdram, size};
    // Same two branches as func_8001FC90 at 0x8001FD38 and 0x800206C8.
    return g.valid(0x800DD5E4U, 4) && !g.word(0x800D0948U) && g.word(0x800DD5E4U) != 0;
}
Snapshot read_native_options(const uint8_t* rdram, size_t size) {
    Snapshot s;
    const Guest g{rdram, size};
    if (!native_options_active(rdram, size)) return s;
    const unsigned player = g.byte(0x8018BBFDU);
    const int selection = g.signed_half(0x800DD5E0U);
    if (player >= 4 || selection < 0 || selection > 6) return s;
    const uint32_t profile = 0x8018BBF8U + player * 0x7AU;
    s.original = read_original_font(rdram, size);
    if (!s.original) return s;
    // The native draw routine indexes DD5E8 (labels), DD674 (18-byte value
    // selector records), DD61C (value strings), and the selected save slot's
    // packed option nibbles. No mock values or copied options are used.
    for (int i = 0; i <= 6; ++i) {
        const unsigned value = i == 0 ? 1U :
            (g.byte(profile + 0x1AU + unsigned(i / 2)) >> ((i & 1) * 4)) & 15U;
        if (value > 8) return {};
        const unsigned name_index = g.half(0x800DD674U + unsigned(i * 18) + value * 2);
        if (name_index >= 22) return {};
        const std::string name = plain_text(g.string(g.word(0x800DD61CU + name_index * 4)));
        if (name.empty()) return {};
        const uint32_t color = selection == i ? 0xC8FFC8FFU : 0x408080FFU;
        if (i == 0) s.rows[52] = {160, 50, color, name};
        else {
            const int row = 51 + i * 2;
            s.rows[row] = {90, 60 + i * 15, color, plain_text(g.string(g.word(0x800DD5E8U + unsigned(i) * 4)))};
            s.rows[row + 1] = {200, 60 + i * 15, color, name};
        }
    }
    if (classify(s.rows) != Screen::Options) return {};
    s.focused_setting = selection;
    s.screen = Screen::Options;
    const uint32_t raw = g.word(0x800D0954U);
    float zoom = 0; std::memcpy(&zoom, &raw, sizeof(zoom));
    if (!(zoom >= .99f && selection == 6)) return s;
    s.screen = Screen::Controls;
    s.preset = s.rows[64].text;
    const unsigned preset = g.byte(profile + 0x1D) & 15;
    if (preset > 7 || !g.valid(0x800E6808U + preset * 96, 96)) return {};
    // Input aliases are the fixed keyboard/classic-pad translation in
    // frontend.cpp. Modern on-foot/bike pad aliases are decorated by the
    // bridge, after reading these native per-preset action masks.
    struct Key { uint16_t bit; const char* pad; const char* key; };
    static constexpr Key keys[] = {
        {0x8000, "A", "Z/Space"}, {0x4000, "X/B", "X"}, {0x2000, "LT", "C"},
        {0x1000, "Start", "Enter"}, {0x0800, "D-Up", "Up"},
        {0x0400, "D-Down", "Down"}, {0x0200, "D-Left", "Left"}, {0x0100, "D-Right", "Right"},
        {0x0020, "LB", "Q"}, {0x0010, "RB", "E"}, {0x0008, "RS-Up", "I"},
        {0x0004, "RS-Down", "K"}, {0x0002, "RS-Left/Y", "J"}, {0x0001, "RS-Right", "L"}
    };
    static constexpr int ranges[5][2] = {{12,24},{24,31},{36,39},{31,36},{39,48}};
    for (int group = 0; group < 5; ++group) {
        auto& bindings = s.native_controls[group];
        bindings.push_back({"Move", "LS", "WASD"});
        for (int action = 0; action < 48; ++action) {
            if (!(action < 12 || (action >= ranges[group][0] && action < ranges[group][1]))) continue;
            const std::string label = plain_text(g.string(g.word(0x800DA70CU + unsigned(action) * 4)));
            const uint16_t mask = g.half(0x800E6808U + preset * 96 + unsigned(action) * 2);
            if (label.empty() || !mask) continue;
            Binding line{label, {}, {}};
            for (const auto& k : keys) if (mask & k.bit) {
                if (!line.pad.empty()) { line.pad += " / "; line.keyboard += " / "; }
                line.pad += k.pad; line.keyboard += k.key;
            }
            auto existing = std::find_if(bindings.begin(), bindings.end(), [&](const Binding& b) { return b.action == label; });
            if (existing == bindings.end()) bindings.push_back(std::move(line));
            else { existing->pad += " / " + line.pad; existing->keyboard += " / " + line.keyboard; }
        }
    }
    return s;
}
const char* screen_name(Screen s) {
    switch (s) {
        case Screen::Profiles: return "profiles";
        case Screen::Summary: return "summary";
        case Screen::Options: return "options";
        case Screen::Controls: return "controls";
        case Screen::Graphics: return "graphics";
        case Screen::Schemes: return "schemes";
        default: return "native";
    }
}
} // namespace sote::menu_skin
