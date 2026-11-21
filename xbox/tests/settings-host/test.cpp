#include <cassert>
#include <vector>
#include "../../src/modern_platform.cpp"
static int movie_enabled;static unsigned movie_available;
extern "C" void xb_boss_movies_set_enabled(int enabled){movie_enabled=enabled;}
extern "C" uint32_t xb_boss_movies_available_mask(void){return movie_available;}
unsigned fake_ticks=1000;bool fail_write=false;bool active_foot=true;
extern "C" {volatile uint32_t sote_controls_status[6]{},sote_controls_masks[48]{},sote_bike_status[8]{},sote_outrider_status[3]{},sote_vi_count=100;}
namespace sote::modern_controls {
void publish(Input){}
bool aiming_active(){return active_foot;}
bool on_foot_active(){return active_foot;}
uint16_t map_buttons(uint16_t b){return b;}
}
template<class T> void disk(int slot,const T& r){FILE* f=std::fopen(paths[slot],"wb");assert(f);assert(std::fwrite(&r,1,sizeof(r),f)==sizeof(r));std::fclose(f);}
int main(){
 for(auto p:paths)std::remove(p);
 LegacyRecord v1{};v1.magic=0x53435431;v1.generation=7;std::memcpy(v1.values,defaults,sizeof(defaults));v1.values[0]=1;v1.values[2]=210;v1.checksum=hash_bytes(&v1,sizeof(v1)-4);disk(0,v1);
 initialize();assert(modern&&values[2]==210&&generation==7&&xb_hd_textures_preferred()==1);assert(xb_binding_get(0,12)==0xffff);
 assert(xb_pc_control_count()==10&&xb_cheat_count()==9);
 xb_pc_control_adjust(1,1);assert(extras[0]==1);xb_pc_control_adjust(3,1);assert(extras[1]==105);
 assert(xb_binding_set(0,12,2));assert(!xb_binding_set(0,13,2));assert(!xb_binding_set(0,3,8));assert(!xb_binding_set(0,14,0x1000));
 assert(xb_controls_save());Record saved{};assert(read_record(active,saved));assert(saved.magic==0x53435435&&saved.bindings[0][12]==2&&saved.extras[0]==1);
 int oldactive=active;fail_write=true;assert(!xb_controls_save());assert(active==oldactive&&read_record(active,saved));fail_write=false;
 Version2 v2{};v2.magic=0x53435432;v2.generation=50;v2.reserved=1;std::memcpy(v2.values,defaults,sizeof(defaults));v2.values[1]=1;v2.checksum=hash_bytes(&v2,sizeof(v2)-4);disk(0,v2);assert(read_record(0,saved));assert(saved.values[1]==1&&saved.bindings[0][12]==0xffff);
 v2.checksum^=1;disk(0,v2);assert(!read_record(0,saved));
 std::vector<uint8_t> ram(0x800000);using C=sote::cheats::State;
 C::write<uint16_t>(ram.data(),0x8013CE0E,4);C::write<uint32_t>(ram.data(),0x800DD2B0,2);C::write<uint32_t>(ram.data(),0x80112B30,0x80100000);
 extras[5]=extras[6]=1;cheat_state.cheat_write_float(ram.data(),0x801000b4,100);xb_cheats_tick(ram.data());cheat_state.cheat_write_float(ram.data(),0x801000b4,25);C::write<uint32_t>(ram.data(),0x800E0EB0,1);xb_cheats_tick(ram.data());assert(cheat_state.cheat_read_float(ram.data(),0x801000b4)==100);assert(C::read<uint32_t>(ram.data(),0x800E0EB0)==3);cheat_state.cheat_write_float(ram.data(),0x801000b4,0);xb_cheats_tick(ram.data());assert(cheat_state.cheat_read_float(ram.data(),0x801000b4)==0);
 modern=true;uint16_t buttons=0;float x=0,y=0;xb_controller_filter(ram.data(),&buttons,&x,&y,1,-1,0,1);assert(buttons==0x4000);buttons=0x4000;xb_controller_filter(ram.data(),&buttons,&x,&y,0,0,0,0);assert(buttons==4);
 extras[10]=1;assert(xb_rapid_fire(0x4000)==0x4000);fake_ticks+=150;assert(xb_rapid_fire(0x4000)==0);
 active_foot=false;C::write<uint16_t>(ram.data(),0x8013CE0E,17);sote_bike_status[0]=1;sote_bike_status[2]=100;sote_controls_status[1]=1;sote_controls_masks[32]=0x8000;sote_controls_masks[31]=0x4000;buttons=0;x=.8f;y=0;xb_controller_filter(ram.data(),&buttons,&x,&y,0,0,0,1);assert(buttons&0x8000);assert(x>0&&x<.8f&&y>0);

 xb_binding_reset_context(0);assert(xb_binding_get(0,12)==0xffff);
 assert(xb_binding_effective(0,12,0x4000)==0x2000);
 assert(xb_binding_effective(0,13,0x8000)==0x8000);
 assert(xb_binding_effective(3,31,0x4000)==0x40);
 assert(xb_controller_physical(0,1,0)==0x40);
 assert(xb_binding_set(0,12,0x40));assert(xb_binding_effective(0,12,0x4000)==0x40);
 sote_controls_status[2]=0;sote_controls_status[4]=17;sote_controls_masks[12]=0x4000;
 assert(xb_binding_buttons(ram.data(),0x40,0x40)==0x4000);
 sote_controls_status[1]=0;assert(xb_binding_buttons(ram.data(),0x40,0x40)==0);
 buttons=0x40;x=y=0;xb_controller_filter(ram.data(),&buttons,&x,&y,0,0,0,0);assert(!(buttons&0x40));
 assert(xb_controls_save());assert(read_record(active,saved)&&saved.bindings[0][12]==0x40);
 assert(!xb_pc_movies_preferred()&&!xb_pc_movies_available());xb_pc_movies_toggle();assert(xb_pc_movies_preferred()&&movie_enabled&&!xb_pc_movies_available());
 assert(xb_controls_save());assert(read_record(active,saved)&&saved.pc_movies==1);
 initialized=false;pc_movies_preference=0;movie_enabled=0;initialize();assert(xb_pc_movies_preferred()&&movie_enabled);
 Version3 v3{};v3.magic=0x53435433;v3.generation=99;std::memcpy(v3.values,defaults,sizeof(defaults));std::memcpy(v3.extras,extra_defaults,sizeof(extra_defaults));std::memset(v3.bindings,0xff,sizeof(v3.bindings));v3.checksum=hash_bytes(&v3,sizeof(v3)-4);disk(0,v3);assert(read_record(0,saved)&&saved.pc_movies==0&&saved.magic==0x53435435);
 // Actual v4 layout: every previous field survives, including PC movies and bindings.
 Version4 v4{};v4.magic=0x53435434;v4.generation=310;std::memcpy(v4.values,defaults,sizeof(defaults));v4.values[2]=225;
 std::memcpy(v4.extras,extra_defaults,sizeof(extra_defaults));v4.extras[5]=v4.extras[6]=1;
 std::memset(v4.bindings,0xff,sizeof(v4.bindings));v4.bindings[2][12]=0x40;v4.pc_movies=1;
 v4.checksum=hash_bytes(&v4,sizeof(v4)-4);disk(0,v4);
 initialized=false;active=-1;hd_preference=1;generation=0;initialize();
 assert(active==0&&generation==310&&hd_preference==1&&pc_movies_preference==1&&values[2]==225&&extras[5]&&extras[6]&&binding_overrides[2][12]==0x40);
 xb_hd_textures_toggle();assert(!xb_hd_textures_preferred()&&sote_hd_setting[0]==0);
 fail_write=true;assert(!xb_controls_save()&&active==0&&generation==310);fail_write=false;
 assert(xb_controls_save()&&active==1&&generation==311);assert(read_record(1,saved)&&saved.hd_textures==0);
 assert(!std::memcmp(saved.values,v4.values,sizeof(v4.values))&&!std::memcmp(saved.extras,v4.extras,sizeof(v4.extras))&&
        !std::memcmp(saved.bindings,v4.bindings,sizeof(v4.bindings))&&saved.pc_movies==1);
 initialized=false;active=-1;hd_preference=1;initialize();assert(active==1&&!xb_hd_textures_preferred()); // OFF survives remount
 xb_hd_textures_toggle();assert(xb_controls_save()&&active==0&&generation==312&&read_record(0,saved)&&saved.hd_textures==1);
 initialized=false;active=-1;hd_preference=0;initialize();assert(active==0&&xb_hd_textures_preferred()); // ON survives remount
 saved.hd_textures=2;saved.checksum=hash(saved);disk(0,saved);assert(!read_record(0,saved)); // checksum-valid invalid flag
 initialized=false;active=-1;hd_preference=1;initialize();assert(active==1&&generation==311&&!xb_hd_textures_preferred()); // corrupt newest falls back
 v4.pc_movies=2;v4.checksum=hash_bytes(&v4,sizeof(v4)-4);disk(0,v4);assert(!read_record(0,saved));
 for(auto p:paths)std::remove(p);
 std::puts("PASS original controls/cheats/bindings/movies, v1-v4 migration, v5 ON/OFF journal in both directions, failed writes and corrupt-newest fallback");
}
