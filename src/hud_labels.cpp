#include "menu_skin.hpp"
#include "recomp.h"
#include <algorithm>
#include <cmath>
#include <cstring>

extern "C" void func_8001CE98(uint8_t*, recomp_context*);

// Exclude baked lettering using the native sprite clipper, retaining frame,
// icon and texture-pack rendering. Original font mode takes the retail path.
extern "C" int sote_draw_hud_without_label(uint8_t* rdram, void* opaque) {
    static thread_local bool drawing = false;
    if (drawing || !sote::menu_skin::native_text_enabled() ||
        sote::menu_skin::menu_font_original()) return 0;
    auto* ctx = static_cast<recomp_context*>(opaque);
    const uint32_t sprite = uint32_t(ctx->r4);
    const uint32_t table = MEM_W(0, S32(0x8013ce2c));
    auto valid = [](uint32_t a, unsigned size) {
        return a >= 0x80000000U && a < 0x80800000U && size <= 0x80800000U-a;
    };
    if (!valid(table, 48) || !valid(sprite, 40)) return 0;
    int slot = -1;
    for (int i : {0,1,2,3,4,5,11}) if (uint32_t(MEM_W(i*4, S32(table))) == sprite) slot = i;
    if (slot < 0 || MEM_HU(6,S32(sprite)) != 36 || MEM_HU(4,S32(sprite)) != (slot==11 ? 64 : 96)) return 0;
    float sx, sy;
    uint32_t xb = MEM_W(8,S32(sprite)), yb = MEM_W(12,S32(sprite));
    std::memcpy(&sx,&xb,4); std::memcpy(&sy,&yb,4);
    if (!std::isfinite(sx) || !std::isfinite(sy) || sx<=0 || sy<=0 || sx>4 || sy>4) return 0;
    const int x = MEM_H(0,S32(sprite)), y = MEM_H(2,S32(sprite));
    const int left = x + int(std::lround((slot==11 ? 3 : 20)*sx));
    const int right = left + int(std::lround((slot==11 ? 38 : 48)*sx));
    // The upscale's glyph outline reaches into row 21; include that fringe.
    const int top = y + int(std::lround(21*sy));
    // The bottom frame starts at row 33. Excluding row 33 cuts a notch
    // where the label well meets its vertical divider.
    const int bottom = y + int(std::lround(33*sy));
    // spScissor globals: left, right, top, bottom. Restore after the split draw.
    constexpr uint32_t addresses[] = {0x8018b998,0x8018b990,0x8018b99c,0x8018b994};
    int saved[4]; for(int i=0;i<4;++i) saved[i]=MEM_W(0,S32(addresses[i]));
    const int regions[][4] = {
        {saved[0],saved[1],saved[2],std::min(top,saved[3])},
        {saved[0],saved[1],std::max(bottom,saved[2]),saved[3]},
        {saved[0],std::min(left,saved[1]),std::max(top,saved[2]),std::min(bottom,saved[3])},
        {std::max(right,saved[0]),saved[1],std::max(top,saved[2]),std::min(bottom,saved[3])}
    };
    const auto incoming = *ctx;
    drawing = true;
    for (const auto& region : regions) {
        if(region[0]>=region[1] || region[2]>=region[3]) continue;
        for(int i=0;i<4;++i) MEM_W(0,S32(addresses[i]))=region[i];
        *ctx = incoming;
        func_8001CE98(rdram,ctx);
    }
    for(int i=0;i<4;++i) MEM_W(0,S32(addresses[i]))=saved[i];
    drawing = false;
    return 1;
}
