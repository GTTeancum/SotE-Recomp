#include <assert.h>
#include <stdio.h>
#include "triangle_material.h"

int main(void) {
    const uint32_t w0=0xFC119623u, w1=0xFF2FFFFFu;
    assert(xb_triangle_texture_primitive(w0,w1,0));
    for(unsigned cycle=1;cycle<4;cycle++)
        assert(!xb_triangle_texture_primitive(w0,w1,cycle));
    for(unsigned bit=0;bit<32;bit++) {
        assert(!xb_triangle_texture_primitive(w0^(1u<<bit),w1,0));
        assert(!xb_triangle_texture_primitive(w0,w1^(1u<<bit),0));
    }
    /* Decode the GBI combine fields independently of the classifier.
       RGB: TEXEL0(1), ZERO(15/7), PRIMITIVE(3), ZERO(7).
       Alpha: TEXEL0(1), ZERO(7), PRIMITIVE(3), ZERO(7).
       Both encoded cycles agree; hardware one-cycle operation therefore
       selects the same expression regardless of which cycle is inspected. */
    assert(((w0>>20)&15)==1 && ((w1>>28)&15)==15);
    assert(((w0>>15)&31)==3 && ((w1>>15)&7)==7);
    assert(((w0>>12)&7)==1 && ((w1>>12)&7)==7);
    assert(((w0>>9)&7)==3 && ((w1>>9)&7)==7);
    assert(((w0>>5)&15)==1 && ((w1>>24)&15)==15);
    assert((w0&31)==3 && ((w1>>6)&7)==7);
    assert(((w1>>21)&7)==1 && ((w1>>3)&7)==7);
    assert(((w1>>18)&7)==3 && (w1&7)==7);
    puts("triangle material: exact recognition, rejection, and RGBA mux decode PASS");
    return 0;
}
