"""Read a captured Xbox guest frame; no emulator or host input."""
import json
import math
import struct
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--run', type=Path, default=ROOT / 'build/xbox/xemu')
run = parser.parse_args().run.resolve()
if not run.is_relative_to(ROOT):
    raise ValueError('Frame evidence must stay inside this repository')
ram = (run / 'first-graphics-rdram.bin').read_bytes()
state = json.loads((run / 'result.json').read_text())['subsystems']
def word(p):
    return struct.unpack_from('<I', ram, p & 0x7fffff)[0]
def signed(v):
    return (v & 65535) - (65536 if v & 32768 else 0)
def vector(v, m):
    return [sum(v[r] * m[r][c] for r in range(4)) for c in range(4)]
identity = [[int(r == c) for c in range(4)] for r in range(4)]
segments = [0] * 16
stacks = [[(identity, 0)], [(identity, 0)]]
vertices = [None] * 32
groups = {}
geometry = 0
scissor = None
viewport = None
material = {}
budget = 200000
def physical(p):
    return (segments[(p >> 24) & 15] + (p & 0xffffff)) & 0xffffff
def triangle(indices):
    for i in indices:
        assert vertices[i] is not None
    vs = [vertices[i] for i in indices]
    g = groups[vs[0][2]]
    g['triangles'] += 1
    g['draw_state'] = dict(scissor=scissor, viewport=viewport, material=material.copy())
    clip = [v[1] for v in vs]
    if all(v[3] > 0 for v in clip):
        xy = [(v[0]/v[3], -v[1]/v[3]) for v in clip]
        area = ((xy[1][0]-xy[0][0])*(xy[2][1]-xy[0][1])-
                (xy[1][1]-xy[0][1])*(xy[2][0]-xy[0][0]))
        culled = area == 0 or (geometry & 0x1000 and area < 0) or (geometry & 0x2000 and area > 0)
        g['face_culled'] += int(bool(culled))
    if any(all(v[3] + sign * v[axis] < 0 for v in clip)
           for axis in range(3) for sign in (-1, 1)):
        g['outside'] += 1
def display_list(address, depth=0):
    global geometry, budget, scissor, viewport
    assert depth < 32
    pc = physical(address)
    while True:
        budget -= 1
        assert budget > 0
        a, b = word(pc), word(pc + 4)
        pc += 8
        op = a >> 24
        if op == 0xb8:
            return
        if op == 6:
            if (a >> 16) & 1:
                pc = physical(b)
            else:
                display_list(b, depth + 1)
        elif op == 1:
            flags = (a >> 16) & 255
            p = physical(b)
            values = [signed(word(p + (i // 2) * 4) >> (0 if i & 1 else 16)) +
                      ((word(p + 32 + (i // 2) * 4) >> (0 if i & 1 else 16)) & 65535) / 65536
                      for i in range(16)]
            m = [values[i:i+4] for i in range(0, 16, 4)]
            stack = stacks[flags & 1]
            if not flags & 2:
                m = [vector(row, stack[-1][0]) for row in m]
            if flags & 4:
                stack.append((m, p))
            else:
                stack[-1] = (m, p)
        elif op == 0xbd:
            stacks[b & 1].pop()
        elif op == 0xbc and a & 255 == 6:
            segments[((a >> 8) & 65535) // 4] = b
        elif op == 0xb6:
            geometry &= ~b
        elif op == 0xb7:
            geometry |= b
        elif op == 3 and (a >> 16) & 255 == 0x80:
            p = physical(b)
            viewport = [signed(word(p+i*2//4*4) >> (0 if i & 1 else 16)) for i in range(8)]
        elif op == 0xed:
            scissor = [(a >> 12 & 4095)/4, (a & 4095)/4, (b >> 12 & 4095)/4, (b & 4095)/4]
        elif op in (0xfc, 0xb9, 0xba, 0xff, 0xfe):
            material[hex(op)] = [hex(a), hex(b)]
        elif op == 4:
            first, count = ((a >> 16) & 255) // 5, (a >> 9) & 127
            p = physical(b)
            key = (stacks[0][-1][1], stacks[1][-1][1], p)
            g = groups.setdefault(key, dict(matrix=[hex(v) for v in key], transform=stacks[1][-1][0], geometry=hex(geometry),
                points=[], clip=[], triangles=0, outside=0, face_culled=0))
            for i in range(count):
                xy, z = word(p + i*16), word(p + i*16 + 4)
                local = [signed(xy >> 16), signed(xy), signed(z >> 16), 1]
                world = vector(local, stacks[0][-1][0])
                clip = vector(world, stacks[1][-1][0])
                vertices[first+i] = (world, clip, key)
                g['points'].append(world[:3]); g['clip'].append(clip)
        elif op == 0xbf:
            triangle([(b >> s & 255)//5 for s in (16, 8, 0)])
        elif op == 0xb1:
            for v in (a, b): triangle([(v >> s & 255)//5 for s in (16, 8, 0)])
        elif op == 0xb5:
            v = [(b >> s & 255)//5 for s in (24, 16, 8, 0)]
            triangle(v[:3]); triangle([v[0], v[2], v[3]])
        # Deliberately retain culled lists to inspect their transformed bounds.
        elif op in (0xe4, 0xe5):
            pc += 16

tasks = state['sote_graphics_tasks']
tasks = sorted([tasks[i:i+3] for i in range(0, 12, 3)], key=lambda t:t[0])
for task in tasks[-2:]:
    display_list(task[1])
actor = state['sote_player_status'][1]
pos = [struct.unpack('<f', struct.pack('<I', word(actor + 0x50 + 4*i)))[0] for i in range(3)]
out = []
for g in groups.values():
    points, clip = g.pop('points'), g.pop('clip')
    center = [sum(p[i] for p in points)/len(points) for i in range(3)]
    g['center'] = [round(v, 3) for v in center]
    g['distance_to_actor'] = round(math.dist(center, pos), 3)
    g['clip_bounds'] = [[min(p[i] for p in clip), max(p[i] for p in clip)] for i in range(4)]
    out.append(g)
out.sort(key=lambda g:g['distance_to_actor'])
(run / 'model-bounds.json').write_text(json.dumps(dict(actor=pos, tasks=tasks, models=out), indent=2))
print(json.dumps(dict(actor=pos, models=out[:6]), indent=2))
