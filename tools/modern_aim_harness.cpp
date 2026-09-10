#include "modern_controls.hpp"
#include "controls_menu.hpp"
#include "recomp.h"
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

extern "C" uint32_t sote_is_bike_stage_active() { return 0; }
extern "C" void sote_modern_projectile(uint8_t*,void*);
namespace {
struct Vec { float x,y,z; };
template<class T> void put(uint8_t* r,uint32_t a,T v) { std::memcpy(r+((a&0x7fffff)^(sizeof(T)==2?2:0)),&v,sizeof(v)); }
template<class T> T get(uint8_t* r,uint32_t a) { T v;std::memcpy(&v,r+((a&0x7fffff)^(sizeof(T)==2?2:0)),sizeof(v));return v; }
int failures=0, calls=0;
float wall=100;
bool blocked_target=false;
constexpr uint32_t player=0x80200000, collision=0x80201000, camera=0x80202000,
    bolt=0x80203000, pos=bolt+0x50, vel=bolt+0xa0, target=0x80204000, pool=0x80205000, stack=0x80300000;
void check(bool ok,const char* name) { if(!ok){std::cerr<<"FAIL: "<<name<<'\n';++failures;} }
float norm(Vec a) {return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);}
}
// Native-trace double: a plane perpendicular to the camera ray. The hook still
// uses the real guest ABI, copied context, source pool, gating and INI parser.
extern "C" void func_800654A0(uint8_t* ram,recomp_context* c) {
    ++calls;
    check(uint32_t(c->r6)==collision,"trace excludes shooter");
    auto to=get<Vec>(ram,uint32_t(c->r4)),from=get<Vec>(ram,uint32_t(c->r5));
    const Vec delta{to.x-from.x,to.y-from.y,to.z-from.z};
    float fraction=delta.y>0 ? (wall-from.y)/delta.y : -1;
    if(blocked_target && to.x>0.1f) fraction=0.5f;
    c->f0.fl=(fraction>=0 && fraction<=1) ? norm(delta)*fraction : -1;
    put<uint32_t>(ram,uint32_t(c->r7),0);
    // Destroy scratch registers to verify no changes leak into the spawn.
    c->r4=0; c->r16=0; c->f12.fl=999;
}
int main(int argc,char** argv) {
    const auto root=std::filesystem::absolute(argc>1?argv[1]:"aim-test-settings");
    auto configure=[&](float strength,bool convergence=true) {
        std::filesystem::create_directories(root);
        {std::ofstream f(root/"CONTROLS_MODERN.INI"); f<<"on_foot_convergence = "<<convergence
            <<"\non_foot_magnetism_strength = "<<strength<<"\non_foot_magnetism_cone_degrees = 2\n";}
        sote::controls_menu::initialize(root);
        using namespace sote::controls_menu;
        if(current_scheme(SchemeSlot::OnFoot)!=ControlScheme::Modern)cycle_scheme(SchemeSlot::OnFoot,1);
    };
    configure(0);
    std::vector<uint8_t> memory(8*1024*1024); auto* ram=memory.data();
    put(ram,player+0x70,collision);put(ram,0x800D08A4,camera);put<int16_t>(ram,0x8018DDE8,5);
    put(ram,camera+0x44,Vec{1.2f,-8,1.2f});put(ram,camera+0x24,Vec{0,1,0});
    put(ram,pos,Vec{0,0,0});put(ram,stack+0x68,player);
    sote::modern_controls::publish({{},{},true});sote_modern_begin(ram,player);
    recomp_context c{}; c.r29=(int32_t)stack;c.r17=(int32_t)0x80112e60;put(ram,0x80112e60,bolt);
    const auto original_context=c;
    for(float distance:{2.0f,100.0f,1400.0f}) {
        wall=distance;put(ram,vel,Vec{0,20,0});calls=0;
        sote_modern_projectile(ram,&c);
        const auto v=get<Vec>(ram,vel);
        check(std::abs(v.x*(distance/v.y)-1.2f)<0.001f && std::abs(v.z*(distance/v.y)-1.2f)<0.001f,"near/far convergence hits camera point");
        check(std::abs(norm(v)-20)<0.0001f,"projectile speed preserved");
        check(calls==2,"camera and muzzle collision traces");
        check(std::memcmp(&c,&original_context,sizeof(c))==0,"spawn registers preserved");
        check(get<Vec>(ram,pos).x==0,"muzzle never teleports to camera");
    }
    wall=-4;put(ram,vel,Vec{0,20,0});sote_modern_projectile(ram,&c);
    check(get<Vec>(ram,vel).x==0,"surface behind muzzle cannot reverse shot");
    wall=2000;put(ram,vel,Vec{0,20,0});sote_modern_projectile(ram,&c);
    check(std::abs(get<Vec>(ram,vel).x/get<Vec>(ram,vel).y-1.2f/1492)<0.00001f,"sky uses finite convergence range");
    // Native eligible target in a small cone, at 50 units.
    put<int32_t>(ram,0x80112840,1);put<int32_t>(ram,0x80112844,0xa0);put(ram,0x80112848,pool);
    put<uint32_t>(ram,pool+0x84,1);put<uint16_t>(ram,pool+0x68,2);put(ram,pool+0x98,target);
    put<uint32_t>(ram,target,0x48756d6e);put<uint16_t>(ram,target+6,2);put<float>(ram,target+0x60,100);
    put(ram,target+0x50,Vec{2,50,1.2f});
    configure(1);put(ram,vel,Vec{0,20,0});sote_modern_projectile(ram,&c);
    auto v=get<Vec>(ram,vel);check(std::abs(v.x/v.y-2.0f/50)<0.00001f,"full magnetism reaches visible candidate");
    configure(.5f);put(ram,vel,Vec{0,20,0});sote_modern_projectile(ram,&c);
    v=get<Vec>(ram,vel);check(v.x/v.y>1.2f/1492 && v.x/v.y<2.0f/50,"partial strength bounded between base and target");
    blocked_target=true;put(ram,vel,Vec{0,20,0});sote_modern_projectile(ram,&c);
    v=get<Vec>(ram,vel);check(v.x/v.y<0.003f,"occluded target cannot attract shot");blocked_target=false;
    put(ram,target+0x50,Vec{20,50,1.2f});put(ram,vel,Vec{0,20,0});sote_modern_projectile(ram,&c);
    check(get<Vec>(ram,vel).x/get<Vec>(ram,vel).y<0.003f,"outside-cone target ignored");
    configure(0);put(ram,vel,Vec{0,20,0});calls=0;
    put(ram,stack+0x68,player+0x1000);sote_modern_projectile(ram,&c);
    check(calls==0,"enemy projectiles untouched");put(ram,stack+0x68,player);
    sote::modern_controls::publish({{},{},false});sote_modern_begin(ram,player);sote_modern_projectile(ram,&c);
    check(calls==0,"disconnected controller disables assistance");
    sote::modern_controls::publish({{},{},true});sote_modern_begin(ram,player);
    configure(0,false);put(ram,vel,Vec{0,20,0});calls=0;sote_modern_projectile(ram,&c);
    check(calls==0 && get<Vec>(ram,vel).x==0,"INI disables convergence without traces");
    configure(1); sote::controls_menu::cycle_scheme(sote::controls_menu::SchemeSlot::OnFoot,1);
    sote_modern_begin(ram,player);calls=0;sote_modern_projectile(ram,&c);
    check(calls==0 && get<Vec>(ram,vel).x==0,"Classic is untouched");
    std::cout<<"modern aim failures="<<failures<<'\n';return failures?1:0;
}
