These diagnostics do not install or modify game files.
Set SOTE_CUE_WORK to a disposable work directory containing:
  candidates/ : the three MP3 files from the uploaded fetcher ZIP
  references/ : HERO.WAV and DEATH2.WAV
  native_audio/native_21.s16le, native_61.s16le, native_62.s16le
The native files are mono signed little-endian 16-bit PCM at the verified
11,025 Hz playback rate, produced by the supplied ROM / generated-RSP decoder
from the previous menu/crawl audit. No ROM or native decoded audio is included.
Requires Python, NumPy, SciPy, SoundFile, and (for plots) Matplotlib.
Run initial_compare.py, spectral_compare.py; then refine_local.py once for each
ID 62, 61, 21. Run refine_local.py 62 7.182040816326531 for the loop anchor.
Then run coherence_test.py, inspect_edits.py, and make_plots.py.
Refinement checks only the expected candidate near spectrally identified
positions. The initial waveform/spectral searches compare all three files.
The abandoned full-file fine-speed search is not part of the final evidence.
