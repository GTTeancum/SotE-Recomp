#include "modern_controls.hpp"
#include "controls_menu.hpp"
#include "recomp.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" void func_800654A0(uint8_t*, recomp_context*);

namespace {
struct Vec { float x, y, z; };
struct ShotObservation { uint32_t projectile=0; Vec target{}; };
struct CameraArmState {
    uint32_t owner=0;
    int16_t event=-1;
    Vec anchor{};
    float distance=0;
} camera_arm;
ShotObservation observed_shots[256];
unsigned next_observation=0;
Vec operator+(Vec a, Vec b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec operator-(Vec a, Vec b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec operator*(Vec a, float b) { return {a.x*b,a.y*b,a.z*b}; }
float dot(Vec a, Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
float length(Vec a) { return std::sqrt(dot(a,a)); }
bool finite(Vec a) { return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z); }
Vec unit(Vec a) { const float n=length(a); return n>0.0001f ? a*(1/n) : Vec{}; }
bool address(uint32_t a, uint32_t size=12) {
    return a>=0x80000000U && a<=0x80800000U-size;
}
template<class T> T read(uint8_t* ram, uint32_t a) {
    T v; std::memcpy(&v,ram+((a&0x7fffff)^(sizeof(T)==2?2:0)),sizeof(T)); return v;
}
template<class T> void write(uint8_t* ram, uint32_t a,T v) {
    std::memcpy(ram+((a&0x7fffff)^(sizeof(T)==2?2:0)),&v,sizeof(T));
}
struct Hit { float distance; uint32_t object; };
enum class TraceKind { Projectile, Camera };
// Native endpoint/start trace, excluding the shooter's collision object. Use
// a private guest stack and copied registers; never disturb the spawn's ABI.
Hit trace(uint8_t* ram, const recomp_context& caller, Vec from, Vec to, uint32_t ignore,
          TraceKind kind=TraceKind::Projectile) {
    auto ctx=caller;
    // The odd-register view must refer to this copy, not the caller's FPRs.
    ctx.f_odd=ctx.mips3_float_mode ? &ctx.f1.u32l : &ctx.f0.u32h;
    const uint32_t sp=(uint32_t(caller.r29)-0x100U)&~15U;
    ctx.r29=(int32_t)sp;
    write(ram,sp+0x40,to); write(ram,sp+0x50,from);
    write<uint32_t>(ram,sp+0x60,0); write<uint32_t>(ram,sp+0x64,0);
    ctx.r4=(int32_t)(sp+0x40); ctx.r5=(int32_t)(sp+0x50);
    ctx.r6=(int32_t)ignore; ctx.r7=(int32_t)(sp+0x60);
    write<uint32_t>(ram,sp+0x10,sp+0x64);
    write<float>(ram,sp+0x14,0.0f);
    // Match the retail camera's 8006947C argument. Its world/dynamic sweep
    // excludes the player. Projectile queries additionally scan 80112830
    // through 80026480; that extra pass does not inherit the ignored object.
    write<uint32_t>(ram,sp+0x18,kind==TraceKind::Camera ? 0 : 1);
    func_800654A0(ram,&ctx);
    return {ctx.f0.fl,read<uint32_t>(ram,sp+0x60)};
}
bool visible(uint8_t* ram,const recomp_context& ctx,Vec from,Vec to,uint32_t ignore,uint32_t target) {
    const Hit h=trace(ram,ctx,from,to,ignore);
    return std::isfinite(h.distance) && (h.distance<0 || h.distance>=length(to-from)-0.02f || h.object==target);
}
}

extern "C" void sote_modern_projectile(uint8_t* ram, void* context) {
    auto& ctx=*static_cast<recomp_context*>(context);
    // 80031FE0 has created the laser's world-space muzzle and unit direction.
    // Hook before it scales that direction into velocity; other weapons never
    // enter here. Its saved owner argument is at SP+68 after the 48-byte frame.
    const uint32_t sp=uint32_t(ctx.r29);
    if (!address(sp,0x6c) || !address(sp-0x1800U,0x1800)) return;
    const uint32_t shooter=read<uint32_t>(ram,sp+0x68);
    if (std::getenv("SOTE_TRACE_PROJECTILE_SPAWNS")) {
        std::printf("[sote][projectile-spawn] shooter=%08X modern_player=%d\n",
            shooter,sote::modern_controls::owns_player(shooter));
    }
    if (!sote::modern_controls::owns_player(shooter) || !address(shooter,0x250)) return;
    const auto settings=sote::controls_menu::modern_controls_tuning().on_foot;
    const int camera_slot=read<int16_t>(ram,0x8018DDE8);
    // Native death, camera switching and terrain transitions can select a
    // different camera without changing the player object. The visible
    // reticle still belongs to the live view, so converge against that view
    // rather than silently disabling alignment outside shoulder slot 5.
    if (!settings.convergence || camera_slot < 0 || camera_slot > 5) return;
    if (!address(uint32_t(ctx.r17),4)) return;
    const uint32_t projectile=read<uint32_t>(ram,uint32_t(ctx.r17))+uint32_t(ctx.r16);
    if (!address(projectile,0xb0)) return;
    const uint32_t position=projectile+0x50, velocity=projectile+0xa0;
    const uint32_t camera=read<uint32_t>(ram,0x800D08A4);
    if (!address(camera,0x80)) return;
    const Vec muzzle=read<Vec>(ram,position), original=read<Vec>(ram,velocity);
    const Vec eye=read<Vec>(ram,camera+0x44), forward=unit(read<Vec>(ram,camera+0x24));
    const float speed=length(original);
    if (!finite(muzzle)||!finite(eye)||!finite(forward)||!std::isfinite(speed)||speed<0.01f ||
        dot(forward,forward)<0.9f || length(eye-muzzle)>40.0f) return;
    const uint32_t ignore=read<uint32_t>(ram,shooter+0x70);
    if (!address(ignore,0x20)) return;
    const Hit center=trace(ram,ctx,eye,eye+forward*settings.convergence_range,ignore);
    if (!std::isfinite(center.distance)) return;
    const float distance=center.distance>=0 ? std::min(center.distance,settings.convergence_range) : settings.convergence_range;
    Vec target=eye+forward*distance;
    // A surface between eye and muzzle must not turn a shot backwards.
    if (dot(target-muzzle,forward)<=0.05f) return;
    Vec direction=unit(target-muzzle);
    uint32_t assisted=0;
    if (settings.magnetism_strength>0) {
        const uint32_t pool=0x80112838;
        const int count=read<int32_t>(ram,pool+8), stride=read<int32_t>(ram,pool+12);
        const uint32_t base=read<uint32_t>(ram,pool+16);
        float best=std::cos(settings.magnetism_cone_degrees*0.01745329252f);
        // The same eligible-object pool and alive/targetable gates as 8005B33C.
        if (count>=0 && count<=4096 && stride>=0x9c && stride<=4096 &&
            address(base,uint32_t(count)*uint32_t(stride))) {
            for (int i=0;i<count;++i) {
                const uint32_t record=base+i*stride;
                if (!read<uint32_t>(ram,record+0x84) || !(read<uint16_t>(ram,record+0x68)&2)) continue;
                const uint32_t object=read<uint32_t>(ram,record+0x98);
                if (!address(object,0x70) || object==ignore || !(read<uint16_t>(ram,object+6)&2)) continue;
                const uint32_t tag=read<uint32_t>(ram,object);
                if (tag==0x426f7878 || tag==0x426f7373 || !(read<float>(ram,object+0x60)>0)) continue;
                Vec candidate=read<Vec>(ram,object+0x50);
                candidate.z+=(read<float>(ram,object+0x68)+read<float>(ram,object+0x6c))*0.5f;
                const Vec delta=candidate-eye;
                const float range=length(delta), alignment=dot(unit(delta),forward);
                if (!finite(candidate)||range<0.5f||range>settings.magnetism_range||alignment<=best) continue;
                if (!visible(ram,ctx,eye,candidate,ignore,object) || !visible(ram,ctx,muzzle,candidate,ignore,object)) continue;
                best=alignment; assisted=object; target=candidate;
            }
        }
        if (assisted) direction=unit(direction*(1-settings.magnetism_strength)+unit(target-muzzle)*settings.magnetism_strength);
    }
    // Check the final muzzle path as well. Keep the real muzzle origin: native
    // projectile collision handles any cover encountered along this direction.
    const Hit obstruction=trace(ram,ctx,muzzle,muzzle+direction*length(target-muzzle),ignore);
    write(ram,velocity,direction*speed);
    // Keep the visible bolt's orientation aligned with its launch direction.
    const Vec right=unit(std::abs(direction.z)<0.999f ? Vec{direction.y,-direction.x,0} : Vec{-direction.z,0,direction.x});
    const Vec up{right.y*direction.z-right.z*direction.y,right.z*direction.x-right.x*direction.z,
                 right.x*direction.y-right.y*direction.x};
    write(ram,projectile+0x20,right);
    write(ram,projectile+0x30,direction);
    write(ram,projectile+0x40,up);
    if (std::getenv("SOTE_TRACE_MODERN_AIM")) {
        if (std::getenv("SOTE_DIAGNOSTIC_OFFSCREEN")) {
            static bool scanned_slopes=false;
            if (!scanned_slopes && std::getenv("SOTE_DIAGNOSTIC_FIND_SLOPE")) {
                scanned_slopes=true;
                uint32_t sector=read<uint32_t>(ram,0x800D2400);
                int found=0;
                for (int n=0;n<1024 && address(sector,0xBC) && found<12;++n) {
                    const Vec center=read<Vec>(ram,sector+4);
                    if (!finite(center)) break;
                    const auto sample=[&](float dx,float dy) {
                        const Vec from{center.x+dx,center.y+dy,center.z+32};
                        const Hit h=trace(ram,ctx,from,{from.x,from.y,center.z-64},ignore);
                        return h.distance>=0 && h.distance<=96 ? from.z-h.distance : NAN;
                    };
                    const float z=sample(0,0), gx=(sample(.5f,0)-z)*2,
                        gy=(sample(0,.5f)-z)*2, grade=std::hypot(gx,gy);
                    if (std::isfinite(grade) && grade>.1f && grade<1.5f) {
                        std::printf("[sote][slope-candidate] sector=%u pos=%.3f,%.3f,%.3f grade=%.4f,%.4f\n",
                            read<uint32_t>(ram,sector),center.x,center.y,z,gx,gy);
                        ++found;
                    }
                    const uint32_t next=read<uint32_t>(ram,sector+0xB8);
                    if (next==sector) break;
                    sector=next;
                }
            }
            const Vec feet=read<Vec>(ram,shooter+0x50);
            const auto floor_height=[&](float dx,float dy) {
                const Vec from{feet.x+dx,feet.y+dy,feet.z+4};
                const Hit hit=trace(ram,ctx,from,{from.x,from.y,feet.z-8},ignore);
                return hit.distance>=0 && hit.distance<=12 ? from.z-hit.distance : NAN;
            };
            const float center_z=floor_height(0,0);
            std::printf("[sote][aim-ground] feet=%.3f,%.3f,%.3f floor=%.4f grade=%.4f,%.4f\n",
                feet.x,feet.y,feet.z,center_z,
                (floor_height(.5f,0)-center_z)*2,
                (floor_height(0,.5f)-center_z)*2);
        }
        for (auto& shot : observed_shots)
            if (shot.projectile==projectile) shot.projectile=0;
        observed_shots[next_observation++ % 256] = {projectile,target};
        std::printf("[sote][aim] camera=%.3f,%.3f,%.3f forward=%.5f,%.5f,%.5f muzzle=%.3f,%.3f,%.3f target=%.3f,%.3f,%.3f shot=%.5f,%.5f,%.5f range=%.3f muzzle_hit=%.3f assisted=%08X speed=%.3f\n",
            eye.x,eye.y,eye.z,forward.x,forward.y,forward.z,muzzle.x,muzzle.y,muzzle.z,
            target.x,target.y,target.z,direction.x,direction.y,direction.z,distance,obstruction.distance,assisted,speed);
    }
}

// Read-only observation after native collision selection and before effects.
extern "C" void sote_modern_impact(uint8_t* ram, void* context) {
    if (!std::getenv("SOTE_TRACE_MODERN_AIM")) return;
    const auto& ctx=*static_cast<recomp_context*>(context);
    const uint32_t sp=uint32_t(ctx.r29);
    if (!address(sp,0x254)) return;
    const uint32_t projectile=read<uint32_t>(ram,sp+0x250);
    const Vec hit=read<Vec>(ram,sp+0x224);
    for (auto& shot : observed_shots) {
        if (!projectile || shot.projectile!=projectile) continue;
        std::printf("[sote][aim-impact] projectile=%08X target=%.4f,%.4f,%.4f hit=%.4f,%.4f,%.4f error=%.6f\n",
            projectile,shot.target.x,shot.target.y,shot.target.z,hit.x,hit.y,hit.z,length(hit-shot.target));
        shot.projectile=0;
    }
}

extern "C" void sote_modern_camera_resolve(uint8_t* ram, void* context) {
    const auto& ctx=*static_cast<recomp_context*>(context);
    const uint32_t object=uint32_t(ctx.r16), stack=uint32_t(ctx.r29);
    if (!sote::modern_controls::owns_player(object) || !address(object,0x150) ||
        !address(stack,0xa0) || !address(stack-0x1800U,0x1800)) {
        camera_arm={};
        return;
    }
    const Vec anchor=read<Vec>(ram,object+0x138);
    Vec original=read<Vec>(ram,stack+0x8c);
    // Process-local renderer reproduction: hold a previously logged camera
    // direction while moving around its location. Never active in normal play.
    if (std::getenv("SOTE_DIAGNOSTIC_OFFSCREEN")) {
        if (const char* pose=std::getenv("SOTE_DIAGNOSTIC_CAMERA_ARM")) {
            Vec diagnostic{};
            if (std::sscanf(pose,"%f:%f:%f",&diagnostic.x,&diagnostic.y,&diagnostic.z)==3 &&
                finite(diagnostic) && length(diagnostic)>0.1f && length(diagnostic)<40.0f)
                original=anchor+diagnostic;
        }
    }
    const Vec arm=original-anchor;
    const float distance=length(arm);
    const uint32_t ignore=read<uint32_t>(ram,object+0x70);
    if (!finite(anchor) || !finite(original) || !address(ignore,0x20) || distance<0.001f) return;
    // The retail ray backs off one unit ALONG the arm. At a grazing angle
    // that leaves almost no perpendicular clearance for the visible near
    // plane. Sweep a one-unit box around the eye, shortening the same arm.
    // All traces start around Dash's head, ignore his collision object, and
    // use original world/dynamic collision. Classic never enters this path.
    float safe=distance;
    for (int x : {-1,1}) for (int y : {-1,1}) for (int z : {-1,1}) {
        const Vec offset{float(x),float(y),float(z)};
        const Hit hit=trace(ram,ctx,anchor+offset,original+offset,ignore,TraceKind::Camera);
        if (std::isfinite(hit.distance) && hit.distance>=0 && hit.distance<=distance)
            safe=std::min(safe,std::max(0.0f,hit.distance-0.05f));
    }
    if (std::getenv("SOTE_TRACE_CAMERA_QUERY_COMPARE")) {
        static unsigned comparison_tick=0;
        if (++comparison_tick%30==0) {
            float projectile_limit=distance;
            uint32_t hit_object=0;
            for (int x : {-1,1}) for (int y : {-1,1}) for (int z : {-1,1}) {
                const Vec offset{float(x),float(y),float(z)};
                const Hit hit=trace(ram,ctx,anchor+offset,original+offset,ignore);
                if (std::isfinite(hit.distance) && hit.distance>=0 && hit.distance<projectile_limit) {
                    projectile_limit=std::max(0.0f,hit.distance-0.05f);
                    hit_object=hit.object;
                }
            }
            if (safe-projectile_limit>.5f)
                std::printf("[sote][camera-query] anchor=%.3f,%.3f,%.3f camera_limit=%.3f projectile_limit=%.3f hit=%08X tag=%08X ignored=%08X\n",
                    anchor.x,anchor.y,anchor.z,safe,projectile_limit,hit_object,
                    address(hit_object,4) ? read<uint32_t>(ram,hit_object) : 0,ignore);
        }
    }
    const float collision_limit=safe;
    const int16_t event=read<int16_t>(ram,0x8013CE0E);
    // Corner rays can briefly clear a crate edge while Dash slides against
    // it. Do not turn a single clear result into a full-distance camera cut.
    // Retract immediately for real geometry; only limit outward recovery.
    // New actors, levels and teleports discard the previous obstruction.
    if (camera_arm.owner==object && camera_arm.event==event &&
        length(anchor-camera_arm.anchor)<4.0f) {
        const uint64_t bits=(uint64_t(read<uint32_t>(ram,0x8018E998))<<32) |
            read<uint32_t>(ram,0x8018E99C);
        double dt;
        std::memcpy(&dt,&bits,sizeof(dt));
        if (std::isfinite(dt) && dt>=0)
            safe=std::min(safe,camera_arm.distance+12.0f*float(std::min(dt,1.0/30.0)));
    }
    camera_arm={object,event,anchor,safe};
    const Vec eye=anchor+arm*(safe/distance);
    write(ram,stack+0x8c,eye);
    if (std::getenv("SOTE_TRACE_CAMERA_COLLISION")) {
        static unsigned tick=0;
        if (++tick%30==0 || std::getenv("SOTE_TRACE_FRAME_COMMANDS")) {
            const Hit up=trace(ram,ctx,eye,eye+Vec{0,0,2},ignore,TraceKind::Camera);
            std::printf("[sote][camera-clearance] tick=%u anchor=%.3f,%.3f,%.3f original=%.3f,%.3f,%.3f eye=%.3f,%.3f,%.3f arm=%.3f safe=%.3f collision_limit=%.3f ceiling_gap=%.3f\n",
                tick,anchor.x,anchor.y,anchor.z,original.x,original.y,original.z,eye.x,eye.y,eye.z,distance,safe,collision_limit,up.distance);
        }
    }
}
