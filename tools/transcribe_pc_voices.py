"""Create review transcripts of installed PC voice clips with a cached model.

Automated recognition is diagnostic evidence; check ambiguous words against
the source audio and the game's visible communication before changing a map.
"""

import argparse
import subprocess
from pathlib import Path

import numpy as np
from faster_whisper import WhisperModel


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("clips", nargs="+", help="WAV names in the PC Sdata folder")
    parser.add_argument(
        "--sdata",
        type=Path,
        default=Path("C:/Games/Star Wars Shadows of the Empire/Sdata"),
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/diagnostics/pc_voice_transcripts.txt"),
    )
    args = parser.parse_args()
    model = WhisperModel(
        "small.en", device="cpu", compute_type="int8", local_files_only=True
    )
    lines = [
        "Automated faster-whisper small.en transcript of installed PC WAV files.",
        "Verify ambiguous words against source audio and visible game messages.",
        "",
    ]
    for name in args.clips:
        path = args.sdata / name
        if not path.is_file():
            raise FileNotFoundError(path)
        decoded = subprocess.run(
            [
                "ffmpeg", "-hide_banner", "-loglevel", "error", "-i", str(path),
                "-f", "s16le", "-ar", "16000", "-ac", "1", "pipe:1",
            ],
            check=True,
            capture_output=True,
        ).stdout
        audio = np.frombuffer(decoded, dtype="<i2").astype(np.float32) / 32768.0
        segments, info = model.transcribe(audio, beam_size=5, vad_filter=False)
        lines.append(f"[{name}] duration={info.duration:.2f}s")
        lines.extend(
            f"{segment.start:.2f}-{segment.end:.2f}: {segment.text.strip()}"
            for segment in segments
        )
        lines.append("")
        print(f"{name}: {info.duration:.2f}s", flush=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(args.output)


if __name__ == "__main__":
    main()
