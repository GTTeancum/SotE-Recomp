#pragma once
#include "control_bindings.hpp"
#include "menu_skin.hpp"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

struct GameMemory {
    std::vector<uint8_t> bytes=std::vector<uint8_t>(0x800000);
    uint32_t strings=0x80220000;
    std::array<std::array<uint16_t,48>,8> masks{};
    std::array<std::string,48> labels{};
    void byte(uint32_t a,uint8_t v){bytes[(a&0x7fffff)^3]=v;}
    void half(uint32_t a,uint16_t v){byte(a,uint8_t(v>>8));byte(a+1,uint8_t(v));}
    void word(uint32_t a,uint32_t v){half(a,uint16_t(v>>16));half(a+2,uint16_t(v));}
    uint32_t text(const std::string& s){auto p=strings;for(char c:s)byte(strings++,uint8_t(c));byte(strings++,0);return p;}
    GameMemory() {
        // Explicit, compact fixture for the native standard control table.
        // --rom validation overwrites it with the user's real tables in RAM.
        masks[0]={0,0,0,0x1000,0,0,0,0,0x800,0x400,0x200,0x100,
            0x4000,0x8000,0x2000,0x10,0,0,0x10,4,2,0x8000,8,0x21,
            0x30,0,0,0x8000,0x200e,1,0x4000,0x4004,0x800a,1,0x2020,0x10,
            0xc008,0x2016,0x21,0x10,0x8000,0x21,0x4000,0x200c,0x8002,0,0,0};
        for(int p=1;p<8;++p)masks[p]=masks[0];
        labels={"View Lock","Invert","Advanced","Pause","Move","","","","Camera","","","",
            "Fire","Jump","Aim/Look","Strafe","Strafe Lf","Strafe Rt","Activate","Duck","Jetpack","Thrust","Weapon","Camera",
            "Brakes","Lf Brake","Rt Brake","Thrust","Harpoon","Camera","Fire","Brakes","Accelerate","Camera","Lf Kick","Rt Kick",
            "Fire","Missile","Camera","Decelerate","Accelerate","Camera","Fire","Missile","Roll","Roll Lf","Roll Rt","Autolevel"};
        for(int a=0;a<48;++a)word(0x800da70c+a*4,text(labels[a]));
        install_masks();
        // Artificial glyphs: no game-font asset is distributed by this test.
        word(0x800da818,0x80210000);half(0x800da81c,13);
        for(unsigned i=0;i<95;++i){uint32_t a=0x800da818+i*14;half(a+6,7);half(a+8,0);half(a+10,0);half(a+12,0);half(a+14,0);half(a+16,5);half(a+18,9);}
        half(0x800da818+('A'-32)*14+6,9);
        for(int i=0;i<4096;++i)byte(0x80210000+i,0xff);
        word(0x8013ce30,0x80200000);word(0x8013ce2c,0x80201000);
        for(int i=0;i<80;++i)half(0x80111110+i*4,uint16_t(-1000));
        const char* names[]={"", "Overlay Displays","Seeker Camera","Sound Effects","Music","Sound Panning","Controls"};
        word(0x800dd61c,text("Return to Main Menu"));
        for(int i=1;i<22;++i)word(0x800dd61c+i*4,text(i==1?"Standard":"Alternate"));
        for(int i=0;i<7;++i){word(0x800dd5e8+i*4,text(names[i]));for(int j=0;j<9;++j)half(0x800dd674+i*18+j*2,uint16_t(i==0?0:1+j));}
        byte(0x8018bbfd,0);set_preset(0);show_controls(true);
        word(0x8018e998,0x3f90a3d7);word(0x8018e99c,0x0a3d70a4); // 1/60 double
    }
    void install_masks(){for(int p=0;p<8;++p)for(int a=0;a<48;++a)half(0x800e6808+p*96+a*2,masks[p][a]);}
    void set_preset(int p){half(0x800d252c,uint16_t(p));byte(0x8018bbf8+0x1d,uint8_t(p));}
    void show_controls(bool show){word(0x800dd5e4,show?1:0);word(0x800d0948,0);half(0x800dd5e0,6);word(0x800d0954,show?0x3f800000:0);}
    sote::control_bindings::NativeTable table(int preset,bool modern_foot=false,bool modern_bike=false) const {
        return {preset,masks[preset],labels,modern_foot,modern_bike};
    }
    // An optional, caller-owned decompressed recomp ROM. Only control-table
    // facts are read; the ROM is neither copied into the package nor executed.
    bool load_native_tables(const std::filesystem::path& rom) {
        std::ifstream in(rom,std::ios::binary);if(!in)return false;
        constexpr std::streamoff offset=0xc00000+(0x800e6808-0x80001ec0);
        in.seekg(offset);std::array<uint8_t,8*96> raw{};
        if(!in.read(reinterpret_cast<char*>(raw.data()),raw.size()))return false;
        for(int p=0;p<8;++p)for(int a=0;a<48;++a)masks[p][a]=uint16_t((unsigned(raw[p*96+a*2])<<8)|raw[p*96+a*2+1]);
        for(int p=0;p<8;++p)if(masks[p][3]!=0x1000)return false;
        install_masks();
        // Use the caller-owned ROM's original glyph descriptor and atlas for
        // visual smoke evidence. No font bytes are embedded in this test.
        auto copy_static=[&](uint32_t address,size_t length) {
            std::vector<uint8_t> data(length);
            in.seekg(0xc00000+std::streamoff(address)-0x80001ec0);
            if(!in.read(reinterpret_cast<char*>(data.data()),data.size()))return false;
            for(size_t i=0;i<length;++i)byte(address+uint32_t(i),data[i]);
            return true;
        };
        if(!copy_static(0x800da818,1340))return false;
        uint32_t pixels=0;for(int i=0;i<4;++i)pixels=(pixels<<8)|bytes[((0x800da818+i)&0x7fffff)^3];
        if(pixels<0x80001ec0||pixels+4096>0x800ee740||!copy_static(pixels,4096))return false;
        return true;
    }
};
