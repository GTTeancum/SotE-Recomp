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
 count=played.size(); sote_pc_voice_harpoon(ram.data(),0x800866CC); check(played.size()==count+1 && played.back()=="ILU33.WAV"); // success is detached, not lost
 count=played.size();
 sote_pc_voice_harpoon(ram.data(),0x80086DA8); check(played.size()==count); // launch validation
 sote_pc_voice_harpoon(ram.data(),0x800867C8); check(played.size()==count);
 advance(180); set_word(0x800E1A80,0); count=played.size();
 sote_pc_voice_harpoon(ram.data(),0x80086A64); check(played.size()==count);
 const char* stages[] = {
  "~o~cStage One~n~n~n~n~n~cRogue Group:~n~cTake out those Imperial Probe Droids.~n~cBe careful not to shoot our own turrets!",
  "~o~cStage Two~n~n~n~n~n~cRogue Group:~n~cThese AT-STs should be no problem~n~cfor our blasters.",
  "~o~cStage Three~n~n~n~n~n~cRogue Group:~n~cTry using your harpoon and tow cables~n~con the AT-AT.",
  "~o~cStage Four~n~n~n~n~n~cGood Job, Rogue Group.~n~cWe still need to buy more time~n~cfor our transports to escape.~n~cKeep at them!"
 };
 const char* files[] = {"ILU01.WAV", "ILU04.WAV", "ILU05.WAV", "ILU07.WAV"};
 set_event(30); count=played.size();
 for(auto stage:stages) draw(stage);
 check(played.size()==count); // Hoth instructions must not leak into Skyhook.
 for(auto e:{2,3}) {
  set_event(e);
  for(int i=0;i<4;++i) {
   count=played.size(); draw(stages[i]);
   check(played.size()==count+1 && played.back()==files[i]);
   for(int n=0;n<180;++n) { advance(1); draw(stages[i]); }
   check(played.size()==count+1);
  }
 }
 const char* warnings[] = {"~o~cHey, I'm on your side!", "~o~cDon't shoot Rebel forces!", "~o~cWe're on the same side!"};
 set_event(30); count=played.size();
 for(auto warning:warnings) draw(warning);
 check(played.size()==count);
 set_event(3);
 for(int i=0;i<9;++i) {
  // Reorder the three texts so bank rotation is independent of wording.
  int selection=(i/3+i)%3;
  advance(61); count=played.size(); draw(warnings[selection]);
  std::string expected="IR103.WAV";
  expected[2]=char('1'+i%3); expected[4]=selection==1?'1':'3';
  check(played.size()==count+1 && played.back()==expected);
  for(int n=0;n<120;++n) { advance(1); draw(warnings[selection]); }
  check(played.size()==count+1);
 }
 set_event(2); draw(warnings[0]); check(played.back()=="IR103.WAV");
 count=played.size(); set_event(30); draw("~o~cReturn to Battle!"); check(played.size()==count);
 set_event(3); draw("~o~cReturn to Battle!"); check(played.back()=="ILU22.WAV");
 count=played.size();
 for(int n=0;n<120;++n) { advance(1); draw("~o~cReturn to Battle!"); }
 check(played.size()==count);
 advance(61); draw("~o~cReturn to Battle!"); check(played.size()==count+1);
 count=played.size();
 set_event(30); draw("~o~cYou lost the tow cable."); check(played.size()==count);
 set_event(3); draw("~o~cYou lost the tow cable."); check(played.size()==count+1 && played.back()=="ILU31.WAV");
 count=played.size();
 for(auto source:{0x800867C8U,0x80086A64U,0x80087270U,0x80087500U,0x80087610U})
  sote_pc_voice_harpoon(ram.data(),source);
 check(played.size()==count); // clearing alone never speaks
 for(int n=0;n<120;++n) { advance(1); draw("~o~cYou lost the tow cable."); }
 check(played.size()==count);
 advance(61); draw("~o~cYou lost the tow cable."); check(played.size()==count+1);
 count=played.size(); set_event(30); draw("~o~cFire tow cable!"); check(played.size()==count);
 set_event(3); draw(warnings[0]); draw(warnings[1]); // Advance warning bank.
 count=played.size(); draw("~o~cFire tow cable!");
 check(played.size()==count+1 && played.back()=="IR108.WAV");
 for(int n=0;n<120;++n) { advance(1); draw("~o~cFire tow cable!"); }
 check(played.size()==count+1);
 advance(61); draw("~o~cFire tow cable!"); check(played.size()==count+2 && played.back()=="IR108.WAV");
 std::cout << "PC voice level, visibility, repeat and harpoon guards passed\n";
}
