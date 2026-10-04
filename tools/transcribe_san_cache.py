"""Diagnostic transcript of locally decoded SAN audio; not a runtime dependency.

Requires the optional faster-whisper package and a locally cached model. The
transcript is automated evidence to compare against native story screens, not
a substitute for listening to the game's final audio output.
"""

import argparse
import subprocess
from pathlib import Path

import numpy as np
from faster_whisper import WhisperModel


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("movies", nargs="+")
    parser.add_argument("--cache", type=Path, default=Path("SotE_Recompiled/Sdata/SAN_CACHE"))
    parser.add_argument("--output", type=Path, default=Path("build/diagnostics/san_intro_audio_transcripts.txt"))
    args = parser.parse_args()
    model = WhisperModel("small.en", device="cpu", compute_type="int8", local_files_only=True)
    lines = ["Automated faster-whisper small.en transcript of local SAN cache audio.",
             "Verify ambiguous words against the source audio and native story screens.", ""]
    for name in args.movies:
        pcm = args.cache / name / "audio.pcm"
        if not pcm.is_file():
            raise FileNotFoundError(pcm)
        decoded = subprocess.run(
            ["ffmpeg", "-hide_banner", "-loglevel", "error", "-f", "s16le",
             "-ar", "22050", "-ac", "2", "-i", str(pcm), "-f", "s16le",
             "-ar", "16000", "-ac", "1", "pipe:1"],
            check=True, capture_output=True,
        ).stdout
        audio = np.frombuffer(decoded, dtype="<i2").astype(np.float32) / 32768.0
        segments, info = model.transcribe(audio, beam_size=5, vad_filter=False)
        lines.extend([f"[{name}] duration={info.duration:.1f}s"])
        lines.extend(f"{segment.start:.1f}-{segment.end:.1f}: {segment.text.strip()}"
                     for segment in segments)
        lines.append("")
        print(f"{name}: {info.duration:.1f}s", flush=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(args.output)


if __name__ == "__main__":
    main()
