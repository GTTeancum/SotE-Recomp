#include "rapid_fire.hpp"
#include "cheats.hpp"
#include "modern_controls.hpp"
#include "controls_menu.hpp"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <vector>

extern "C" uint32_t sote_is_bike_stage_active() { return 0; }
extern "C" uint32_t sote_modern_aim(uint8_t*, uint32_t);
extern "C" uint32_t sote_modern_take_camera_request();
extern "C" void sote_modern_camera_offset(uint8_t*, uint32_t, uint32_t);
namespace {
int failures = 0;
void check(bool value, const char* message) {
    if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
template<class T> void put(std::vector<uint8_t>& ram, uint32_t address, T value) {
    const auto offset = (address & 0x7FFFFF) ^ (sizeof(T) == 2 ? 2 : 0);
    std::memcpy(ram.data() + offset, &value, sizeof(T));
}
template<class T> T get(std::vector<uint8_t>& ram, uint32_t address) {
    T value;
    const auto offset = (address & 0x7FFFFF) ^ (sizeof(T) == 2 ? 2 : 0);
    std::memcpy(&value, ram.data() + offset, sizeof(T));
    return value;
}
}
int main() {
    using namespace sote::modern_controls;
    check(shape_stick({.08f, .08f}, .12f, 1.7f).x == 0, "radial drift rejection");
    auto diagonal = shape_stick({.5f, .1f}, .12f, 1.0f);
    check(std::fabs(diagonal.x / diagonal.y - 5) < .0001f, "preserve shallow diagonals");
    check(std::fabs(std::hypot(shape_stick({1,1},.12f,1).x,
        shape_stick({1,1},.12f,1).y) - 1) < .0001f, "no diagonal speed boost");
    float previous = 0;
    for (int i = 0; i <= 1000; ++i) {
        auto s = shape_stick({i / 1000.0f, 0}, .12f, 1.7f);
        check(s.x >= previous && s.x <= 1, "monotonic aim response");
        check(s.x - previous < .003f, "no activation jump");
        previous = s.x;
    }
    const auto quarter = shape_stick({.25f,0},.12f,1.7f).x;
    const auto half = shape_stick({.5f,0},.12f,1.7f).x;
    check(quarter > 0 && quarter < half && half < 1, "distinct analog turn speeds");
    check(shape_stick({NAN,0},.12f,1).x == 0, "reject invalid device input");
    float pitch30 = 0, pitch120 = 0;
    for (int i=0;i<30;++i) pitch30 = advance_pitch(pitch30,.25f,90,1.0/30);
    for (int i=0;i<120;++i) pitch120 = advance_pitch(pitch120,.25f,90,1.0/120);
    check(std::fabs(pitch30-pitch120)<.001f, "frame-rate-independent pitch");
    check(advance_pitch(20,0,90,1.0/60)==20, "release holds pitch");
    check(advance_pitch(54,1,90,.1)==55, "pitch upper limit");
    check(advance_pitch(-54,-1,90,.1)==-55, "pitch lower limit");
    check(advance_pitch(20,1,90,NAN)==20, "invalid delta rejected");

    // Exercise the actual guest hooks against word-swapped RDRAM and native
    // decoded input locations. This checks integration, not just stick math.
    const auto settings = std::filesystem::temp_directory_path() /
        "sote-modern-controls-harness-settings";
    std::filesystem::create_directories(settings);
    sote::controls_menu::initialize(settings);
    using sote::controls_menu::SchemeSlot;
    using sote::controls_menu::ControlScheme;
    if (sote::controls_menu::current_scheme(SchemeSlot::OnFoot) != ControlScheme::Modern)
        sote::controls_menu::cycle_scheme(SchemeSlot::OnFoot,1);
    std::vector<uint8_t> ram(8*1024*1024);
    const uint32_t object=0x80200000, stack=0x80300000;
    put(ram,object+0xB4,100.0f);
    put(ram,0x800D252C,int16_t(6));
    const uint32_t table = 0x800E6808 + 6*96;
    put(ram,table+0x1A,uint16_t(0x4000));
    put(ram,table+0x18,uint16_t(0x8000));
    put(ram,table+0x24,uint16_t(0x8));
    put(ram,table+0x1C,uint16_t(0x1));
    double dt = 1.0/60;
    uint64_t bits;
    std::memcpy(&bits,&dt,8);
    put(ram,0x8018E998,uint32_t(bits>>32));
    put(ram,0x8018E99C,uint32_t(bits));
    publish({{.5f,.5f},{.5f,.5f},true});
    sote_modern_begin(ram.data(),object);
    check(map_buttons(0x8000)==0x4000 && map_buttons(0x4000)==0x8000 &&
        map_buttons(0x10)==0x8 && map_buttons(0x2000)==0x1 &&
        map_buttons(0x1000)==0x1000,
        "actions follow saved retail preset and preserve Start");
    check(sote_modern_take_camera_request()==1 && sote_modern_take_camera_request()==0,
        "Modern requests native camera transition exactly once");
    const uint32_t cameras=0x80210000;
    put(ram,0x800D08A0,cameras);
    put(ram,object+0x108,1.0f);
    put(ram,object+0x11C,1.0f);
    sote_modern_camera_offset(ram.data(),object,stack);
    check(get<float>(ram,stack+0x8C)==1.2f &&
        get<float>(ram,cameras+6*128+0x70)==20,
        "shoulder eye offset precedes collision and target follows weapon");
    sote_modern_decode(ram.data(),object,stack);
    check(get<int16_t>(ram,stack+0x17E)==1 && get<int16_t>(ram,stack+0x170)==1,
        "simultaneous forward movement and strafe");
    check(get<int16_t>(ram,stack+0x178)==0 && get<int16_t>(ram,stack+0x17A)==0,
        "left stick cannot turn body");
    sote_modern_yaw(ram.data(),object);
    const auto live_tuning = sote::controls_menu::modern_controls_tuning().on_foot;
    check(std::fabs(get<float>(ram,object+0xA4)+
        shape_stick({.5f,.5f},live_tuning.aim_deadzone,live_tuning.aim_curve).x*live_tuning.yaw_speed)<.001f,
        "diagonal aim uses radial magnitude");
    check(sote_modern_aim(ram.data(),stack)==1 && get<float>(ram,stack+0x108)<0,
        "downward stick produces downward native pitch");
    check(get<float>(ram,stack+0x108)==get<float>(ram,stack+0xFC),
        "weapon and body aim use consistent pitch");
    publish({{}, {}, true});
    sote_modern_begin(ram.data(),object);
    sote_modern_yaw(ram.data(),object);
    check(get<float>(ram,object+0xA4)==0,"release stops yaw immediately");
    const float held_pitch = pitch_degrees();
    sote_modern_begin(ram.data(),object);
    check(pitch_degrees()==held_pitch,"neutral input does not recenter");
    add_mouse_delta(10, -5);
    sote_modern_begin(ram.data(), object);
    sote_modern_yaw(ram.data(), object);
    check(std::fabs(get<float>(ram, object + 0xA4) + 72.0f) < .01f,
        "raw mouse X turns through the Modern yaw hook without stick input");
    check(std::fabs(pitch_degrees() - (held_pitch + .6f)) < .01f,
        "raw mouse Y changes the same persistent Modern pitch as the stick");
    const float mouse_pitch = pitch_degrees();
    sote_modern_begin(ram.data(), object);
    sote_modern_yaw(ram.data(), object);
    check(get<float>(ram, object + 0xA4) == 0 && pitch_degrees() == mouse_pitch,
        "mouse deltas are consumed once and neutral motion holds pitch");
    add_mouse_delta(10, -5);
    const auto vehicle_mouse = consume_mouse_stick(.12f, false, 1.0/60.0);
    check(std::abs(vehicle_mouse.x-.4f)<.0001f && std::abs(vehicle_mouse.y-.2f)<.0001f,
        "vehicle mouse motion becomes native analog steering and pitch");
    const auto vehicle_neutral = consume_mouse_stick(.12f, false, 1.0/60.0);
    check(vehicle_neutral.x==0 && vehicle_neutral.y==0,
        "vehicle mouse deltas are consumed once without a lingering turn");
    const float before_suspend = pitch_degrees();
    publish({});
    sote_modern_begin(ram.data(), object);
    publish({{},{},true});
    sote_modern_begin(ram.data(), object);
    check(!sote_modern_take_camera_request() && pitch_degrees()==before_suspend,
        "menu/movie input suspension does not force shoulder camera or reset pitch");
    put(ram,object+0x74,uint32_t(0x400));
    publish({{},{.5f,-.5f},true});
    sote_modern_begin(ram.data(),object);
    sote_modern_yaw(ram.data(),object);
    check(aiming_active() && get<float>(ram,object+0xA4)<0,
        "live underwater flag retains right-stick aiming");
    publish({{},{},true});
    put(ram,object+0xB4,0.0f);
    sote_modern_begin(ram.data(),object);
    const auto before_death = ram;
    sote_modern_decode(ram.data(),object,stack);
    sote_modern_yaw(ram.data(),object);
    check(ram==before_death && !aiming_active(),"death suppresses hooks");
    check(pitch_degrees()==0,"death clears pitch before respawn");
    put(ram,object+0x74,uint32_t(0));
    put(ram,object+0xB4,100.0f);
    sote_modern_begin(ram.data(),object);
    check(sote_modern_take_camera_request()==1,"respawn reinitializes native camera");
    publish({});
    sote_modern_begin(ram.data(),object);
    check(!aiming_active() && pitch_degrees()==0,"disconnect clears aim");
    publish({{},{},true});
    sote_modern_begin(ram.data(),object);
    add_mouse_delta(0,20);
    sote_modern_begin(ram.data(),object);
    check(pitch_degrees()!=0,"recenter fixture starts tilted");
    request_recenter();
    sote_modern_begin(ram.data(),object);
    check(pitch_degrees()==0,"Center Camera levels pitch");
    request_quick_turn();
    float total_turn=0;
    for(int i=0;i<60;++i) {
        sote_modern_begin(ram.data(),object);
        sote_modern_yaw(ram.data(),object);
        total_turn+=get<float>(ram,object+0xA4)/60;
    }
    check(std::fabs(total_turn-180)<.01f,"quick turn integrates exactly 180 degrees");
    put<int16_t>(ram,0x8013CE0E,8);
    put<float>(ram,0x800E539C,0); put<uint32_t>(ram,0x800E5838,0);
    put<float>(ram,0x800E57B8,0);
    publish({{.7f,.7f},{.8f,-.5f},true});
    put<float>(ram,object+0xA4,123.0f);
    sote_modern_train_begin(ram.data(),object);
    check(sote_modern_take_camera_request()==0,"train keeps its native camera path");
    sote_modern_train_decode(ram.data(),object,stack);
    sote_modern_train_yaw(ram.data(),object);
    check(get<int16_t>(ram,stack+0x1BE)==1 && get<int16_t>(ram,stack+0x1B0)==1,
        "train translates diagonal movement to forward and strafe");
    check(get<int16_t>(ram,stack+0x1BA)==0 && get<int16_t>(ram,stack+0x1B8)==0,
        "train left stick does not add native turning");
    check(get<float>(ram,object+0xA8)<0 && get<float>(ram,object+0xA4)==123.0f,
        "train right stick changes angular rate without corrupting linear velocity");
    put<uint32_t>(ram,object+0x1B4,0x80123450);
    check(sote_modern_train_pose(ram.data(),object,stack)==1 &&
        get<float>(ram,stack+0xD4)>0 &&
        get<float>(ram,stack+0xD4)==get<float>(ram,stack+0xE0) &&
        get<uint32_t>(ram,object+0x1B4)==0x80123450,
        "train camera and weapon share pitch without overwriting actor pointer");
    put<float>(ram,0x800E57B8,1);
    sote_modern_train_begin(ram.data(),object);
    auto train_blocked=ram;
    sote_modern_train_decode(ram.data(),object,stack);
    sote_modern_train_yaw(ram.data(),object);
    check(sote_modern_train_pose(ram.data(),object,stack)==0,"train script gate suppresses pose");
    check(ram==train_blocked && !aiming_active(),"train script gate suppresses Modern input");
    put<float>(ram,0x800E57B8,0);
    publish({{1,0},{1,1},true});
    sote::controls_menu::cycle_scheme(SchemeSlot::OnFoot,1);
    const auto before_classic = ram;
    sote_modern_begin(ram.data(),object);
    sote_modern_decode(ram.data(),object,stack);
    sote_modern_yaw(ram.data(),object);
    check(sote_modern_train_pose(ram.data(),object,stack)==0,"Classic suppresses train pose");
    check(ram==before_classic && !aiming_active(),"Classic leaves guest state untouched");
    sote_modern_camera_offset(ram.data(),object,stack);
    check(get<float>(ram,cameras+6*128+0x70)==0,
        "Classic restores native camera target offset");
    {
        std::vector<uint8_t> memory(8*1024*1024);
        auto* data=memory.data();
        sote::cheats::State cheats;
        sote::cheats::Settings flags;
        put(memory,0x800DD2B0,uint32_t(2));
        put(memory,0x8013CE0E,int16_t(4));
        put(memory,0x80112B30,object);
        put(memory,object+0xB4,100.0f);
        put(memory,object+0xC0,50.0f);
        put(memory,0x800E1B54,int16_t(10));
        auto original=memory;
        cheats.apply(data,flags,1,true,0,false);
        check(memory==original,"disabled cheats leave memory unchanged");
        flags={true,true,true,true,true};
        cheats.apply(data,flags,1,true,0,false);
        put(memory,object+0xB4,25.0f);put(memory,object+0xC0,2.0f);
        put(memory,0x800E1B54,int16_t(1));put(memory,0x800E0EB0,uint32_t(1));
        cheats.apply(data,flags,1,true,0,false);
        check(get<float>(memory,object+0xB4)==100 && get<float>(memory,object+0xC0)==50 &&
            get<int16_t>(memory,0x800E1B54)==10 && get<uint32_t>(memory,0x800E0EB0)==3,
            "cheats replenish health, ammo, missiles and lives");
        put(memory,object+0xB4,0.0f);
        cheats.apply(data,flags,1,true,0,false);
        check(get<float>(memory,object+0xB4)==0,"lethal health permits native respawn");
        put(memory,object+0xB4,25.0f);
        cheats.apply(data,flags,2,false,0,false);
        check(get<float>(memory,object+0xB4)==25,"vehicle contexts do not write stale on-foot health");
        put(memory,0x8013CE0E,int16_t(3));put(memory,0x801EBC40,110.0f);
        cheats.apply(data,flags,0,false,0,false);
        put(memory,0x801EBC40,20.0f);
        cheats.apply(data,flags,0,false,0,false);
        check(get<float>(memory,0x801EBC40)==110,"Hoth vehicle health replenishes");
        put(memory,0x8013CE0E,int16_t(8));put(memory,0x801A7BA8,100.0f);
        cheats.apply(data,flags,3,true,0,false);put(memory,0x801A7BA8,10.0f);
        cheats.apply(data,flags,3,true,0,false);
        check(get<float>(memory,0x801A7BA8)==100,"Ord vehicle health replenishes");
        put(memory,0x8013CE0E,int16_t(10));put(memory,0x801A7BA8,20.0f);
        cheats.apply(data,flags,3,true,0,false);
        check(get<float>(memory,0x801A7BA8)==20,"Ord fixed address is never written in IG-88 stage");
        put(memory,object+0x240,uint32_t(40));
        cheats.apply(data,flags,2,false,object,false);
        check(get<uint32_t>(memory,object+0x240)==40,"stale Outrider pointer is ignored");
        cheats.apply(data,flags,2,false,object,true);
        check(get<uint32_t>(memory,object+0x240)==500,"live Outrider health replenishes");
        put(memory,0x800DD2B0,uint32_t(0));original=memory;
        cheats.apply(data,flags,2,false,object,true);
        check(memory==original,"non-gameplay state is untouched");
    }
    {
        sote::cheats::RapidFireGate gate;
        check(gate.fire(false,5,true,10),"rapid fire disabled preserves held fire");
        check(gate.fire(true,5,true,10),"rapid fire starts with a shot");
        check(!gate.fire(true,5,true,10.15),"rapid fire releases between pulses");
        check(gate.fire(true,5,true,10.21),"rapid fire repeats at configured rate");
        check(!gate.fire(true,5,false,10.22),"releasing fire stops rapid fire");
        check(gate.fire(true,5,true,10.23),"new press fires immediately");
        for(int rate=1;rate<=10;++rate) {
            gate={};int edges=0;bool before=false;
            for(int tick=0;tick<10000;++tick) {
                const bool firing=gate.fire(true,float(rate),true,tick/10000.0);
                edges+=firing&&!before; before=firing;
            }
            check(edges==rate,"rapid fire produces requested 1-10 pulse rate");
        }
    }
    std::cout << "Modern controls: " << failures << " failures\n";
    return failures ? 1 : 0;
}
