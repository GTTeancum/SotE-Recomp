#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include "rom_io.h"
#include "rdram_guard.h"

enum { SECTOR = 2048, WINDOW = 4 * SECTOR };
_Static_assert(XB_ROM_BYTES % SECTOR == 0, "final optical sector must be complete");
static HANDLE image = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION guard;
static int initialized;
static unsigned first, valid;
static uint8_t cache[WINDOW];
volatile uint32_t sote_rom_io_status[XB_ROM_COUNTERS];

int xb_rom_open(void) {
    /* Only the bootstrap initializes this lock. Guest/VI/audio workers do
       not open files; a level transition does not affect image ownership. */
    if (!initialized) { InitializeCriticalSection(&guard); initialized = 1; }
    EnterCriticalSection(&guard);
    int ret = 0;
    if (image == INVALID_HANDLE_VALUE) {
        HANDLE h = CreateFileA("D:\\sote.us.v1.2.z64", GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) ret = XB_ROM_NO_FILE;
        else {
            LARGE_INTEGER size;
            if (!GetFileSizeEx(h, &size) || size.QuadPart != XB_ROM_BYTES) {
                CloseHandle(h); ret = XB_ROM_BAD_SIZE;
            } else {
                valid = 0; image = h; ++sote_rom_io_status[XB_ROM_OPENS];
            }
        }
    }
    LeaveCriticalSection(&guard);
    return ret;
}

void xb_rom_close(void) {
    if (!initialized) return;
    EnterCriticalSection(&guard);
    valid = 0;
    if (image != INVALID_HANDLE_VALUE) { CloseHandle(image); image = INVALID_HANDLE_VALUE; }
    LeaveCriticalSection(&guard);
}

/* Synchronous handle, absolute byte offset: never share or seek a mutable
   file cursor. Match the staged NXDK ReadFile completion semantics. */
static int read_exact(unsigned offset, void* dst, unsigned bytes) {
    LARGE_INTEGER pos; pos.QuadPart = offset;
    IO_STATUS_BLOCK io = {0};
    if (image == INVALID_HANDLE_VALUE) return -1;
    NTSTATUS st = NtReadFile(image, NULL, NULL, NULL, &io, dst, bytes, &pos);
    if (st == STATUS_PENDING) {
        st = NtWaitForSingleObject(image, FALSE, NULL);
        if (NT_SUCCESS(st)) st = io.Status;
    }
    return NT_SUCCESS(st) && io.Information == bytes ? 0 : -1;
}

int xb_rom_raw(uint32_t offset, void* dst, uint32_t bytes) {
    if (!initialized || offset > XB_ROM_BYTES || bytes > XB_ROM_BYTES - offset) return -1;
    EnterCriticalSection(&guard);
    int ret = image == INVALID_HANDLE_VALUE ? -1 : 0;
    if (!ret && bytes) {
        ++sote_rom_io_status[XB_ROM_RAW_READS];
        ret = read_exact(offset, dst, bytes);
        if (!ret) sote_rom_io_status[XB_ROM_RAW_BYTES] += bytes;
    }
    if (ret) ++sote_rom_io_status[XB_ROM_IO_ERRORS];
    LeaveCriticalSection(&guard);
    return ret;
}

int xb_rom_guest(uint8_t* rdram, uint32_t offset, uint32_t ram, uint32_t bytes) {
    if (!initialized || offset > XB_ROM_BYTES || bytes > XB_ROM_BYTES - offset ||
        ram > XB_RDRAM_BYTES || bytes > XB_RDRAM_BYTES - ram) return -1;
    xb_rdram_offset(ram,bytes,XB_RDRAM_PI);
    EnterCriticalSection(&guard);
    int ret = image == INVALID_HANDLE_VALUE ? -1 : 0, fetched = 0;
    ++sote_rom_io_status[XB_ROM_GUEST_REQUESTS];
    for (unsigned left = bytes; !ret && left;) {
        if (offset < first || offset - first >= valid) {
            /* A 4-byte uncached lookup reads one DVD sector. Contiguous bulk
               copies read up to four sectors at a time, never cross EOF, and
               don't displace 12 MiB of shared RAM with a whole-ROM preload.
               Publish the new range ONLY after a full successful read. */
            first = offset & ~(SECTOR - 1u); valid = 0;
            unsigned n = ((offset - first + left + SECTOR - 1u) / SECTOR) * SECTOR;
            if (n > WINDOW) n = WINDOW;
            ++sote_rom_io_status[XB_ROM_GUEST_FETCHES];
            if (read_exact(first, cache, n)) { ret = -1; break; }
            valid = n; fetched = 1; sote_rom_io_status[XB_ROM_GUEST_FETCH_BYTES] += n;
        }
        unsigned pos = offset - first, n = valid - pos;
        if (n > left) n = left;
        /* The guest's logical byte i lives at host byte i^3. This works
           across odd offsets, word/sector boundaries and partial tails. */
        for (unsigned i = 0; i < n; ++i) rdram[(ram + i) ^ 3u] = cache[pos + i];
        offset += n; ram += n; left -= n;
    }
    if (ret) ++sote_rom_io_status[XB_ROM_IO_ERRORS];
    else {
        sote_rom_io_status[XB_ROM_GUEST_BYTES] += bytes;
        if (bytes && !fetched) ++sote_rom_io_status[XB_ROM_ALL_HIT];
    }
    LeaveCriticalSection(&guard);
    return ret;
}
