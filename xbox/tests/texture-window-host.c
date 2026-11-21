#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "texture_window.h"

/* Independent reference for the observed I4 TMEM tile: line=4 words,
   mask T=6, unmasked S. Deliberately nonperiodic over 64 S texels. */
static uint8_t memory[4096];
static unsigned sample(int s,unsigned t) {
    unsigned y=t&63,x=(unsigned)s;
    unsigned a=((32*y+(x>>1))^((y&1)*4))&4095;
    return (x&1)?memory[a]&15:memory[a]>>4;
}
static void check(float a,float b,float c) {
    float s[3]={a,b,c};int origin;unsigned width;
    assert(xb_texture_s_window(s,&origin,&width));
    assert(width && !(width&(width-1)) && width<=4096);
    unsigned pixels[4096];
    for(unsigned y=0;y<64;y++) {
        for(unsigned x=0;x<width;x++)pixels[x]=sample((int)x+origin,y);
        for(unsigned i=0;i<=20;i++)for(unsigned j=0;j<=20-i;j++) {
            /* Positive normalized weights also cover perspective-correct
               interpolation's convex range. Check both bilinear taps. */
            float t=(s[0]*i+s[1]*j+s[2]*(20-i-j))/20.0f;
            int tap=(int)floorf(t);
            assert(tap-origin>=0 && (unsigned)(tap+1-origin)<width);
            assert(pixels[tap-origin]==sample(tap,y));
            assert(pixels[tap+1-origin]==sample(tap+1,y));
        }
    }
}
int main(void) {
    for(unsigned i=0;i<4096;i++)memory[i]=(uint8_t)((i*73)^(i>>4)^(i>>8));
    check(0,62,31);check(-0.5f,63.5f,19);check(62,128,75);
    check(-65,-1,-33);check(8190,8193,8191.5f);check(20,20,20);
    int origin;unsigned width;
    float large[3]={0,4096,1},nan[3]={NAN,0,1},inf[3]={0,INFINITY,1};
    assert(!xb_texture_s_window(large,&origin,&width));
    assert(!xb_texture_s_window(nan,&origin,&width));
    assert(!xb_texture_s_window(inf,&origin,&width));
    /* Prove the fixture distinguishes real TMEM from an invented64-wrap. */
    unsigned differs=0;
    for(unsigned x=0;x<64;x++)differs|=sample(x,0)^sample(x+64,0);
    assert(differs);
    puts("texture window: footprint, signed/overflow TMEM, bilinear taps, guards PASS");
}
