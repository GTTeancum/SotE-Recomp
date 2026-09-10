#include "recomp_hooks.h"
#include "hd_audio.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
std::vector<std::string> played;
namespace sote::hd_audio {
bool play_file(std::string_view name, float) { played.emplace_back(name); return true; }
}
int main() {
 std::vector<uint8_t> ram(8*1024*1024);
 auto set_event = [&](uint16_t e) { std::memcpy(ram.data()+((0x13CE0E)^2), &e, 2); sote_pc_voice_frame(ram.data()); };
 auto set_word = [&](uint32_t addr, uint32_t value) { std::memcpy(ram.data()+(addr&0x7FFFFF), &value, 4); };
 auto draw = [&](const char* text) { for(size_t i=0;i<=std::strlen(text);++i) ram[(0x100+i)^3]=text[i]; sote_pc_voice_text(ram.data(),0x80000100); };
 auto advance = [&](int n) { while(n--) sote_pc_voice_frame(ram.data()); };
 auto check = [&](bool ok) { if(!ok) { std::cerr << "Failure after " << played.size() << " voices\n"; std::exit(1); } };
 const char* text="~oDestroy the turrets at the~nend of each arm of the~nstation.";
 set_event(2); draw(text); check(played.empty());
 set_event(28); draw(text); check(played.size()==1 && played.back()=="ILU16.WAV");
 for(int i=0;i<300;++i) { advance(1); draw(text); draw(text); }
 check(played.size()==1);
 advance(61); draw(text); check(played.size()==2);
 draw("~oUnrelated dialogue."); check(played.size()==2);
 sote_pc_voice_text(ram.data(),0xFFFFFFFF); check(played.size()==2);
 draw("~oLet's get out of here!"); check(played.back()=="ILU19.WAV");
 draw("~oThat does it for the turrets.~nNow fly inside and destroy~nthe power core!"); check(played.back()=="ILU17.WAV");
 draw("~oThe Empire is attacking~nXizor's base and us!"); check(played.back()=="ILU13.WAV");
 draw("~oWait... Where's Dash?~nHe must not have made it~nout of the skyhook before~nit blew..."); check(played.back()=="ILU20.WAV");
 auto count=played.size();
 sote_pc_voice_harpoon(ram.data(),0x800870C8); check(played.size()==count);
 set_event(2); set_word(0x800E1A84,0x80001000);
 sote_pc_voice_harpoon(ram.data(),0x80086FCC); check(played.size()==count);
 set_word(0x800E1A84,0); sote_pc_voice_harpoon(ram.data(),0x80086FCC); check(played.back()=="ILU32.WAV");
 count=played.size(); sote_pc_voice_harpoon(ram.data(),0x80086FCC); check(played.size()==count);
 advance(180); sote_pc_voice_harpoon(ram.data(),0x800870C8); check(played.back()=="ILU29.WAV");
 advance(180); set_word(0x800E1A80,1);
 count=played.size(); sote_pc_voice_harpoon(ram.data(),0x800866CC); check(played.size()==count); // successful trip is not cable loss
 sote_pc_voice_harpoon(ram.data(),0x80086DA8); check(played.size()==count); // launch validation
 sote_pc_voice_harpoon(ram.data(),0x800867C8); check(played.back()=="ILU31.WAV");
 advance(180); set_word(0x800E1A80,0); count=played.size();
 sote_pc_voice_harpoon(ram.data(),0x80086A64); check(played.size()==count);
 std::cout << "PC voice level, visibility, repeat and harpoon guards passed\n";
}
