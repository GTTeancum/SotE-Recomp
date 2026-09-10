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
    put(ram,0x800D252C,int16_t(6));
    const uint32_t table = 0x800E6808 + 6*96;
    put(ram,table+0x1A,uint16_t(0x4000));
    put(ram,table+0x18,uint16_t(0x8000));
    put(ram,table+0x24,uint16_t(0x8));
    double dt = 1.0/60;
    uint64_t bits;
    std::memcpy(&bits,&dt,8);
    put(ram,0x8018E998,uint32_t(bits>>32));
    put(ram,0x8018E99C,uint32_t(bits));
    publish({{.5f,.5f},{.5f,.5f},true});
    sote_modern_begin(ram.data(),object);
    check(map_buttons(0x8000)==0x4000 && map_buttons(0x4000)==0x8000 &&
        map_buttons(0x10)==0x8 && map_buttons(0x1000)==0x1000,
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
    check(std::fabs(get<float>(ram,object+0xA4)+
        shape_stick({.5f,.5f},.12f,1.7f).x*180)<.001f,
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
    put(ram,object+0x74,uint32_t(0x400));
    sote_modern_begin(ram.data(),object);
    const auto before_death = ram;
    sote_modern_decode(ram.data(),object,stack);
    sote_modern_yaw(ram.data(),object);
    check(ram==before_death && !aiming_active(),"death suppresses hooks");
    check(pitch_degrees()==0,"death clears pitch before respawn");
    put(ram,object+0x74,uint32_t(0));
    sote_modern_begin(ram.data(),object);
    check(sote_modern_take_camera_request()==1,"respawn reinitializes native camera");
    publish({});
    sote_modern_begin(ram.data(),object);
    check(!aiming_active() && pitch_degrees()==0,"disconnect clears aim");
    publish({{1,0},{1,1},true});
    sote::controls_menu::cycle_scheme(SchemeSlot::OnFoot,1);
    const auto before_classic = ram;
    sote_modern_begin(ram.data(),object);
    sote_modern_decode(ram.data(),object,stack);
    sote_modern_yaw(ram.data(),object);
    check(ram==before_classic && !aiming_active(),"Classic leaves guest state untouched");
    sote_modern_camera_offset(ram.data(),object,stack);
    check(get<float>(ram,cameras+6*128+0x70)==0,
        "Classic restores native camera target offset");
    std::cout << "Modern controls: " << failures << " failures\n";
    return failures ? 1 : 0;
}
