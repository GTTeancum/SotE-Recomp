#pragma once
#include <stdint.h>
#define XB_ASSET_BOUNCE_BYTES 8192u
#define XB_ASSET_MAX_BYTES (4u*1024u*1024u)
/* Initialize once in bootstrap before renderer workers run. */
void xb_asset_io_init(void);
/* Return 1 only when exact index size was verified and every byte was read.
   Caller owns capacity, invalidates old readers before entry, and never
   publishes a partially transferred slot. Optional legacy path is tried
   only for missing path/file, never for access errors or a corrupt file. */
/* Require the *full* immutable file to match file_bytes. Copy only [begin,
   begin+bytes) or fail; legacy fallback is still missing-only. */
int xb_asset_read_range_exact(const char* preferred,const char* legacy,void* dst,
                            uint32_t file_bytes,uint32_t begin,uint32_t bytes);
int xb_asset_read_paths_exact(const char* preferred,const char* legacy,void* dst,uint32_t bytes);
int xb_asset_read_exact(const char* path,void* dst,uint32_t bytes);
/* attempts, opens, native reads, successfully copied bytes, completed files,
   missing/open failures, invalid/size/transfer/close errors, legacy opens. */
extern volatile uint32_t sote_asset_io[8];
