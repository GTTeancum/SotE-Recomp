#pragma once
#include <stdint.h>

namespace xb_audio {
// Both RSP vector bytes and guest words use reversed byte order in memory.
// One aligned 32-bit copy therefore transfers four consecutive RSP bytes.
template<bool store,class Vector>
static inline void transfer_word(Vector& v,uint8_t* dmem,unsigned element,unsigned address) {
    uint8_t* mem=dmem+(address&4095);
    const unsigned offset=12-(element&12);
    if constexpr(store)__builtin_memcpy_inline(mem,reinterpret_cast<const uint8_t*>(v.u128)+offset,4);
    else __builtin_memcpy_inline(reinterpret_cast<uint8_t*>(v.u128)+offset,mem,4);
}

// Audio resampling reads four consecutive samples on halfword boundaries.
template<bool store,class Vector>
static inline void transfer_samples(Vector& v,uint8_t* dmem,unsigned element,unsigned address) {
    for(unsigned i=0;i<8;i+=2){
        uint8_t* mem=dmem+(((address+i)&4095)^2);
        unsigned offset=14-((element+i)&14);
        if constexpr(store)__builtin_memcpy_inline(mem,reinterpret_cast<const uint8_t*>(v.u128)+offset,2);
        else __builtin_memcpy_inline(reinterpret_cast<uint8_t*>(v.u128)+offset,mem,2);
    }
}

template<bool store,class Vector>
static inline void transfer_words(Vector& v,uint8_t* dmem,unsigned e,unsigned address,unsigned bytes) {
    switch(bytes){
    case 16: transfer_word<store>(v,dmem,e+12,address+12); [[fallthrough]];
    case 12: transfer_word<store>(v,dmem,e+8,address+8); [[fallthrough]];
    case 8:  transfer_word<store>(v,dmem,e+4,address+4); [[fallthrough]];
    case 4:  transfer_word<store>(v,dmem,e,address);
    }
}
}
