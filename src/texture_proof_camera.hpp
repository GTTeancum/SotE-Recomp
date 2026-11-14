#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Process-local diagnostic camera. Only active with the offscreen harness AND
// an explicit SOTE_TEXTURE_PROOF_CAMERA=angle:distance:eyeHeight:targetHeight.
// Positions are relative to Dash's native head anchor. Does not modify his
// geometry, animations, world position, lighting or texture coordinates.
namespace sote::texture_proof {
struct Vec { float x,y,z; };
template<class T> inline T read(uint8_t* ram,uint32_t a) {
    T v; std::memcpy(&v,ram+(a&0x7fffff),sizeof(T)); return v;
}
template<class T> inline void write(uint8_t* ram,uint32_t a,T v) {
    std::memcpy(ram+(a&0x7fffff),&v,sizeof(T));
}
inline bool valid(uint32_t a,uint32_t size) {
    return a>=0x80000000U && a<=0x80800000U-size;
}
inline bool apply(uint8_t* ram,uint32_t object,uint32_t stack) {
    const char* setting=std::getenv("SOTE_TEXTURE_PROOF_CAMERA");
    if (!setting || !std::getenv("SOTE_DIAGNOSTIC_OFFSCREEN") || !ram ||
        !valid(object,0x150) || !valid(stack,0x98)) return false;
    float angle=0,distance=0,eyeHeight=0,targetHeight=0;
    char trailing=0;
    if (std::sscanf(setting,"%f:%f:%f:%f%c",&angle,&distance,&eyeHeight,&targetHeight,&trailing)!=4 ||
        !std::isfinite(angle) || !std::isfinite(distance) ||
        !std::isfinite(eyeHeight) || !std::isfinite(targetHeight) ||
        distance<2 || distance>30 || std::fabs(eyeHeight)>15 || std::fabs(targetHeight)>15) return false;
    const uint32_t cameras=read<uint32_t>(ram,0x800D08A0);
    if (!valid(cameras,7*128)) return false;
    const Vec anchor=read<Vec>(ram,object+0x138);
    const Vec forward=read<Vec>(ram,object+0x118);
    const float n=std::hypot(forward.x,forward.y);
    if (!std::isfinite(n) || n<.001f || !std::isfinite(anchor.x) ||
        !std::isfinite(anchor.y) || !std::isfinite(anchor.z)) return false;
    const float radians=angle*0.017453292519943295f;
    const float fx=forward.x/n,fy=forward.y/n;
    const Vec eye{anchor.x+distance*(fx*std::cos(radians)-fy*std::sin(radians)),
                  anchor.y+distance*(fy*std::cos(radians)+fx*std::sin(radians)),
                  anchor.z+targetHeight+eyeHeight};
    write(ram,stack+0x8c,eye);
    write(ram,cameras+6*128+0x6c,Vec{0,0,targetHeight});
    static unsigned frames=0;
    if (++frames%120==1) {
        std::printf("[sote][texture-proof-camera] angle=%.1f distance=%.2f eye=%.3f,%.3f,%.3f target=%.3f,%.3f,%.3f\n",
            angle,distance,eye.x,eye.y,eye.z,anchor.x,anchor.y,anchor.z+targetHeight);
        std::fflush(stdout);
    }
    return true;
}
}
