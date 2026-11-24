#pragma once
#include <xmmintrin.h>
#include <stdint.h>

/* The Xbox CPU has MMX and SSE1. Each half holds four of the RSP's eight
   lanes. Its accumulator is three parallel 16-bit slices, including bits
   that do not appear in the current instruction's saturated result. */
namespace xb_audio {
// Current Clang lowers its legacy MMX intrinsics through SSE2. Use MMX
// register constraints explicitly for the Pentium III target. A memory
// alternative makes Clang spill each intermediate even when already in MMX.
#define XB_PACKED_BINARY(name, instruction) \
    static inline __m64 name(__m64 a,__m64 b) { \
        __asm__(instruction " %1,%0" : "+y"(a) : "y"(b)); return a; \
    }
XB_PACKED_BINARY(add, "paddw")
XB_PACKED_BINARY(sub, "psubw")
XB_PACKED_BINARY(signed_add_sat, "paddsw")
XB_PACKED_BINARY(signed_sub_sat, "psubsw")
XB_PACKED_BINARY(minimum, "pminsw")
XB_PACKED_BINARY(maximum, "pmaxsw")
XB_PACKED_BINARY(add_unsigned_sat, "paddusw")
XB_PACKED_BINARY(sub_unsigned_sat, "psubusw")
XB_PACKED_BINARY(mul_low, "pmullw")
XB_PACKED_BINARY(mul_high_signed, "pmulhw")
XB_PACKED_BINARY(mul_high_unsigned, "pmulhuw")
XB_PACKED_BINARY(equal, "pcmpeqw")
XB_PACKED_BINARY(greater, "pcmpgtw")
XB_PACKED_BINARY(bit_and, "pand")
XB_PACKED_BINARY(bit_andnot, "pandn")
XB_PACKED_BINARY(bit_or, "por")
XB_PACKED_BINARY(bit_xor, "pxor")
XB_PACKED_BINARY(unpack_low, "punpcklwd")
XB_PACKED_BINARY(unpack_high, "punpckhwd")
XB_PACKED_BINARY(saturated_pack, "packssdw")
#undef XB_PACKED_BINARY

static inline __m64 zeros(){__m64 v; __asm__("pxor %0,%0":"=y"(v)); return v;}
static inline __m64 sign(__m64 v){__asm__("psraw $15,%0":"+y"(v));return v;}
static inline __m64 top_bit(__m64 v){__asm__("psrlw $15,%0":"+y"(v));return v;}
static inline __m64 twice(__m64 v){return add(v,v);}
template<unsigned mask> static inline __m64 shuffle(__m64 v){
    __m64 out;__asm__("pshufw %2,%1,%0":"=y"(out):"y"(v),"i"(mask));return out;
}
static inline __m64 broadcast(int value){
    __m64 v;__asm__("movd %1,%0":"=y"(v):"rm"(value));return shuffle<0>(v);
}

enum class Multiply { VMUDL, VMUDM, VMUDN, VMUDH, VMADH, VMADM, VMADN, VMULF, VMACF };

static inline __m64 load_half(const uint64_t& value) {
    __m64 lanes;
    __builtin_memcpy(&lanes,&value,sizeof(lanes));
    return lanes;
}
static inline void store_half(uint64_t& value,__m64 lanes) {
    __builtin_memcpy(&value,&lanes,sizeof(lanes));
}

template<unsigned e,class Vector>
static inline __m64 select_half(const Vector& v,unsigned half) {
    if constexpr(e>=8)return broadcast(v.s16(e-8));
    __m64 lanes=load_half(v.u128[half]);
    if constexpr(e<2)return lanes;
    // RSP element 0 occupies the high end of the host vector.
    if constexpr(e==2)return shuffle<_MM_SHUFFLE(3,3,1,1)>(lanes);
    if constexpr(e==3)return shuffle<_MM_SHUFFLE(2,2,0,0)>(lanes);
    constexpr unsigned lane=3-(e&3);
    return shuffle<_MM_SHUFFLE(lane,lane,lane,lane)>(lanes);
}

// VCOL stores each carry as an all-ones lane. Applying it to the
// smaller operand first preserves signed saturation at both endpoints.
template<unsigned e,class Core,class Vector>
inline void vector_add(Core& core,Vector& dest,const Vector& lhs,const Vector& rhs) {
    const __m64 selected[]={select_half<e>(rhs,0),select_half<e>(rhs,1)};
    for(unsigned half=0;half<2;half++){
        const __m64 a=load_half(lhs.u128[half]),b=selected[half];
        const __m64 carry=load_half(core.vpu.vcol.u128[half]);
        store_half(core.vpu.accl.u128[half],sub(add(a,b),carry));
        const __m64 small=signed_sub_sat(minimum(a,b),carry);
        store_half(dest.u128[half],signed_add_sat(small,maximum(a,b)));
        store_half(core.vpu.vcol.u128[half],zeros());
        store_half(core.vpu.vcoh.u128[half],zeros());
    }
}

// Each returned carry is either zero or -1. Subtract it from the next slice.
static inline __m64 carry_mask(__m64 lhs,__m64 rhs,__m64 sum) {
    __m64 no_carry=equal(sum,add_unsigned_sat(lhs,rhs));
    return equal(no_carry,zeros());
}

static inline __m64 choose(__m64 mask,__m64 yes,__m64 no){
    return bit_or(bit_and(mask,yes),bit_andnot(mask,no));
}

// VGE and VCL clamp the envelope ramp. VCL can retain either comparison
// flag from the previous instruction, independently for each sample.
template<bool clip,unsigned e,class Core,class Vector>
inline void envelope_compare(Core& core,Vector& dest,const Vector& lhs,const Vector& rhs){
    const __m64 selected[]={select_half<e>(rhs,0),select_half<e>(rhs,1)};
    for(unsigned half=0;half<2;half++){
        const __m64 a=load_half(lhs.u128[half]),b=selected[half],zero=zeros();
        const __m64 low=load_half(core.vpu.vcol.u128[half]);
        const __m64 high=load_half(core.vpu.vcoh.u128[half]);
        __m64 le,ge,result;
        if constexpr(clip){
            const __m64 sum=add(a,b),sum_zero=equal(sum,zero);
            const __m64 no_carry=equal(sum,add_unsigned_sat(a,b));
            const __m64 extended=load_half(core.vpu.vce.u128[half]);
            le=choose(bit_andnot(high,low),
                      choose(extended,bit_or(sum_zero,no_carry),bit_and(sum_zero,no_carry)),
                      load_half(core.vpu.vccl.u128[half]));
            ge=choose(bit_or(low,high),load_half(core.vpu.vcch.u128[half]),
                      equal(sub_unsigned_sat(b,a),zero));
            result=choose(choose(low,le,ge),choose(low,sub(zero,b),b),a);
            store_half(core.vpu.vce.u128[half],zero);
        }else{
            le=bit_or(greater(a,b),bit_andnot(bit_and(low,high),equal(a,b)));
            ge=zero;
            result=maximum(a,b);
        }
        store_half(core.vpu.vccl.u128[half],le);
        store_half(core.vpu.vcch.u128[half],ge);
        store_half(core.vpu.accl.u128[half],result);
        store_half(dest.u128[half],result);
        store_half(core.vpu.vcol.u128[half],zero);
        store_half(core.vpu.vcoh.u128[half],zero);
    }
}

template<Multiply op,unsigned e,class Core,class Vector>
inline void multiply(Core& core,Vector& dest,const Vector& lhs,const Vector& rhs) {
    using M=Multiply;
    constexpr bool fractional=op==M::VMULF || op==M::VMACF;
    constexpr bool high=op==M::VMUDH || op==M::VMADH;
    constexpr bool accumulate=op==M::VMADM || op==M::VMADN || op==M::VMACF;
    // Broadcasts can read either half. Snapshot both before writing an
    // aliased destination (e.g. VMADH v30,v2,v30[7]).
    const __m64 selected[2]={select_half<e>(rhs,0),select_half<e>(rhs,1)};
    for(unsigned half=0;half<2;half++){
        const __m64 a=load_half(lhs.u128[half]), b=selected[half];
        const __m64 zero=zeros();
        __m64 low=mul_low(a,b),mid,top,result;
        if constexpr(high || fractional)mid=mul_high_signed(a,b);
        else {
            mid=mul_high_unsigned(a,b);
            if constexpr(op==M::VMADM || op==M::VMUDM)
                mid=sub(mid,bit_and(sign(a),b));
            if constexpr(op==M::VMADN || op==M::VMUDN)
                mid=sub(mid,bit_and(sign(b),a));
        }
        top=sign(mid);
        if constexpr(op==M::VMUDL){low=mid;mid=top=zero;}
        if constexpr(high){
            top=mid;mid=low;
            if constexpr(op==M::VMADH){
                const __m64 previous=load_half(core.vpu.accm.u128[half]);
                mid=add(previous,low);
                top=sub(add(top,load_half(core.vpu.acch.u128[half])),
                                 carry_mask(previous,low,mid));
            }else low=zero;
        }
        if constexpr(fractional){
            mid=bit_or(twice(mid),top_bit(low));
            low=twice(low);
            if constexpr(op==M::VMULF){
                // Add the rounding bias to the full 48-bit doubled product.
                __m64 carry=sign(low);
                low=bit_xor(low,broadcast(-32768));
                top=sub(top,bit_and(carry,equal(mid,broadcast(-1))));
                mid=sub(mid,carry);
            }
        }
        if constexpr(accumulate){
            const __m64 old_low=load_half(core.vpu.accl.u128[half]);
            const __m64 old_mid=load_half(core.vpu.accm.u128[half]);
            const __m64 sum_low=add(old_low,low),sum_mid=add(old_mid,mid);
            const __m64 from_low=carry_mask(old_low,low,sum_low);
            const __m64 from_mid=bit_or(carry_mask(old_mid,mid,sum_mid),
                bit_and(from_low,equal(sum_mid,broadcast(-1))));
            low=sum_low;mid=sub(sum_mid,from_low);
            top=sub(add(load_half(core.vpu.acch.u128[half]),top),from_mid);
        }
        if constexpr(op!=M::VMADH)store_half(core.vpu.accl.u128[half],low);
        store_half(core.vpu.accm.u128[half],mid);
        store_half(core.vpu.acch.u128[half],top);
        if constexpr(op==M::VMUDN || op==M::VMUDL)result=low;
        else if constexpr(op==M::VMUDM)result=mid;
        else if constexpr(op==M::VMADN){
            const __m64 fits=equal(top,sign(mid));
            const __m64 positive=greater(top,broadcast(-1));
            result=bit_or(bit_and(fits,low),bit_andnot(fits,positive));
        }else result=saturated_pack(unpack_low(mid,top),unpack_high(mid,top));
        store_half(dest.u128[half],result);
    }
}
}
