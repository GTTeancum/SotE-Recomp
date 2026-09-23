// Production mixer tests: intro/loop intervals, resampling, fallback and real menu.
#include "hd_music.hpp"
#define STB_VORBIS_HEADER_ONLY
#include "stb/stb_vorbis.c"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace hd=sote::hd_music;namespace fs=std::filesystem;
unsigned checks=0, scenarios=0; uint64_t samples_checked=0;
void check(bool v,const char* s){if(!v)throw std::runtime_error(s);++checks;}
struct PCM{std::vector<short> pcm;int channels,frequency;size_t frames()const{return pcm.size()/channels;}};
PCM decode(const fs::path&p){std::ifstream f(p,std::ios::binary);std::vector<unsigned char>b{std::istreambuf_iterator<char>(f),{}};PCM a{};short*raw=nullptr;int n=stb_vorbis_decode_memory(b.data(),int(b.size()),&a.channels,&a.frequency,&raw);std::unique_ptr<short,decltype(&std::free)>own(raw,&std::free);if(n<=0||!raw)throw std::runtime_error("fixture decode failed");a.pcm.assign(raw,raw+size_t(n)*a.channels);return a;}
void setmap(const fs::path&root,const std::string&file,bool loop){std::ofstream f(root/"Sdata/MUSIC/n64_music_map.tsv");f<<"main_menu \""<<file<<"\" "<<(loop?"loop":"once")<<"\n";f.close();hd::initialize(root);hd::command("Main Menu");}
int16_t sat(float f){return int16_t(std::lround(std::clamp(f,-32768.0F,32767.0F)));}
void exercise(const fs::path&root,const std::string&file,bool loop,size_t begin,size_t end,uint32_t rate,size_t nframes){
 ++scenarios;auto a=decode(root/"Sdata/MUSIC"/file);setmap(root,file,loop);check(hd::replace_background(0x62,20000),"valid file must replace native");
 std::vector<int16_t>out(nframes*2);for(size_t i=0;i<nframes;++i){out[i*2]=123;out[i*2+1]=-456;}
 check(hd::mix_into(out.data(),out.size(),rate),"valid voice must mix");
 double pos=0;bool finished=false;size_t wraps=0;float gain=20000.0F/32767.0F;
 const size_t stop=loop?end:a.frames();
 for(size_t i=0;i<nframes;++i){
  for(size_t ch=0;ch<2;++ch){float v=ch?-456.0F:123.0F;
   if(!finished){size_t p=std::min(size_t(pos),stop-1),q=p+1<stop?p+1:(loop?begin:p),c=a.channels==1?0:ch;
    float x=a.pcm[p*a.channels+c],y=a.pcm[q*a.channels+c];v+=(x+(y-x)*float(pos-std::floor(pos)))*gain;}
   if(out[i*2+ch]!=sat(v))throw std::runtime_error("PCM differs from independent loop reference");++samples_checked;
  }
  if(!finished){pos+=double(a.frequency)/rate;if(pos>=stop){if(loop){pos=begin+std::fmod(pos-begin,double(end-begin));++wraps;}else{pos=stop;finished=true;}}}
 }
 check(true,"all PCM samples agree");auto st=hd::status();check(std::abs(st.position_frames-pos)<1e-7,"fractional phase preserved");check(st.background_finished==finished,"EOF state correct");
 if(loop){check(wraps>0,"test must cross loop boundary");check(st.position_frames>=begin&&st.position_frames<end,"playback returns to loop interval, not intro");}
 check(hd::replace_background(0x62,0),"zero gain refresh still consumes native");
 auto before=hd::status().position_frames;check(!hd::mix_into(out.data(),out.size(),0),"zero output rate rejected");check(hd::status().position_frames==before,"invalid rate cannot advance voice");
 hd::reset();check(!hd::status().background_active,"reset stops loop");
 std::printf("PASS scenario %u: %s %s rate=%u frames=%zu wraps=%zu\n",scenarios,file.c_str(),loop?"loop":"once",rate,nframes,wraps);
}
int main(int argc,char**argv)try{
 if(argc!=3)throw std::runtime_error("usage: verify_music_loops fixtures real_project");
 fs::path root=fs::absolute(argv[1]),real=fs::absolute(argv[2]);fs::current_path(root);
 for(uint32_t rate:{1U,22050U,44100U,48000U,96000U})exercise(root,"tagged.ogg",true,137,1361,rate,rate==1?3:size_t(rate)*1/3+1);
 exercise(root,"base.ogg",true,0,2048,22050,10000);
 exercise(root,"lowercase.ogg",true,137,1361,48000,12000);
 exercise(root,"only_start.ogg",true,137,2048,44100,10000);
 exercise(root,"only_end.ogg",true,0,1361,44100,10000);
 exercise(root,"tagged.ogg",false,137,1361,48000,12000);
 for(const char*bad:{"negative.ogg","signed_plus.ogg","nan.ogg","overflow.ogg","past_eof.ogg","reversed.ogg","empty.ogg","duplicate.ogg","conflicting_duplicate.ogg"}){
  ++scenarios;setmap(root,bad,true);check(!hd::replace_background(0x62,30000),"invalid tags must leave native request intact");check(!hd::status().background_active,"bad file has no active voice");std::printf("PASS scenario %u: invalid %s -> native fallback\n",scenarios,bad);
 }
 // Real newly encoded menu, over intro + TWO loop boundaries, using native mix rate.
 const fs::path menu=real/"Sdata/MUSIC/main_menu.ogg";check(fs::is_regular_file(menu),"real menu exists");
 fs::copy_file(menu,root/"Sdata/MUSIC/real_menu.ogg",fs::copy_options::overwrite_existing);
 exercise(root,"real_menu.ogg",true,319890,1576634,22050,80*22050);
 setmap(root,"real_menu.ogg",true);check(hd::replace_background(0x62,20000),"real menu starts");std::vector<int16_t>silence(1000);
 hd::command("Them");check(!hd::status().background_active,"crawl command immediately stops menu");
 hd::command("Main");check(hd::replace_background(0x62,20000),"menu restarts on actual command");check(hd::status().position_frames==0,"new menu command replays intro");
 check(hd::replace_background(0x62,0),"muted menu consumes native");auto zero=silence;hd::mix_into(zero.data(),zero.size(),22050);check(zero==silence,"muted loop adds no PCM");check(hd::status().position_frames>0,"muted loop keeps advancing");
 hd::end_background_frame(false);check(!hd::status().background_active,"native stop ends loop");
 std::printf("PASS %u assertions across %u scenarios; %llu PCM samples checked (not independent scenarios)\n",checks,scenarios,(unsigned long long)samples_checked);return 0;
}catch(const std::exception&e){std::fprintf(stderr,"FAIL after %u checks: %s\n",checks,e.what());return 1;}
