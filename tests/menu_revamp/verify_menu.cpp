#include "menu_skin.hpp"
#include "graphics_menu.hpp"
#include "controls_menu.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
namespace sote::menu_skin { void configure_menu_fonts(const std::filesystem::path&); }
extern "C" int sote_is_bike_stage_active() { return 0; }
using namespace sote::menu_skin;
int checks=0, failures=0;
void check(bool v,const char*name){++checks;if(!v){++failures;std::cerr<<"FAIL "<<name<<"\n";}}
struct Memory {
 std::vector<uint8_t> b=std::vector<uint8_t>(0x800000);
 uint32_t strings=0x80220000;
 void byte(uint32_t a,unsigned v){b[(a&0x7fffff)^3]=uint8_t(v);}
 void half(uint32_t a,unsigned v){byte(a,v>>8);byte(a+1,v);}
 void word(uint32_t a,uint32_t v){half(a,v>>16);half(a+2,v);}
 uint32_t text(const std::string& s){uint32_t a=strings;for(char c:s)byte(strings++,unsigned(c));byte(strings++,0);return a;}
 void row(unsigned i,std::string str,int x=160,uint32_t color=0x408080ff){word(0x80200000+i*4,text(str));half(0x80111110+i*4,x);half(0x80111112+i*4,70);word(0x80111250+i*4,color);}
 Memory(){
  word(0x8013ce30,0x80200000);word(0x8013ce2c,0x80201000);
  for(int i=0;i<80;i++)half(0x80111110+i*4,-1000);
  // Artificial solid glyphs for structural tests. No retail font bytes/ROM needed.
  word(0x800da818,0x80210000);half(0x800da81c,13);
  for(unsigned i=0;i<95;i++){uint32_t a=0x800da818+i*14;half(a+6,i=='A'-32?9:7);half(a+8,0);half(a+10,0);half(a+12,0);half(a+14,0);half(a+16,5);half(a+18,9);}
  for(int i=0;i<4096;i++)byte(0x80210000+i,0xff);
 }
 Snapshot snapshot(){return read_guest(b.data(),b.size());}
 void profiles(){for(int i=0;i<4;i++){row(51+i,"Player "+std::to_string(i),160,i==1?0xc8ffc8ff:0x408080ff);row(70+i,"Medium");}row(76,"Options");row(77,"Rename");row(78,"Clear");row(68,"Use L or R to select");}
 void native_options(){
  word(0x800dd5e4,1);word(0x800d0948,0);half(0x800dd5e0,3);word(0x800d0954,0);byte(0x8018bbfd,0);
  const char* labels[]={"", "Overlay Displays","Seeker Camera","Sound Effects","Music","Sound Panning","Controls"};
  word(0x800dd61c,text("Return to Main Menu"));
  for(int i=1;i<=8;i++)word(0x800dd61c+i*4,text("Value "+std::to_string(i)));
  for(int i=0;i<7;i++){word(0x800dd5e8+i*4,text(labels[i]));for(int j=0;j<9;j++)half(0x800dd674+i*18+j*2,i==0?0:j);}
  for(int i=0;i<48;i++)word(0x800da70c+i*4,text("Action "+std::to_string(i)));
  for(int p=0;p<8;p++)for(int i=0;i<48;i++)half(0x800e6808+p*96+i*2,0x8000>>p);
 }
 void thumb(unsigned row_index,uint8_t palette_index){
  uint32_t sprite=0x80300000+row_index*64,tiles=0x80310000+row_index*96;
  word(0x80201000+row_index*4,sprite);half(0x80111110+row_index*4,40);half(0x80111112+row_index*4,70);word(0x80111250+row_index*4,0xffffffff);
  half(sprite+4,160);half(sprite+6,120);half(sprite+28,12);byte(sprite+32,2);byte(sprite+33,1);word(sprite+24,0x80320000);word(sprite+36,tiles);
  half(0x80320000,0xf801);half(0x80320002,0x07c1);
  for(unsigned t=0;t<12;t++){half(tiles+t*8,40);half(tiles+t*8+2,40);word(tiles+t*8+4,0x80330000+row_index*19200+t*1600);for(unsigned k=0;k<1600;k++)byte(0x80330000+row_index*19200+t*1600+k,palette_index);}
 }
};
uint64_t hash(const Image& im){uint64_t h=14695981039346656037ull;for(auto v:im.rgba)h=(h^v)*1099511628211ull;return h;}
void press(uint16_t bits){float x=0,y=0;uint16_t zero=0;sote::graphics_menu::filter_input(&zero,&x,&y);sote::graphics_menu::filter_input(&bits,&x,&y);}
int main(int argc,char**argv){
 if(argc<2)return 2;
 auto scratch=std::filesystem::path(argv[1]);std::filesystem::create_directories(scratch/"Sdata/UI");
 sote::graphics_menu::initialize(scratch);sote::controls_menu::initialize(scratch);initialize(scratch);set_renderer_available(true);
 check(plain_text("~s~cTest~n~~line")=="Test\nline","native formatting");
 check(read_guest(nullptr,0).screen==Screen::Native,"null memory fallback");
 Memory m;m.profiles();auto s=m.snapshot();check(s.screen==Screen::Profiles,"profile identification");check(s.original&&s.original->valid,"runtime font decode");
 const auto before=m.b;s=m.snapshot();check(before==m.b,"capture never writes guest memory");
 const auto baseline=render(s,960,540);check(baseline.valid(),"profile raster");
 check(!render(s,100,100).valid(),"small window fallback");
 for(auto [w,h]:std::array<std::pair<unsigned,unsigned>,5>{{{640,480},{1280,720},{1920,1080},{1024,1024},{2560,1080}}})check(render(s,w,h).valid(),"aspect-safe output extent");
 m.row(50,"Rename");check(m.snapshot().screen==Screen::Native,"rename prompt is not hidden");m.row(50,"");m.row(67,"Are you sure?");check(m.snapshot().screen==Screen::Native,"clear prompt is not hidden");m.row(67,"");
 m.word(0x8013ce30,0x807ffffe);check(m.snapshot().screen==Screen::Native,"bad pointer fallback");m.word(0x8013ce30,0x80200000);
 for(size_t size:{size_t(0),size_t(1),size_t(0x800),size_t(0x100000),size_t(0x200001)})check(read_guest(m.b.data(),size).screen==Screen::Native,"short memory bounds");
 capture(m.b.data());auto live=latest();check(live&&live->screen==Screen::Profiles,"published profile");auto serial=live?live->serial:0;capture(m.b.data());check(latest()&&latest()->serial==serial,"static snapshot upload cache");
 std::this_thread::sleep_for(std::chrono::milliseconds(170));check(!latest(),"stale snapshot timeout");
 Memory summary;summary.row(52,"Player: TEST");summary.row(55,"Difficulty Setting: Medium");summary.row(56,"Lives:");summary.row(57,"4");summary.row(58,"Time");summary.row(59,"0:01:23");summary.row(60,"Challenge Pts");summary.row(61,"8 of 12");summary.row(50,"Selected Level");summary.thumb(40,0);summary.thumb(45,1);summary.half(0x80111112+40*4,200);
 auto sum=summary.snapshot();check(sum.screen==Screen::Summary&&sum.thumbnail.valid(),"summary dynamic thumbnail");check(sum.thumbnail.valid()&&sum.thumbnail.rgba[1]==255&&sum.thumbnail.rgba[0]==0,"selected carousel thumbnail, not Hoth");check(render(sum).valid(),"summary raster");
 summary.half(0x80111110+45*4,-1000);sum=summary.snapshot();check(sum.thumbnail.valid()&&sum.thumbnail.rgba[0]==255,"carousel follows another visible card");
 Memory options;options.native_options();
 for(int player=0;player<4;player++)for(int selection=0;selection<=6;selection++){
  options.byte(0x8018bbfd,player);options.half(0x800dd5e0,selection);
  auto o=read_native_options(options.b.data(),options.b.size());check(o.screen==Screen::Options&&o.focused_setting==selection,"all players and native option focus rows");
 }
 options.byte(0x8018bbfd,0);options.half(0x800dd5e0,3);
 for(int i=1;i<=6;i++)for(unsigned v=0;v<=8;v++){
  uint32_t a=0x8018bbf8+0x1a+i/2;options.byte(a,v<<((i&1)*4));
  auto o=read_native_options(options.b.data(),options.b.size());check(o.rows[52+i*2].text==(v==0?"Return to Main Menu":"Value "+std::to_string(v)),"packed native settings, no hardcoded values");options.byte(a,0);
 }
 auto opt=read_native_options(options.b.data(),options.b.size());check(render(opt).valid(),"options raster");
 for(unsigned p=0;p<8;p++){
  options.byte(0x8018bbf8+0x1d,p);options.half(0x800dd5e0,6);options.word(0x800d0954,0x3f800000);
  auto guide=read_native_options(options.b.data(),options.b.size());check(guide.screen==Screen::Controls,"native controls preset decode");
  check(!guide.native_controls[0].empty()&&!guide.native_controls[4].empty(),"vehicle sections populated");check(render(guide).valid(),"controls raster");
 }
 sote_capture_native_options(options.b.data());check(controls_visible(),"controls runtime visibility");scroll_controls(1000);sote_capture_native_options(options.b.data());auto end=latest();check(end&&end->controls_scroll>0,"guide scroll down");scroll_controls(-1000);sote_capture_native_options(options.b.data());check(latest()&&latest()->controls_scroll==0,"guide scroll bounds");
 options.byte(0x8018bbf8+0x1d,0);options.word(0x800d0954,0);options.half(0x800dd5e0,0);
 sote_capture_native_options(options.b.data());press(0x0800);sote_capture_native_options(options.b.data());check(latest()&&latest()->rows[65].text=="< Graphics >","appended graphics entry focus");check(latest()&&latest()->rows[63].text=="Controls","native Controls row preserved");
 press(0x8000);sote_capture_native_options(options.b.data());check(latest()&&latest()->screen==Screen::Graphics,"open graphics extension");check(render(*latest()).valid(),"graphics raster");
 press(0x4000);sote_capture_native_options(options.b.data());press(0x0100);sote_capture_native_options(options.b.data());press(0x8000);sote_capture_native_options(options.b.data());check(latest()&&latest()->screen==Screen::Schemes,"open scheme extension");check(render(*latest()).valid(),"scheme raster");
 press(0x0400);press(0x0100);sote_capture_native_options(options.b.data());check(latest()&&latest()->rows[54].text=="Modern","scheme value changes through real input handler");
 press(0x4000);sote_capture_native_options(options.b.data());press(0x0400);
 // Missing fonts, malformed files and path traversal all fall back to original.
 auto root=scratch/"Sdata/UI";
 for(const std::string file:{"missing.ttf","../escape.ttf","broken.ttf"}){
  std::ofstream(root/"fonts.ini")<<"default="<<file<<"\n";std::ofstream(root/"broken.ttf")<<"not a font";configure_menu_fonts(root);check(hash(render(s,960,540))==hash(baseline),"bad font safe fallback");
 }
 if(argc>2){
  std::filesystem::copy_file(argv[2],root/"test-font.ttf",std::filesystem::copy_options::overwrite_existing);
  std::ofstream(root/"fonts.ini")<<"default=original\nprofile.name=test-font.ttf\n";configure_menu_fonts(root);check(hash(render(s,960,540))!=hash(baseline),"per-role modern font substitution");
  std::ofstream(root/"fonts.ini")<<"default=test-font.ttf\nprofile=original\n";configure_menu_fonts(root);check(hash(render(s,960,540))==hash(baseline),"page override preserves original font");
  std::ofstream(root/"fonts.ini")<<"profile.name=test-font.ttf\nprofile.name.0=original\nprofile.name.1=original\nprofile.name.2=original\nprofile.name.3=original\n";configure_menu_fonts(root);check(hash(render(s,960,540))==hash(baseline),"individual overrides win over role");
 }
 std::cout<<"Menu checks: "<<checks<<"; failures: "<<failures<<"\n";return failures?1:0;
}
