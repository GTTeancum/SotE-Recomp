#pragma once
#include <stdint.h>

/* A single read-only image belongs to the XBE, not to each guest PI request
   or level. Both bootstrap identity checks and gameplay read that same handle. */
#define XB_ROM_BYTES (12u * 1024u * 1024u)
#define XB_RDRAM_BYTES (8u * 1024u * 1024u)
enum { XB_ROM_NO_FILE = -1, XB_ROM_BAD_SIZE = -2 };
int xb_rom_open(void);                   /* bootstrap only; before guest work */
void xb_rom_close(void);                 /* process exit/failed bootstrap */
int xb_rom_raw(uint32_t offset, void* dst, uint32_t bytes);
int xb_rom_guest(uint8_t* rdram, uint32_t offset, uint32_t ram, uint32_t bytes);

/* Cumulative modulo-2^32 counters. "Fetch" = requested Xbox kernel read,
   not a DVD seek/physical read. Raw bootstrap and guest-format reads separate. */
enum { XB_ROM_OPENS, XB_ROM_GUEST_REQUESTS, XB_ROM_GUEST_BYTES,
       XB_ROM_GUEST_FETCHES, XB_ROM_GUEST_FETCH_BYTES, XB_ROM_ALL_HIT,
       XB_ROM_IO_ERRORS, XB_ROM_RAW_READS, XB_ROM_RAW_BYTES, XB_ROM_COUNTERS };
extern volatile uint32_t sote_rom_io_status[XB_ROM_COUNTERS];
