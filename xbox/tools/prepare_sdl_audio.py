"""Build a private Xbox variant of the installed SDL converter."""
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text()

def insert(anchor, addition):
    global source
    if source.count(anchor) != 1:
        raise SystemExit(f"Unrecognized SDL resampler at {anchor}")
    source = source.replace(anchor, anchor + addition)

insert("static float *ResamplerFilterDifference = NULL;",
       '\n#include "stereo_resample.h"\n'
       '_Static_assert(RESAMPLER_ZERO_CROSSINGS + 1 == XB_AUDIO_TAPS, "SDL filter width");\n')
insert("kaiser_and_sinc(ResamplerFilter, ResamplerFilterDifference, RESAMPLER_FILTER_SIZE, beta);",
       "\n        xb_prepare_stereo_filter(ResamplerFilter, ResamplerFilterDifference, "
       "RESAMPLER_SAMPLES_PER_ZERO_CROSSING, RESAMPLER_FILTER_SIZE);")
insert("    double outtime = 0.0;\n    int i, j, chan;",
       "\n    if (chans == 2 && inrate == 22050 && outrate == 48000 && outframes <= XB_AUDIO_FRAMES)\n"
       "        return xb_resample_stereo(lpadding, rpadding, inbuf, inframes, paddinglen, dst, outframes);")
Path(sys.argv[2]).write_text("/* Xbox build: cached SDL stereo filter coefficients. */\n" + source)
