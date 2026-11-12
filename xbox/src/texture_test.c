#include "texture.h"
#include <string.h>
void xb_unsupported(uint32_t id) __attribute__((noreturn));
volatile uint32_t sote_texture_checks;
static void expect(uint32_t actual,uint32_t expected) {
    if(actual!=expected)xb_unsupported(0xE5021000+sote_texture_checks);
    sote_texture_checks++;
}
void xb_texture_selftest(uint8_t* ram) {
    const unsigned src=0x7f0000;
    /* Same 64x128 I4 block-load geometry as the retail startup font. Distinct
       rows expose incorrect DXT swaps; every texel has a known expected value. */
    for(unsigned y=0;y<128;y++)for(unsigned x=0;x<64;x+=2) {
        unsigned a=(x+y)&15,b=(x+y+1)&15;ram[(src+y*32+x/2)^3]=(a<<4)|b;
    }
    xb_texture_command(ram,0xFD900000,src);
    xb_texture_command(ram,0xF5900000,0x07080200);
    xb_texture_command(ram,0xF3000000,0x077FF200);
    xb_texture_command(ram,0xF5800800,0x00080200);
    xb_texture_command(ram,0xF2000000,0x000FC1FC);
    for(unsigned y=0;y<128;y++)for(unsigned x=0;x<64;x++)
        expect(xb_texture_pixel(0,x,y,0),((x+y)&15)*17*0x01010101u);
    /* IA8 alpha and intensity are independent. */
    ram[src^3]=0xA3;
    xb_texture_command(ram,0xFD680000,src);
    xb_texture_command(ram,0xF5680200,0);
    xb_texture_command(ram,0xF4000000,0);
    xb_texture_command(ram,0xF2000000,0);
    expect(xb_texture_pixel(0,0,0,0),0x33AAAAAA);
    /* Palette entry 0: opaque red, then IA16 interpretation of same bytes. */
    ram[src^3]=0xF8;ram[(src+1)^3]=1;
    xb_texture_command(ram,0xFD100000,src);
    xb_texture_command(ram,0xF5100100,0x07000000);
    xb_texture_command(ram,0xF0000000,0x07000000);
    ram[src^3]=0;
    xb_texture_command(ram,0xFD480000,src);
    xb_texture_command(ram,0xF5480200,0);
    xb_texture_command(ram,0xF4000000,0);
    expect(xb_texture_pixel(0,0,0,2),0xFFFF0000);
    expect(xb_texture_pixel(0,0,0,3),0x01F8F8F8);
    /* RGBA32 is split between TMEM banks rather than stored interleaved. */
    const uint8_t rgba[8]={10,20,30,40,50,60,70,80};
    for(unsigned i=0;i<8;i++)ram[(src+i)^3]=rgba[i];
    xb_texture_command(ram,0xFD180001,src);
    xb_texture_command(ram,0xF5180200,0);
    xb_texture_command(ram,0xF4000000,0x00004000);
    xb_texture_command(ram,0xF2000000,0x00004000);
    expect(xb_texture_pixel(0,0,0,0),0x280A141E);
    expect(xb_texture_pixel(0,1,0,0),0x50323C46);
    memset(ram+src,0,4096);
    memset(xb_tiles,0,sizeof(xb_tiles));
}
