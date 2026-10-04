"""Find stable, small state values across native ending scene snapshots."""

from pathlib import Path
import struct
import sys


snapshot_dir = Path(sys.argv[1])
vis = [900, 1200, 1500, 1800, 2100, 2400, 2700]
data = [(snapshot_dir / f"rdram_{vi:07d}_ev31.bin").read_bytes() for vi in vis]

for start, end in [(0xD0000, 0x150000), (0, 0x800000)]:
    matches = []
    for offset in range(start, end, 2):
        # RDRAM is word-swapped on the host; MEM_H reads little endian at ^2.
        values = [struct.unpack_from("<h", snapshot, offset ^ 2)[0] for snapshot in data]
        if (
            all(-1 <= value <= 100 for value in values)
            and values[0] == values[1]
            and values[2] == values[3]
            and values[5] == values[6]
            and len({values[0], values[2], values[5]}) == 3
        ):
            matches.append((hex(0x80000000 + offset), values))
    print(f"{hex(start)}-{hex(end)}: {len(matches)} matches")
    for match in matches[:100]:
        print(*match)
    if matches:
        break

print("Stable byte candidates in likely global-data range:")
matches = []
for offset in range(0xD0000, 0x150000):
    values = [snapshot[offset] for snapshot in data]
    if (
        values[0] == values[1]
        and values[2] == values[3]
        and values[5] == values[6]
        and len({values[0], values[2], values[5]}) == 3
    ):
        matches.append((hex(0x80000000 + offset), values))
print(len(matches), "matches")
for match in matches[:100]:
    print(*match)

if len(sys.argv) > 2:
    idle_dir = Path(sys.argv[2])
    idle = [(idle_dir / f"rdram_{vi:07d}_ev31.bin").read_bytes()
            for vi in (900, 1200, 1500, 1800)]
    print("Input-dependent small halfwords, 900 matched then 1500/1800 differ:")
    matches = []
    for offset in range(0xD0000, 0x150000, 2):
        advanced = [struct.unpack_from("<h", snapshot, offset ^ 2)[0]
                    for snapshot in data[:4]]
        neutral = [struct.unpack_from("<h", snapshot, offset ^ 2)[0]
                   for snapshot in idle]
        if (
            advanced[0] == neutral[0]
            and all(0 <= value <= 255 for value in advanced + neutral)
            and advanced[2] == advanced[3]
            and neutral[2] == neutral[3]
            and advanced[2] != neutral[2]
        ):
            matches.append((hex(0x80000000 + offset), advanced, neutral))
    print(len(matches), "matches")
    for match in matches[:150]:
        print(*match)

print("Monotonic small halfwords in global-data range:")
matches = []
for offset in range(0xD0000, 0x150000, 2):
    values = [struct.unpack_from("<h", snapshot, offset ^ 2)[0] for snapshot in data]
    if (
        all(0 <= value <= 100 for value in values)
        and all(a <= b for a, b in zip(values, values[1:]))
        and len(set(values)) >= 3
    ):
        matches.append((hex(0x80000000 + offset), values))
print(len(matches), "matches")
for match in matches[:100]:
    print(*match)
