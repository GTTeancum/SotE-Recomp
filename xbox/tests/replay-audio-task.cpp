/* Compile once with the scalar librecomp headers and once with the Xbox
   headers. Both programs read exactly the same captured input. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "aspMain.cpp"

uint8_t dmem[4096];
uint16_t rspReciprocals[512],rspInverseSquareRoots[512];
extern "C" {
unsigned xb_rdram_committed_limit;
void xb_rdram_bounds_fault(uint32_t, uint64_t, uint32_t) { std::abort(); }
}
int main(int argc, char** argv) {
    assert(argc == 3);
    FILE* src = fopen(argv[1], "rb");
    uint32_t header[2];
    assert(src && fread(header, sizeof(header), 1, src) == 1);
    assert(header[1] == 0x400000 || header[1] == 0x800000);
    xb_rdram_committed_limit = header[1];
    std::vector<uint8_t> ram(header[1]);
    assert(fread(dmem, 1, sizeof(dmem), src) == sizeof(dmem));
    assert(fread(ram.data(), 1, ram.size(), src) == ram.size() && fgetc(src) == EOF);
    fclose(src);
    uint32_t task[16];
    memcpy(task, dmem + 0xfc0, sizeof(task));
    assert(task[0] == 2 && (task[4] & 0x1fffffff) == 0xcf890);
    assert(task[13] >= 8 && !(task[13] & 7));
    uint32_t commands = task[12] & 0x1fffffff;
    assert(commands <= ram.size() && task[13] <= ram.size() - commands);
    // These are the tables installed before the first mixer task on Xbox.
    rspReciprocals[0] = 0xffff;
    for (unsigned i = 1; i < 512; ++i)
        rspReciprocals[i] = uint16_t((((UINT64_C(1) << 34) / (i + 512) + 1) >> 8));
    for (unsigned i = 0; i < 512; ++i) {
        uint64_t a = (i + 512) >> (i & 1), lo = 1 << 17, hi = 1 << 19;
        while (lo + 1 < hi) {
            uint64_t mid = (lo + hi) / 2;
            if (a * mid * mid < (UINT64_C(1) << 44)) lo = mid;
            else hi = mid;
        }
        rspInverseSquareRoots[i] = uint16_t(lo >> 1);
    }
    assert(aspMain(ram.data(), task[4]) == RspExitReason::Broke);
    __asm__ volatile("emms" ::: "memory");
    FILE* dst = fopen(argv[2], "wb");
    assert(dst && fwrite(dmem, 1, sizeof(dmem), dst) == sizeof(dmem));
    assert(fwrite(ram.data(), 1, ram.size(), dst) == ram.size());
    assert(fclose(dst) == 0);
    printf("Replayed %u commands from input poll %u.\n", task[13] / 8, header[0]);
}
