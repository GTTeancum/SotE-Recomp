#include "modern_platform.hpp"
#include <cstdio>
#include <cstring>
namespace {
bool initialized;
bool modern;
void initialize() {
    if(initialized)return;
    initialized=true;
    // Explicit setting until the Xbox options UI exposes scheme selection.
    // Flight retains its original controller regardless of this on-foot setting.
    FILE* file=std::fopen("D:\\xbox-controls.cfg","rb");
    if(file){char line[128];while(std::fgets(line,sizeof(line),file))
        if(std::strncmp(line,"on_foot=modern",14)==0)modern=true;std::fclose(file);}
}
}
namespace sote::controls_menu {
ControlScheme current_scheme(SchemeSlot slot) {
    initialize();return slot==SchemeSlot::OnFoot && modern?ControlScheme::Modern:ControlScheme::Classic;
}
ModernControlsTuning modern_controls_tuning(){return {};}
}
extern "C" void xb_modern_publish(int connected,float move_x,float move_y,float look_x,float look_y) {
    sote::modern_controls::publish({{move_x,move_y},{look_x,look_y},connected!=0});
}
extern "C" unsigned xb_modern_buttons(unsigned buttons) {
    return sote::modern_controls::aiming_active()?sote::modern_controls::map_buttons(buttons):buttons;
}
