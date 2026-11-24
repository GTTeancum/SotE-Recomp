"""Compare full DMEM and RDRAM after replaying a real Xbox audio task."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
upstream = root / "third_party/N64ModernRuntime/librecomp/include/librecomp"
build = root / "build/xbox"


def compare(capture, compiler):
    with tempfile.TemporaryDirectory(dir=build, prefix="audio-replay-") as work:
        work = Path(work)
        env = dict(os.environ, TMP=str(work), TEMP=str(work), TMPDIR=str(work))
        vector_header = (upstream / "rsp_vu.hpp").read_text()
        arch = "#if defined(__x86_64__) || defined(_M_X64)"
        assert vector_header.count(arch) == 1
        results = []
        for variant, impl in (("scalar", upstream), ("xbox", build / "librecomp")):
            inc = work / variant
            lib = inc / "librecomp"
            lib.mkdir(parents=True)
            (inc / "ultramodern").mkdir()
            (inc / "ultramodern/ultra64.h").write_text("struct OSTask;\n")
            (inc / "recomp.h").write_text(
                "#include <cassert>\n#include <cstring>\n"
                "#define S32(value) static_cast<int32_t>(value)\n"
                "#define U32(value) static_cast<uint32_t>(value)\n"
                "#define MEM_B(off,addr) reinterpret_cast<int8_t*>(rdram)[((U32(addr)+U32(off))^3u)&0x7fffffu]\n")
            (lib / "rsp_vu.hpp").write_text(vector_header.replace(arch, "#if 0"))
            for name in ("rsp.hpp", "rsp_vu_impl.hpp"):
                (lib / name).write_bytes((impl / name).read_bytes())
            exe = inc / ("replay.exe" if os.name == "nt" else "replay")
            subprocess.run([compiler, "-O2", "-std=c++17", "-msse", "-mmmx", "-fwrapv",
                            "-fno-strict-aliasing", "-D__builtin_memcpy_inline=__builtin_memcpy",
                            "-I", str(inc), "-I", "xbox/include", "-I", "generated",
                            "xbox/tests/replay-audio-task.cpp", "-o", str(exe)], cwd=root, env=env, check=True)
            output = inc / "result.bin"
            subprocess.run([str(exe), str(capture), str(output)], cwd=root, env=env, check=True)
            results.append(output.read_bytes())
        if results[0] != results[1]:
            raise ValueError("Scalar and Xbox mixers produced different memory")
        original = capture.read_bytes()[8:]
        changed = sum(a != b for a, b in zip(original[4096:], results[0][4096:]))
        if not changed:
            raise ValueError("The captured task did not produce any guest memory changes")
        print(f"DMEM and RDRAM matched ({len(results[0]):,} bytes, {changed:,} guest bytes changed). "
              f"SHA-256 {hashlib.sha256(results[0]).hexdigest()}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--cxx", default=os.environ.get("CXX", "c++"))
    args = parser.parse_args()
    compare(args.capture.resolve(), args.cxx)
