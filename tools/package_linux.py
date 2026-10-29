"""Package an existing native Linux build without copying texture staging trees."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[1]


def inside(path):
    path = (ROOT / path).resolve()
    if not path.is_relative_to(ROOT):
        raise ValueError(f'Package paths must stay inside the repository: {path}')
    return path


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--version', default='0.9.7')
    ap.add_argument('--build', default='build/linux')
    ap.add_argument('--output', default='build/release-0.9.7-linux')
    ap.add_argument('--deps', default='.local/linux/deps')
    ap.add_argument('--textures', default='SotE_Recompiled/textures')
    args = ap.parse_args()
    if not re.fullmatch(r'\d+\.\d+\.\d+', args.version):
        ap.error('Expected a numeric release version')
    build, output, deps, textures = map(inside, [args.build, args.output, args.deps, args.textures])
    name = f'SotE-Recomp-{args.version}-linux-x86_64'
    stage = output / name
    archive = output / f'{name}.tar.gz'
    if stage.exists() or archive.exists():
        raise FileExistsError('Package output already exists')
    source = build / 'sote_recomp'
    sdk = deps / 'usr/lib/x86_64-linux-gnu'
    patchelf = deps / 'usr/bin/patchelf'
    if not patchelf.is_file():
        patchelf = Path(shutil.which('patchelf') or '')
    if not patchelf.is_file():
        raise FileNotFoundError('patchelf is required to relocate the package')
    libraries = {
        'libsandec.so': build / 'libsandec.so',
        'libSDL2-2.0.so.0': sdk / 'sdl2-classic/libSDL2-2.0.so.0',
        'libdbus-1.so.3': sdk / 'libdbus-1.so.3',
        'libsamplerate.so.0': sdk / 'libsamplerate.so.0',
        'libXss.so.1': sdk / 'libXss.so.1',
    }
    for f in [source, *libraries.values()]:
        if not f.is_file():
            raise FileNotFoundError(f)
    for pack in ['SotE_V1_HD', 'SotE_AI_Upscale']:
        if not (textures / pack / 'rt64.json').is_file():
            raise FileNotFoundError(pack)
    (stage / 'lib').mkdir(parents=True)
    shutil.copy2(source, stage / 'sote_recomp')
    subprocess.run([str(patchelf), '--set-rpath', '$ORIGIN/lib', str(stage / 'sote_recomp')], check=True)
    (stage / 'sote_recomp').chmod(0o755)
    for name, f in libraries.items():
        dest = stage / 'lib' / name
        shutil.copy2(f, dest)
        subprocess.run([str(patchelf), '--set-rpath', '$ORIGIN', str(dest)], check=True)
        dest.chmod(0o755)
    (stage / 'Play.sh').write_text('''#!/bin/sh
set -eu
APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$APP_DIR"
export LD_LIBRARY_PATH="$APP_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$APP_DIR/sote_recomp" "$@"
''')
    (stage / 'Play.sh').chmod(0o755)
    docs = {
        f'docs/RELEASE_{args.version}_LINUX.md': 'README.md',
        'LICENSE': 'LICENSE', 'LICENSES.md': 'LICENSES.md',
        'config/CONTROLS_MODERN.INI': 'CONTROLS_MODERN.INI',
        'Sdata/UI/fonts.ini': 'Sdata/UI/fonts.ini',
    }
    for doc in ['TEXTURE_PACKS_USER.md', 'MODERN_AIM.md', 'LEEBO_VOICES_USER.md',
                'CONTROL_REBINDING.md', 'PC_VOICES.md', 'EXTERNAL_MUSIC.md',
                'OPTIONAL_ENHANCEMENTS.md', 'LINUX_BUILD.md']:
        docs['docs/' + doc] = doc
    for src, dest in docs.items():
        path = stage / dest
        path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / src, path)
    shutil.copytree(ROOT / 'third_party/sandec', stage / 'sandec-source', ignore=shutil.ignore_patterns('.git', '__pycache__'))
    notices = {
        'third_party/N64ModernRuntime/COPYING': 'N64ModernRuntime-COPYING.txt',
        'third_party/rt64/LICENSE': 'RT64-LICENSE.txt',
        'third_party/N64Recomp/LICENSE': 'N64Recomp-LICENSE.txt',
        'docs/licenses/JSON-LICENSE.txt': 'JSON-LICENSE.txt',
    }
    for dep in ['fmt', 'rabbitizer', 'sljit', 'tomlplusplus']:
        notices[f'third_party/N64ModernRuntime/N64Recomp/lib/{dep}/LICENSE'] = dep + '-LICENSE.txt'
    for dep in ['miniz', 'o1heap']:
        notices[f'third_party/N64ModernRuntime/thirdparty/{dep}/LICENSE'] = dep + '-LICENSE.txt'
    for dep in ['plume', 'stb', 'xxHash', 'zstd', 'spirv-cross', 're-spirv',
                'hlslpp', 'ddspp', 'im3d', 'implot', 'nativefiledialog-extended']:
        notices[f'third_party/rt64/src/contrib/{dep}/LICENSE'] = dep + '-LICENSE.txt'
    notices['third_party/rt64/src/contrib/imgui/LICENSE.txt'] = 'imgui-LICENSE.txt'
    notices['third_party/rt64/src/contrib/plume/contrib/VulkanMemoryAllocator/LICENSE.txt'] = 'VulkanMemoryAllocator-LICENSE.txt'
    licenses = stage / 'licenses'; licenses.mkdir()
    for src, dest in notices.items():
        shutil.copy2(ROOT / src, licenses / dest)
    for dep in ['libsdl2-classic', 'libdbus-1-3', 'libsamplerate0', 'libxss1']:
        shutil.copy2(deps / 'usr/share/doc' / dep / 'copyright', licenses / f'{dep}-copyright.txt')
    versions = {}
    for f in [stage / 'sote_recomp', *(stage / 'lib').iterdir()]:
        info = subprocess.check_output(['readelf', '--version-info', str(f)], text=True)
        for group in ['GLIBC', 'GLIBCXX', 'CXXABI']:
            for v in re.findall(r'Name: ' + group + r'_(\d+(?:\.\d+)+)', info):
                old = versions.get(group, '0')
                if tuple(map(int, v.split('.'))) > tuple(map(int, old.split('.'))):
                    versions[group] = v
    commit = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
    metadata = dict(version=args.version, platform='linux-x86_64', status='experimental',
                    source_commit=commit, source_url=f'https://github.com/GTTeancum/SotE-Recomp/tree/{commit}',
                    source_executable_sha256=sha(source), executable_sha256=sha(stage / 'sote_recomp'),
                    required_symbol_versions=versions, build_system='Ubuntu 26.04.1 WSL2; Clang 21.1.8',
                    relocation='patchelf: executable $ORIGIN/lib; bundled libraries $ORIGIN',
                    runtime_validation='Offscreen software Vulkan only; later blue-frame failure and WSL HD startup timeout remain open',
                    bundled_libraries={name: sha(stage / 'lib' / name) for name in libraries})
    (stage / 'BUILD_INFO.json').write_text(json.dumps(metadata, indent=2) + '\n')
    required = ', '.join(f'{k}_{v}' for k, v in versions.items())
    with (stage / 'README.md').open('a') as f:
        f.write(f'\nRequired system symbol versions: **{required}** or newer.\n')
        f.write(f'\n[Source for this build]({metadata["source_url"]}).\n')
    # Keep artwork in its existing directory and stream it into the archive.
    # No second unpacked texture staging tree is created.
    def normalize(member):
        if Path(member.name).suffix.lower() in {'.z64', '.n64', '.v64', '.san', '.wav', '.ogg', '.pcm', '.tmem', '.log'} or Path(member.name).name in {'main.bin', 'vulkan-default-v1.applied'}:
            raise ValueError(f'Private media or test state in package: {member.name}')
        member.uid = member.gid = 0
        member.uname = member.gname = ''
        member.mode = 0o755 if member.isdir() or Path(member.name).name in {'Play.sh', 'sote_recomp'} else 0o644
        return member
    print('Packaging Linux build; required symbols:', versions, flush=True)
    with tarfile.open(archive, 'w:gz', compresslevel=6) as tar:
        tar.add(stage, arcname=stage.name, filter=normalize)
        for pack in ['SotE_V1_HD', 'SotE_AI_Upscale']:
            tar.add(textures / pack, arcname=f'{stage.name}/textures/{pack}', filter=normalize)
    archive.with_name(archive.name + '.sha256').write_text(f'{sha(archive)}  {archive.name}\n')
    print(archive, archive.stat().st_size, flush=True)


if __name__ == '__main__':
    main()
