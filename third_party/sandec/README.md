# sandec

`sandec.c` and `sandec.h` are from
[mlauss2/sandec](https://github.com/mlauss2/sandec), commit
`a2c4f040f0c1d73a1e0d92046320948d3d4d3d57`, by Manuel Lauss.
They are distributed under LGPL-2.1-only; see `LICENSE` here. The source is
modified locally to preserve SotE's STOR/FTCH reference frame after delta
decoding, avoiding stale images during the final held frames. The game links a replaceable shared library to decode user-provided
SAN video and audio during playback, without decoded files on disk. The full
library source and LGPL license are supplied with releases. No movie data is
redistributed.
