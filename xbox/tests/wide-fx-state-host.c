#include <assert.h>
#include "wide_fx_state.h"
int main(void){unsigned v=0x00504340;
assert(xb_wide_fx_render_state(0xc80,v));assert(xb_wide_fx_render_state(0xc80,v+1));
for(unsigned i=1;i<32;i++){assert(!xb_wide_fx_render_state(0xc80,v^(1u<<i)));assert(!xb_wide_fx_render_state(0xc80,(v+1)^(1u<<i)));}
for(unsigned i=0;i<32;i++)assert(!xb_wide_fx_render_state(0xc80^(1u<<i),v));return 0;}
