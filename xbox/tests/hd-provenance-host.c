#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "texture.h"
void xb_unsupported(uint32_t x){(void)x;abort();}
int main(void){
 uint8_t* ram=calloc(1,0x800000);assert(ram);uint16_t e=3;memcpy(ram+(0x13ce0e^2),&e,2);
 /* Palette source then CI4 block loaded through tile7 but rendered via tile0. */
 xb_texture_command(ram,0xFD100000,0x200000);
 xb_texture_command(ram,0xF5100100,0x07000000);
 xb_texture_command(ram,0xF0000000,0x0703c000);
 xb_texture_command(ram,0xFD500000,0x200020);
 xb_texture_command(ram,0xF5500000,0x07000000);
 xb_texture_command(ram,0xF3000000,0x073ff100);
 xb_texture_command(ram,0xF5400800,0);
 uint32_t key[17];assert(xb_texture_slot_key(0,2,64,32,key));
 assert(key[0]==3&&key[1]==0x200020&&key[2]==64&&key[3]==32);
 assert(key[4]==2&&key[5]==0&&key[6]==4&&key[9]==32768);
 assert(key[13]==2&&key[14]==2&&key[15]==1&&key[16]==0x200000);
 assert(!xb_texture_slot_key(8,2,64,32,key));free(ram);return 0;
}
