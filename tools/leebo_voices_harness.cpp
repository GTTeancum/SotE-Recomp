#include "hd_audio.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace sote::hd_audio;
void require(bool ok, const char* why) { if(!ok) throw std::runtime_error(why); }
int main(int argc, char** argv) {
 require(argc==3,"runtime directory and fixture required");
 initialize(argv[1]); require(is_enabled(),"PC WAV assets unavailable");
 std::ifstream input(argv[2]); require(bool(input),"fixture unavailable");
 std::string row, first; unsigned count=0; uint64_t vi=100;
 while(std::getline(input,row)) {
  const auto a=row.find('\t'), b=row.find('\t',a+1);
  const int event = row.substr(a+1,b-a-1)=="ILB38.WAV" ? 24 : 11;
  const auto text=row.substr(b+1); if(first.empty()) first=text;
  require(has_voice_for_text(text),"missing mapping");
  require(play_visible_voice(text,vi,event),"WAV failed to load/enqueue");
  for(int n=0;n<400;++n) require(!play_visible_voice(text,++vi,event),"continuous draw replayed");
  std::vector<int16_t> samples(48000*2*8);
  require(mix_into(samples.data(),samples.size(),48000),"voice not mixed");
  require(std::any_of(samples.begin(),samples.end(),[](int16_t x){return x!=0;}),"voice silent");
  vi+=61; require(play_visible_voice(text,vi,event),"message did not rearm");
  // Drain before the next clip so each non-silent assertion tests that clip.
  std::fill(samples.begin(),samples.end(),0); mix_into(samples.data(),samples.size(),48000);
  ++count;
 }
 require(!play_visible_voice("~h~oYou need a security key~nof some sort.",++vi,11),"sewage-gate speech leaked to Gall");
 require(!play_visible_voice("~h~oYou need a security key~nof some sort.",++vi,26),"sewage-gate speech leaked to palace");
 require(count==32,"coverage count changed");
 require(!has_voice_for_text("~oLet's get out of here!"),"non-Leebo built-in mapping");
 require(!play_visible_voice("unrelated menu text",++vi,11),"unmapped text played");
 require(play_visible_voice(first,++vi,4),"level transition did not rearm");
 require(!play_visible_voice(first,vi,4),"duplicate slot replayed");
 require(play_visible_voice(first,1,4),"VI reset did not rearm");
 require(has_voice_for_text("Dfob"),"Gall alias lost");
 std::cout << count << " ROM messages loaded and mixed, visibility/retry checks passed\n";
}
