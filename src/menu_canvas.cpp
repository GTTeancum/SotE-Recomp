#include "menu_skin.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string_view>
#include <unordered_map>

// Private implementation: never reuse or replace RT64/ImGui's font context.
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

namespace sote::menu_skin {
namespace {
using Color = uint32_t; // RRGGBBAA
constexpr Color gold = 0xA8802EFF, green = 0x79F06AFF, teal = 0x3F8078FF;
constexpr Color cyan = 0x6FE9FFFF, purple = 0x6A63C9FF;
std::mutex font_mutex;
std::filesystem::path ui_root;
std::map<std::string, std::string> font_roles;
struct RasterGlyph { int w = 0, h = 0, x = 0, y = 0; float advance = 0; std::vector<uint8_t> alpha; };
struct FontFace {
    std::vector<uint8_t> bytes;
    stbtt_fontinfo info{};
    bool valid = false;
    std::map<std::pair<unsigned, int>, RasterGlyph> cache;
};
std::map<std::string, std::unique_ptr<FontFace>> faces;

std::string trim(std::string v) {
    const auto a = v.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    v = v.substr(a, v.find_last_not_of(" \t\r\n") - a + 1);
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
    return v;
}
uint32_t be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
// Reject truncated SFNT directories before passing a local, user-selected font
// to stb. This is an asset loader, not a sandbox for untrusted network fonts.
bool valid_sfnt(const std::vector<uint8_t>& b) {
    if (b.size() < 12) return false;
    const uint32_t magic = be32(b.data());
    if (magic != 0x00010000U && magic != 0x4F54544FU && magic != 0x74727565U) return false;
    const size_t count = size_t(b[4]) << 8 | b[5];
    if (!count || count > 256 || 12 + count * 16 > b.size()) return false;
    for (size_t i = 0; i < count; ++i) {
        const size_t p = 12 + i * 16;
        const size_t offset = be32(b.data() + p + 8), length = be32(b.data() + p + 12);
        if (offset > b.size() || length > b.size() - offset) return false;
    }
    return true;
}
FontFace* face_for(std::string role) {
    std::string path = "original";
    while (true) {
        const auto it = font_roles.find(role);
        if (it != font_roles.end()) { path = it->second; break; }
        const auto dot = role.rfind('.');
        if (dot != std::string::npos) role.resize(dot);
        else if (role != "default") role = "default";
        else break;
    }
    if (path.empty() || path == "original") return nullptr;
    if (auto it = faces.find(path); it != faces.end()) return it->second->valid ? it->second.get() : nullptr;
    auto f = std::make_unique<FontFace>();
    auto* result = f.get();
    faces[path] = std::move(f); // Cache failures too; do not retry every frame.
    std::error_code ec;
    const std::filesystem::path relative = std::filesystem::path(std::u8string(path.begin(), path.end()));
    bool safe = !relative.is_absolute() && !relative.has_root_name();
    for (const auto& part : relative) if (part == "..") safe = false;
    const auto root = std::filesystem::weakly_canonical(ui_root, ec);
    const auto full = std::filesystem::weakly_canonical(ui_root / relative, ec);
    const auto rel = full.lexically_relative(root);
    if (rel.empty()) safe = false;
    for (const auto& part : rel) if (part == "..") safe = false;
    const auto length = safe && !ec ? std::filesystem::file_size(full, ec) : 0;
    if (safe && !ec && length >= 12 && length <= 16 * 1024 * 1024) {
        std::ifstream in(full, std::ios::binary);
        result->bytes.resize(size_t(length));
        if (in.read(reinterpret_cast<char*>(result->bytes.data()), std::streamsize(length)) && valid_sfnt(result->bytes))
            result->valid = stbtt_InitFont(&result->info, result->bytes.data(), 0) != 0;
    }
    if (!result->valid) {
        std::fprintf(stderr, "[sote][menu] Font '%s' unavailable/invalid; using original glyphs.\n", path.c_str());
        result->bytes.clear();
        return nullptr;
    }
    return result;
}
const RasterGlyph& glyph(FontFace& f, unsigned c, int pixels) {
    const auto key = std::make_pair(c, pixels);
    if (auto i = f.cache.find(key); i != f.cache.end()) return i->second;
    auto& g = f.cache[key];
    int advance = 0, lsb = 0, ascent = 0;
    const float scale = stbtt_ScaleForPixelHeight(&f.info, float(pixels));
    stbtt_GetFontVMetrics(&f.info, &ascent, nullptr, nullptr);
    stbtt_GetCodepointHMetrics(&f.info, int(c), &advance, &lsb);
    unsigned char* bitmap = stbtt_GetCodepointBitmap(&f.info, 0, scale, int(c), &g.w, &g.h, &g.x, &g.y);
    g.advance = advance * scale;
    g.y += int(std::round(ascent * scale));
    if (bitmap && g.w > 0 && g.h > 0 && g.w < 1024 && g.h < 1024)
        g.alpha.assign(bitmap, bitmap + size_t(g.w) * g.h);
    stbtt_FreeBitmap(bitmap, nullptr);
    return g;
}

class Canvas {
public:
    Image image;
    float scale, offset_x, offset_y;
    const OriginalFont& font;
    Canvas(unsigned w, unsigned h, const OriginalFont& f) : image{w, h, std::vector<uint8_t>(size_t(w) * h * 4)},
        scale(std::min(w / 960.0f, h / 540.0f)), offset_x((w - 960 * scale) / 2), offset_y((h - 540 * scale) / 2), font(f) {
        for (size_t i = 3; i < image.rgba.size(); i += 4) image.rgba[i] = 255;
    }
    void pixel(int x, int y, Color c, unsigned coverage = 255) {
        if (x < 0 || y < 0 || x >= int(image.width) || y >= int(image.height)) return;
        const unsigned a = (c & 255) * coverage / 255;
        if (!a) return;
        const size_t p = (size_t(y) * image.width + unsigned(x)) * 4;
        for (int k = 0; k < 3; ++k) {
            const unsigned v = (c >> (24 - k * 8)) & 255;
            image.rgba[p + k] = uint8_t((v * a + image.rgba[p + k] * (255 - a) + 127) / 255);
        }
    }
    void rect(float x, float y, float w, float h, Color c) {
        const int x0 = std::max(0, int(std::round(offset_x + x * scale)));
        const int y0 = std::max(0, int(std::round(offset_y + y * scale)));
        const int x1 = std::min(int(image.width), int(std::round(offset_x + (x + w) * scale)));
        const int y1 = std::min(int(image.height), int(std::round(offset_y + (y + h) * scale)));
        for (int yy = y0; yy < y1; ++yy) for (int xx = x0; xx < x1; ++xx) pixel(xx, yy, c);
    }
    void border(float x, float y, float w, float h, Color c, float t = 1) {
        rect(x, y, w, t, c); rect(x, y + h - t, w, t, c);
        rect(x, y, t, h, c); rect(x + w - t, y, t, h, c);
    }
    void stars() {
        uint32_t seed = 0x534F5445;
        auto next = [&] { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; };
        for (int i = 0; i < 260; ++i) {
            const float x = float(next() % 96000) / 100, y = float(next() % 54000) / 100;
            const uint32_t v = 28 + next() % 92;
            const float size = (i % 13 == 0) ? 1.7f : .8f;
            rect(x, y, size, size, (v << 24) | (v << 16) | ((v + 12) << 8) | 255);
        }
    }
    float measure(std::string_view str, FontFace* face, float pixels) const {
        float sum = 0;
        if (face) {
            const float k = stbtt_ScaleForPixelHeight(&face->info, pixels);
            for (unsigned char ch : str) { int advance, lsb; stbtt_GetCodepointHMetrics(&face->info, ch, &advance, &lsb); sum += advance * k; }
        } else for (unsigned char ch : str) {
            if (ch >= 32 && ch <= 126) sum += font.glyphs[ch - 32].advance * pixels / font.line_height;
        }
        return sum;
    }
    // x is a left edge for align=0, center for 1, right edge for 2.
    // Each independently addressable widget resolves its font at draw time.
    void text(std::string str, const std::string& role, float x, float y, float height,
              Color color, float max_width = 800, int align = 0) {
        if (str.empty() || max_width <= 0) return;
        FontFace* face = face_for(role);
        std::istringstream lines(str); std::string line;
        while (std::getline(lines, line)) {
            float pixels = height * scale;
            const float width = measure(line, face, pixels);
            if (width > max_width * scale) pixels *= max_width * scale / width;
            pixels = std::max(1.0f, pixels);
            float pen = offset_x + x * scale;
            const float measured = measure(line, face, pixels);
            if (align == 1) pen -= measured / 2;
            else if (align == 2) pen -= measured;
            const float top = offset_y + y * scale;
            for (unsigned char c : line) {
                if (c < 32 || c > 126) continue;
                if (face) {
                    const auto& g = glyph(*face, c, std::max(1, int(std::round(pixels))));
                    for (int yy = 0; yy < g.h && !g.alpha.empty(); ++yy) for (int xx = 0; xx < g.w; ++xx)
                        pixel(int(std::round(pen)) + g.x + xx, int(std::round(top)) + g.y + yy, color, g.alpha[size_t(yy) * g.w + xx]);
                    pen += g.advance;
                } else {
                    const auto& g = font.glyphs[c - 32];
                    const float k = pixels / font.line_height;
                    if (g.x >= 0 && g.width > 0 && g.height > 0) {
                        const int left = int(std::round(pen - g.bearing_x * k)), right = int(std::round(pen + (g.width - g.bearing_x) * k));
                        const int t = int(std::round(top - g.bearing_y * k)), bottom = int(std::round(top + (g.height - g.bearing_y) * k));
                        for (int yy = t; yy < bottom; ++yy) for (int xx = left; xx < right; ++xx) {
                            const int gx = std::min(g.width - 1, int((xx - left) / k));
                            const int gy = std::min(g.height - 1, int((yy - t) / k));
                            pixel(xx, yy, color, font.alpha[size_t(g.y + gy) * 64 + g.x + gx]);
                        }
                    }
                    pen += g.advance * k;
                }
            }
            y += height * 1.15f;
        }
    }
    void picture(const Image& source, float x, float y, float w, float h) {
        if (!source.valid()) return;
        const int left = int(std::round(offset_x + x * scale)), top = int(std::round(offset_y + y * scale));
        const int width = std::max(1, int(std::round(w * scale))), height = std::max(1, int(std::round(h * scale)));
        for (int yy = 0; yy < height; ++yy) for (int xx = 0; xx < width; ++xx) {
            const unsigned sx = std::min(source.width - 1, unsigned(xx) * source.width / unsigned(width));
            const unsigned sy = std::min(source.height - 1, unsigned(yy) * source.height / unsigned(height));
            const size_t p = (size_t(sy) * source.width + sx) * 4;
            pixel(left + xx, top + yy, uint32_t(source.rgba[p]) << 24 | uint32_t(source.rgba[p + 1]) << 16 |
                  uint32_t(source.rgba[p + 2]) << 8 | source.rgba[p + 3]);
        }
    }
    void heading(const std::string& title, const std::string& role, Color c = 0x66C2B6FF) {
        FontFace* f = face_for(role);
        const float half = std::min(330.0f, measure(title, f, 30 * scale) / scale / 2 + 16);
        rect(92, 72, std::max(0.0f, 388 - half), 2, purple);
        rect(480 + half, 72, std::max(0.0f, 388 - half), 2, purple);
        text(title, role, 480, 57, 30, c, 640, 1);
    }
};
bool selected(const Row& r) {
    return ((r.color >> 24) & 255) >= 0xA0 && ((r.color >> 16) & 255) >= 0xD0 && ((r.color >> 8) & 255) >= 0xA0;
}
void profiles(Canvas& c, const Snapshot& s) {
    c.stars();
    c.text("Press Start to select", "profile.prompt", 480, 48, 28, gold, 760, 1);
    for (int i = 0; i < 4; ++i) {
        const float y = 107.0f + i * 59;
        const bool active = ((s.rows[51 + i].color >> 16) & 255) >= 160;
        const bool filled = s.rows[70 + i].visible();
        const Color frame = active ? 0x3FDD4AFF : filled ? 0x1D5C4CFF : 0x1A4C44FF;
        const Color ink = active ? green : filled ? 0x2F7160FF : 0x28564CFF;
        c.rect(88, y, 545, 47, 0x000000C0); c.rect(654, y, 218, 47, 0x000000C0);
        c.border(88, y, 545, 47, frame, active ? 2 : 1); c.border(654, y, 218, 47, frame, active ? 2 : 1);
        c.text(s.rows[51 + i].text, "profile.name." + std::to_string(i), 106, y + 10, 27, ink, 508);
        c.text(s.rows[70 + i].text, "profile.difficulty." + std::to_string(i), 763, y + 10, 25, ink, 182, 1);
    }
    c.rect(183, 364, 594, 2, 0x2C7A35FF);
    c.border(182, 391, 596, 48, 0x1F4D2EFF, 2);
    for (int i = 0; i < 3; ++i) {
        const auto& row = s.rows[76 + i];
        const bool focus = ((row.color >> 16) & 255) > 150;
        const float x = 281.0f + i * 199;
        if (focus) c.border(x - 98, 391, 197, 48, 0x3FDD4AFF, 2);
        c.text(row.text, "profile.action." + std::to_string(i), x, 402, 25, focus ? green : 0x34783FFF, 183, 1);
    }
    c.rect(348, 458, 264, 2, 0x2C7A35FF);
    c.text(s.rows[68].text, "profile.hint", 480, 478, 24, gold, 750, 1);
}
void summary(Canvas& c, const Snapshot& s) {
    c.stars(); c.rect(106, 31, 748, 2, 0x149CB9FF); c.rect(202, 96, 556, 2, 0x149CB9FF);
    c.text(s.rows[52].text, "summary.player", 480, 51, 32, 0x4FD4E8FF, 740, 1);
    c.picture(s.thumbnail, 98, 139, 440, 330);
    c.border(95, 136, 446, 336, 0xC9A24AFF, 3);
    for (int i = 0; i < 80; ++i) c.rect(98, 389.0f + i, 440, 1, uint32_t(i * 220 / 80));
    const bool multiline = s.rows[50].text.find('\n') != std::string::npos;
    c.text(s.rows[50].text, "summary.level", 318, multiline ? 409.0f : 437.0f, 25, 0xE7C564FF, 408, 1);
    c.text(s.rows[55].text, "summary.difficulty", 318, 491, 23, 0xC5A04BFF, 475, 1);
    c.text(s.rows[56].text, "summary.label.lives", 610, 204, 26, 0x3F8F74FF, 160);
    c.text(s.rows[57].text, "summary.value.lives", 851, 204, 28, 0x4DDD5EFF, 83, 2);
    c.text(s.rows[58].text, "summary.label.time", 610, 273, 26, 0x3F8F74FF, 250);
    c.text(s.rows[59].text, "summary.value.time", 610, 310, 30, 0x4DDD5EFF, 265);
    c.text(s.rows[60].text, "summary.label.challenge", 610, 376, 26, 0x3F8F74FF, 274);
    c.text(s.rows[61].text, "summary.value.challenge", 610, 413, 30, 0x4DDD5EFF, 265);
}
void options(Canvas& c, const Snapshot& s) {
    c.stars();
    c.heading(s.rows[52].text, "options.return", selected(s.rows[52]) ? cyan : 0x66C2B6FF);
    int index = 0;
    for (int row = 53; row <= 67; row += 2) {
        if (!s.rows[row].visible()) continue;
        const float y = 126.0f + index * 40;
        if (y > 447) break;
        const bool focus = selected(s.rows[row]);
        c.text(s.rows[row].text, "options.label." + std::to_string(row), 200, y, 27, focus ? cyan : teal, 375);
        c.text(s.rows[row + 1].text, "options.value." + std::to_string(row), 660, y, 27, focus ? cyan : teal, 201);
        ++index;
    }
    c.rect(92, 469, 776, 2, purple);
    c.text("Select with your usual controls", "options.hint", 480, 493, 18, 0x697388FF, 745, 1);
}
void graphics(Canvas& c, const Snapshot& s) {
    c.stars(); c.heading(s.rows[52].text, "graphics.heading");
    for (int i = 0; i < 5; ++i) {
        const int row = 53 + i * 2;
        const bool focus = selected(s.rows[row]);
        c.text(s.rows[row].text, "graphics.label." + std::to_string(i), 174, 131.0f + i * 49, 27, focus ? cyan : teal, 360);
        c.text(s.rows[row + 1].text, "graphics.value." + std::to_string(i), 612, 131.0f + i * 49, 27, focus ? cyan : teal, 224);
    }
    for (int row : {63, 65, 66}) if (s.rows[row].visible())
        c.text(s.rows[row].text, "graphics.action." + std::to_string(row), float(s.rows[row].x * 3), 405, 25, selected(s.rows[row]) ? cyan : teal, 205, 1);
    c.rect(92, 452, 776, 2, purple);
    c.text(s.rows[64].text, "graphics.return", 480, 481, 26, selected(s.rows[64]) ? cyan : teal, 740, 1);
}
void binding_column(Canvas& c, const std::vector<Binding>& rows, float x, float top, float width, float spacing,
                    const std::string& prefix, float font_size) {
    for (size_t i = 0; i < rows.size(); ++i) {
        const float y = top + float(i) * spacing;
        if (y + spacing > 477) break;
        const auto& r = rows[i];
        c.text(r.action, prefix + ".action." + std::to_string(i), x, y + 2, font_size, teal, width * .39f);
        const float key_x = x + width * .43f, pad_x = x + width * .72f;
        c.rect(key_x, y, width * .24f, spacing - 4, 0x101C22D0); c.border(key_x, y, width * .24f, spacing - 4, 0x3C5F6BFF);
        c.text(r.keyboard, prefix + ".key." + std::to_string(i), key_x + width * .12f, y + 3, font_size - 2, 0x9FC3C9FF, width * .22f, 1);
        c.rect(pad_x, y, width * .25f, spacing - 4, 0x252B32F0); c.border(pad_x, y, width * .25f, spacing - 4, 0x4A5058FF);
        c.text(r.pad, prefix + ".pad." + std::to_string(i), pad_x + width * .125f, y + 3, font_size - 2, 0xCFD6DEFF, width * .23f, 1);
        c.rect(x, y + spacing - 1, width, 1, 0x6A63C928);
    }
}
void controls(Canvas& c, const Snapshot& s) {
    c.stars(); c.heading("Controls", "controls.heading");
    c.text("Action", "controls.columns.action", 115, 108, 19, teal, 250);
    c.text("Keyboard", "controls.columns.keyboard", 546, 108, 19, teal, 200, 1);
    c.text("Controller", "controls.columns.pad", 763, 108, 19, teal, 200, 1);
    c.rect(92, 138, 776, 1, purple);
    const char* titles[] = {"On Foot", "Ship - Snowspeeder", "Turret", "Speeder Bike", "Ship - Outrider"};
    const char* ids[] = {"foot", "ship.snowspeeder", "turret", "bike", "ship.outrider"};
    // Discrete row scrolling avoids cropped text and keeps the page chrome fixed.
    int line = 0;
    for (int group : {0, 1, 4, 3, 2}) {
        auto y_of = [&](int n) { return 150.0f + (n - s.controls_scroll) * 29.0f; };
        float y = y_of(line++);
        if (y >= 150 && y <= 412)
            c.text(titles[group], std::string("controls.") + ids[group] + ".heading", 115, y, 24, 0x939FEAFF, 720);
        for (size_t i = 0; i < s.native_controls[group].size(); ++i) {
            y = y_of(line++);
            if (y < 150 || y > 412) continue;
            const auto& binding = s.native_controls[group][i];
            const std::string role = std::string("controls.") + ids[group];
            const bool focus = s.rebinding && s.binding_editing && s.binding_group == group && s.binding_row == int(i);
            if (focus) c.rect(103, y - 1, 765, 28, 0x333055C0);
            c.text(binding.action, role + ".action." + std::to_string(i), 115, y + 3, 22, focus ? cyan : teal, 302);
            c.rect(438, y, 215, 26, 0x111C27FF); c.border(438, y, 215, 26, 0x43536FFF);
            c.text(binding.keyboard, role + ".key." + std::to_string(i), 545.5f, y + 4, 20, 0xB1BDE3FF, 200, 1);
            c.rect(666, y, 195, 26, 0x262A36FF); c.border(666, y, 195, 26, 0x535768FF);
            c.text(binding.pad, role + ".pad." + std::to_string(i), 763.5f, y + 4, 20, 0xD2D5E7FF, 182, 1);
            if (focus) c.border(s.binding_column == 0 ? 438 : 666, y,
                s.binding_column == 0 ? 215 : 195, 26, cyan);
        }
        ++line; // breathing space between vehicle sections
    }
    c.rect(886, 150, 3, 288, 0x26243BFF);
    const float thumb = std::max(24.0f, 288.0f * 10 / std::max(10, line));
    c.rect(884, 150 + (288 - thumb) * s.controls_scroll / std::max(1, line - 10), 7, thumb, purple);
    c.rect(92, 455, 776, 2, purple);
    c.text("Preset", "controls.preset.label", 116, 479, 25, teal, 190);
    c.text(s.preset, "controls.preset.value", 850, 479, 25, cyan, 489, 2);
    if (!s.rebinding) {
        c.text("Scroll: wheel / PgUp-PgDn / right stick", "controls.scroll_hint", 480, 513, 16, 0x76819FFF, 790, 1);
        return;
    }
    if (!s.binding_editing) {
        c.text("Enter/A: edit bindings   Left/Right: native preset   Up/Down: options",
            "controls.rebind.open", 480, 510, 14, 0xA5B2CDFF, 850, 1);
        c.text("Wheel: inspect bindings   Graphics / Controls schemes remain on the Options page",
            "controls.rebind.reference_help", 480, 527, 12, 0x76819FFF, 850, 1);
        return;
    }
    c.text(s.binding_status, "controls.rebind.status", 480, 91, 13, 0xBAD2E7FF, 756, 1);
    c.text("Up/Down: action   Left/Right: device   Enter/A: change   Esc/B: back",
        "controls.rebind.navigation", 480, 510, 14, 0xA5B2CDFF, 850, 1);
    c.text("Delete/X: clear   F9/Y: restore section   Wheel / PgUp-PgDn: scroll",
        "controls.rebind.help", 480, 527, 12, 0x76819FFF, 850, 1);
    if (s.binding_title.empty()) return;
    // A real host modal, rendered with the same per-widget font resolver.
    c.rect(0, 0, 960, 540, 0x000008BE);
    c.rect(116, 140, 728, 292, 0x0B1220FF);
    c.border(116, 140, 728, 292, purple);
    c.rect(139, 191, 682, 1, purple);
    c.text(s.binding_title, "controls.rebind.dialog.title", 480, 162, 25, cyan, 664, 1);
    // Bound line length before rasterization; long conflict names cannot spill
    // behind a dialog button or off the bottom of the screen.
    std::istringstream paragraphs(s.binding_detail); std::string paragraph, wrapped;
    int count = 0;
    while (std::getline(paragraphs, paragraph) && count < 6) {
        std::istringstream words(paragraph); std::string word, line;
        while (words >> word) {
            if (!line.empty() && line.size() + word.size() + 1 > 62) {
                wrapped += line + "\n"; line.clear();
                if (++count >= 6) break;
            }
            if (!line.empty()) line += " ";
            line += word;
        }
        if (!line.empty() && count < 6) { wrapped += line + "\n"; ++count; }
    }
    c.text(wrapped, "controls.rebind.dialog.detail", 480, 210, 19, 0xCCD8ECFF, 660, 1);
    c.text(s.binding_hint, "controls.rebind.dialog.hint", 480, 393, 16, 0x98B6D0FF, 674, 1);
}
void schemes(Canvas& c, const Snapshot& s) {
    c.stars(); c.heading("Controls", "schemes.heading");
    c.text("On Foot", "schemes.foot.heading", 96, 110, 25, selected(s.rows[53]) ? cyan : 0x8F9ADEFF, 230);
    c.text(s.rows[54].text, "schemes.foot.scheme", 438, 113, 23, selected(s.rows[53]) ? cyan : teal, 150, 2);
    c.rect(96, 145, 350, 1, purple);
    c.text("Speeder Bike", "schemes.bike.heading", 516, 110, 25, selected(s.rows[55]) ? cyan : 0x8F9ADEFF, 224);
    c.text(s.rows[56].text, "schemes.bike.scheme", 858, 113, 23, selected(s.rows[55]) ? cyan : teal, 135, 2);
    c.rect(516, 145, 350, 1, purple);
    binding_column(c, s.on_foot, 96, 165, 350, 25, "schemes.foot", 19);
    binding_column(c, s.bike, 516, 165, 350, 33, "schemes.bike", 22);
    c.text("Keyboard / Controller", "schemes.legend", 688, 373, 18, 0x697388FF, 345, 1);
    c.text("Left / Right: change scheme", "schemes.hint", 688, 408, 17, 0x697388FF, 345, 1);
    c.rect(92, 474, 776, 2, purple);
    c.text("Apply", "schemes.apply", 335, 495, 26, selected(s.rows[57]) ? cyan : teal, 220, 1);
    c.text("Return", "schemes.return", 626, 495, 26, selected(s.rows[58]) ? cyan : teal, 220, 1);
}
} // namespace

void configure_menu_fonts(const std::filesystem::path& root) {
    std::lock_guard lock(font_mutex);
    ui_root = root;
    font_roles.clear(); faces.clear();
    std::ifstream in(root / "fonts.ini");
    std::string line;
    while (std::getline(in, line)) {
        if (line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
        if (auto p = line.find('#'); p != std::string::npos) line.resize(p);
        if (auto p = line.find('='); p != std::string::npos) {
            const std::string key = trim(line.substr(0, p)), value = trim(line.substr(p + 1));
            if (!key.empty() && !value.empty()) font_roles[key] = value;
        }
    }
}
Image render(const Snapshot& snapshot, unsigned width, unsigned height) {
    if (snapshot.screen == Screen::Native || !snapshot.original || !snapshot.original->valid ||
        width < 320 || height < 180 || width > 3840 || height > 2160) return {};
    std::lock_guard lock(font_mutex);
    Canvas c(width, height, *snapshot.original);
    switch (snapshot.screen) {
        case Screen::Profiles: profiles(c, snapshot); break;
        case Screen::Summary: summary(c, snapshot); break;
        case Screen::Options: options(c, snapshot); break;
        case Screen::Controls: controls(c, snapshot); break;
        case Screen::Graphics: graphics(c, snapshot); break;
        case Screen::Schemes: schemes(c, snapshot); break;
        default: return {};
    }
    return std::move(c.image);
}
} // namespace sote::menu_skin
