#pragma once
#include "texture.h"
unsigned xb_hd_hash_pages(const xb_tile* tile,unsigned width,unsigned height,unsigned tlut);
uint64_t xb_hd_hash(const uint8_t* memory,const xb_tile* tile,unsigned width,unsigned height,unsigned tlut);
