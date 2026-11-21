#include <stdint.h>
#include <fenv.h>
/* Installed NXDK's rounding.c masks the wrong bits and declares its store
   as an input operand. Keep the correction local; do not modify the SDK. */
int fegetround(void) {
    uint16_t control;__asm__ volatile("fnstcw %0":"=m"(control));return control&0xC00;
}
int fesetround(int mode) {
    if(mode!=FE_TONEAREST && mode!=FE_DOWNWARD && mode!=FE_UPWARD && mode!=FE_TOWARDZERO)return -1;
    uint16_t control;uint32_t mxcsr;
    __asm__ volatile("fnstcw %0":"=m"(control));
    __asm__ volatile("stmxcsr %0":"=m"(mxcsr));
    control=(control&~0xC00u)|(unsigned)mode;
    mxcsr=(mxcsr&~0x6000u)|((unsigned)mode<<3);
    __asm__ volatile("fldcw %0"::"m"(control));
    __asm__ volatile("ldmxcsr %0"::"m"(mxcsr));return 0;
}
/* NXDK's supplied lrint/llrint bodies assert. Pentium III x87 FISTP implements
   the active rounding mode and integer-indefinite result for invalid input.
   Use explicit memory operands to avoid SSE2 (not present on original Xbox). */
long lrint(double value) {
    int32_t result;__asm__ volatile("fldl %1; fistpl %0":"=m"(result):"m"(value):"st");return result;
}
long lrintf(float value) {
    int32_t result;__asm__ volatile("flds %1; fistpl %0":"=m"(result):"m"(value):"st");return result;
}
long long llrint(double value) {
    int64_t result;__asm__ volatile("fldl %1; fistpll %0":"=m"(result):"m"(value):"st");return result;
}
long long llrintf(float value) {
    int64_t result;__asm__ volatile("flds %1; fistpll %0":"=m"(result):"m"(value):"st");return result;
}
unsigned xb_math_selftest(void) {
    int saved=fegetround();unsigned checks=0,errors=0;
#define CHECK(x) do{checks++;if(!(x))errors++;}while(0)
    fesetround(FE_TONEAREST);
    CHECK(lrint(2.5)==2);CHECK(lrint(-2.5)==-2);CHECK(lrintf(3.5f)==4);
    CHECK(llrint(4294967296.5)==4294967296LL);CHECK(llrintf(-3.5f)==-4);
    fesetround(FE_DOWNWARD);CHECK(lrint(2.9)==2);CHECK(lrintf(-2.1f)==-3);
    fesetround(FE_UPWARD);CHECK(llrint(2.1)==3);CHECK(llrintf(-2.9f)==-2);
    fesetround(FE_TOWARDZERO);CHECK(lrint(-2.9)==-2);CHECK(llrint(2.9)==2);
    fesetround(saved);return errors?0:checks;
}
