/* Standalone differential test against the signed 48-bit RSP definition. */
#include "rsp_audio_mmx.hpp"
#if !defined(__clang__) && defined(__GNUC__)
#define __builtin_memcpy_inline __builtin_memcpy
#endif
#include "rsp_audio_words.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>

using xb_audio::Multiply;
struct Vector {
    uint64_t u128[2];
    uint16_t get(unsigned lane) const {return uint16_t(u128[lane/4] >> (16*(lane%4)));}
    int16_t s16(unsigned element) const {return int16_t(get(7-element));}
    void put(unsigned lane,uint16_t value) {
        unsigned shift=16*(lane%4);
        u128[lane/4]=(u128[lane/4]&~(UINT64_C(0xffff)<<shift))|(uint64_t(value)<<shift);
    }
};
struct Core {struct {Vector accl,accm,acch,vcol,vcoh,vccl,vcch,vce;} vpu;};
static uint32_t rng=14857;
static uint32_t random_word(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static Vector input() {
    Vector v{};
    const uint16_t edges[]={0,1,0x7fff,0x8000,0xffff,0xfffe,0x4000,0xc000};
    for(unsigned i=0;i<8;i++){
        unsigned r=random_word();
        v.put(i,r&16 ? uint16_t(r>>8) : edges[r&7]);
    }
    return v;
}

template<Multiply op,unsigned e> static void check() {
    using M=Multiply;
    for(unsigned trial=0;trial<1024;trial++){
        Core actual{{input(),input(),input()}},expected=actual;
        Vector a=input(),b=input(),independent{};
        Vector& rhs=(trial&4)?a:b;
        Vector& dest=(trial%3==0)?a:(trial%3==1)?rhs:independent;
        Vector answer{};
        for(unsigned i=0;i<8;i++){
            unsigned n=7-i;
            unsigned picked=e<2?n:e<4?((n&6)|(e&1)):e<8?((n&4)|(e&3)):(e-8);
            uint16_t x=a.get(i),y=rhs.get(7-picked);
            int64_t acc=(uint64_t(expected.vpu.acch.get(i))<<32)|
                        (uint64_t(expected.vpu.accm.get(i))<<16)|expected.vpu.accl.get(i);
            int64_t result=0;
            if constexpr(op==M::VMUDL)result=(uint32_t(x)*y)>>16;
            if constexpr(op==M::VMUDH || op==M::VMADH){
                result=int64_t(int16_t(x))*int16_t(y)*65536;
                if(op==M::VMADH)result+=acc;
            }
            if constexpr(op==M::VMADM || op==M::VMUDM)
                result=int64_t(int16_t(x))*y+(op==M::VMADM?acc:0);
            if constexpr(op==M::VMADN || op==M::VMUDN)
                result=int64_t(x)*int16_t(y)+(op==M::VMADN?acc:0);
            if constexpr(op==M::VMULF || op==M::VMACF)
                result=int64_t(int16_t(x))*int16_t(y)*2+(op==M::VMULF?0x8000:acc);
            uint16_t low=result,mid=uint64_t(result)>>16,top=uint64_t(result)>>32;
            expected.vpu.accl.put(i,low);expected.vpu.accm.put(i,mid);expected.vpu.acch.put(i,top);
            int32_t high_signed=int32_t((uint32_t(top)<<16)|mid);
            uint16_t out;
            if constexpr(op==M::VMUDL || op==M::VMUDN)out=low;
            else if constexpr(op==M::VMUDM)out=mid;
            else if constexpr(op==M::VMADN)out=high_signed< -32768?0:high_signed>32767?0xffff:low;
            else out=high_signed< -32768?0x8000:high_signed>32767?0x7fff:mid;
            answer.put(i,out);
        }
        xb_audio::multiply<op,e>(actual,dest,a,rhs);
        _mm_empty();
        assert(!memcmp(&expected,&actual,sizeof(expected)) && !memcmp(&dest,&answer,sizeof(dest)));
    }
}
template<unsigned e> static void check_add(){
    for(unsigned trial=0;trial<1024;trial++){
        Core actual{},expected{};
        Vector a=input(),b=input(),separate{},answer{};
        Vector& rhs=(trial&4)?a:b;
        Vector& dest=trial%3==0?a:trial%3==1?rhs:separate;
        actual.vpu.accm=expected.vpu.accm=input();
        actual.vpu.acch=expected.vpu.acch=input();
        actual.vpu.vcoh=input();
        for(unsigned i=0;i<8;i++){
            unsigned lane=7-i;
            unsigned pick=e<2?lane:e<4?((lane&6)|(e&1)):e<8?((lane&4)|(e&3)):e-8;
            unsigned carry=random_word()&1;
            actual.vpu.vcol.put(i,carry?0xffff:0);
            int sum=int16_t(a.get(i))+rhs.s16(pick)+carry;
            expected.vpu.accl.put(i,uint16_t(sum));
            answer.put(i,sum<-32768?0x8000:sum>32767?0x7fff:uint16_t(sum));
        }
        xb_audio::vector_add<e>(actual,dest,a,rhs);
        _mm_empty();
        assert(!memcmp(&actual,&expected,sizeof(actual)) && !memcmp(&dest,&answer,sizeof(dest)));
    }
}
template<bool clip,unsigned e> static void check_envelope(){
    for(unsigned trial=0;trial<1024;trial++){
        Core actual{},expected{};
        Vector a=input(),b=input(),independent{},answer{};
        Vector& rhs=(trial&4)?a:b;
        Vector& dest=trial%3==0?a:trial%3==1?rhs:independent;
        actual.vpu.accm=expected.vpu.accm=input();
        actual.vpu.acch=expected.vpu.acch=input();
        for(unsigned i=0;i<8;i++){
            unsigned lane=7-i;
            unsigned pick=e<2?lane:e<4?((lane&6)|(e&1)):e<8?((lane&4)|(e&3)):e-8;
            uint16_t x=a.get(i),y=rhs.get(7-pick);
            unsigned flags=random_word();
            bool low=flags&1,high=flags&2,le=flags&4,ge=flags&8,extended=flags&16;
            actual.vpu.vcol.put(i,-int(low));actual.vpu.vcoh.put(i,-int(high));
            actual.vpu.vccl.put(i,-int(le));actual.vpu.vcch.put(i,-int(ge));
            actual.vpu.vce.put(i,-int(extended));
            uint16_t result;
            if constexpr(clip){
                if(low){
                    unsigned sum=unsigned(x)+y;
                    if(!high)le=extended?(uint16_t(sum)==0 || sum<=65535):sum==0;
                    result=le?uint16_t(-y):x;
                }else{
                    if(!high)ge=x>=y;
                    result=ge?y:x;
                }
            }else{
                le=int16_t(x)>int16_t(y) || (x==y && !(low&&high));
                ge=false;result=le?x:y;
                expected.vpu.vce.put(i,-int(extended));
            }
            expected.vpu.vccl.put(i,-int(le));expected.vpu.vcch.put(i,-int(ge));
            expected.vpu.accl.put(i,result);answer.put(i,result);
        }
        xb_audio::envelope_compare<clip,e>(actual,dest,a,rhs);
        _mm_empty();
        assert(!memcmp(&actual,&expected,sizeof(actual)) && !memcmp(&dest,&answer,sizeof(dest)));
    }
}
template<unsigned e> static void elements() {
    check_add<e>();check_envelope<false,e>();check_envelope<true,e>();
    check<Multiply::VMUDL,e>();check<Multiply::VMUDM,e>();check<Multiply::VMUDN,e>();
    check<Multiply::VMUDH,e>();check<Multiply::VMADM,e>();check<Multiply::VMADN,e>();
    check<Multiply::VMADH,e>();check<Multiply::VMULF,e>();check<Multiply::VMACF,e>();
    if constexpr(e<15)elements<e+1>();
}
static void transfers(){
    uint8_t memory[4096],expected[4096];
    for(unsigned i=0;i<4096;i++)memory[i]=expected[i]=uint8_t(random_word());
    for(unsigned address=0;address<4096;address+=4)for(unsigned e=0;e<16;e+=4)
        for(unsigned bytes=0;bytes<=16;bytes+=4){
            Vector reg=input(),loaded=reg;
            auto* raw=reinterpret_cast<uint8_t*>(loaded.u128);
            for(unsigned i=0;i<bytes;i++)raw[15-((e+i)&15)]=memory[((address+i)^3)&4095];
            xb_audio::transfer_words<false>(reg,memory,e,address,bytes);
            assert(!memcmp(&loaded,&reg,sizeof(reg)));
            const Vector value=input();
            const auto* src=reinterpret_cast<const uint8_t*>(value.u128);
            for(unsigned i=0;i<bytes;i++)expected[((address+i)^3)&4095]=src[15-((e+i)&15)];
            xb_audio::transfer_words<true>(value,memory,e,address,bytes);
            assert(!memcmp(expected,memory,sizeof(memory)));
        }
    for(unsigned address=0;address<4096;address+=2)for(unsigned e:{0u,8u}){
        Vector reg=input(),loaded=reg;
        auto* raw=reinterpret_cast<uint8_t*>(loaded.u128);
        for(unsigned i=0;i<8;i++)raw[15-e-i]=memory[((address+i)^3)&4095];
        xb_audio::transfer_samples<false>(reg,memory,e,address);
        assert(!memcmp(&loaded,&reg,sizeof(reg)));
        const Vector value=input();
        const auto* src=reinterpret_cast<const uint8_t*>(value.u128);
        for(unsigned i=0;i<8;i++)expected[((address+i)^3)&4095]=src[15-e-i];
        xb_audio::transfer_samples<true>(value,memory,e,address);
        assert(!memcmp(expected,memory,sizeof(memory)));
    }

}
int main(){
    elements<0>();transfers();
    puts("147456 vector multiplies, 16384 vector adds, 32768 envelope comparisons and 49152 vector transfers matched, including accumulator slices, aliases and DMEM wrap.");
}
