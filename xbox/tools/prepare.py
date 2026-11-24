"""Prepare an Xbox-only slice of existing generated game code, without editing it."""
from pathlib import Path
import hashlib
import json
import re
import random
import zlib

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/xbox'
OUT.mkdir(parents=True, exist_ok=True)
functions = {}
for path in sorted((ROOT/'generated/RecompiledFuncs').glob('*.c')):
    source = path.read_text()
    matches = list(re.finditer(r'^RECOMP_FUNC void (\w+)\(', source, re.M))
    for i, match in enumerate(matches):
        functions[match[1]] = source[match.start():matches[i+1].start() if i+1<len(matches) else len(source)]
selected = set()
def include(name):
    if name in selected: return
    selected.add(name)
    for callee in re.findall(r'\b(\w+)\(rdram, ctx\)', functions[name]):
        if callee in functions: include(callee)
include('func_80000E64')  # Original Ogre decompressor and its actual dependencies.
include('func_80017B60')  # Original retail startup, to the first thread request.
include('func_80017BBC')  # Retail idle thread and its manager initialization.
include('func_80017CC8')  # Main game thread, preserving its direct game dependencies.
# Resident function pointers (audio callbacks, actors, menus) are not visible
# in a direct call closure. Include the complete decompressed executable.
for name,body in functions.items():
    first=re.search(r'// 0x([0-9A-Fa-f]{8}):',body)
    if first and 0x80000400<=int(first[1],16)<0x800EE740:include(name)
# Trace named SDK game routines too (alInit, spFinish, etc.), not only func_*.
# Main-thread imports remain explicit; no generated successful stub services.
main_dependencies=set()
def trace_main(name):
    if name in main_dependencies:return
    main_dependencies.add(name)
    for callee in re.findall(r'\b(\w+)\(rdram, ctx\)',functions[name]):
        if callee in functions:trace_main(callee)
trace_main('func_80017CC8')
main_source=''.join(functions[name] for name in sorted(main_dependencies))
main_calls=set(re.findall(r'\b(\w+)\(rdram\b',main_source))
(OUT/'main-thread-dependencies.json').write_text(json.dumps(dict(
    entry='80017CC8',routines=sorted(main_dependencies),
    runtime_imports=sorted(n for n in main_calls if n.endswith('_recomp') and n not in functions),
    project_hooks=sorted(n for n in main_calls if n.startswith('sote_')),
    note='Direct call closure only; indirect function lookup and overlays also required.'),indent=2))
traced=[]
for name in sorted(selected):
    body=functions[name]
    address=re.search(r'// 0x([0-9A-Fa-f]{8}):',body)[1]
    traced.append(body.replace('{','{\n    xb_trace(0x'+address+'u);',1))
retail_source=''.join(traced)
# Xbox-only display correction; do not overwrite a guest string allocation.
anchor='sote_native_text_begin(rdram, (uint32_t)ctx->r5);'
assert retail_source.count(anchor)==1
retail_source=retail_source.replace(anchor,anchor+' if (xb_native_pause_heading(rdram, (uint32_t)ctx->r5)) return;')
# Apply the scale to consumers of the authored range. The retail camera keeps
# its unmodified value so resetting the FOV cannot multiply a previous edit.
for pc, load, register in (
    ('800C1230', 'MEM_W(ctx->r29, 0X7C)', 'f6'),  # guPerspective zFar
    ('800BAF40', 'MEM_W(ctx->r1, 0X2F20)', 'f10'),  # squared-distance visibility test
):
    anchor = re.search(r'// 0x' + pc + r':[^\n]+\n    ctx->' + register +
                       r'\.u32l = ' + re.escape(load) + ';', retail_source)
    assert anchor, f'Missing camera instruction {pc}'
    scale = 'xb_camera_distance_scale(MEM_HU(0, (int32_t)0x8013CE0E))'
    retail_source = retail_source.replace(anchor[0], anchor[0] + f'\n    ctx->{register}.fl *= {scale};', 1)
retail_source = '#include "draw_distance.h"\n' + retail_source
# The retail star painter runs on the CPU between two graphics tasks. Keep
# every original store, including the dim edge samples. Mark a 32-bit pixel
# only after both halfwords have been written.
anchor = 'func_8001DFB0(rdram, ctx);'
assert retail_source.count(anchor) == 1
retail_source = retail_source.replace(anchor, 'xb_starfield_begin(rdram, (uint32_t)ctx->r5);\n    ' + anchor)
for pc, expected in [('8001DEB8', 2), ('8001DF00', 2), ('8001DF20', 2), ('8001DF24', 1)]:
    pattern = rf'(// 0x{pc}: sh[^\n]*\n\s*MEM_H\(0X0, ctx->r2\) = ctx->r\d+;)'
    retail_source, count = re.subn(pattern, r'\1\n        xb_starfield_pixel((unsigned)ctx->r5, (unsigned)ctx->r6);', retail_source)
    assert count == expected, f'Missing star framebuffer store at {pc}'
# Match the world projection horizontally before the retail screen bounds
# check, so turning the ship moves distant stars with the world in widescreen.
pattern = r'(// 0x8001DCFC: lwc1)'
retail_source, count = re.subn(pattern, r'if (xb_wide_projection) ctx->f6.fl *= 0.75f;\n    \1', retail_source)
assert count == 1
retail_source = '#include "starfield.h"\n' + retail_source
(OUT/'retail.c').write_text('#include <string.h>\n#include "recomp.h"\n#include "funcs.h"\n#include "recomp_hooks.h"\nvoid xb_trace(uint32_t);\nint xb_native_pause_heading(uint8_t*,uint32_t);\n'+retail_source)
(OUT/'funcs.h').write_text((ROOT/'generated/RecompiledFuncs/funcs.h').read_text())
# Temporary fail-fast import boundaries are named and recorded, never successful
# stubs. They disappear as each real Xbox service is implemented.
native='\n'.join(p.read_text() for p in (ROOT/'xbox/src').glob('*.c'))
# Reuse the production aim/controller algorithms. Adapt only platform services
# in private generated copies; never fork a reduced gameplay implementation.
for name in ('modern_controls','modern_aim'):
    source=(ROOT/f'src/{name}.cpp').read_text()
    native+='\n'+source
    source=source.replace('#include "controls_menu.hpp"','#include "modern_platform.hpp"')
    # NXDK's C++ stdio wrapper does not import printf into namespace std.
    source=source.replace('std::printf(', '::printf(')
    if name=='modern_controls':
        source=source.replace('#include <chrono>','').replace('#include <mutex>','')
        source=source.replace('std::mutex input_mutex;','XboxInputMutex input_mutex;')
        source=source.replace('std::lock_guard lock{input_mutex};','XboxInputGuard lock{input_mutex};')
        old='return std::chrono::duration_cast<std::chrono::milliseconds>(\n        std::chrono::steady_clock::now().time_since_epoch()).count();'
        assert source.count(old)==1
        source=source.replace(old,'return GetTickCount();')
    if name=='modern_aim':
        anchor='if (!address(projectile,0xb0)) return;'
        assert source.count(anchor)==1
        source='#include "hardware_test.h"\n'+source.replace(anchor,anchor+'\n    xb_hardware_player_shot(shooter);')
    (OUT/f'{name}_xbox.cpp').write_text(source)
implemented=set(re.findall(r'\b(?:void|int|uint32_t|double)\s+(\w+)\s*\([^;{}]*\)\s*\{',native))
imports=sorted(set(re.findall(r'\b(\w+_recomp)\(rdram, ctx\)',retail_source))-selected-implemented)
with (OUT/'funcs.h').open('a') as declarations_out:
    declarations_out.write('\n'+''.join(f'void {name}(uint8_t*, recomp_context*);\n' for name in imports))
hooks=set(re.findall(r'\b(sote_\w+)\(',retail_source))-implemented
declarations=(ROOT/'src/recomp_hooks.h').read_text()
missing=[];boundaries=[]
for name in imports:
    missing.append(name)
    boundaries.append(f'void {name}(uint8_t* rdram,recomp_context* ctx){{xb_unsupported({len(missing)});}}')
for name in sorted(hooks):
    match=re.search(r'((?:void|int|uint32_t|double)\s+'+re.escape(name)+r'\s*\([^;]+\));',declarations)
    assert match, f'Missing hook signature: {name}'
    missing.append(name);boundaries.append(match[1]+f'{{xb_unsupported({len(missing)});}}')
(OUT/'unsupported.json').write_text(json.dumps({i+1:n for i,n in enumerate(missing)},indent=2))
lookup=[]
for name in sorted(selected):
    match=re.search(r'// 0x([0-9A-Fa-f]{8}):',functions[name])
    assert match,name
    lookup.append((int(match[1],16),name))
assert len({a for a,n in lookup})==len(lookup)
table='\n'.join(f'{{0x{addr:08X}u,{name}}},' for addr,name in sorted(lookup))
(OUT/'imports.c').write_text('#include "recomp.h"\n#include "funcs.h"\n#include "recomp_hooks.h"\n'
    'void xb_unsupported(uint32_t id) __attribute__((noreturn));\n'+'\n'.join(boundaries)+
    '\nstatic const struct {uint32_t address;recomp_func_t* function;} entries[]={\n'+table+'\n};\n'
    'recomp_func_t* get_function(int32_t vram){unsigned lo=0,hi=sizeof(entries)/sizeof(entries[0]);'
    'while(lo<hi){unsigned mid=(lo+hi)/2;if(entries[mid].address<(uint32_t)vram)lo=mid+1;else hi=mid;}'
    'if(lo<sizeof(entries)/sizeof(entries[0])&&entries[lo].address==(uint32_t)vram)return entries[lo].function;'
    'xb_unsupported((uint32_t)vram);}\n')
header=(ROOT/'third_party/N64ModernRuntime/N64Recomp/include/recomp.h').read_text()
needle='#elif defined(_MSC_VER)\n\n#include <intrin.h>'
assert header.count(needle)==1
header=header.replace(needle,'#elif defined(SOTE_XBOX)\n#include "wide_multiply.h"\n\n'+needle)
(OUT/'recomp.h').write_text(header)
# Keep generated CPU accessors unchanged; the reserved/decommitted span
# is the safety barrier. Instrument only the actual RSP bulk DMA boundaries
# in an Xbox-local header copy, not the third-party checkout.
rsp=(ROOT/'third_party/N64ModernRuntime/librecomp/include/librecomp/rsp.hpp').read_text()
rsp=rsp.replace('#include "rsp_vu.hpp"', '#include "librecomp/rsp_vu.hpp"\n#include "rdram_guard.h"')
for length in ('rd_len','wr_len'):
    needle=f'    assert(dmem_addr + {length} <= 0x1000);'
    assert rsp.count(needle)==1
    rsp=rsp.replace(needle,needle+f'\n    xb_rdram_offset(dram_addr,{length},XB_RDRAM_SP);')
# Both memories are word-swapped. A whole aligned word is byte-identical in
# either store. Leave partial and unaligned transfers on the original path.
for length,destination,source in (
    ('rd_len','dmem + dmem_addr','rdram + dram_addr'),
    ('wr_len','rdram + dram_addr','dmem + dmem_addr'),
):
    loop=f'    for (uint32_t i = 0; i < {length}; i++)'
    assert rsp.count(loop)==1
    rsp=rsp.replace(loop,f'''    if (((dmem_addr | {length}) & 3) == 0) {{
        __builtin_memcpy({destination}, {source}, {length});
        return;
    }}
'''+loop)
# The scalar audio command interpreter frequently reads aligned commands and
# coefficients. The upstream byte loops still handle misalignment and wrapping.
for name,size,store in (
    ('W_LOAD',4,False),('W_STORE',4,True),('HU_LOAD',2,False),
    ('H_LOAD',2,False),('H_STORE',2,True),
):
    method=re.compile(r'(static inline \w+ RSP_MEM_'+name+r'\([^)]*\) \{\n)')
    signed='int16_t' if name=='H_LOAD' else f'uint{size*8}_t'
    body=(f'{signed} value=({signed})val; __builtin_memcpy_inline(ptr,&value,{size}); return;'
          if store else f'{signed} value; __builtin_memcpy_inline(&value,ptr,{size}); return value;')
    mask=4-size
    rsp,n=method.subn(r'\1'+f'''    if (((addr+offset)&{size-1})==0) {{
        uint8_t* ptr=dmem+(((addr+offset)&4095)^{mask});
        {body}
    }}
''',rsp)
    assert n==1
(OUT/'librecomp').mkdir(exist_ok=True)
(OUT/'librecomp/rsp.hpp').write_text(rsp)
# Keep the translated audio program and its register layout. Replace the
# multiply instructions with kernels for this CPU, in the Xbox include path.
vu=(ROOT/'third_party/N64ModernRuntime/librecomp/include/librecomp/rsp_vu_impl.hpp').read_text()
for op in ('VMUDL','VMUDM','VMUDN','VMUDH','VMADH','VMADM','VMADN','VMULF','VMACF'):
    head=f'auto RSP::{op}(r128& vd, cr128& vs, cr128& vt) -> void {{'
    assert vu.count(head)==1,op
    condition='if constexpr (!U) ' if op in ('VMULF','VMACF') else ''
    vu=vu.replace(head,head+f'\n    {condition}return xb_audio::multiply<xb_audio::Multiply::{op},e>(*this,vd,vs,vt);')
for op,implementation in (('VADD','vector_add<e>'),('VGE','envelope_compare<false,e>'),
                          ('VCL','envelope_compare<true,e>')):
    head=f'auto RSP::{op}(r128& vd, cr128& vs, cr128& vt) -> void {{'
    assert vu.count(head)==1,op
    vu=vu.replace(head,head+f'\n    return xb_audio::{implementation}(*this,vd,vs,vt);')
for op in ('LQV','LRV','SQV','LDV','SDV'):
    head=re.compile(r'(auto RSP::'+op+r'\([^)]*\) -> void \{\n    auto address = rs \+ imm \* \d+;)')
    if op=='LRV': element,addr,size='16-(address&15)','address&~15u','address&15'
    elif op in ('SQV','LQV'): element,addr,size='0','address','16-(address&15)'
    else: element,addr,size='e','address','8'
    condition='e==0' if op in ('LRV','LQV','SQV') else '(e==0 || e==8)'
    store='true' if op.startswith('S') else 'false'
    samples=f'if(!(address&1))return xb_audio::transfer_samples<{store}>(vt,dmem,e,address);' if op in ('LDV','SDV') else ''
    vu,n=head.subn(r'\1'+f'''
    if constexpr({condition}) {{
        if(!(address&3))return xb_audio::transfer_words<{store}>(vt,dmem,{element},{addr},{size});
        {samples}
    }}''',vu)
    assert n==1,op
(OUT/'librecomp/rsp_vu_impl.hpp').write_text('#include "rsp_audio_mmx.hpp"\n#include "rsp_audio_words.hpp"\n'+vu)
# Exact shared cheat logic; native guest pointers receive the same range guard.
cheats=(ROOT/'src/cheats.hpp').read_text()
needle='((address&0x7fffff)^(sizeof(T)==2?2:0))'
assert cheats.count(needle)==2
cheats='#include "rdram_guard.h"\n'+cheats.replace(needle,'xb_rdram_offset('+needle+',sizeof(T),XB_RDRAM_MENU)')
(OUT/'cheats.hpp').write_text(cheats)
rom=ROOT/'SotE_Recompiled/sote.us.v1.2.z64'
image=ROOT/'generated/main.bin'
assert rom.stat().st_size==0xC00000 and image.stat().st_size==0xEC880
rom_hash=hashlib.sha256(rom.read_bytes()).hexdigest()
assert rom_hash in {
    'e7085e013123537f34e0edec8801318016da4dbac424172d6dc5f3b67d98642c',
    '2802bf4135842f7c8d254349ed7ac2641f6d7ff45e9d2d01304e1455706dd103',
}, 'Not a supported USA Rev 1.2 ROM'
rom_crc=zlib.crc32(rom.read_bytes())
image_crc=zlib.crc32(image.read_bytes())
(OUT/'rom_identity.h').write_text(f'#define SOTE_ROM_CRC 0x{rom_crc:08X}u\n#define SOTE_IMAGE_CRC 0x{image_crc:08X}u\n')
(OUT/'source-manifest.json').write_text(json.dumps(dict(
    functions=sorted(selected),rom_crc=f'{rom_crc:08X}',image_crc=f'{image_crc:08X}',
    source_sha256=hashlib.sha256((OUT/'retail.c').read_bytes()).hexdigest()),indent=2))
print(f'Prepared {len(selected)} original routines; ROM CRC {rom_crc:08X}, image CRC {image_crc:08X}')
# Reference results use Python integers, independent of the target limb math.
rng=random.Random(0x534F5445)
pairs=[(0,0),(0xffffffffffffffff,0xffffffffffffffff),(1<<63,1<<63),
       (0xffffffff,0x100000001)]+[(rng.getrandbits(64),rng.getrandbits(64)) for _ in range(60)]
lines=[];mask=(1<<64)-1
for a,b in pairs:
    unsigned=a*b
    signed=(a if a<1<<63 else a-(1<<64))*(b if b<1<<63 else b-(1<<64))
    lines.append('{'+','.join(f'0x{x&mask:016X}ULL' for x in (a,b,unsigned,unsigned>>64,signed,signed>>64))+'},')
(OUT/'multiply_vectors.h').write_text('static const uint64_t multiply_vectors[][6]={\n'+'\n'.join(lines)+'\n};\n')
