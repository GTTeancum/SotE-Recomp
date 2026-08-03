/*
 * N64Recomp suppresses a small set of hardware/kernel routines that
 * N64ModernRuntime deliberately replaces at a higher level. Call sites still
 * use the `<name>_recomp` ABI, so the game supplies the inert CP0/TLB shims
 * below. They must never emulate the original N64 kernel scheduler.
 */

#include <stdint.h>

#include "recomp.h"

void __osSetSR_recomp(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
}

void osMapTLBRdb_recomp(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
}

void __osSetCompare_recomp(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
}

void __osGetCause_recomp(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    ctx->r2 = 0;
}

void __osDequeueThread_recomp(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
}

void __osDispatchThread_recomp(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
}

void __d_to_ll_recomp(uint8_t* rdram, recomp_context* ctx) {
    int64_t value = (int64_t)ctx->f12.d;
    (void)rdram;
    ctx->r2 = (int32_t)(value >> 32);
    ctx->r3 = (int32_t)value;
}

void __d_to_ull_recomp(uint8_t* rdram, recomp_context* ctx) {
    uint64_t value = (uint64_t)ctx->f12.d;
    (void)rdram;
    ctx->r2 = (int32_t)(value >> 32);
    ctx->r3 = (int32_t)value;
}

void __f_to_ull_recomp(uint8_t* rdram, recomp_context* ctx) {
    uint64_t value = (uint64_t)ctx->f12.fl;
    (void)rdram;
    ctx->r2 = (int32_t)(value >> 32);
    ctx->r3 = (int32_t)value;
}
