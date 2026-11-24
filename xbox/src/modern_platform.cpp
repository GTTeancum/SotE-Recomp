#include "modern_platform.hpp"
#include "hardware_test.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>
#include "cheats.hpp"
#include "rapid_fire.hpp"
#include "menu_settings.h"
#include "boss_movie_queue.h"
#include <nxdk/mount.h>
#include <xboxkrnl/xboxkrnl.h>
extern "C" {volatile uint32_t sote_hd_setting[4]={1,0,0,0};} // preferred, journal generation, UI toggles, verified commits this boot
namespace {
bool initialized;
bool modern;
sote::controls_menu::ModernControlsTuning tuning;
// Small two-copy settings journal, separate from the original game EEPROM.
// Never replace the last valid copy before the new copy has been verified.
struct LegacyRecord { unsigned magic, generation; int values[12]; unsigned checksum; };
// v2 reserved slot retains compatibility with the short-lived font preference.
// Read 0/1 and ignore it; migrate all twelve controls into the current record.
struct Version2 { unsigned magic, generation; int values[12]; unsigned reserved, checksum; };
struct Version3 { unsigned magic, generation; int values[12]; int extras[12]; uint16_t bindings[5][48]; unsigned checksum; };
struct Version4 { unsigned magic, generation; int values[12]; int extras[12]; uint16_t bindings[5][48]; unsigned pc_movies, checksum; };
struct Record { unsigned magic, generation; int values[12]; int extras[12]; uint16_t bindings[5][48]; unsigned pc_movies, hd_textures, checksum; };
static_assert(sizeof(Version4)==592 && sizeof(Record)==596, "Settings wire layout changed");
unsigned hd_preference=1; // fresh/legacy journals default ON when a pack is installed
unsigned pc_movies_preference;
uint16_t binding_overrides[5][48];
unsigned generation; int active=-1;
const char* paths[]={"E:\\UDATA\\534f5445\\controls0.bin","E:\\UDATA\\534f5445\\controls1.bin"};
int values[12]={0,0,180,90,18,18,17,1,1500,0,2,150};
// Bike, move sensitivity, aim sensitivity, trigger deadzone/sensitivity, six cheats, fire rate.
const int extra_defaults[12]={0,100,100,24,100,0,0,0,0,0,0,4};
int extras[12]={0,100,100,24,100,0,0,0,0,0,0,4};
const int extra_low[12]={0,10,10,0,10,0,0,0,0,0,0,1};
const int extra_high[12]={1,300,300,95,300,1,1,1,1,1,1,10};
const int defaults[12]={0,0,180,90,18,18,17,1,1500,0,2,150};
const int low[12]={0,0,30,15,0,0,10,0,100,0,1,10};
const int high[12]={1,1,540,270,95,95,30,1,3000,100,10,500};
const int step[12]={1,1,15,15,1,1,1,1,100,5,1,10};
unsigned hash_bytes(const void* p,unsigned size) {unsigned h=2166136261u;const unsigned char* b=(const unsigned char*)p;for(unsigned i=0;i<size;i++)h=(h^b[i])*16777619u;return h;}
unsigned hash(const Record& r){return hash_bytes(&r,sizeof(r)-4);}
bool read_record(int slot,Record& r) {
    FILE* f=std::fopen(paths[slot],"rb");if(!f)return false;
    unsigned size=std::fread(&r,1,sizeof(r),f);int trailing=std::fgetc(f);std::fclose(f);
    if(trailing!=EOF)return false;
    if(size==sizeof(LegacyRecord)&&r.magic==0x53435431) {
        LegacyRecord old{};std::memcpy(&old,&r,sizeof(old));
        if(old.checksum!=hash_bytes(&old,sizeof(old)-4))return false;
        std::memcpy(r.extras,extra_defaults,sizeof(extra_defaults));std::memset(r.bindings,0xff,sizeof(r.bindings));r.pc_movies=0;r.hd_textures=1;r.magic=0x53435435;r.checksum=hash(r);
    } else if(size==sizeof(Version2)&&r.magic==0x53435432) {
        Version2 old{};std::memcpy(&old,&r,sizeof(old));
        if(old.reserved>1||old.checksum!=hash_bytes(&old,sizeof(old)-4))return false;
        std::memcpy(r.extras,extra_defaults,sizeof(extra_defaults));std::memset(r.bindings,0xff,sizeof(r.bindings));r.pc_movies=0;r.hd_textures=1;r.magic=0x53435435;r.checksum=hash(r);
    } else if(size==sizeof(Version3)&&r.magic==0x53435433) {
        Version3 old{};std::memcpy(&old,&r,sizeof(old));
        if(old.checksum!=hash_bytes(&old,sizeof(old)-4))return false;
        r.pc_movies=0;r.hd_textures=1;r.magic=0x53435435;r.checksum=hash(r);
    } else if(size==sizeof(Version4)&&r.magic==0x53435434) {
        Version4 old{};std::memcpy(&old,&r,sizeof(old));
        if(old.pc_movies>1||old.checksum!=hash_bytes(&old,sizeof(old)-4))return false;
        r.hd_textures=1;r.magic=0x53435435;r.checksum=hash(r);
    } else if(size!=sizeof(r)||r.magic!=0x53435435||r.pc_movies>1||r.hd_textures>1||r.checksum!=hash(r))return false;
    for(int i=0;i<12;i++)if(r.values[i]<low[i]||r.values[i]>high[i]||
        r.extras[i]<extra_low[i]||r.extras[i]>extra_high[i])return false;
    for(auto& group:r.bindings)for(unsigned mask:group)if(mask!=0xffff && (mask&~0xff7fU))return false;
    return true;
}
void apply() {
xb_boss_movies_set_enabled(pc_movies_preference!=0);
modern=values[0]!=0;auto& t=tuning.on_foot;t.invert_y=values[1]!=0;t.yaw_speed=values[2];t.pitch_speed=values[3];t.movement_deadzone=values[4]/100.f;t.aim_deadzone=values[5]/100.f;t.aim_curve=values[6]/10.f;t.convergence=values[7]!=0;t.convergence_range=values[8];t.magnetism_strength=values[9]/100.f;t.magnetism_cone_degrees=values[10];t.magnetism_range=values[11];
}
void initialize() {
    if(initialized)return;
    initialized=true;std::memset(binding_overrides,0xff,sizeof(binding_overrides));
    // Disc configuration supplies first-run defaults; saved UI settings win.
    // Flight retains its original controller regardless of this on-foot setting.
    FILE* file=std::fopen("D:\\xbox-controls.cfg","rb");
    if(file){char line[128];while(std::fgets(line,sizeof(line),file))
        if(std::strncmp(line,"on_foot=modern",14)==0)values[0]=1;std::fclose(file);}
    if(xb_hardware_test) {
        paths[0]="D:\\controls0.bin";
        paths[1]="D:\\controls1.bin";
    } else {
        if(!nxIsDriveMounted('E'))nxMountDrive('E',"\\Device\\Harddisk0\\Partition1\\");
        CreateDirectoryA("E:\\UDATA",nullptr);CreateDirectoryA("E:\\UDATA\\534f5445",nullptr);
    }
    Record a{},b{};bool av=read_record(0,a),bv=read_record(1,b);
    if(av||bv){active=bv&&(!av||(int)(b.generation-a.generation)>0)?1:0;const Record& r=active?b:a;generation=r.generation;std::memcpy(values,r.values,sizeof(values));std::memcpy(extras,r.extras,sizeof(extras));std::memcpy(binding_overrides,r.bindings,sizeof(binding_overrides));pc_movies_preference=r.pc_movies;hd_preference=r.hd_textures;}
    sote_hd_setting[0]=hd_preference;sote_hd_setting[1]=generation;apply();
}
}
namespace sote::controls_menu {
ControlScheme current_scheme(SchemeSlot slot) {
    initialize();return ((slot==SchemeSlot::OnFoot && modern)||(slot==SchemeSlot::Bike&&extras[0]))?ControlScheme::Modern:ControlScheme::Classic;
}
ModernControlsTuning modern_controls_tuning(){initialize();return tuning;}
}
extern "C" void xb_modern_publish(int connected,float move_x,float move_y,float look_x,float look_y) {
    sote::modern_controls::publish({{move_x,move_y},{look_x,look_y},connected!=0});
}
extern "C" unsigned xb_modern_buttons(unsigned buttons) {
    return sote::modern_controls::aiming_active()?sote::modern_controls::map_buttons(buttons):buttons;
}

extern "C" int xb_hd_textures_preferred(void){initialize();return hd_preference!=0;}
extern "C" void xb_hd_textures_toggle(void){initialize();hd_preference=!hd_preference;sote_hd_setting[0]=hd_preference;sote_hd_setting[2]++;}
extern "C" int xb_pc_movies_preferred(void){initialize();return pc_movies_preference!=0;}
extern "C" void xb_pc_movies_toggle(void){initialize();pc_movies_preference=!pc_movies_preference;xb_boss_movies_set_enabled(pc_movies_preference!=0);}
extern "C" int xb_pc_movies_available(void){initialize();return xb_boss_movies_available_mask()!=0;}
extern "C" int xb_controls_value(int row) {initialize();return row>=0&&row<12?values[row]:0;}
extern "C" void xb_controls_change(int row,int direction) {initialize();if(row==12){std::memcpy(values,defaults,sizeof(values));apply();return;}if(row<0||row>=12)return;int v=values[row]+direction*step[row];if(high[row]==1)v=!values[row];if(v<low[row])v=low[row];if(v>high[row])v=high[row];values[row]=v;apply();}
extern "C" int xb_controls_save() {
    initialize();Record r{};r.magic=0x53435435;r.pc_movies=pc_movies_preference;r.hd_textures=hd_preference;r.generation=generation+1;std::memcpy(r.values,values,sizeof(values));std::memcpy(r.extras,extras,sizeof(extras));std::memcpy(r.bindings,binding_overrides,sizeof(binding_overrides));r.checksum=hash(r);int slot=active==0?1:0;
    HANDLE f=CreateFileA(paths[slot],GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)return 0;
    DWORD n=0;IO_STATUS_BLOCK io;bool ok=WriteFile(f,&r,sizeof(r),&n,nullptr)&&n==sizeof(r);if(ok)ok=NT_SUCCESS(NtFlushBuffersFile(f,&io));if(!CloseHandle(f))ok=false;
    Record check{};if(!ok||!read_record(slot,check)||std::memcmp(&r,&check,sizeof(r)))return 0;active=slot;generation=r.generation;sote_hd_setting[1]=generation;sote_hd_setting[3]++;return 1;
}

namespace {
const char* control_labels[]={"On Foot","Speeder Bike","Move Deadzone","Move Sens","Aim Deadzone","Aim Sens","Trigger Deadzone","Trigger Sens","Yaw Speed","Pitch Speed"};
const char* control_help[]={"Choose Modern aiming or Classic controls on foot.","Modern: RT accelerates, LT brakes; left stick steers.","Ignore small movement-stick input to prevent drift.","Scale movement-stick input.","Ignore small aiming-stick input to prevent drift.","Scale the right-stick activation threshold.","Choose how far a trigger moves before responding.","Scale trigger input after its deadzone.","Set Modern horizontal aiming speed.","Set Modern vertical aiming speed."};
const char* cheat_labels[]={"Infinite Health","Infinite Lives","Infinite Ammo","Infinite Missiles","Vehicle Health","Rapid Fire","Fire Rate","Aim Assist","Assist Width"};
const char* cheat_help[]={"Restore nonlethal damage; lethal damage still allows respawn.","Maintain at least three spare lives.","Keep acquired ammunition from decreasing.","Keep acquired vehicle missiles from decreasing.","Replenish Hoth, train and Outrider health after nonlethal damage.","Pulse held fire in Modern on-foot controls.","Fire pulses per second; weapon cooldowns still apply.","Adjust how strongly Modern aim follows nearby targets.","Adjust the angle in which aim assistance finds targets."};
int* control_value(int row) {
    const int legacy[]={0,-1,4,-1,5,-1,-1,-1,2,3};
    const int added[]={-1,0,-1,1,-1,2,3,4,-1,-1};
    if(row<0||row>=10)return nullptr;
    return legacy[row]>=0?&values[legacy[row]]:&extras[added[row]];
}
float axis(float v,float deadzone,float sensitivity) {
    float magnitude=std::fabs(v);if(magnitude<=deadzone)return 0;
    float shaped=std::min(1.f,(magnitude-deadzone)/(1.f-deadzone)*sensitivity);
    return v<0?-shaped:shaped;
}
sote::cheats::State cheat_state;
sote::cheats::RapidFireGate rapid_gate;
float bike_steer,bike_throttle,bike_phase,bike_brake_phase;
}
extern "C" int xb_pc_control_count(){return 10;}
extern "C" const char* xb_pc_control_label(int row){return row>=0&&row<10?control_labels[row]:"";}
extern "C" const char* xb_pc_control_help(int row){return row>=0&&row<10?control_help[row]:"";}
extern "C" const char* xb_pc_control_value(int row){
    initialize();static char text[10][64];if(row<0||row>=10)return "";
    const int v=*control_value(row);if(row<2)return v?"Modern":"Classic";
    const int lo=row==8?30:row==9?15:(row==3||row==5||row==7)?10:0;
    const int hi=row==8?540:row==9?270:(row==3||row==5||row==7)?300:95;
    char slider[13]="[----------]";int tick=(v-lo)*9/(hi-lo);slider[1+std::clamp(tick,0,9)]='|';
    if(row>=8)std::snprintf(text[row],64,"%s %d",slider,v);
    else std::snprintf(text[row],64,"%s %.2f",slider,v/100.0);
    return text[row];
}
extern "C" void xb_pc_control_adjust(int row,int direction){
    initialize();if(row<0||row>=10||!direction)return;int& v=*control_value(row);
    if(row<2)v=!v;else {const int lo=row==8?30:row==9?15:(row==3||row==5||row==7)?10:0;
      const int hi=row==8?540:row==9?270:(row==3||row==5||row==7)?300:95;
      const int step=row==8?10:row==9?5:(row==3||row==5||row==7)?5:2;v=std::clamp(v+(direction<0?-step:step),lo,hi);}
    apply();
}
extern "C" int xb_cheat_count(){return 9;}
extern "C" const char* xb_cheat_label(int row){return row>=0&&row<9?cheat_labels[row]:"";}
extern "C" const char* xb_cheat_help(int row){return row>=0&&row<9?cheat_help[row]:"";}
extern "C" const char* xb_cheat_value(int row){
    initialize();static char text[3][32];if(row<0||row>=9)return "";
    if(row<6)return extras[5+row]?"On":"Off";
    if(row==6)std::snprintf(text[0],32,"%d per sec%s",extras[11],extras[11]==4?" (Default)":"");
    if(row==7){if(!values[9])return "Off";std::snprintf(text[1],32,"%d / 10",values[9]/10);}
    if(row==8)std::snprintf(text[2],32,"%d deg",values[10]);return text[row-6];
}
extern "C" void xb_cheat_adjust(int row,int direction){
    initialize();if(row<0||row>=9||!direction)return;
    if(row<6)extras[5+row]=!extras[5+row];
    else if(row==6)extras[11]=std::clamp(extras[11]+(direction<0?-1:1),1,10);
    else if(row==7)values[9]=std::clamp(values[9]+(direction<0?-10:10),0,100);
    else values[10]=std::clamp(values[10]+(direction<0?-1:1),1,8);apply();
}
extern "C" {
extern volatile uint32_t sote_controls_status[6],sote_controls_masks[48];
extern volatile uint32_t sote_bike_status[8],sote_outrider_status[3],sote_vi_count;
}
// Directly reads the same word-swapped guest fields as the PC backend. It never
// allocates in guest memory. Reuse the tested PC cheat routines without forks.
extern "C" void xb_cheats_tick(uint8_t* rdram){
    initialize();const auto event=sote::cheats::State::read<uint16_t>(rdram,0x8013CE0E);
    const bool onfoot=event!=0&&event!=2&&event!=3&&event!=6&&event!=8&&event!=17&&event!=29&&event!=30;
    const int level=event==3?0:event==8?3:event==6?2:(event==29||event==30)?9:-1;
    sote::cheats::Settings s{};s.infinite_health=extras[5];s.infinite_lives=extras[6];s.infinite_ammo=extras[7];s.infinite_missiles=extras[8];s.infinite_vehicle_health=extras[9];
    cheat_state.apply(rdram,s,level,onfoot,sote_outrider_status[0],sote_outrider_status[2]&&sote_vi_count-sote_outrider_status[1]<=12);
}
extern "C" void xb_controller_filter(uint8_t* rdram,uint16_t* buttons,float* mx,float* my,float rx,float ry,float lt,float rt){
    initialize();*buttons&=uint16_t(~0x40U);const unsigned event=sote::cheats::State::read<uint16_t>(rdram,0x8013CE0E);
    float x=axis(*mx,values[4]/100.f,extras[1]/100.f),y=axis(*my,values[4]/100.f,extras[1]/100.f);
    lt=std::max(0.f,axis(lt,extras[3]/100.f,extras[4]/100.f));rt=std::max(0.f,axis(rt,extras[3]/100.f,extras[4]/100.f));
    // Match the PC Modern pad aliases, not the Classic canonical mapping.
    // Right-stick look is analogue only in Modern: never synthesize C actions.
    const bool foot_modern=modern&&sote::modern_controls::on_foot_active();
    const unsigned physical=*buttons;
    if(foot_modern) {
        *buttons=uint16_t(physical&~0xE03fU);
        if(physical&0x8000)*buttons|=0x8000;
        if(physical&0x4000)*buttons|=0x0004;
        if(physical&0x0002)*buttons|=0x0010;
        if(physical&0x0008)*buttons|=0x0002;
        if(physical&0x0030)*buttons|=0x0008;
        if(physical&0x0800)*buttons|=0x0020;
        if(rt>0)*buttons|=0x4000;
    } else {
        if(rt>0)*buttons|=0x2000;
        const float aim_threshold=std::min(.999f,values[5]/float(extras[2]));
        if(rx>aim_threshold)*buttons|=1;if(rx<-aim_threshold)*buttons|=2;
        if(ry>aim_threshold)*buttons|=4;if(ry<-aim_threshold)*buttons|=8;
    }
    if(extras[0]&&event==17&&sote_bike_status[0]&&sote_vi_count-sote_bike_status[2]<=12){
        // PC Modern bike: nonlinear steering + low-pass, duty-cycled real
        // Accelerate/Brakes action bits selected by the active native preset.
        *buttons&=uint16_t(~(0x8000|0x4000|0x2000));
        float shaped=std::copysign(std::pow(std::fabs(x),1.6f),x)*std::max(.35f,1.f-rt*.45f);
        bike_steer=bike_steer*.18f+shaped*.82f;
        bike_throttle=bike_throttle*.25f+(rt-lt)*.75f;x=bike_steer;y=bike_throttle;
        bike_phase+=rt;bike_brake_phase+=lt;
        unsigned accel=sote_controls_status[1]?sote_controls_masks[32]:0x8000;
        unsigned brake=sote_controls_status[1]?sote_controls_masks[31]:0x4000;
        if(bike_phase>=1){bike_phase-=1;*buttons|=accel?accel:0x8000;}
        if(bike_brake_phase>=1){bike_brake_phase-=1;*buttons|=brake?brake:0x4000;}
    } else bike_steer=bike_throttle=bike_phase=bike_brake_phase=0;
    *mx=x;*my=y;
}
extern "C" unsigned xb_rapid_fire(unsigned buttons){
    initialize();const bool active=modern&&sote::modern_controls::aiming_active();
    // map_buttons has already translated canonical fire to the native action.
    const unsigned fire=sote::modern_controls::map_buttons(0x4000);
    if(fire&&!rapid_gate.fire(active&&extras[10],float(extras[11]),(buttons&fire)!=0,GetTickCount()/1000.0))buttons&=~fire;
    return buttons;
}
extern "C" unsigned xb_binding_get(int context,int action){
    initialize();return context>=0&&context<5&&action>=0&&action<48?binding_overrides[context][action]:0xffff;
}
extern "C" int xb_binding_set(int context,int action,unsigned mask){
    initialize();if(context<0||context>=5||action<0||action>=48||
        (mask!=0xffff&&(mask&~0xff7fU)))return 0;
    // Pause/navigation stays bound to Start; do not permit trapping the user.
    if(action==3 || (mask!=0xffff&&(mask&0x1000)))return 0;
    if(mask&&mask!=0xffff)for(int i=0;i<48;i++)
        if(i!=action&&binding_overrides[context][i]!=0xffff&&(binding_overrides[context][i]&mask))return 0;
    binding_overrides[context][action]=uint16_t(mask);return 1;
}
extern "C" void xb_binding_reset_context(int context){
    initialize();if(context>=0&&context<5)std::memset(binding_overrides[context],0xff,sizeof(binding_overrides[context]));
}
extern "C" unsigned xb_binding_buttons(uint8_t* rdram,unsigned physical,unsigned translated){
    initialize();const unsigned event=sote::cheats::State::read<uint16_t>(rdram,0x8013CE0E);
    if(!sote_controls_status[1]||sote_controls_status[2]>=5||sote_controls_status[4]!=event)return translated&~0x40U;
    const unsigned context=sote_controls_status[2];
    unsigned removed=0,added=0;
    for(unsigned i=0;i<48;i++) {
        unsigned source=binding_overrides[context][i];if(source==0xffff||i==3)continue;
        unsigned target=sote_controls_masks[i];if(!target)continue;
        removed|=target;
        if(source) {
            unsigned canonical=source&~0x40U;
            if(modern&&context==0) {
                canonical=source&~0xE07fU;
                if(source&0x8000)canonical|=0x8000;
                if(source&0x4000)canonical|=4;
                if(source&2)canonical|=0x10;
                if(source&8)canonical|=2;
                if(source&0x30)canonical|=8;
                if(source&0x800)canonical|=0x20;
                if(source&0x2000)canonical|=0x4000;
            }
            removed|=modern&&context==0?sote::modern_controls::map_buttons(canonical):canonical;
            if(physical&source)added|=target;
        }
    }
    return ((translated&~removed)|added)&~0x40U;
}

extern "C" unsigned xb_controller_physical(unsigned buttons,float lt,float rt){
    initialize();return buttons|(axis(rt,extras[3]/100.f,extras[4]/100.f)>0?0x2000:0)|(axis(lt,extras[3]/100.f,extras[4]/100.f)>0?0x40:0);
}

extern "C" unsigned xb_binding_effective(int context,int action,unsigned native_mask){
    initialize();if(context<0||context>=5||action<0||action>=48)return 0;
    unsigned override=binding_overrides[context][action];if(override!=0xffff)return override;
    if(context==0&&modern){
        // Native action indices are the production modern_controls binding_offsets / 2.
        switch(action){
        case 12:return 0x2000; // Fire: RT
        case 13:return 0x8000; // Jump: A
        case 18:return 0x0002; // X
        case 19:return 0x4000; // B
        case 20:return 0x0008; // Y
        case 22:return 0x0030; // either shoulder
        case 23:return 0x0800; // D-pad Up
        default:return native_mask&0x1f00U; // only untouched Start/D-pad bits
        }
    }
    if(context==3&&extras[0]){
        if(action==32)return 0x2000; // Accelerate: RT
        if(action==31)return 0x0040; // Brake: LT (host-only)
        return native_mask&~0xe000U;
    }
    return native_mask&~0x40U;
}
