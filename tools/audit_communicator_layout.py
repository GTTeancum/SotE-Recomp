"""Extract the full radio/dialogue corpus for the production-font render audit.

Private ROM text and render artifacts stay in the caller's repository-local
output directory. This does not validate message timing or audio playback.
"""
from pathlib import Path
import argparse
import json
import re
import struct

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--segments', type=Path, required=True)
    args = parser.parse_args()
    data = args.rom.read_bytes()
    base = 0x80001ec0
    strings = {base + m.start(): m.group()[:-1].decode('ascii')
               for m in re.finditer(rb'[ -~]{5,}\x00', data)}
    table = {struct.unpack_from('>I', data, 0xdf4cc + 4*i)[0] for i in range(30)}
    segment_strings = {}
    for path in sorted(args.segments.glob('seg*.bin')):
        for match in re.finditer(rb'~[ -~]{5,}\x00', path.read_bytes()):
            text = match.group()[:-1].decode('ascii')
            # Hoth radio pool, plus the train's visible sharp-turn warning.
            if (path.name == 'seg03.bin' and text.startswith('~o')) or text == '~o~cWarning! Sharp Turn!':
                segment_strings[f'{path.stem}+{match.start():X}'] = text
    # Include the entire dialogue pools, including messages without a PC WAV.
    selected = {a for a in strings if 0x800d0e20 <= a <= 0x800d15d4 or
                0x800d163c <= a <= 0x800d1b18} | table
    # Match every original string exercised by the separate Hoth/Skyhook voice
    # harness; this includes centered pilot warnings and four stage briefings.
    harness = Path('tools/pc_voices_harness.cpp').read_text()
    fixtures = set(re.findall(r'"(~[^"\n]*)"', harness))
    fixtures |= {line.split('\t', 2)[2] for line in
                 Path('tools/leebo_voice_cases.tsv').read_text().splitlines()}
    missing = []
    for text in fixtures:
        if text == '~oUnrelated dialogue.':
            continue
        matches = [a for a, s in strings.items() if s == text]
        if not matches and text not in segment_strings.values():
            missing.append(text)
        selected.update(matches)
    # Include additional on-screen radio/status messages that have no WAV key.
    for a, text in strings.items():
        if any(term in text.lower() for term in
               ('tow cable', 'harpoon', 'collision warning', 'turret range warning',
                'incoming bomb', 'jetpack malfunction', 'return to battle')):
            selected.add(a)
    corpus = []
    for address in sorted(selected):
        text = strings[address]
        layout = 'centered' if '~c' in text else 'paragraph'
        corpus.append(dict(address=f'{address:08X}', layout=layout, text=text))
    for source, text in segment_strings.items():
        corpus.append(dict(address=source, layout='centered', text=text))
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output/'corpus.json').write_text(json.dumps(corpus, indent=2))
    (args.output/'corpus.tsv').write_text(''.join(
        f"{c['address']}\t{c['layout']}\t{c['text']}\n" for c in corpus))
    print(f'{len(corpus)} messages: '+', '.join(
        f'{sum(c["layout"]==kind for c in corpus)} {kind}'
        for kind in ('paragraph', 'centered')))
    if missing:
        raise SystemExit('Missing fixture strings: '+repr(missing))
    print('All 30 table entries and all Leebo/Hoth/Skyhook text fixtures covered.')

if __name__ == '__main__':
    main()
