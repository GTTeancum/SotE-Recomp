// Executes the real generated music dispatcher/updater/allocator in isolated RDRAM.
// No ROM or game process is used by this test; the runner extracts the source bodies.
#include "recomp.h"
#include "recomp_hooks.h"
#include "hd_music.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" {
void func_80006CB0(uint8_t*,recomp_context*);
void func_80007088(uint8_t*,recomp_context*);
void func_800073B4(uint8_t*,recomp_context*);
void func_80007340(uint8_t*,recomp_context*);
void func_80006668(uint8_t*,recomp_context*);
}
namespace hd=sote::hd_music;
namespace fs=std::filesystem;
static std::vector<uint8_t> ram(8*1024*1024);
static recomp_context ctx{};
static unsigned passed=0;
static void check(bool value,const std::string& message) {
 if(!value) throw std::runtime_error(message);
 ++passed;
}
static int32_t& w(uint32_t addr){return *reinterpret_cast<int32_t*>(&ram[addr&0x1fffffffU]);}
static void f(uint32_t addr,float value){std::memcpy(&w(addr),&value,4);}
static void clear_slots() {
 std::memset(ram.data()+0x110ee8,0,0x200);
 for(unsigned i=0;i<8;i++)for(uint32_t base:{0x80110ee8U,0x80110fe8U}) {w(base+32*i)=-1;w(base+32*i+4)=-1;}
}
static void reset(const fs::path& root) {
 sote_music_reset(ram.data());std::fill(ram.begin(),ram.end(),0);ctx={};ctx.r29=0xffffffff807ff000ULL;
 clear_slots();f(0x800d0764,1.0f);f(0x800d2528,1.0f);f(0x800d2524,1.0f);
 w(0x8018e9b0)=42;w(0x800d075c)=-1;hd::initialize(root);
}
static void command(const std::string& text) {
 constexpr uint32_t addr=0x80200000;
 for(size_t i=0;i<=text.size();i++)ram[((addr+i)&0x1fffffffU)^3U]=i==text.size()?0:static_cast<uint8_t>(text[i]);
 ctx.r4=static_cast<int32_t>(addr);func_80006CB0(ram.data(),&ctx);
}
static void frame() {
 const auto sp=ctx.r29;func_80007088(ram.data(),&ctx);
 check(ctx.r29==sp,"native updater restores guest stack");++w(0x8018e9b0);
}
static int slot_for(int sound) {
 for(int i=0;i<8;i++)if(w(0x80110fe8+32*i)!=-1 && w(0x80110fe8+32*i+4)==sound)return i;
 return -1;
}
static void request(int sound,int volume,bool effect=false,int continuous=0) {
 ctx.r4=sound;ctx.r5=7;ctx.r6=0x3e800000;ctx.r7=volume;w(static_cast<uint32_t>(ctx.r29)+0x10)=continuous;
 const auto sp=ctx.r29;
 (effect?func_80007340:func_800073B4)(ram.data(),&ctx);
 check(ctx.r29==sp,"native request restores guest stack");
}
static std::vector<int16_t> mix(size_t frames=500,int16_t native=0,uint32_t rate=22050) {
 std::vector<int16_t> pcm(frames*2,native);hd::mix_into(pcm.data(),pcm.size(),rate);return pcm;
}
static void set_disable(bool disabled) {
#ifdef _WIN32
 _putenv_s("SOTE_DISABLE_HD_MUSIC",disabled?"1":"");
#else
 if(disabled)setenv("SOTE_DISABLE_HD_MUSIC","1",1);else unsetenv("SOTE_DISABLE_HD_MUSIC");
#endif
}
struct Cue {const char* name;int sound;int volume;const char* slot;const char* file;};
static constexpr Cue cues[]={
 {"Main Menu",0x62,31000,"main_menu","main_menu.ogg"},
 {"Theme",0x0c,31000,"title_theme","Track02.ogg"},
 {"1. Battle of Hoth",0x0d,28000,"battle_of_hoth","Track03.ogg"},
 {"2a. Hoth Base",0x35,25000,"escape_from_echo_base","Track04.ogg"},
 {"2b. Hoth Base",0x35,25000,"escape_from_echo_base","Track04.ogg"},
 {"3. Asteroid",0x33,32000,"asteroid_field","Track06.ogg"},
 {"4a. Ord Mantell",0x36,28000,"ord_mantell_junkyard","Track07.ogg"},
 {"4b. Boss",0x37,20000,"ord_mantell_boss","Track12.ogg"},
 {"6a. Gall",0x38,25000,"gall_spaceport","Track13.ogg"},
 {"6b. Gall",0x38,25000,"gall_spaceport","Track13.ogg"},
 {"7. Speeder",0x7c,28000,"mos_eisley_beggars_canyon","Track08.ogg"},
 {"9a. Freighter",0x35,25000,"imperial_freighter","Track04.ogg"},
 {"9b. Freighter",0x35,25000,"imperial_freighter","Track04.ogg"},
 {"9c. Freighter",0x35,25000,"imperial_freighter","Track04.ogg"},
 {"10. Sewers",0x34,21000,"sewers_of_imperial_city","Track10.ogg"},
 {"11a. Palace",0x7d,25000,"xizors_palace","Track11.ogg"},
 {"11b. Palace",0x7d,25000,"xizors_palace","Track11.ogg"},
 {"12. Skyhook Chase",0x33,25000,"skyhook_station_chase","Track06.ogg"},
 {"13. Skyhook",0x7e,31000,"skyhook_battle","Track14.ogg"},
 {"Boss",0x37,25000,"boss_battle","Track12.ogg"},
};
int main(int argc,char** argv)try{
 if(argc<3)throw std::runtime_error("usage: verify_native_music fixture_root real_project_root");
 const fs::path root=fs::absolute(argv[1]),real=fs::absolute(argv[2]);fs::current_path(root);
 set_disable(false);
 // Verify EVERY selector using the original generated dispatcher, not a mirror of its switch.
 reset(root);
 for(const auto& cue:cues){
  sote_music_reset(ram.data());clear_slots();command(cue.name);
  check(w(0x800d075c)==cue.sound,std::string(cue.name)+": native sound ID");
  check(w(0x800d0760)==cue.volume,std::string(cue.name)+": native base volume");
  check(!hd::status().background_active,"command alone must not start playback");frame();
  auto st=hd::status();
  check(st.background_active,std::string(cue.name)+": actual native request starts external file");
  check(st.cue==cue.slot && st.file==cue.file,std::string(cue.name)+": selected file");
  check(slot_for(cue.sound)<0,"native background sound never enters desired queue");
  check(std::abs(st.gain-float(cue.volume)/32767.f)<1e-5,"native base volume preserved");
  auto pcm=mix();check(std::any_of(pcm.begin(),pcm.end(),[](auto s){return s!=0;}),"replacement creates actual PCM");
  std::printf("PASS selector %-23s native=%02X file=%s\n",cue.name,cue.sound,cue.file);
 }
 // Repeated requests, subsection aliases and menu volume/fades must not restart the file.
 reset(root);command("2a. Hoth Base");frame();mix(100);auto before=hd::status();
 command("2b. Hoth Base");frame();auto after=hd::status();
 check(after.position_frames==before.position_frames && after.background_starts==before.background_starts,"subsection alias does not restart music");
 f(0x800d0764,.5f);f(0x800d2528,.4f);w(0x800dd2bc)=1;frame();
 check(std::abs(hd::status().gain-2500.f/32767.f)<1e-5,"native fade, menu volume, and dim state multiply in original order");
 f(0x800d2528,0.f);frame();auto silent=mix(400,1234);check(std::all_of(silent.begin(),silent.end(),[](auto s){return s==1234;}),"zero music volume leaves native effects bit-identical");
 check(hd::status().position_frames>after.position_frames,"muted external track still advances");
 f(0x800d2528,1.f);f(0x800d0764,1.f);w(0x800dd2bc)=0;frame();
 const auto pos=hd::status().position_frames;
 ctx.r4=0;func_80006CB0(ram.data(),&ctx);command("");frame();check(hd::status().position_frames==pos,"null and empty commands are no-ops");
 // Both native early exits must close the background scope.
 command("None");frame();check(!hd::status().background_active,"None stops the external track");
 clear_slots();request(0x21,20000);check(hd::status().stingers==1 && slot_for(0x21)<0,"stinger after negative-ID early exit is not mistaken for background");
 reset(root);command("1. Battle");frame();w(0x800d06f0)=1;frame();check(!hd::status().background_active,"native disabled branch stops refresh playback");
 request(0x61,10000);check(hd::status().stingers==1 && slot_for(0x61)<0,"stinger after disabled branch remains correctly scoped");
 w(0x800d06f0)=0;frame();check(hd::status().background_active,"native updater resumes replacement");
 command("Cut ");frame();check(!hd::status().background_active,"cutscene silence command obeyed");
 command("1. Battle");frame();command("unrecognized selector");frame();check(!hd::status().background_active,"unknown nonempty selector obeys native stop");
 // Nonmusic sound requests are left alone even when they share a bank ID.
 reset(root);command("1. Battle");frame();request(0x0d,10000,true);check(slot_for(0x0d)>=0,"SFX wrapper using same sample ID is not intercepted");
 clear_slots();request(0x55,12345);check(slot_for(0x55)>=0,"unclassified request outside background scope remains native");
 // Source-rate resampling, loop and one-shot end behavior, and defensive input validation.
 reset(root);command("Theme");frame();mix(100,0,44100);check(std::abs(hd::status().position_frames-50)<1e-6,"resampling advances in source frames");
 mix(22050);check(hd::status().background_finished,"one-shot reaches EOF");auto starts=hd::status().background_starts;frame();check(hd::status().background_starts==starts && hd::status().background_finished,"repeated native refresh does not restart completed one-shot");
 auto tail=mix(256,321);check(std::all_of(tail.begin(),tail.end(),[](auto s){return s==321;}),"one-shot EOF leaves native PCM alone");
 command("Main Menu");frame();mix(22050);check(hd::status().background_active && !hd::status().background_finished,"loop wraps and stays active");
 auto st=hd::status();int16_t odd[]={111,222,333};hd::mix_into(odd,3,22050);check(odd[2]==333,"odd trailing sample is untouched");
 check(!hd::mix_into(nullptr,10,22050),"null PCM is safe");check(!hd::mix_into(odd,3,0),"invalid sample rate is safe");
 func_80006668(ram.data(),&ctx);check(!hd::status().background_active && hd::status().stingers==0,"native scene reset stops external voices");
 // Effects are added at unity gain, not multiplied by the deleted 0.25 ducking factor.
 reset(root);command("1. Battle");frame();auto clean=mix(500,0);
 reset(root);command("1. Battle");frame();auto with_sfx=mix(500,1000);
 for(size_t i=0;i<clean.size();i++)check(std::abs(int(with_sfx[i])-int(clean[i])-1000)<=1,"unity native effect contribution");
 // Missing/corrupt/disabled replacements use the real native allocator; no cue is dropped.
 const auto map=root/"Sdata/MUSIC/n64_music_map.tsv";
 {std::ofstream o(map);o<<"battle_of_hoth missing.ogg loop\n";}
 reset(root);command("1. Battle");frame();check(slot_for(0x0d)>=0,"missing OGG enqueues native background");check(!hd::status().background_active,"missing OGG never consumes request");
 // Switching out of a native fallback retires only the tagged background slot.
 const int old=slot_for(0x0d);w(0x80110ee8+32*old)=77;w(0x80110ee8+32*old+4)=0x0d;
 request(0x55,15000,true);const int effect=slot_for(0x55);check(effect>=0,"test effect allocated");
 command("3. Asteroids");frame();check(w(0x80110fe8+32*old)==-1,"old native background is retired on replacement transition");
 check(w(0x80110ee8+32*old)==77,"live AL handle left for native scheduler to stop/deallocate");
 check(slot_for(0x55)==effect,"fallback retirement preserves unrelated effect slot");
 {std::ofstream o(root/"Sdata/MUSIC/broken.ogg");o<<"not an ogg";}{std::ofstream o(map);o<<"battle_of_hoth broken.ogg\n";}
 reset(root);command("1. Battle");frame();check(slot_for(0x0d)>=0,"corrupt OGG enqueues native background");
 fs::remove(map);set_disable(true);reset(root);command("1. Battle");frame();check(slot_for(0x0d)>=0 && !hd::status().enabled,"explicit disable restores original native path");
 set_disable(false);
 // Parse filename quoting/mode and load a mono 48-kHz Vorbis file.
 {std::ofstream o(map);o<<"# parser fixture\nmain_menu = \"mono fixture.ogg\" once # quoted name and explicit mode\n";}
 reset(root);command("Main Menu");frame();auto mono=mix(200,0,48000);check(hd::status().file=="mono fixture.ogg","quoted relative map override");
 for(size_t i=0;i<mono.size();i+=2)check(mono[i]==mono[i+1],"mono decoded equally into stereo");mix(48000,0,48000);check(hd::status().background_finished,"explicit once overrides default loop");fs::remove(map);
 // Regression: the menu must never be given the crawl by a built-in guess.
 const auto menu_file=root/"Sdata/MUSIC/main_menu.ogg";
 fs::remove(menu_file);
 reset(root);command("Them");frame();mix(123);
 check(hd::status().background_active && hd::status().file=="Track02.ogg","crawl starts the verified Track02 recording");
 const auto crawl_starts=hd::status().background_starts;
 command("Main");
 check(!hd::status().background_active,"Main stops crawl immediately, before the next native update");
 check(hd::status().file.empty(),"Main must not preload the crawl as menu music when its dedicated replacement is absent");
 auto boundary=mix(200,765);
 check(std::all_of(boundary.begin(),boundary.end(),[](auto v){return v==765;}),"no crawl PCM leaks across the menu command");
 frame();
 check(w(0x800d075c)==0x62 && slot_for(0x62)>=0,"missing menu replacement uses actual native 0x62 allocation");
 check(hd::status().background_starts==crawl_starts,"Main does not restart Track02 as a loop");
 for(int i=0;i<12;++i){frame();auto pcm=mix(64,432);check(std::all_of(pcm.begin(),pcm.end(),[](auto v){return v==432;}),"native menu PCM is unaltered by the external mixer");}
 const int menu_slot=slot_for(0x62);request(0x55,17000,true);const int menu_effect=slot_for(0x55);
 command("Them");frame();
 check(hd::status().background_active && hd::status().file=="Track02.ogg","return from native menu starts crawl only on Them");
 check(w(0x80110fe8+32*menu_slot)==-1,"native menu desired slot retired when crawl is actually replaced");
 check(slot_for(0x55)==menu_effect,"native menu transition preserves unrelated effects");
 command("None");frame();check(!hd::status().background_active,"None after crawl leaves no stale external background");
 // A deployed MUSIC map must override a legacy parent map that reused Track02.
 {std::ofstream o(root/"Sdata/n64_music_map.tsv");o<<"main_menu Track02.ogg loop\n";}
 {std::ofstream o(map);o<<"main_menu main_menu.ogg loop\n";}
 reset(root);command("Main Menu");frame();
 check(slot_for(0x62)>=0 && !hd::status().background_active,"corrected deployed map defeats stale parent Track02 mapping");
 check(hd::status().file.empty(),"stale parent crawl not decoded for menu");
 // Dedicated external menu assets remain supported, and are not started by command alone.
 fs::copy_file(root/"Sdata/MUSIC/tone.ogg",menu_file,fs::copy_options::overwrite_existing);
 reset(root);command("Main Menu");check(!hd::status().background_active,"dedicated menu preload does not start without native request");frame();
 check(hd::status().background_active && hd::status().file=="main_menu.ogg" && slot_for(0x62)<0,"dedicated external menu replaces native 0x62");
 mix(100);const auto menu_pos=hd::status().position_frames;command("Main");frame();
 check(hd::status().position_frames==menu_pos,"Main prefix alias does not restart dedicated menu");
 command("Them");frame();check(hd::status().file=="Track02.ogg" && hd::status().position_frames==0,"dedicated menu switches cleanly to crawl");
 command("Main");frame();check(hd::status().file=="main_menu.ogg" && hd::status().position_frames==0,"crawl switches cleanly to dedicated menu");
 fs::remove(map);fs::remove(root/"Sdata/n64_music_map.tsv");
 std::puts("PASS menu/crawl separation: immediate stop, native fallback, legacy-map precedence, dedicated asset, effects preserved");
 // Finally decode every selected recording from the USER'S REAL OGG PACK.
 reset(real);
 for(const auto& cue:cues){sote_music_reset(ram.data());clear_slots();command(cue.name);frame();const auto actual=hd::status();
  if(cue.sound==0x62 && !fs::is_regular_file(real/"Sdata/MUSIC/main_menu.ogg")) {
   check(!actual.background_active && actual.file.empty(),"real pack: menu does not borrow crawl");
   check(slot_for(0x62)>=0,"real pack: original menu request preserved without matched PC menu file");
  } else {
   check(actual.background_active && actual.file==cue.file,std::string("real pack: ")+cue.name);
   check(slot_for(cue.sound)<0,"real OGG replaces native allocation");mix(1024);
  }
 }
 // The supplied short PC cues stay externally replaced by the real deployed map.
 for(int sound:{0x21,0x61}){reset(real);request(sound,24000);check(slot_for(sound)<0 && hd::status().stingers==1,"real short cue is external, not native");auto pcm=mix(2048);check(std::any_of(pcm.begin(),pcm.end(),[](auto v){return v!=0;}),"real short cue produces PCM");}
 std::printf("PASS all %u assertions; 20 selectors, original guest code, fallback/volume/loop/SFX checks, and real OGG decode\n",passed);
 return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s (%u assertions passed)\n",e.what(),passed);return 1;}
