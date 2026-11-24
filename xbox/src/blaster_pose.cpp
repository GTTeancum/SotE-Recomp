#include "blaster_pose.h"
#include "first_person_blaster.hpp"
#include "rdram_guard.h"
#include <cstring>

void xb_update_blaster(uint8_t* ram,uint32_t player,int train) {
    auto word=[ram](uint32_t a){uint32_t v;std::memcpy(&v,ram+(a&0x7fffff),4);return v;};
    auto half=[ram](uint32_t a){uint16_t v;std::memcpy(&v,ram+((a&0x7fffff)^2),2);return v;};
    auto scalar=[&](uint32_t a){auto bits=word(a);float f;std::memcpy(&f,&bits,4);return f;};
    bool visible=player>=0x80000000u && xb_rdram_range_valid(player-0x80000000u,0x264,xb_rdram_committed_limit)
        && word(0x800DD2B0)==2;
    if(visible && train)
        visible=word(0x800D08A8)==4 && !(word(player+0x74)&0x400) && word(player)==0x48756D54;
    else if(visible)
        visible=player==word(0x80112B30) && half(0x8018DDE8)==0 &&
                half(player+0xB8)==0 && scalar(player+0xB4)>0 && scalar(0x800DEB78)<=0;
    uint64_t bits=(uint64_t(word(0x8018E998))<<32)|word(0x8018E99C);
    double dt;std::memcpy(&dt,&bits,8);
    sote::first_person_blaster::animation.update(visible?player:0,dt);
}
