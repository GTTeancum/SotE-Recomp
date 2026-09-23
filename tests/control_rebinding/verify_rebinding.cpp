#include "control_bindings.hpp"
#include "frontend.hpp"
#include "graphics_menu.hpp"
#include "controls_menu.hpp"
#include "modern_controls.hpp"
#include "menu_skin.hpp"
#include "fixture.hpp"
#include <Windows.h>
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <stdexcept>
#include <thread>

namespace cb=sote::control_bindings;
namespace ms=sote::menu_skin;
namespace cm=sote::controls_menu;
static bool bike_active=false;
extern "C" uint32_t sote_is_bike_stage_active(){return bike_active?1:0;}
int checks=0,failures=0;
void check(bool ok,const std::string& name){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<name<<"\n";}}
void require(bool ok,const std::string& name){check(ok,name);if(!ok)throw std::runtime_error(name);}
bool same(const cb::PhysicalInput& a,const cb::PhysicalInput& b){return a.keys==b.keys&&a.buttons==b.buttons&&a.axes==b.axes&&a.connected==b.connected;}
const cb::Action& find_target(const cb::Layout& l,int group,int device,cb::Token t) {
    const auto it=std::find_if(l[group].begin(),l[group].end(),[&](const cb::Action& a){return std::find(a.targets[device].begin(),a.targets[device].end(),t)!=a.targets[device].end();});
    if(it==l[group].end())throw std::runtime_error("Target not present: "+cb::token_name(t));
    return *it;
}
void pure_tests(GameMemory& memory,const std::filesystem::path& scratch) {
    std::mt19937 random(42042);
    for(int preset=0;preset<8;++preset)for(bool modern:{false,true}) {
        const auto layout=cb::make_layout(memory.table(preset,modern,modern));
        for(int g=0;g<5;++g) {
            std::set<cb::Token> owned;
            for(const auto& a:layout[g])for(const auto& target:a.targets)for(auto token:target)
                require(owned.insert(token).second,"a physical output belongs to only one logical row");
            cb::Assignments clean;
            for(int run=0;run<20;++run) {
                cb::PhysicalInput raw;raw.connected=true;
                for(auto& k:raw.keys)k=uint8_t(random()%2);
                for(auto& b:raw.buttons)b=uint8_t(random()%2);
                for(int a=0;a<6;++a)raw.axes[a]=int16_t(a<4?int(random()%65536)-32768:int(random()%32768));
                check(same(raw,clean.apply(raw,layout[g])),"unmodified physical snapshot is bit-identical, all presets/contexts/schemes");
            }
        }
    }
    const auto layout=cb::make_layout(memory.table(0));
    const auto& fire=find_target(layout,0,0,cb::key('X'));
    const auto& jump=find_target(layout,0,0,cb::key('Z'));
    cb::Assignments b;b.overrides[cb::storage_key(fire,cb::Device::Keyboard)]=cb::key('F');
    cb::PhysicalInput raw;raw.connected=true;raw.keys['F']=1;
    auto out=b.apply(raw,layout[0]);check(out.keys['X']&&!out.keys['F'],"replacement emits target and consumes physical source");
    raw.keys['F']=0;raw.keys['X']=1;out=b.apply(raw,layout[0]);check(!out.keys['X'],"old key removed");
    raw.keys['X']=0;raw.keys['Z']=1;out=b.apply(raw,layout[0]);check(out.keys['Z'],"other default control preserved");
    const auto conflict=b.conflicts(layout[0],fire,cb::Device::Keyboard,cb::key('Z'));
    check(conflict.size()==1&&conflict[0]==jump.id,"conflict finds grouped Jump / Thrust");
    const auto& snow=find_target(layout,1,0,cb::key('X'));
    check(b.conflicts(layout[1],snow,cb::Device::Keyboard,cb::key('F')).empty(),"same input may be reused in another gameplay context");
    b.overrides[cb::storage_key(jump,cb::Device::Keyboard)]=cb::key('X');
    raw={};raw.keys['X']=1;out=b.apply(raw,layout[0]);check(out.keys['Z']&&!out.keys['X'],"chained remaps read physical snapshot, not prior synthetic output");
    b.overrides[cb::storage_key(jump,cb::Device::Keyboard)]={};
    raw={};raw.keys['Z']=1;raw.keys[32]=1;out=b.apply(raw,layout[0]);check(!out.keys['Z']&&!out.keys[32],"clearing a row removes every original alias");
    std::string err;auto path=scratch/"roundtrip"/"bindings.ini";
    require(b.save(path,err),"save new assignment file");cb::Assignments restored;require(restored.load(path,err),"read saved assignment file");
    check(restored.overrides==b.overrides,"persisted values round trip including explicit unbound");
    b.overrides[cb::storage_key(fire,cb::Device::Keyboard)]=cb::key('G');require(b.save(path,err),"replace existing settings atomically");
    require(restored.load(path,err),"reload replacement file");check(restored.overrides==b.overrides,"replacement saved, not stale first file");
    std::filesystem::create_directories(scratch/"bad");std::ofstream(scratch/"bad"/"Sdata")<<"not a directory";
    check(!b.save(scratch/"bad"/"Sdata"/"bindings.ini",err)&&!err.empty(),"save failures are reported");
    std::ofstream bad(path);bad<<"[bindings]\nanything.key = key:999\nreserved.key=key:27\nwrong.pad=key:70\ninvalid.pad=axis:5:-\nvalid.pad=axis:5:+\nnone.key=none\n";bad.close();
    require(restored.load(path,err),"malformed entries tolerated");check(restored.overrides.size()==2,"invalid, wrong-device, and reserved bindings rejected");
    for(auto token:{cb::key('F'),cb::button(0),cb::button(20),cb::axis(3,-1),cb::axis(5),cb::Token{}}){cb::Token decoded;check(cb::parse_token(cb::token_text(token),decoded)&&decoded==token,"token serialization");}
    cb::Token invalid;check(!cb::parse_token("axis:0:+junk",invalid),"invalid suffix rejected");
    std::cout<<"PASS pure mapping/persistence tests\n";
}

struct Harness {
    GameMemory& memory;
    SDL_Joystick* joystick=nullptr;
    int index=-1;
    uint16_t buttons=0;float x=0,y=0;
    bool menu=true;
    cb::Context context=cb::Context::OnFoot;
    explicit Harness(GameMemory& m):memory(m) {
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
        require(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)==0,"SDL controller subsystem");
        index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,SDL_CONTROLLER_BUTTON_MAX,0);
        require(index>=0,"attach actual SDL virtual controller");
        joystick=SDL_JoystickOpen(index);require(joystick!=nullptr,"open virtual controller");
        clear();require(sote::frontend::initialize(),"production SDL frontend initialize");
        SDL_version version{};SDL_GetVersion(&version);std::cout<<"SDL runtime: "<<int(version.major)<<'.'<<int(version.minor)<<'.'<<int(version.patch)<<"\n";
        require(SDL_IsGameController(index)==SDL_TRUE,"virtual controller recognized by SDL gamecontroller API");
    }
    ~Harness(){sote::frontend::shutdown();if(joystick)SDL_JoystickClose(joystick);if(index>=0)SDL_JoystickDetachVirtual(index);SDL_Quit();}
    void clear(){test_win_keys.fill(0);if(joystick){for(int b=0;b<SDL_CONTROLLER_BUTTON_MAX;++b)SDL_JoystickSetVirtualButton(joystick,b,0);for(int a=0;a<6;++a)SDL_JoystickSetVirtualAxis(joystick,a,a>=4?-32768:0);}}
    void step(){
        memory.show_controls(menu);
        if(menu)sote_capture_native_options(memory.bytes.data());
        else {sote::menu_skin::capture(memory.bytes.data());sote_bindings_context(memory.bytes.data(),int(context));}
        const bool modern_foot=!menu&&context==cb::Context::OnFoot&&cm::current_scheme(cm::SchemeSlot::OnFoot)==cm::ControlScheme::Modern;
        if(modern_foot)sote_modern_begin(memory.bytes.data(),0x80250000);
        sote::frontend::poll_input();
        if(modern_foot)sote_modern_begin(memory.bytes.data(),0x80250000);
        if(menu)sote_capture_native_options(memory.bytes.data());
        sote::frontend::get_input(0,&buttons,&x,&y);
    }
    void neutral(){clear();step();step();}
    void keydown(int k){test_win_keys[k]=1;step();}
    void presskey(int k){neutral();keydown(k);neutral();}
    void paddown(int b){require(SDL_JoystickSetVirtualButton(joystick,b,1)==0,"SDL set virtual button");step();}
    void presspad(int b){neutral();paddown(b);neutral();}
    void target(int g,cb::Token t,int d){
        if(!cb::editor_view().editing)presskey(13);
        const auto l=cb::current_layout();const auto& a=find_target(l,g,d,t);
        const int row=int(&a-l[g].data());
        cb::scroll(-1000);int offset=0;bool found=false;
        for(int group:{0,1,4,3,2}){if(group==g){offset+=row;found=true;break;}offset+=int(l[group].size());}
        require(found,"target control section");cb::scroll(offset);step();
        if(cb::editor_view().column!=d)presskey(d==0?37:39);
        check(cb::editor_view().group==g&&cb::editor_view().row==row,"target row selected");
    }
    void begin_key_capture(){neutral();keydown(13);check(cb::editor_view().phase==cb::Phase::Release,"Enter opens release gate");step();check(cb::editor_view().phase==cb::Phase::Release,"held Enter cannot bind itself");neutral();check(cb::editor_view().phase==cb::Phase::Capture,"capture armed after all opener inputs released");}
    void begin_pad_capture(){neutral();paddown(0);check(cb::editor_view().phase==cb::Phase::Release,"A opens release gate");step();check(cb::editor_view().phase==cb::Phase::Release,"held A cannot bind itself");neutral();check(cb::editor_view().phase==cb::Phase::Capture,"pad capture armed after release");}
};
void preview(const std::filesystem::path& file) {
    const auto snapshot=ms::latest();require(bool(snapshot),"preview has live snapshot");
    const auto image=ms::render(*snapshot,1280,720);require(image.valid(),"preview renders valid surface");
    std::filesystem::create_directories(file.parent_path());std::ofstream out(file,std::ios::binary);
    out<<"P6\n"<<image.width<<" "<<image.height<<"\n255\n";
    for(size_t i=0;i<image.rgba.size();i+=4)out.write(reinterpret_cast<const char*>(image.rgba.data()+i),3);
    require(bool(out),"write CPU-rendered smoke preview");
}
void integration_tests(GameMemory& memory,const std::filesystem::path& root) {
    cm::initialize(root);sote::graphics_menu::initialize(root);ms::initialize(root);ms::set_renderer_available(true);cb::initialize(root);
    Harness h(memory);h.neutral();require(ms::latest()&&ms::latest()->rebinding,"native Controls screen activates binding model");
    check(!cb::editor_view().editing,"native preset/reference mode precedes editor");
    h.keydown(39);check(h.buttons==0x0100,"native preset Right remains available before editor");h.neutral();
    const auto before_editor_serial=ms::latest()->serial;
    h.presskey(13);check(cb::editor_view().editing,"Enter opens binding editor rather than changing native settings");
    check(ms::latest()->serial>before_editor_serial,"editor opening invalidates renderer snapshot");
    int r=cb::editor_view().row;h.presskey(40);check(cb::editor_view().row==r+1,"Down selects next action");h.presskey(38);check(cb::editor_view().row==r,"Up selects previous action");
    h.presskey(39);check(cb::editor_view().column==1,"Right selects controller column");h.presskey(37);check(cb::editor_view().column==0,"Left selects keyboard column");
    h.target(0,cb::key('X'),0);preview(root/"controls_editor.ppm");h.begin_key_capture();preview(root/"controls_capture.ppm");
    check(ms::latest()&&!ms::latest()->binding_title.empty()&&ms::render(*ms::latest()).valid(),"capture dialog is rendered from live model with original font");
    h.keydown('F');check(cb::editor_view().phase==cb::Phase::Settle,"new key saved on capture");check(h.buttons==0&&h.x==0&&h.y==0,"capture input never reaches guest");h.neutral();
    auto expected=cb::saved_assignments();check(std::any_of(expected.begin(),expected.end(),[](const auto& p){return p.second==cb::key('F');}),"key assignment persisted");
    h.menu=false;h.neutral();h.keydown('F');check(h.buttons==0x4000,"F reaches original native Fire through production poll/get_input");h.neutral();h.keydown('X');check(h.buttons==0,"old X binding suppressed in gameplay");h.neutral();h.keydown('Z');check(h.buttons==0x8000,"other gameplay keyboard binding preserved");h.neutral();h.paddown(2);check(h.buttons==0x4000,"keyboard reassignment does not change controller binding");h.neutral();
    cb::initialize(root);h.neutral();h.keydown('F');check(h.buttons==0x4000,"saved binding survives full binding subsystem restart");h.neutral();
    h.menu=true;h.neutral();h.target(0,cb::button(2),1);h.begin_pad_capture();h.paddown(3);
    check(cb::editor_view().phase==cb::Phase::ConflictRelease,"same-context controller conflict prompts, not silent double assignment");
    preview(root/"controls_conflict.ppm");
    check(ms::latest()&&ms::latest()->binding_detail.find("Jetpack")!=std::string::npos,"conflict dialog identifies other action");h.neutral();check(cb::editor_view().phase==cb::Phase::Conflict,"conflict confirmation waits for candidate release");
    h.presspad(1);check(cb::saved_assignments()==expected,"B cancels conflict without overwriting bindings");
    h.begin_pad_capture();h.paddown(3);h.neutral();h.presspad(0);
    check(cb::editor_view().phase==cb::Phase::Browse,"confirmed conflict returns to editor after release");
    h.menu=false;h.neutral();h.paddown(3);check(h.buttons==0x4000,"reassigned Y fires without also toggling Jetpack");h.neutral();h.paddown(2);check(h.buttons==0,"old controller X removed");h.neutral();h.paddown(1);check(h.buttons==0,"old controller B alias removed");h.neutral();h.keydown('J');check(h.buttons==2,"conflict unbinds only controller, not keyboard");h.neutral();
    h.menu=true;h.neutral();h.target(0,cb::button(2),1);h.begin_pad_capture();h.paddown(1);h.neutral();
    const auto with_b=cb::saved_assignments();
    check(std::any_of(with_b.begin(),with_b.end(),[](auto const& p){return p.second==cb::button(1);}),"B itself is assignable during capture");
    h.begin_pad_capture();require(SDL_JoystickSetVirtualAxis(h.joystick,5,32767)==0,"set virtual right trigger");h.step();h.neutral();
    h.menu=false;h.neutral();SDL_JoystickSetVirtualAxis(h.joystick,5,32767);h.step();check(h.buttons==0x4000,"captured RT reaches native Fire");h.neutral();h.paddown(1);check(h.buttons==0,"previous custom B binding removed");h.neutral();
    h.menu=true;h.neutral();h.target(0,cb::key('X'),0);auto before=cb::saved_assignments();h.begin_key_capture();test_window_focused=false;h.step();test_window_focused=true;h.neutral();check(cb::saved_assignments()==before,"focus loss cancels pending capture");
    h.begin_key_capture();h.presskey(27);check(cb::saved_assignments()==before,"Escape cancels capture");
    h.presskey(46);check(cb::saved_assignments().at(cb::storage_key(find_target(cb::current_layout(),0,0,cb::key('X')),cb::Device::Keyboard)).kind==cb::Kind::None,"Delete clears selected binding");
    h.menu=false;h.neutral();h.keydown('F');check(h.buttons==0,"cleared custom F no longer fires");h.neutral();h.keydown('X');check(h.buttons==0,"clear does not restore old default");h.neutral();
    h.menu=true;h.neutral();h.target(0,cb::key('X'),0);h.presskey(120);check(cb::editor_view().phase==cb::Phase::Defaults,"F9 requests defaults with confirmation");h.presskey(13);check(cb::saved_assignments().empty(),"restore clears overrides in selected context");
    h.menu=false;h.neutral();h.keydown('X');check(h.buttons==0x4000,"default key restored");h.neutral();h.paddown(3);check(h.buttons==2,"default conflicting action restored");h.neutral();
    // Clear both Pause columns, then prove reserved Escape still gets the
    // player back to a menu without an auto-close on the next held poll.
    h.menu=true;h.neutral();h.target(0,cb::key(13),0);h.presskey(46);
    h.target(0,cb::button(6),1);h.presspad(2);
    h.menu=false;h.neutral();h.keydown(13);check(h.buttons==0,"Pause keyboard can be cleared");h.neutral();
    h.paddown(6);check(h.buttons==0,"Pause controller can be cleared");h.neutral();
    h.keydown(27);check(h.buttons==0x1000,"reserved Escape still pauses with both Pause bindings cleared");
    h.step();check(h.buttons==0,"held recovery Escape emits no repeated Pause pulse");
    h.menu=true;h.step();check(h.buttons==0,"held Escape does not close newly opened Controls menu");h.neutral();
    // Context-local reassignment of two different native actions to one key.
    h.menu=true;h.neutral();h.target(0,cb::key('Z'),0);h.begin_key_capture();h.keydown('F');h.neutral();
    h.target(1,cb::key('X'),0);h.begin_key_capture();h.keydown('F');check(cb::editor_view().phase==cb::Phase::Settle,"no cross-context conflict");h.neutral();
    h.menu=false;h.context=cb::Context::OnFoot;h.neutral();h.keydown('F');check(h.buttons==0x8000,"On Foot F is Jump / Thrust");h.neutral();
    h.context=cb::Context::Snowspeeder;h.keydown('F');check(h.buttons==0x4000,"Snowspeeder F is Fire");h.neutral();
    // Movement is remappable independently; native stick and Modern input
    // snapshot continue to be produced by the existing frontend code.
    h.menu=true;h.neutral();h.target(0,cb::key('W'),0);h.begin_key_capture();h.keydown('T');h.neutral();
    h.menu=false;h.context=cb::Context::OnFoot;h.neutral();h.keydown('T');check(h.y>0.60f,"new movement key reaches N64 analog stick");h.neutral();h.keydown('W');check(h.y==0,"old movement key suppressed");h.neutral();
    // Modern uses a separate physical path. Test the production path, not
    // merely the pure snapshot transformer, and preserve native preset masks.
    cm::cycle_scheme(cm::SchemeSlot::OnFoot,1);
    for(int preset=0;preset<8;++preset) {
        memory.set_preset(preset);h.menu=false;h.context=cb::Context::OnFoot;h.neutral();
        SDL_JoystickSetVirtualAxis(h.joystick,5,32767);h.step();
        check(h.buttons==memory.masks[preset][12],"Modern RT retains native Fire mask for every preset");h.neutral();
    }
    memory.set_preset(0);h.menu=true;h.neutral();h.target(0,cb::axis(5),1);h.begin_pad_capture();h.paddown(8);h.neutral();
    h.menu=false;h.neutral();h.paddown(8);check(h.buttons==memory.masks[0][12],"Modern Fire reassigned to RS click reaches native mask");h.neutral();
    SDL_JoystickSetVirtualAxis(h.joystick,5,32767);h.step();check(h.buttons==0,"Modern old RT suppressed after reassignment");h.neutral();
    h.menu=true;h.neutral();h.target(0,cb::axis(3,-1),1);h.begin_pad_capture();h.paddown(7);h.neutral();
    h.menu=false;h.neutral();const auto old_pitch=sote::modern_controls::pitch_degrees();h.paddown(7);
    check(h.buttons==0&&sote::modern_controls::pitch_degrees()>old_pitch,"rebound look input reaches Modern analog aim without native button leakage");h.neutral();
    SDL_JoystickSetVirtualAxis(h.joystick,3,-32768);h.step();const auto after_old=sote::modern_controls::pitch_degrees();h.step();
    check(sote::modern_controls::pitch_degrees()==after_old,"old Modern look axis suppressed");h.neutral();
    // Save failure must leave the previous live binding intact too.
    h.menu=true;h.neutral();h.target(0,cb::axis(5),1);before=cb::saved_assignments();
    const auto settings_dir=root/"Sdata", parked=root/"Sdata.saved";
    std::filesystem::rename(settings_dir,parked);std::ofstream(settings_dir)<<"test file blocks directory";
    h.begin_pad_capture();h.paddown(15);h.neutral();
    check(cb::saved_assignments()==before&&cb::editor_view().status.find("Not saved")!=std::string::npos,"save failure leaves live settings unchanged and shows error");
    std::filesystem::remove(settings_dir);std::filesystem::rename(parked,settings_dir);
    // Reset this Modern section without destroying Classic or Snowspeeder edits.
    h.presskey(120);h.presskey(13);const auto restored_section=cb::saved_assignments();
    check(std::any_of(restored_section.begin(),restored_section.end(),[](auto const& p){return p.first.find("classic.snowspeeder")!=std::string::npos;}),"restore section preserves another context");
    check(std::any_of(restored_section.begin(),restored_section.end(),[](auto const& p){return p.first.find("classic.foot")!=std::string::npos;}),"restore Modern section preserves Classic bindings");
    cm::cycle_scheme(cm::SchemeSlot::OnFoot,1);cm::cycle_scheme(cm::SchemeSlot::Bike,1);bike_active=true;
    h.menu=false;h.context=cb::Context::Bike;h.neutral();
    SDL_JoystickSetVirtualAxis(h.joystick,5,32767);h.step();check(h.buttons==cb::bike_button(true),"Modern bike full RT accelerates");h.neutral();
    h.menu=true;h.neutral();h.target(3,cb::axis(5),1);h.begin_pad_capture();h.paddown(8);h.neutral();
    h.menu=false;h.neutral();h.paddown(8);check(h.buttons==cb::bike_button(true),"rebound Modern bike accelerator reaches production duty-cycle path");h.neutral();
    SDL_JoystickSetVirtualAxis(h.joystick,5,32767);h.step();check(h.buttons==0,"old Modern bike trigger suppressed");h.neutral();
    cm::cycle_scheme(cm::SchemeSlot::Bike,1);bike_active=false;h.context=cb::Context::OnFoot;
    // Actual controller detach while capture is armed, not just a mock flag.
    h.menu=true;h.neutral();h.target(0,cb::button(2),1);h.begin_pad_capture();before=cb::saved_assignments();
    require(SDL_JoystickDetachVirtual(h.index)==0,"detach actual SDL virtual controller");h.index=-1;h.step();h.neutral();
    check(cb::saved_assignments()==before&&cb::editor_view().status.find("disconnected")!=std::string::npos,"hot unplug cancels pending binding without saving");
    h.presskey(27);check(h.buttons==0&&!cb::editor_view().editing,"Back closes editor without exiting native Options");
    h.keydown(39);check(h.buttons==0x100,"native preset navigation restored after editor closes");h.neutral();
    h.keydown(27);check(h.buttons==0x4000,"Escape in reference mode emits one native Back pulse");h.step();check(h.buttons==0,"native Back pulse not held");
    std::cout<<"PASS production SDL frontend/menu/persistence smoke tests\n";
}
int main(int argc,char** argv) {
    std::cout << std::unitbuf;
    try {
        if(argc<2)return 2;
        auto root=std::filesystem::path(argv[1]);std::filesystem::create_directories(root);
        GameMemory memory;if(argc>2)require(memory.load_native_tables(argv[2]),"load user-owned native control tables for all eight presets");
        pure_tests(memory,root);
        integration_tests(memory,root/"runtime");
    } catch(const std::exception& e){++failures;std::cerr<<"FATAL "<<e.what()<<"\n";}
    std::cout<<"Rebinding checks: "<<checks<<"; failures: "<<failures<<"\n";return failures?1:0;
}
