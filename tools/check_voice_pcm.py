"""Locate a supplied mono voice WAV in a diagnostic stereo PCM dump.

The dump is written by diagnose_san_placement.ps1 -AudioProbe. A strong
correlation establishes that the clip entered the game's mixed audio stream;
the paired native capture and queue log establish which message was visible.
"""

import argparse
from pathlib import Path
import wave

import numpy as np
from scipy.signal import fftconvolve


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("voice_wav", type=Path)
    parser.add_argument("game_pcm", type=Path)
    parser.add_argument("--seconds", type=float, default=2.0)
    parser.add_argument("--pcm-rate", type=int, default=22050,
                        help="sample rate of the game PCM dump (check runtime log)")
    args = parser.parse_args()
    if args.pcm_rate <= 0:
        parser.error("PCM rate must be positive")

    with wave.open(str(args.voice_wav), "rb") as voice:
        if voice.getnchannels() != 1 or voice.getsampwidth() not in (1, 2):
            parser.error("voice WAV must be 8-bit or 16-bit mono PCM")
        voice_rate = voice.getframerate()
        raw = voice.readframes(min(voice.getnframes(),
                                   round(args.seconds * voice_rate)))
        if voice.getsampwidth() == 1:
            template = (np.frombuffer(raw, dtype=np.uint8).astype(np.float64) - 128) * 256
        else:
            template = np.frombuffer(raw, dtype="<i2").astype(np.float64)
        sample_rate = args.pcm_rate
        if voice_rate != sample_rate:
            # The runtime mixer uses linear interpolation between voice frames.
            positions = np.arange(round(len(template) * sample_rate / voice_rate)) * voice_rate / sample_rate
            template = np.interp(positions, np.arange(len(template)), template)

    if len(template) < sample_rate // 2:
        parser.error("voice comparison needs at least half a second")
    samples = np.fromfile(args.game_pcm, dtype="<i2")
    if len(samples) % 2:
        parser.error("stereo PCM dump has an odd sample count")
    output = samples.astype(np.float64).reshape(-1, 2).mean(axis=1)
    if len(output) < len(template):
        parser.error("PCM dump is shorter than the voice comparison")

    template -= template.mean()
    product = fftconvolve(output, template[::-1], mode="valid")
    energy_sum = np.cumsum(np.r_[0.0, output * output])
    n = len(template)
    energy = np.sqrt(np.maximum(0.0, energy_sum[n:] - energy_sum[:-n]))
    correlation = product / (np.linalg.norm(template) *
                             np.maximum(energy, 1.0))
    valid = energy > 1000.0
    score = np.where(valid, np.abs(correlation), 0.0)
    peak = int(np.argmax(score))
    if score[peak] == 0.0:
        parser.error("no non-silent audio window found")

    secondary = score.copy()
    secondary[max(0, peak - sample_rate):peak + sample_rate] = 0.0
    print(f"voice={args.voice_wav.name} dump={args.game_pcm}")
    print(f"peak={correlation[peak]:.6f} at={peak / sample_rate:.3f}s "
          f"next_outside_1s={secondary.max():.6f}")


if __name__ == "__main__":
    main()
