/*
 * A/V decoder for LucasArts SMUSH ANM/NUT/SAN/SNM files.
 *
 * Written in 2024-2026 by Manuel Lauss <manuel.lauss@gmail.com>
 *
 * Some algorithms were taken from FFmpeg/ScummVM/smushplay,
 *  others were reversed from the various game executables.
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#include <memory.h>
#include <stdlib.h>
#include "sandec.h"

#ifndef _max
#define _max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef _min
#define _min(a,b) ((a) > (b) ? (b) : (a))
#endif

#define bswap_16(value) \
	((((value) & 0xff) << 8) | ((value) >> 8))

#define bswap_32(value) \
	(((uint32_t)bswap_16((uint16_t)((value) & 0xffff)) << 16) | \
	  (uint32_t)bswap_16((uint16_t)((value) >> 16)))

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

#define le32_to_cpu(x) (x)
#define le16_to_cpu(x) (x)
#define be32_to_cpu(x) bswap_32(x)
#define be16_to_cpu(x) bswap_16(x)
#define cpu_to_le16(x) (x)

#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

#define be32_to_cpu(x)  (x)
#define be16_to_cpu(x)  (x)
#define le32_to_cpu(x)  bswap_32(x)
#define le16_to_cpu(x)  bswap_16(x)
#define cpu_to_le16(x)  bswap_16(x)

#else

#error "unknown endianness"

#endif

/* bytewise read an unaligned 16bit value from memory */
static inline uint16_t ua16(const uint8_t *p)
{
	return p[0] | (p[1] << 8);
}

/* bytewise read an unaligned 32bit value from memory */
static inline uint32_t ua32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
		((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* chunk identifiers LE */
#define ANIM	0x4d494e41
#define AHDR	0x52444841
#define FRME	0x454d5246
#define NPAL	0x4c41504e
#define FOBJ	0x4a424f46
#define IACT	0x54434149
#define TRES	0x53455254
#define STOR	0x524f5453
#define FTCH	0x48435446
#define XPAL	0x4c415058
#define iMUS	0x53554d69
#define MAP_	0x2050414d
#define FRMT	0x544d5246
#define DATA	0x41544144
#define PSAD	0x44415350
#define SAUD	0x44554153
#define STRK	0x4B525453
#define SDAT	0x54414453
#define PVOC	0x434f5650
#define PSD2	0x32445350
#define SANM	0x4d4e4153
#define SHDR	0x52444853
#define FLHD	0x44484c46
#define BL16	0x36316c42
#define WAVE	0x65766157
#define ANNO	0x4f4e4e41
#define IMA4	0x494d4134
#define FADE	0x45444146
#define FDHD	0x44484446
#define FFRM	0x4d524646
#define GOST	0x54534f47

/* ANM_FLAGS: various flags passed to the fob decoders and font renderer */
#define ANM_FLAG_IGN_FOB_OFS		0x0001
#define ANM_FLAG_ORIGIN_CENTER		0x0002
#define ANM_FLAG_DECODE_PRETEND 	0x0010
#define ANM_FLAG_SKIP_CLR_DST		0x0020
#define ANM_FLAG_CODEC_OPAQUE		0x0100
/* GOST codec mirroring flags */
#define ANM_FLAG_FLIPX			0x2000
#define ANM_FLAG_FLIPY			0x4000

/* reasonable maximum size of a FRME */
#define FRME_MAX_SIZE	(4 << 20)

/* ANIM maximum image size */
#define FOBJ_MAXX	640
#define FOBJ_MAXY	480

/* sizes of various internal work buffers */
#define SZ_IACT		(4096)
#define SZ_PAL		(256 * sizeof(uint32_t))
#define SZ_DELTAPAL	(768 * sizeof(int16_t))
#define SZ_SHIFTPAL	(768 * sizeof(int16_t))
#define SZ_C47IPTBL	(256 * 256)
#define SZ_ADSTBUF	(393216)
#define SZ_ANMBUFS (SZ_IACT + (2 * SZ_PAL) + SZ_DELTAPAL + SZ_C47IPTBL + SZ_SHIFTPAL)

/* codec47 glyhps */
#define GLYPH_COORD_VECT_SIZE 16
#define NGLYPHS 256

static const int ATRK_DEST_RATE = 22050;

/* maximum volume. */
static const int ATRK_VOL_MAX = 127;

/* 1M for Dig */
#define _ATRK_DATSZ	(1 << 20)
_Static_assert((_ATRK_DATSZ > 0) && ((_ATRK_DATSZ & (_ATRK_DATSZ - 1)) == 0),
	       "_ATRK_DATSZ must be a power of 2.");

static const uint32_t ATRK_DATSZ = _ATRK_DATSZ;
static const uint32_t ATRK_DATMASK = (_ATRK_DATSZ - 1);

#define ATRK_MAX		16
#define ATRK_MAX_STRK_SIZE	5120

/* PSAD flags defining audio track type (voice/music/sfx).
 * ANIMv1 could only differentiate between them based on the chunk name,
 * while ANIMv2 added these flags to the PSAD header itself.
 */
#define SAUD_FLAG_TRK_MASK	0xc0
#define SAUD_FLAG_TRK_VOICE	0x80
#define SAUD_FLAG_TRK_MUSIC	0x40
#define SAUD_FLAG_TRK_SFX	0x00
/* source format flags */
#define ATRK_1CH		0x01
#define ATRK_SRC8BIT		0x02
#define ATRK_SRC12BIT		0x04

enum atrk_state {
	STATE_UNUSED = 0,	/* unused				*/
	STATE_HEADER = 1,	/* waiting for header completion	*/
	STATE_BLOCKED = 2,	/* waiting for 1-frame PCM data		*/
	STATE_MIXABLE = 3,	/* ACTIVE, consider for mixing		*/
	STATE_MIXED = 4,	/* has been mixed already in this frme	*/
	STATE_NEWDATA = 5	/* new playlen has been set, reconsider */
};


struct sanatrk;
struct sanmsa;

/* per-track resample+upmix function to 2ch/16bit/22.050kHz		*/
typedef uint32_t (*atrk_rsp_fn)(struct sanatrk*, int16_t*, uint32_t);

struct sanatrk {
	/* ATRK fields */
	uint8_t *data;		/* input data buffer			*/
	uint32_t rdptr;		/* read pointer				*/
	uint32_t wrptr;		/* write pointer			*/
	uint32_t flags;		/* source format/type flags		*/
	enum atrk_state state;	/* Track state				*/
	atrk_rsp_fn resample;	/* destformat conversion function	*/
	int32_t dataleft;	/* SAUD data left until track ends	*/
	uint32_t dstfavail;	/* frames in dest format available	*/
	uint32_t src_accum;	/* SRC accumulator 16.16		*/
	uint32_t src_cnvrate;	/* SRC conversion ratio 16.16		*/
	uint16_t srate;		/* source sample rate			*/

	/* PSAD/SAUD-related fields */
	uint16_t trkid;		/* ID of this track			*/
	uint16_t curridx;	/* index of the last PSAD chunk		*/
	uint16_t maxidx;	/* expected highest PSAD chunk index	*/
	uint16_t vol;		/* PSAD/iMUS stream volume		*/
	int8_t pan;		/* PSAD stream pan			*/
	uint32_t playlen;	/* amount of bytes still to play	*/
	uint32_t dstpavail;	/* frames in dest format still to play	*/
	uint32_t af0;		/* 435af0 of the channel		*/
	uint32_t af4;		/* 435af4 of the channel		*/
	uint8_t strk[ATRK_MAX_STRK_SIZE];	/* STRK opcodes		*/
	uint16_t strkptr;	/* STRK pc				*/
	uint16_t strksz;	/* STRK data size			*/
	struct sanmsa *msa;	/* ptr back to shared parent		*/
};

/* Multistream Audio container */
struct sanmsa {
	struct sanatrk atrk[ATRK_MAX];	/* audio tracks 		*/
	uint8_t *audrsb1;	/* 8 generic 1-frame resample dest buf	*/
	uint32_t audminframes;	/* 4 dest sample rate frames for a frame*/
	uint8_t sou_hooks[256];		/* 256 PSAD shared sound hooks	*/
	uint16_t sou_vol_sfx;			/* sfx volume		*/
	uint16_t sou_vol_voice;			/* voice volume		*/
	uint16_t sou_vol_music;			/* global music volume	*/
	uint16_t sou_vol_global;		/* overall volume	*/
	int16_t sou_vol_damp;		/* damping volume, 255 no damp	*/
	uint16_t sou_damp_min;		/* minimum damping volume	*/
	uint16_t sou_damp_max;		/* maximum damping volume	*/
	uint16_t sou_damp_dip_rate;	/* damping volume reduct. rate  */
	uint16_t sou_damp_rise_rate;	/* damping volume augment. rate */
	uint32_t srcrate;		/* 4 SAN audio samplerate in Hz	*/
	uint32_t destrate;		/* 4 desired destination rate	*/
	uint8_t numtrk;			/* number of allocated tracks	*/
};

/* internal context: per-file */
struct sanrt {
	uint32_t frmebufsz;	/* 4 size of buffer below		*/
	uint8_t *fcache;	/* 8 one cached FRME object		*/
	uint8_t *fbuf;		/* 8 current front buffer		*/
	uint8_t *buf0;		/* 8 c37/47/48 front buffer		*/
	uint8_t *buf1;		/* 8 c47 delta buffer 1			*/
	uint8_t *buf2;		/* 8 c47 delta buffer 2			*/
	uint8_t *buf3;		/* 8 STOR buffer			*/
	uint8_t *buf4;		/* 8 last full frame for interpolation  */
	uint8_t *buf5;		/* 8 interpolated frame                 */
	uint8_t *vbuf;		/* 8 final image buffer passed to caller*/
	uint16_t pitch;		/* 2 image pitch			*/
	uint16_t bufw;		/* 2 alloc'ed buffer width/pitch	*/
	uint16_t bufh;		/* 2 alloc'ed buffer height		*/
	uint16_t def_anm_flags;	/* 2 movie default anm flags		*/
	int16_t  lastseq;	/* 2 c47 last sequence id		*/
	uint16_t subid;		/* 2 subtitle message number		*/
	uint16_t to_store;	/* 2 STOR encountered			*/
	uint16_t currframe;	/* 2 current frame index		*/
	uint16_t iactpos;	/* 2 IACT buffer write pointer		*/
	uint8_t *iactbuf;	/* 8 4kB for IACT chunks 		*/
	uint8_t *c47ipoltbl;	/* 8 c47 interpolation table Compression 1 */
	int16_t  *deltapal;	/* 8 768x 16bit for XPAL chunks		*/
	int16_t *shiftpal;	/* 8 256x shifted pal			*/
	uint32_t *palette;	/* 8 256x ABGR				*/
	uint32_t *iactpal;	/* 8 IACT-8 palette copy		*/
	uint32_t fbsize;	/* 4 size of the framebuffers		*/
	uint32_t framedur;	/* 4 standard frame duration		*/
	uint16_t FRMEcnt;	/* 2 number of FRMEs in SAN		*/
	uint16_t version;	/* 2 SAN version number			*/
	uint8_t  have_vdims:1;	/* 1 we have valid video dimensions	*/
	uint8_t  have_frame:1;	/* 1 we have a valid video frame	*/
	uint8_t  have_itable:1;	/* 1 have c47/48 interpolation table    */
	uint8_t  can_ipol:1;	/* 1 do an interpolation                */
	uint8_t  have_ipframe:1;/* 1 we have an interpolated frame      */
	uint8_t  iact8c4x:1;	/* 1 IACT 8 for codec47/48 titles	*/
	uint8_t  iactimus:2;	/* 1 is TheDig/IACT 8/0/0/x>0 is audio	*/
	uint8_t  iactpal1;	/* 1 number of crossfade steps		*/
	uint8_t  iactpal2;	/* 1 stepsize				*/
	uint16_t iactpalfrme;	/* 2 curre FRME num at iactpal cmd	*/
	void	 *membase;	/* 8 base to allocated mem block	*/
	uint8_t *last_fobj;	/* 8 ptr to last FOBJ, for GOST		*/
	uint32_t last_fobj_size;/* 4 size of last FOBJ			*/
	uint8_t	 mortimer:1;	/* 1 upscale for Mortimer		*/
};

/* internal context: static stuff. */
struct sanctx {
	struct sanrt rt;
	struct sanio *io;
	struct sanmsa *msa;	/* 8 ATRK infra				*/
	uint32_t adestrate;	/* 4 desired audio output rate		*/
	int errdone;		/* latest error status */
	uint8_t *adstbuf1;	/* 8 audio buffer 1			*/

	/* codec47 static data */
	uint8_t c47_glyph4x4[NGLYPHS][16];
	uint8_t c47_glyph8x8[NGLYPHS][64];
	uint8_t c4tbl[2][256][16];
	uint8_t c23lut[256];
	uint8_t c45tbl1[768];
	uint8_t c45tbl2[0x8000];
	uint16_t c4tblparam;
	uint16_t vima_pred_tbl[5786];
};

/* VIMA data tables */
static const uint8_t vima_size_table[] = {
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
};

static const int8_t vima_itbl1[] = {
	-1, 4, -1, 4
};

static const int8_t vima_itbl2[] = {
	-1, -1, 2, 6, -1, -1, 2, 6
};

static const int8_t vima_itbl3[] = {
	-1, -1, -1, -1, 1, 2, 4, 6, -1, -1, -1, -1, 1, 2, 4, 6
};

static const int8_t vima_itbl4[] = {
	-1, -1, -1, -1, -1, -1, -1, -1, 1,  1,  1,  2,  2,  4,  5,  6,
	-1, -1, -1, -1, -1, -1, -1, -1, 1,  1,  1,  2,  2,  4,  5,  6
};

static const int8_t vima_itbl5[] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	1,  1,  1,  1,  1,  2,  2,  2,  2,  4,  4,  4,  5,  5,  6,  6,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	1,  1,  1,  1,  1,  2,  2,  2,  2,  4,  4,  4,  5,  5,  6,  6
};

static const int8_t vima_itbl6[] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,
	2,  2,  4,  4,  4,  4,  4,  4,  5,  5,  5,  5,  6,  6,  6,  6,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,
	2,  2,  4,  4,  4,  4,  4,  4,  5,  5,  5,  5,  6,  6,  6,  6
};

static const int8_t *const vima_itbls[] = {
	vima_itbl1,  vima_itbl2,  vima_itbl3, vima_itbl4, vima_itbl5, vima_itbl6
};

/* adpcm standard step table */
#define ADPCM_STEP_COUNT 89
const int16_t adpcm_step_table[ADPCM_STEP_COUNT] = {
	7,     8,     9,    10,    11,    12,    13,    14,    16,    17,
	19,    21,    23,    25,    28,    31,    34,    37,    41,    45,
	50,    55,    60,    66,    73,    80,    88,    97,   107,   118,
	130,   143,   157,   173,   190,   209,   230,   253,   279,   307,
	337,   371,   408,   449,   494,   544,   598,   658,   724,   796,
	876,   963,  1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
	2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,
	5894,  6484,  7132,  7845,  8630,  9493, 10442, 11487, 12635, 13899,
	15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static const int8_t adpcm_index_table[16] = {
	-1, -1, -1, -1,  2,  4,  6,  8, -1, -1, -1, -1,  2,  4,  6,  8
};

/* Codec37/Codec48 motion vectors */
static const int8_t c37_mv[3][512] = {
	{
	0,   0,   1,   0,   2,   0,   3,   0,   5,   0,   8,   0,  13,   0,  21,
	0,  -1,   0,  -2,   0,  -3,   0,  -5,   0,  -8,   0, -13,   0, -17,   0,
	-21,   0,   0,   1,   1,   1,   2,   1,   3,   1,   5,   1,   8,   1,  13,
	1,  21,   1,  -1,   1,  -2,   1,  -3,   1,  -5,   1,  -8,   1, -13,   1,
	-17,   1, -21,   1,   0,   2,   1,   2,   2,   2,   3,   2,   5,   2,   8,
	2,  13,   2,  21,   2,  -1,   2,  -2,   2,  -3,   2,  -5,   2,  -8,   2,
	-13,   2, -17,   2, -21,   2,   0,   3,   1,   3,   2,   3,   3,   3,   5,
	3,   8,   3,  13,   3,  21,   3,  -1,   3,  -2,   3,  -3,   3,  -5,   3,
	-8,   3, -13,   3, -17,   3, -21,   3,   0,   5,   1,   5,   2,   5,   3,
	5,   5,   5,   8,   5,  13,   5,  21,   5,  -1,   5,  -2,   5,  -3,   5,
	-5,   5,  -8,   5, -13,   5, -17,   5, -21,   5,   0,   8,   1,   8,   2,
	8,   3,   8,   5,   8,   8,   8,  13,   8,  21,   8,  -1,   8,  -2,   8,
	-3,   8,  -5,   8,  -8,   8, -13,   8, -17,   8, -21,   8,   0,  13,   1,
	13,   2,  13,   3,  13,   5,  13,   8,  13,  13,  13,  21,  13,  -1,  13,
	-2,  13,  -3,  13,  -5,  13,  -8,  13, -13,  13, -17,  13, -21,  13,   0,
	21,   1,  21,   2,  21,   3,  21,   5,  21,   8,  21,  13,  21,  21,  21,
	-1,  21,  -2,  21,  -3,  21,  -5,  21,  -8,  21, -13,  21, -17,  21, -21,
	21,   0,  -1,   1,  -1,   2,  -1,   3,  -1,   5,  -1,   8,  -1,  13,  -1,
	21,  -1,  -1,  -1,  -2,  -1,  -3,  -1,  -5,  -1,  -8,  -1, -13,  -1, -17,
	-1, -21,  -1,   0,  -2,   1,  -2,   2,  -2,   3,  -2,   5,  -2,   8,  -2,
	13,  -2,  21,  -2,  -1,  -2,  -2,  -2,  -3,  -2,  -5,  -2,  -8,  -2, -13,
	-2, -17,  -2, -21,  -2,   0,  -3,   1,  -3,   2,  -3,   3,  -3,   5,  -3,
	8,  -3,  13,  -3,  21,  -3,  -1,  -3,  -2,  -3,  -3,  -3,  -5,  -3,  -8,
	-3, -13,  -3, -17,  -3, -21,  -3,   0,  -5,   1,  -5,   2,  -5,   3,  -5,
	5,  -5,   8,  -5,  13,  -5,  21,  -5,  -1,  -5,  -2,  -5,  -3,  -5,  -5,
	-5,  -8,  -5, -13,  -5, -17,  -5, -21,  -5,   0,  -8,   1,  -8,   2,  -8,
	3,  -8,   5,  -8,   8,  -8,  13,  -8,  21,  -8,  -1,  -8,  -2,  -8,  -3,
	-8,  -5,  -8,  -8,  -8, -13,  -8, -17,  -8, -21,  -8,   0, -13,   1, -13,
	2, -13,   3, -13,   5, -13,   8, -13,  13, -13,  21, -13,  -1, -13,  -2,
	-13,  -3, -13,  -5, -13,  -8, -13, -13, -13, -17, -13, -21, -13,   0, -17,
	1, -17,   2, -17,   3, -17,   5, -17,   8, -17,  13, -17,  21, -17,  -1,
	-17,  -2, -17,  -3, -17,  -5, -17,  -8, -17, -13, -17, -17, -17, -21, -17,
	0, -21,   1, -21,   2, -21,   3, -21,   5, -21,   8, -21,  13, -21,  21,
	-21,  -1, -21,  -2, -21,  -3, -21,  -5, -21,  -8, -21, -13, -21, -17, -21,
	0, 0
	},
	 {
	0,   0,  -8, -29,   8, -29, -18, -25,  17, -25,   0, -23,  -6, -22,   6,
	-22, -13, -19,  12, -19,   0, -18,  25, -18, -25, -17,  -5, -17,   5, -17,
	-10, -15,  10, -15,   0, -14,  -4, -13,   4, -13,  19, -13, -19, -12,  -8,
	-11,  -2, -11,   0, -11,   2, -11,   8, -11, -15, -10,  -4, -10,   4, -10,
	15, -10,  -6,  -9,  -1,  -9,   1,  -9,   6,  -9, -29,  -8, -11,  -8,  -8,
	-8,  -3,  -8,   3,  -8,   8,  -8,  11,  -8,  29,  -8,  -5,  -7,  -2,  -7,
	0,  -7,   2,  -7,   5,  -7, -22,  -6,  -9,  -6,  -6,  -6,  -3,  -6,  -1,
	-6,   1,  -6,   3,  -6,   6,  -6,   9,  -6,  22,  -6, -17,  -5,  -7,  -5,
	-4,  -5,  -2,  -5,   0,  -5,   2,  -5,   4,  -5,   7,  -5,  17,  -5, -13,
	-4, -10,  -4,  -5,  -4,  -3,  -4,  -1,  -4,   0,  -4,   1,  -4,   3,  -4,
	5,  -4,  10,  -4,  13,  -4,  -8,  -3,  -6,  -3,  -4,  -3,  -3,  -3,  -2,
	-3,  -1,  -3,   0,  -3,   1,  -3,   2,  -3,   4,  -3,   6,  -3,   8,  -3,
	-11,  -2,  -7,  -2,  -5,  -2,  -3,  -2,  -2,  -2,  -1,  -2,   0,  -2,   1,
	-2,   2,  -2,   3,  -2,   5,  -2,   7,  -2,  11,  -2,  -9,  -1,  -6,  -1,
	-4,  -1,  -3,  -1,  -2,  -1,  -1,  -1,   0,  -1,   1,  -1,   2,  -1,   3,
	-1,   4,  -1,   6,  -1,   9,  -1, -31,   0, -23,   0, -18,   0, -14,   0,
	-11,   0,  -7,   0,  -5,   0,  -4,   0,  -3,   0,  -2,   0,  -1,   0,   0,
	-31,   1,   0,   2,   0,   3,   0,   4,   0,   5,   0,   7,   0,  11,   0,
	14,   0,  18,   0,  23,   0,  31,   0,  -9,   1,  -6,   1,  -4,   1,  -3,
	1,  -2,   1,  -1,   1,   0,   1,   1,   1,   2,   1,   3,   1,   4,   1,
	6,   1,   9,   1, -11,   2,  -7,   2,  -5,   2,  -3,   2,  -2,   2,  -1,
	2,   0,   2,   1,   2,   2,   2,   3,   2,   5,   2,   7,   2,  11,   2,
	-8,   3,  -6,   3,  -4,   3,  -2,   3,  -1,   3,   0,   3,   1,   3,   2,
	3,   3,   3,   4,   3,   6,   3,   8,   3, -13,   4, -10,   4,  -5,   4,
	-3,   4,  -1,   4,   0,   4,   1,   4,   3,   4,   5,   4,  10,   4,  13,
	4, -17,   5,  -7,   5,  -4,   5,  -2,   5,   0,   5,   2,   5,   4,   5,
	7,   5,  17,   5, -22,   6,  -9,   6,  -6,   6,  -3,   6,  -1,   6,   1,
	6,   3,   6,   6,   6,   9,   6,  22,   6,  -5,   7,  -2,   7,   0,   7,
	2,   7,   5,   7, -29,   8, -11,   8,  -8,   8,  -3,   8,   3,   8,   8,
	8,  11,   8,  29,   8,  -6,   9,  -1,   9,   1,   9,   6,   9, -15,  10,
	-4,  10,   4,  10,  15,  10,  -8,  11,  -2,  11,   0,  11,   2,  11,   8,
	11,  19,  12, -19,  13,  -4,  13,   4,  13,   0,  14, -10,  15,  10,  15,
	-5,  17,   5,  17,  25,  17, -25,  18,   0,  18, -12,  19,  13,  19,  -6,
	22,   6,  22,   0,  23, -17,  25,  18,  25,  -8,  29,   8,  29,   0,  31,
	0, 0
	}, {
	0,   0,  -6, -22,   6, -22, -13, -19,  12, -19,   0, -18,  -5, -17,   5,
	-17, -10, -15,  10, -15,   0, -14,  -4, -13,   4, -13,  19, -13, -19, -12,
	-8, -11,  -2, -11,   0, -11,   2, -11,   8, -11, -15, -10,  -4, -10,   4,
	-10,  15, -10,  -6,  -9,  -1,  -9,   1,  -9,   6,  -9, -11,  -8,  -8,  -8,
	-3,  -8,   0,  -8,   3,  -8,   8,  -8,  11,  -8,  -5,  -7,  -2,  -7,   0,
	-7,   2,  -7,   5,  -7, -22,  -6,  -9,  -6,  -6,  -6,  -3,  -6,  -1,  -6,
	1,  -6,   3,  -6,   6,  -6,   9,  -6,  22,  -6, -17,  -5,  -7,  -5,  -4,
	-5,  -2,  -5,  -1,  -5,   0,  -5,   1,  -5,   2,  -5,   4,  -5,   7,  -5,
	17,  -5, -13,  -4, -10,  -4,  -5,  -4,  -3,  -4,  -2,  -4,  -1,  -4,   0,
	-4,   1,  -4,   2,  -4,   3,  -4,   5,  -4,  10,  -4,  13,  -4,  -8,  -3,
	-6,  -3,  -4,  -3,  -3,  -3,  -2,  -3,  -1,  -3,   0,  -3,   1,  -3,   2,
	-3,   3,  -3,   4,  -3,   6,  -3,   8,  -3, -11,  -2,  -7,  -2,  -5,  -2,
	-4,  -2,  -3,  -2,  -2,  -2,  -1,  -2,   0,  -2,   1,  -2,   2,  -2,   3,
	-2,   4,  -2,   5,  -2,   7,  -2,  11,  -2,  -9,  -1,  -6,  -1,  -5,  -1,
	-4,  -1,  -3,  -1,  -2,  -1,  -1,  -1,   0,  -1,   1,  -1,   2,  -1,   3,
	-1,   4,  -1,   5,  -1,   6,  -1,   9,  -1, -23,   0, -18,   0, -14,   0,
	-11,   0,  -7,   0,  -5,   0,  -4,   0,  -3,   0,  -2,   0,  -1,   0,   0,
	-23,   1,   0,   2,   0,   3,   0,   4,   0,   5,   0,   7,   0,  11,   0,
	14,   0,  18,   0,  23,   0,  -9,   1,  -6,   1,  -5,   1,  -4,   1,  -3,
	1,  -2,   1,  -1,   1,   0,   1,   1,   1,   2,   1,   3,   1,   4,   1,
	5,   1,   6,   1,   9,   1, -11,   2,  -7,   2,  -5,   2,  -4,   2,  -3,
	2,  -2,   2,  -1,   2,   0,   2,   1,   2,   2,   2,   3,   2,   4,   2,
	5,   2,   7,   2,  11,   2,  -8,   3,  -6,   3,  -4,   3,  -3,   3,  -2,
	3,  -1,   3,   0,   3,   1,   3,   2,   3,   3,   3,   4,   3,   6,   3,
	8,   3, -13,   4, -10,   4,  -5,   4,  -3,   4,  -2,   4,  -1,   4,   0,
	4,   1,   4,   2,   4,   3,   4,   5,   4,  10,   4,  13,   4, -17,   5,
	-7,   5,  -4,   5,  -2,   5,  -1,   5,   0,   5,   1,   5,   2,   5,   4,
	5,   7,   5,  17,   5, -22,   6,  -9,   6,  -6,   6,  -3,   6,  -1,   6,
	1,   6,   3,   6,   6,   6,   9,   6,  22,   6,  -5,   7,  -2,   7,   0,
	7,   2,   7,   5,   7, -11,   8,  -8,   8,  -3,   8,   0,   8,   3,   8,
	8,   8,  11,   8,  -6,   9,  -1,   9,   1,   9,   6,   9, -15,  10,  -4,
	10,   4,  10,  15,  10,  -8,  11,  -2,  11,   0,  11,   2,  11,   8,  11,
	19,  12, -19,  13,  -4,  13,   4,  13,   0,  14, -10,  15,  10,  15,  -5,
	17,   5,  17,   0,  18, -12,  19,  13,  19,  -6,  22,   6,  22,   0,  23,
	0, 0
	}
};

/******************************************************************************
 * SAN Codec47 Glyph setup, taken from ffmpeg
 * https://git.ffmpeg.org/gitweb/ffmpeg.git/blob_plain/HEAD:/libavcodec/sanm.c
 */

static const uint8_t c47_glyph4_x[GLYPH_COORD_VECT_SIZE] = {
	0, 1, 2, 3, 3, 3, 3, 2, 1, 0, 0, 0, 1, 2, 2, 1
};

static const uint8_t c47_glyph4_y[GLYPH_COORD_VECT_SIZE] = {
	0, 0, 0, 0, 1, 2, 3, 3, 3, 3, 2, 1, 1, 1, 2, 2
};

static const uint8_t c47_glyph8_x[GLYPH_COORD_VECT_SIZE] = {
	0, 2, 5, 7, 7, 7, 7, 7, 7, 5, 2, 0, 0, 0, 0, 0
};

static const uint8_t c47_glyph8_y[GLYPH_COORD_VECT_SIZE] = {
	0, 0, 0, 0, 1, 3, 4, 6, 7, 7, 7, 7, 6, 4, 3, 1
};

static const int8_t c47_mv[256][2] = {
	{   0,   0 }, {  -1, -43 }, {   6, -43 }, {  -9, -42 }, {  13, -41 },
	{ -16, -40 }, {  19, -39 }, { -23, -36 }, {  26, -34 }, {  -2, -33 },
	{   4, -33 }, { -29, -32 }, {  -9, -32 }, {  11, -31 }, { -16, -29 },
	{  32, -29 }, {  18, -28 }, { -34, -26 }, { -22, -25 }, {  -1, -25 },
	{   3, -25 }, {  -7, -24 }, {   8, -24 }, {  24, -23 }, {  36, -23 },
	{ -12, -22 }, {  13, -21 }, { -38, -20 }, {   0, -20 }, { -27, -19 },
	{  -4, -19 }, {   4, -19 }, { -17, -18 }, {  -8, -17 }, {   8, -17 },
	{  18, -17 }, {  28, -17 }, {  39, -17 }, { -12, -15 }, {  12, -15 },
	{ -21, -14 }, {  -1, -14 }, {   1, -14 }, { -41, -13 }, {  -5, -13 },
	{   5, -13 }, {  21, -13 }, { -31, -12 }, { -15, -11 }, {  -8, -11 },
	{   8, -11 }, {  15, -11 }, {  -2, -10 }, {   1, -10 }, {  31, -10 },
	{ -23,  -9 }, { -11,  -9 }, {  -5,  -9 }, {   4,  -9 }, {  11,  -9 },
	{  42,  -9 }, {   6,  -8 }, {  24,  -8 }, { -18,  -7 }, {  -7,  -7 },
	{  -3,  -7 }, {  -1,  -7 }, {   2,  -7 }, {  18,  -7 }, { -43,  -6 },
	{ -13,  -6 }, {  -4,  -6 }, {   4,  -6 }, {   8,  -6 }, { -33,  -5 },
	{  -9,  -5 }, {  -2,  -5 }, {   0,  -5 }, {   2,  -5 }, {   5,  -5 },
	{  13,  -5 }, { -25,  -4 }, {  -6,  -4 }, {  -3,  -4 }, {   3,  -4 },
	{   9,  -4 }, { -19,  -3 }, {  -7,  -3 }, {  -4,  -3 }, {  -2,  -3 },
	{  -1,  -3 }, {   0,  -3 }, {   1,  -3 }, {   2,  -3 }, {   4,  -3 },
	{   6,  -3 }, {  33,  -3 }, { -14,  -2 }, { -10,  -2 }, {  -5,  -2 },
	{  -3,  -2 }, {  -2,  -2 }, {  -1,  -2 }, {   0,  -2 }, {   1,  -2 },
	{   2,  -2 }, {   3,  -2 }, {   5,  -2 }, {   7,  -2 }, {  14,  -2 },
	{  19,  -2 }, {  25,  -2 }, {  43,  -2 }, {  -7,  -1 }, {  -3,  -1 },
	{  -2,  -1 }, {  -1,  -1 }, {   0,  -1 }, {   1,  -1 }, {   2,  -1 },
	{   3,  -1 }, {  10,  -1 }, {  -5,   0 }, {  -3,   0 }, {  -2,   0 },
	{  -1,   0 }, {   1,   0 }, {   2,   0 }, {   3,   0 }, {   5,   0 },
	{   7,   0 }, { -10,   1 }, {  -7,   1 }, {  -3,   1 }, {  -2,   1 },
	{  -1,   1 }, {   0,   1 }, {   1,   1 }, {   2,   1 }, {   3,   1 },
	{ -43,   2 }, { -25,   2 }, { -19,   2 }, { -14,   2 }, {  -5,   2 },
	{  -3,   2 }, {  -2,   2 }, {  -1,   2 }, {   0,   2 }, {   1,   2 },
	{   2,   2 }, {   3,   2 }, {   5,   2 }, {   7,   2 }, {  10,   2 },
	{  14,   2 }, { -33,   3 }, {  -6,   3 }, {  -4,   3 }, {  -2,   3 },
	{  -1,   3 }, {   0,   3 }, {   1,   3 }, {   2,   3 }, {   4,   3 },
	{  19,   3 }, {  -9,   4 }, {  -3,   4 }, {   3,   4 }, {   7,   4 },
	{  25,   4 }, { -13,   5 }, {  -5,   5 }, {  -2,   5 }, {   0,   5 },
	{   2,   5 }, {   5,   5 }, {   9,   5 }, {  33,   5 }, {  -8,   6 },
	{  -4,   6 }, {   4,   6 }, {  13,   6 }, {  43,   6 }, { -18,   7 },
	{  -2,   7 }, {   0,   7 }, {   2,   7 }, {   7,   7 }, {  18,   7 },
	{ -24,   8 }, {  -6,   8 }, { -42,   9 }, { -11,   9 }, {  -4,   9 },
	{   5,   9 }, {  11,   9 }, {  23,   9 }, { -31,  10 }, {  -1,  10 },
	{   2,  10 }, { -15,  11 }, {  -8,  11 }, {   8,  11 }, {  15,  11 },
	{  31,  12 }, { -21,  13 }, {  -5,  13 }, {   5,  13 }, {  41,  13 },
	{  -1,  14 }, {   1,  14 }, {  21,  14 }, { -12,  15 }, {  12,  15 },
	{ -39,  17 }, { -28,  17 }, { -18,  17 }, {  -8,  17 }, {   8,  17 },
	{  17,  18 }, {  -4,  19 }, {   0,  19 }, {   4,  19 }, {  27,  19 },
	{  38,  20 }, { -13,  21 }, {  12,  22 }, { -36,  23 }, { -24,  23 },
	{  -8,  24 }, {   7,  24 }, {  -3,  25 }, {   1,  25 }, {  22,  25 },
	{  34,  26 }, { -18,  28 }, { -32,  29 }, {  16,  29 }, { -11,  31 },
	{   9,  32 }, {  29,  32 }, {  -4,  33 }, {   2,  33 }, { -26,  34 },
	{  23,  36 }, { -19,  39 }, {  16,  40 }, { -13,  41 }, {   9,  42 },
	{  -6,  43 }, {   1,  43 }, {   0,   0 }, {   0,   0 }, {   0,   0 },
};

enum GlyphEdge {
	LEFT_EDGE,
	TOP_EDGE,
	RIGHT_EDGE,
	BOTTOM_EDGE,
	NO_EDGE
};

enum GlyphDir {
	DIR_LEFT,
	DIR_UP,
	DIR_RIGHT,
	DIR_DOWN,
	NO_DIR
};

static enum GlyphEdge c47_which_edge(int x, int y, int edge_size)
{
	const int edge_max = edge_size - 1;

	if (!y)
		return BOTTOM_EDGE;
	else if (y == edge_max)
		return TOP_EDGE;
	else if (!x)
		return LEFT_EDGE;
	else if (x == edge_max)
		return RIGHT_EDGE;
	else
		return NO_EDGE;
}

static enum GlyphDir c47_which_direction(enum GlyphEdge edge0, enum GlyphEdge edge1)
{
	if ((edge0 == LEFT_EDGE && edge1 == RIGHT_EDGE) ||
		(edge1 == LEFT_EDGE && edge0 == RIGHT_EDGE) ||
		(edge0 == BOTTOM_EDGE && edge1 != TOP_EDGE) ||
		(edge1 == BOTTOM_EDGE && edge0 != TOP_EDGE))
		return DIR_UP;
	else if ((edge0 == TOP_EDGE && edge1 != BOTTOM_EDGE) ||
		(edge1 == TOP_EDGE && edge0 != BOTTOM_EDGE))
		return DIR_DOWN;
	else if ((edge0 == LEFT_EDGE && edge1 != RIGHT_EDGE) ||
		(edge1 == LEFT_EDGE && edge0 != RIGHT_EDGE))
		return DIR_LEFT;
	else if ((edge0 == TOP_EDGE && edge1 == BOTTOM_EDGE) ||
		(edge1 == TOP_EDGE && edge0 == BOTTOM_EDGE) ||
		(edge0 == RIGHT_EDGE && edge1 != LEFT_EDGE) ||
		(edge1 == RIGHT_EDGE && edge0 != LEFT_EDGE))
		return DIR_RIGHT;

	return NO_DIR;
}

/* Interpolate two points. */
static void c47_interp_point(uint8_t *points, int x0, int y0, int x1, int y1,
			 int pos, int npoints)
{
	if (npoints) {
		points[0] = (x0 * pos + x1 * (npoints - pos) + (npoints >> 1)) / npoints;
		points[1] = (y0 * pos + y1 * (npoints - pos) + (npoints >> 1)) / npoints;
	} else {
		points[0] = x0;
		points[1] = y0;
	}
}

static void c47_make_glyphs(uint8_t *pglyphs, const uint8_t *xvec, const uint8_t *yvec,
			const int side_length)
{
	const int glyph_size = side_length * side_length;
	uint8_t *pglyph = pglyphs;

	int i, j;
	for (i = 0; i < GLYPH_COORD_VECT_SIZE; i++) {
		int x0 = xvec[i];
		int y0 = yvec[i];
		enum GlyphEdge edge0 = c47_which_edge(x0, y0, side_length);

		for (j = 0; j < GLYPH_COORD_VECT_SIZE; j++, pglyph += glyph_size) {
			int x1 = xvec[j];
			int y1 = yvec[j];
			enum GlyphEdge edge1 = c47_which_edge(x1, y1, side_length);
			enum GlyphDir dir = c47_which_direction(edge0, edge1);
			int npoints = _max(abs(x1 - x0), abs(y1 - y0));
			int ipoint;

			for (ipoint = 0; ipoint <= npoints; ipoint++) {
				uint8_t point[2];
				int irow, icol;

				c47_interp_point(point, x0, y0, x1, y1, ipoint, npoints);

				switch (dir) {
				case DIR_UP:
					for (irow = point[1]; irow >= 0; irow--)
						pglyph[point[0] + irow * side_length] = 1;
					break;
				case DIR_DOWN:
					for (irow = point[1]; irow < side_length; irow++)
						pglyph[point[0] + irow * side_length] = 1;
					break;
				case DIR_LEFT:
					for (icol = point[0]; icol >= 0; icol--)
						pglyph[icol + point[1] * side_length] = 1;
					break;
				case DIR_RIGHT:
					for (icol = point[0]; icol < side_length; icol++)
						pglyph[icol + point[1] * side_length] = 1;
					break;
				case NO_DIR:
					break;
				}
			}
		}
	}
}

static void c4_5_tilegen(uint8_t *dst, uint8_t param1)
{
	int i, j, k, l, m, n, o;

	for (i = 1; i < 16; i += 2) {
		for (k = 0; k < 16; k++) {
			j = i + param1;
			l = k + param1;
			m = (j + l) / 2;
			n = (j + m) / 2;
			o = (l + m) / 2;
			if (j == m || l == m) {
				*dst++ = l; *dst++ = j; *dst++ = l; *dst++ = j;
				*dst++ = j; *dst++ = l; *dst++ = j; *dst++ = j;
				*dst++ = l; *dst++ = j; *dst++ = l; *dst++ = j;
				*dst++ = l; *dst++ = l; *dst++ = j; *dst++ = l;
			} else {
				*dst++ = m; *dst++ = m; *dst++ = n; *dst++ = j;
				*dst++ = m; *dst++ = m; *dst++ = n; *dst++ = j;
				*dst++ = o; *dst++ = o; *dst++ = m; *dst++ = n;
				*dst++ = l; *dst++ = l; *dst++ = o; *dst++ = m;
			}
		}
	}

	for (i = 0; i < 16; i += 2) {
		for (k = 0; k < 16; k++) {
			j = i + param1;
			l = k + param1;
			m = (j + l) / 2;
			n = (j + m) / 2;
			o = (l + m) / 2;
			if (m == j || m == l) {
				*dst++ = j; *dst++ = j; *dst++ = l; *dst++ = j;
				*dst++ = j; *dst++ = j; *dst++ = j; *dst++ = l;
				*dst++ = l; *dst++ = j; *dst++ = l; *dst++ = l;
				*dst++ = j; *dst++ = l; *dst++ = j; *dst++ = l;
			} else {
				*dst++ = j; *dst++ = j; *dst++ = n; *dst++ = m;
				*dst++ = j; *dst++ = j; *dst++ = n; *dst++ = m;
				*dst++ = n; *dst++ = n; *dst++ = m; *dst++ = o;
				*dst++ = m; *dst++ = m; *dst++ = o; *dst++ = l;
			}
		}
	}
}

static void c33_34_tilegen(uint8_t *dst, int8_t param1)
{
	int i, j, k, l, m, n, o, p;

	for (i = 0; i < 8; i++) {
		for (k = 0; k < 8; k++) {
			j = i + param1;
			l = k + param1;
			p = (j + l) >> 1;
			n = (j + p) >> 1;
			m = (p + l) >> 1;

			*dst++ = p; *dst++ = p; *dst++ = n; *dst++ = j;
			*dst++ = p; *dst++ = p; *dst++ = n; *dst++ = j;
			*dst++ = m; *dst++ = m; *dst++ = p; *dst++ = j;
			*dst++ = l; *dst++ = l; *dst++ = m; *dst++ = p;
		}
	}

	for (i = 0; i < 8; i++) {
		for (k = 0; k < 8; k++) {
			j = i + param1;
			l = k + param1;
			n = (j + l) >> 1;
			m = (l + n) >> 1;

			*dst++ = j; *dst++ = j; *dst++ = j; *dst++ = j;
			*dst++ = n; *dst++ = n; *dst++ = n; *dst++ = n;
			*dst++ = m; *dst++ = m; *dst++ = m; *dst++ = m;
			*dst++ = l; *dst++ = l; *dst++ = l; *dst++ = l;
		}
	}

	for (i = 0; i < 8; i++)	{
		for (k = 0; k < 8; k++) {
			j = i + param1;
			l = k + param1;
			m = (j + l) >> 1;
			n = (j + m) >> 1;
			o = (l + m) >> 1;

			*dst++ = j; *dst++ = j; *dst++ = n; *dst++ = m;
			*dst++ = j; *dst++ = j; *dst++ = n; *dst++ = m;
			*dst++ = n; *dst++ = n; *dst++ = m; *dst++ = o;
			*dst++ = m; *dst++ = m; *dst++ = o; *dst++ = l;
		}
	}

	for (i = 0; i < 8; i++) {
		for (k = 0; k < 8; k++) {
			j = i + param1;
			l = k + param1;
			m = (j + l) >> 1;
			n = (l + m) >> 1;

			*dst++ = j; *dst++ = m; *dst++ = n; *dst++ = l;
			*dst++ = j; *dst++ = m; *dst++ = n; *dst++ = l;
			*dst++ = j; *dst++ = m; *dst++ = n; *dst++ = l;
			*dst++ = j; *dst++ = m; *dst++ = n; *dst++ = l;
		}
	}
}

static void c4_5_param2(struct sanctx *ctx, uint8_t *src, uint16_t cnt,
		       uint8_t clr)
{
	uint8_t c, *dst = (uint8_t *)&(ctx->c4tbl[1]);
	uint32_t loop = cnt * 8;

	while (loop--) {
		c = *src++;
		*dst++ = (c >> 4) + clr;
		*dst++ = (c & 0xf) + clr;
	}
}

/******************************************************************************/

static void blt_solid(uint8_t * __restrict dst, uint8_t * __restrict src,
		      int16_t left, int16_t top, uint16_t srcxoff, uint16_t srcyoff,
		      uint16_t srcwidth, uint16_t srcheight, uint16_t srcpitch,
		      uint16_t dstpitch, uint16_t dstheight)
{
	if ((srcwidth == 0) || (srcheight == 0))
		return;
	if (top < 0) {
		if (-top >= srcheight)
			return;
		srcyoff -= top;
		srcheight += top;
		top = 0;
	}

	if ((top + srcheight) > dstheight) {
		int clip = (top + srcheight) - dstheight;
		if (clip >= srcheight)
			return;
		srcheight -= clip;
	}

	if (left < 0) {
		if (-left >= srcwidth)
			return;
		srcxoff -= left;
		srcwidth += left;
		left = 0;
	}

	if (left + srcwidth > dstpitch) {
		int clip = (left + srcwidth) - dstpitch;
		if (clip >= srcwidth)
			return;
		srcwidth -= clip;
	}
	src += ((uintptr_t)srcyoff * srcpitch) + srcxoff;
	dst += ((uintptr_t)top * dstpitch) + left;
	while ((srcheight--)) {
		memcpy(dst, src, srcwidth);
		src += srcpitch;
		dst += dstpitch;
	}
}

static void blt_mask(uint8_t * __restrict dst, uint8_t * __restrict src,
		     int16_t left, int16_t top, uint16_t srcxoff, uint16_t srcyoff,
		     uint16_t srcwidth, uint16_t srcheight, uint16_t srcpitch,
		     uint16_t dstpitch, uint16_t dstheight, uint8_t skipcolor)
{
	if ((srcwidth == 0) || (srcheight == 0))
		return;
	if (top < 0) {
		if (-top >= srcheight)
			return;
		srcyoff -= top;
		srcheight += top;
		top = 0;
	}

	if ((top + srcheight) > dstheight) {
		int clip = (top + srcheight) - dstheight;
		if (clip >= srcheight)
			return;
		srcheight -= clip;
	}

	if (left < 0) {
		if (-left >= srcwidth)
			return;
		srcxoff -= left;
		srcwidth += left;
		left = 0;
	}

	if (left + srcwidth > dstpitch) {
		int clip = (left + srcwidth) - dstpitch;
		if (clip >= srcwidth)
			return;
		srcwidth -= clip;
	}
	src += ((uintptr_t)srcyoff * srcpitch) + srcxoff;
	dst += ((uintptr_t)top * dstpitch) + left;
	for (int i = 0; (i < srcheight); i++) {
		for (int j = 0; (j < srcwidth); j++) {
			if (src[j] != skipcolor)
				dst[j] = src[j];
		}
		src += srcpitch;
		dst += dstpitch;
	}
}

static void blt_ipol(uint8_t * __restrict dst, uint8_t * __restrict src1,
		     uint8_t * __restrict src2, int16_t left, int16_t top,
		     uint16_t srcxoff, uint16_t srcyoff,
		     uint16_t srcwidth, uint16_t srcheight, uint16_t srcpitch,
		     uint16_t dstpitch, uint16_t dstheight, uint8_t *itbl)
{
	if ((srcwidth == 0) || (srcheight == 0))
		return;
	if (top < 0) {
		if (-top >= srcheight)
			return;
		srcyoff -= top;
		srcheight += top;
		top = 0;
	}

	if ((top + srcheight) > dstheight) {
		int clip = (top + srcheight) - dstheight;
		if (clip >= srcheight)
			return;
		srcheight -= clip;
	}

	if (left < 0) {
		if (-left >= srcwidth)
			return;
		srcxoff -= left;
		srcwidth += left;
		left = 0;
	}

	if (left + srcwidth > dstpitch) {
		int clip = (left + srcwidth) - dstpitch;
		if (clip >= srcwidth)
			return;
		srcwidth -= clip;
	}
	src1 += ((uintptr_t)srcyoff * srcpitch) + srcxoff;
	src2 += ((uintptr_t)srcyoff * srcpitch) + srcxoff;
	dst += ((uintptr_t)top * dstpitch) + left;
	for (int i = 0; (i < srcheight); i++) {
		for (int j = 0; (j < srcwidth); j++) {
			dst[j] = itbl[(src1[j] << 8) | src2[j]];
		}
		src1 += srcpitch;
		src2 += srcpitch;
		dst += dstpitch;
	}
}

static void blt_upscale_2x2(uint8_t * restrict dst, const uint8_t * restrict src,
		      int16_t dstxoff, int16_t dstyoff, uint16_t srcxoff, uint16_t srcyoff,
		      uint16_t srcwidth, uint16_t srcheight, uint16_t srcpitch,
		      uint16_t dstpitch, uint16_t dstheight)
{
	uint32_t dstxstart = (dstxoff < 0) ? 0 : dstxoff;
	uint32_t dstystart = (dstyoff < 0) ? 0 : dstyoff;
	uint32_t dstxend = dstxoff + (srcwidth * 2);
	uint32_t dstyend = dstyoff + (srcheight * 2);

	if (dstxend > dstpitch)
		dstxend = dstpitch;
	if (dstyend > dstheight)
		dstyend = dstheight;
	if (dstxstart >= dstxend || dstystart >= dstyend)
		return;

	for (uint32_t dy = dstystart; dy < dstyend; ++dy) {
		uint32_t sy = srcyoff + (dy - dstyoff) / 2;
		const uint8_t * restrict s = src + (sy * srcpitch);
		uint8_t * restrict d = dst + (dy * dstpitch);

		for (uint32_t dx = dstxstart; dx < dstxend; ++dx) {
			uint32_t sx = srcxoff + (dx - dstxoff) / 2;
			d[dx] = s[sx];
		}
	}
}

/* draw a solid-filled rectangle, for codec44 (NUT fonts) */
static void fillrect(uint8_t *dst, int16_t xoff, int16_t yoff, uint16_t width,
		     uint16_t height, uint16_t maxwidth, uint16_t maxheight, uint8_t color)
{
	if (yoff < 0) {
		if (height <= -yoff)
			return;
		height += yoff;
		yoff = 0;
	}

	if ((int32_t)yoff + height > maxheight) {
		if (yoff >= maxheight)
			return;
		height = maxheight - yoff;
	}

	if (xoff < 0) {
		if (width <= -xoff)
			return;
		width += xoff;
		xoff = 0;
	}

	if ((int32_t)xoff + width > maxwidth) {
		if (xoff >= maxwidth)
			return;
		width = maxwidth - xoff;
	}

	uint8_t *p = dst + ((int32_t)yoff * maxwidth) + xoff;

	while (height-- > 0) {
		memset(p, color, width);
		p += maxwidth;
	}
}

static inline int read_source(struct sanctx *ctx, void *dst, uint32_t sz)
{
	return !(ctx->io->ioread(ctx->io->userctx, dst, sz));
}

static void read_palette(struct sanctx *ctx, uint8_t *src)
{
	struct sanrt *rt = &ctx->rt;
	uint32_t *pal = rt->palette;
	uint8_t t[12];
	int i = 0;

	while (i < 256) {
		t[0] = *src++;
		t[1] = *src++;
		t[2] = *src++;
		*pal++ = 0xffU << 24 | t[2] << 16 | t[1] << 8 | t[0];
		i++;
	}

	/* RA1 always sets color index 0 to full black. */
	if (rt->version < 2)
		rt->palette[0] = 0xffU << 24;
}

static void interpolate_frame(uint8_t * __restrict dst,
			      const uint8_t * __restrict sr1,
			      const uint8_t * __restrict srs,
			      const uint8_t *itbl, const uint16_t w, const uint16_t h)
{
	int i, j, k;

	for (i = 0; i < h; i++) {
		for (j = 0; j < w; j++) {
			k = (*sr1++) << 8 | (*srs++);
			*dst++ = itbl[k];
		}
	}
}

/* swap the 3 buffers according to the codec */
static void c47_swap_bufs(struct sanctx *ctx, uint8_t rotcode)
{
	struct sanrt *rt = &ctx->rt;
	if (rotcode) {
		uint8_t *tmp;
		if (rotcode == 2) {
			tmp = rt->buf1;
			rt->buf1 = rt->buf2;
			rt->buf2 = tmp;
		}
		tmp = rt->buf2;
		rt->buf2 = rt->buf0;
		rt->buf0 = tmp;
	}
}

static void codec47_comp1(uint8_t * __restrict src, uint8_t * __restrict dst_in,
			  const uint8_t *itbl, uint16_t w, uint16_t h)
{
	/* input data is i-frame with half width and height. combining 2 pixels
	 * into a 16bit value, one can then use this value as an index into
	 * the interpolation table to get the missing color between 2 pixels.
	 */
	uint8_t *dst, p8, p82;
	uint16_t px;
	int i, j;

	/* start with 2nd row and create every other.  The first 2 pixels in each
	 * row are taken from the source, the next one is interpolated from the
	 * last and the following one.
	 */
	dst = dst_in + w;
	for (i = 0; i < h; i += 2) {
		p8 = *src++;
		*dst++ = p8;
		*dst++ = p8;
		px = p8;
		for (j = 2; j < w; j += 2) {
			p8 = *src++;
			px = (px << 8) | p8;
			*dst++ = itbl[px];
			*dst++ = p8;
		}
		dst += w;
	}

	/* do the rows: the first is a copy of the 2nd line, the missing ones
	 * are interpolated using the pixels of the rows above and below.
	 */
	memcpy(dst_in, dst_in + w, w);
	dst = dst_in + (w * 2);
	for (i = 2; i < h - 1; i += 2) {
		for (j = 0; j < w; j ++) {	/* walk along the full row */
			p8 = *(dst - w);	/* pixel from row above */
			p82 = *(dst + w);	/* pixel from row below */
			px = (p82 << 8) | p8;
			*dst++ = itbl[px];
		}
		dst += w;
	}
}

static uint8_t* codec47_block(struct sanctx *ctx, uint8_t * __restrict src,
			      uint8_t * __restrict dst, uint8_t * __restrict p1,
			      uint8_t * __restrict p2, const uint16_t w,
			      const uint8_t * __restrict coltbl, uint16_t size,
			      uint32_t *dsize)
{
	uint8_t opc, col[2], c;
	uint16_t i, j;
	uint8_t *pglyph;

	if ((*dsize) < 1)
		return 0;
	opc = *src++;
	(*dsize)--;
	if (opc >= 0xF8) {
		switch (opc) {
		case 0xff:
			if (size == 2) {
				if ((*dsize) < 4)
					return 0;
				*(dst + 0 + 0) = *src++; *(dst + 0 + 1) = *src++;
				*(dst + w + 0) = *src++; *(dst + w + 1) = *src++;
				(*dsize) -= 4;
			} else {
				size >>= 1;
				src = codec47_block(ctx, src, dst, p1, p2, w, coltbl, size, dsize);
				if (!src)
					return 0;
				src = codec47_block(ctx, src, dst + size, p1 + size, p2 + size, w, coltbl, size, dsize);
				if (!src)
					return 0;
				dst += (size * w);
				p1 += (size * w);
				p2 += (size * w);
				src = codec47_block(ctx, src, dst, p1, p2, w, coltbl, size, dsize);
				if (!src)
					return 0;
				src = codec47_block(ctx, src, dst + size, p1 + size, p2 + size, w, coltbl, size, dsize);
			}
			break;
		case 0xfe:
			if ((*dsize) < 1)
				return 0;
			c = *src++;
			(*dsize)--;
			for (i = 0; i < size; i++)
				for (j = 0; j < size; j++)
					*(dst + (i * w) + j) = c;
			break;
		case 0xfd:
			if ((*dsize) < 3)
				return 0;
			opc = *src++;
			col[1] = *src++;
			col[0] = *src++;
			pglyph = (size == 8) ? ctx->c47_glyph8x8[opc] : ctx->c47_glyph4x4[opc];
			for (i = 0; i < size; i++)
				for (j = 0; j < size; j++)
					*(dst + (i * w) + j) = col[*pglyph++];
			(*dsize) -= 3;
			break;
		case 0xfc:
			for (i = 0; i < size; i++)
				for (j = 0; j < size; j++)
					*(dst + (i * w) + j) = *(p1 + (i * w) + j);
			break;
		default:	/* fill a block with color from the 4-color codebook */
			c = coltbl[opc & 3];
			for (i = 0; i < size; i++)
				for (j = 0; j < size; j++)
					*(dst + (i * w) + j) = c;
		}
	} else {
		const int32_t mvoff = c47_mv[opc][0] + (c47_mv[opc][1] * w);
		for (i = 0; i < size; i++)
			for (j = 0; j < size; j++)
				*(dst + (i * w) + j) = *(p2 + (i * w) + j + mvoff);
	}
	return src;
}

static int codec47_comp2(struct sanctx *ctx, uint8_t * __restrict src,
			 uint8_t * __restrict dst, const uint16_t w, const uint16_t h,
			 const uint8_t * __restrict coltbl, uint32_t size)
{
	uint8_t *b1 = ctx->rt.buf1, *b2 = ctx->rt.buf2;
	unsigned int i, j;

	for (j = 0; (j < h) && src && size; j += 8) {
		for (i = 0; (i < w) && src && size; i += 8) {
			src = codec47_block(ctx, src, dst + i, b1 + i, b2 + i, w, coltbl, 8, &size);
		}
		dst += (w * 8);
		b1 += (w * 8);
		b2 += (w * 8);
	}
	return (src == 0) ? 1 : 0;
}

static void codec47_comp5(uint8_t * __restrict src, uint32_t size,
			  uint8_t * __restrict dst, uint32_t left)
{
	uint8_t opc, rlen, col, j;

	while (left && size) {
		opc = *src++;
		size--;
		rlen = (opc >> 1) + 1;
		if (rlen > left)
			rlen = left;
		if (opc & 1) {
			if (size < 1)
				return;
			col = *src++;
			size--;
			for (j = 0; j < rlen; j++)
				*dst++ = col;
		} else {
			if (size < rlen)
				return;
			for (j = 0; j < rlen; j++)
				*dst++ = *src++;
			size -= rlen;
		}
		left -= rlen;
	}
}

static void codec47_itable(struct sanctx *ctx, uint8_t *src)
{
	uint8_t *itbl, *p1, *p2;
	int i, j;

	itbl = ctx->rt.c47ipoltbl;
	for (i = 0; i < 256; i++) {
		p1 = p2 = itbl + i;
		for (j = 256 - i; j; j--) {
			*p1 = *p2 = *src++;
			p1 += 1;
			p2 += 256;
		}
		itbl += 256;
	}
	ctx->rt.have_itable = 1;
}

static int codec47(struct sanctx *ctx, uint8_t *dbuf, uint8_t *src, uint16_t w, uint16_t h,
		   int16_t xoff, int16_t yoff, uint32_t size, const uint16_t anm_flags)
{
	uint8_t *coltbl, comp, newrot, flag, *dst;
	uint32_t decsize;
	uint16_t seq;

	if (size < 26)
		return -60;

	seq =    le16_to_cpu(*(uint16_t *)(src + 0));
	comp =   src[2];
	newrot = src[3];
	flag =   src[4];
	coltbl = src + 8;	/* codebook 4 colors */
	decsize  = le32_to_cpu(ua32(src + 14));	/* decoded (raw frame) size */
	if (decsize > ctx->rt.fbsize)
		decsize = ctx->rt.fbsize;

	if (seq == 0) {
		ctx->rt.lastseq = -1;
		memset(ctx->rt.buf1, src[12], decsize);
		memset(ctx->rt.buf2, src[13], decsize);
	}
	src += 26;
	size -= 26;
	if (flag & 1) {
		if (size < 0x8080)
			return -61;
		codec47_itable(ctx, src);
		src += 0x8080;
		size -= 0x8080;
	}

	if (((anm_flags & ANM_FLAG_DECODE_PRETEND) != 0)
	    && (newrot == 0) && ((comp >= 2) && (comp <= 4))) {
		ctx->rt.lastseq = seq;
		return 0;	/* original returns error here */
	}

	dst = ctx->rt.buf0;
	switch (comp) {
	case 0:	if (size < w * h)
			return -62;
		memcpy(dst, src, w * h);
		break;
	case 1:	if (size < ((w * h) / 4))
			return -63;
		codec47_comp1(src, dst, ctx->rt.c47ipoltbl, w, h);
		break;
	case 2:	if (seq == (ctx->rt.lastseq + 1)) {
			if (0 != codec47_comp2(ctx, src, dst, w, h, coltbl, size))
				return -64;
		}
		break;
	case 3:	memcpy(dst, ctx->rt.buf2, ctx->rt.fbsize); break;
	case 4:	memcpy(dst, ctx->rt.buf1, ctx->rt.fbsize); break;
	case 5:	codec47_comp5(src, size, dst, decsize); break;
	default: break;
	}

	blt_solid(dbuf, dst, xoff, yoff, 0, 0, w, h, w, ctx->rt.pitch, ctx->rt.bufh);

	if (seq == ctx->rt.lastseq + 1)
		c47_swap_bufs(ctx, newrot);

	ctx->rt.lastseq = seq;
	if (seq > 1)
		ctx->rt.can_ipol = 1;

	return 0;
}

/******************************************************************************/

/* scale 4x4 input block to 8x8 output block */
static void c48_4to8(uint8_t * __restrict dst, const uint8_t * __restrict src,
		     const uint16_t w)
{
	uint16_t p;
	/* dst is always aligned, so we can do at least 16bit stores */
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 8; j += 2) { /* 1px > 2x2 block */
			p = *src++;
			p = (p << 8) | p; /* 1x2 line */
			*((uint16_t *)(dst + w * 0 + j)) = p;  // 0|0
			*((uint16_t *)(dst + w * 1 + j)) = p;  // 0|1
		}
		dst += w * 2;
	}
}

static int codec48_comp3(const uint8_t * __restrict src, uint8_t * __restrict dst,
			 const uint8_t * __restrict db, const uint8_t * __restrict itbl,
			 const uint16_t w, const uint16_t h, uint32_t size)
{
	uint8_t opc, sb[16];
	uint32_t ofs;
	int i, j, k, l, m, n;

	for (m = 0; m < h; m += 8) {
		for (n = 0; n < w; n += 8) {
			if (size < 1)
				return 1;
			opc = *src++;
			size -= 1;
			switch (opc) {
			case 0xFF:
				/* 4x4 to 8x8 upscale.  1 Reference color, while the
				 * other 15 have to be interpolated from existing
				 * pixels in adjacent blocks of the current buffer.
				 */
				if (size < 1)
					return 1;
				sb[15] = *src++;
				sb[ 7] = itbl[(*(dst - 1*w + 7 + n) << 8) | sb[15]];
				sb[ 3] = itbl[(*(dst - 1*w + 7 + n) << 8) | sb[ 7]];
				sb[11] = itbl[(sb[15] << 8)               | sb[ 7]];
				sb[ 1] = itbl[(*(dst - 0*w - 1 + n) << 8) | sb[ 3]];
				sb[ 0] = itbl[(*(dst - 0*w - 1 + n) << 8) | sb[ 1]];
				sb[ 2] = itbl[(sb[ 3] << 8)               | sb[ 1]];
				sb[ 5] = itbl[(*(dst + 2*w -1 + n) << 8)  | sb[ 7]];
				sb[ 4] = itbl[(*(dst + 2*w -1 + n) << 8)  | sb[ 5]];
				sb[ 6] = itbl[(sb[ 7] << 8)               | sb[ 5]];
				sb[ 9] = itbl[(*(dst + 3*w -1 + n) << 8)  | sb[11]];
				sb[ 8] = itbl[(*(dst + 3*w -1 + n) << 8)  | sb[ 9]];
				sb[10] = itbl[(sb[11] << 8)               | sb[ 9]];
				sb[13] = itbl[(*(dst + 4*w -1 + n) << 8)  | sb[15]];
				sb[12] = itbl[(*(dst + 4*w -1 + n) << 8)  | sb[13]];
				sb[14] = itbl[(sb[15] << 8)               | sb[13]];
				c48_4to8(dst + n, sb, w);
				size -= 1;
				break;
			case 0xFE:	/* 1x 8x8 block copy, per-block mv from datastream */
				{
				if (size < 2)
					return 1;
				const int16_t mvofs = le16_to_cpu(ua16(src)); src += 2;
				for (i = 0; i < 8; i++) {
					ofs = w * i + n;
					for (k = 0; k < 8; k++)
						*(dst + ofs + k) = *(db + ofs + k + mvofs);
				}
				size -= 2;
				break;
				}
			case 0xFD:
				/* 4x4 to 8x8 upscale.  4 Reference colors, while the
				 * other 12 have to be interpolated from existing
				 * pixels in adjacent blocks of the currenf buffer.
				 */
				if (size < 4)
					return 1;
				sb[ 5] = *src++;
				sb[ 7] = *src++;
				sb[13] = *src++;
				sb[15] = *src++;
				sb[ 1] = itbl[(*(dst - 1*w + 3 + n) << 8) | sb[ 5]];
				sb[ 3] = itbl[(*(dst - 1*w + 7 + n) << 8) | sb[ 7]];
				sb[11] = itbl[(sb[15] << 8)               | sb[ 7]];
				sb[ 9] = itbl[(sb[13] << 8)               | sb[ 5]];
				sb[ 0] = itbl[(*(dst - 0*w - 1 + n) << 8) | sb[ 1]];
				sb[ 2] = itbl[(sb[ 3] << 8)               | sb[ 1]];
				sb[ 4] = itbl[(*(dst + 2*w - 1 + n) << 8) | sb[ 5]];
				sb[ 6] = itbl[(sb[ 7] << 8)               | sb[ 5]];
				sb[ 8] = itbl[(*(dst + 3*w - 1 + n) << 8) | sb[ 9]];
				sb[10] = itbl[(sb[11] << 8)               | sb[ 9]];
				sb[12] = itbl[(*(dst + 4*w - 1 + n) << 8) | sb[13]];
				sb[14] = itbl[(sb[15] << 8)               | sb[13]];
				c48_4to8(dst + n, sb, w);
				size -= 4;
				break;
			case 0xFC:	/* 4x 4x4 blocks copy, per-block mv index from datastream */
				if (size < 4)
					return 1;
				for (i = 0; i < 8; i += 4) {
					for (k = 0; k < 8; k += 4) {
						opc = *src++;
						const int16_t mvofs = c37_mv[0][opc * 2] + (c37_mv[0][opc * 2 + 1] * w);
						for (j = 0; j < 4; j++) {
							ofs = (w * (j + i)) + k + n;
							for (l = 0; l < 4; l++)
								*(dst + ofs + l) = *(db + ofs + l + mvofs);
						}
					}
				}
				size -= 4;
				break;
			case 0xFB: 	/* 4x 4x4 blocks copy, per-block mv from datastream */
				if (size < 8)
					return 1;
				for (i = 0; i < 8; i += 4) {			/* 2 */
					for (k = 0; k < 8; k += 4) {		/* 2 */
						const int16_t mvofs = le16_to_cpu(ua16(src)); src += 2;
						for (j = 0; j < 4; j++) {	/* 4 */
							ofs = (w * (j + i)) + k + n;
							for (l = 0; l < 4; l++)
								*(dst + ofs + l) = *(db + ofs + l + mvofs);
						}
					}
				}
				size -= 8;
				break;
			case 0xFA:	/* 1x 4x4 block from datastream, upscaled to 8x8 */
				if (size < 16)
					return 1;
				c48_4to8(dst + n, src, w);
				src += 16;
				size -= 16;
				break;
			case 0xF9:	/* 16x 2x2 blocks copy, per-block mv index from datastream */
				if (size < 16)
					return 0;
				for (i = 0; i < 8; i += 2) {				/* 4 */
					for (j = 0; j < 8; j += 2) {			/* 4 */
						ofs = (w * i) + j + n;
						opc = *src++;
						const int16_t mvofs = c37_mv[0][opc * 2] + (c37_mv[0][opc * 2 + 1] * w);
						for (l = 0; l < 2; l++) {
							*(dst + ofs + l + 0) = *(db + ofs + l + 0 + mvofs);
							*(dst + ofs + l + w) = *(db + ofs + l + w + mvofs);
						}
					}
				}
				size -= 16;
				break;
			case 0xF8:	/* 16x 2x2 blocks copy, per-block mv from datastream */
				if (size < 32)
					return 1;
				for (i = 0; i < 8; i += 2) {				/* 4 */
					for (j = 0; j < 8; j += 2) {			/* 4 */
						ofs = w * i + j + n;
						const int16_t mvofs = le16_to_cpu(ua16(src)); src += 2;
						for (l = 0; l < 2; l++) {
							*(dst + ofs + l + 0) = *(db + ofs + l + 0 + mvofs);
							*(dst + ofs + l + w) = *(db + ofs + l + w + mvofs);
						}
					}
				}
				size -= 32;
				break;
			case 0xF7:	/* 1x 8x8 block move from datastream */
				if (size < 64)
					return 1;
				for (i = 0; i < 8; i++) {
					ofs = i * w + n;
					for (l = 0; l < 8; l++)
						*(dst + ofs + l) = *src++;
				}
				size -= 64;
				break;
			default:	/* 1x 8x8 block copy, mv index from datastream */
				{
				const int16_t mvofs = c37_mv[0][opc * 2] + (c37_mv[0][opc * 2 + 1] * w);
				for (i = 0; i < 8; i++) {
					ofs = i * w + n;
					for (l = 0; l < 8; l++)
						*(dst + ofs + l) = *(db + ofs + l + mvofs);
				}
				break;
				}
			}
		}
		dst += w * 8;
		db += w * 8;
	}
	return 0;
}

static int codec48(struct sanctx *ctx, uint8_t *dbuf, uint8_t *src, uint16_t w,
		   uint16_t h, int16_t xoff, int16_t yoff, uint32_t size,
		   const uint16_t anm_flags)
{
	uint32_t pktsize, decsize;
	uint8_t comp, flag;
	uint16_t seq;

	if (size < 16)
		return -80;

	comp =	src[0];		/* subcodec */
	if (src[1] != 1)	/* mvec table variant, always 1 with MotS */
		return -22;

	seq = le16_to_cpu(*(uint16_t*)(src + 2));

	/* decsize is the size of the raw frame aligned to 8x8 blocks
	 * when seq == 0, otherwise it's identical to pktsize, which
	 * indicates the number of bytes in the datastream for this packet.
	 */
	decsize = le32_to_cpu(ua32(src + 4));
	pktsize = le32_to_cpu(ua32(src + 8));
	flag =	src[12];
	/* known flag values:
	 * 0x01: skip decoding of comp3 if sequence number is odd and 0x10 is not set.
	 *       used in MM only.
	 * 0x02: if set: copy result line by line, skip a custom color (0).
	 *       this is only used in "Making Magic"; in MotS setting this bit does not
	 *       blit the result to the front buffer (i.e. previous frame is kept), but
	 *       not used in any MotS videos.
	 * 0x04: unknown, always set, never checked for in MM or MotS.
	 * 0x08: interpolation table data (0x8080 bytes) follows after the header.
	 * 0x10: interpolate a frame using the 2 buffers and interpolation table, i.e.
	 *        do not blit the current main buffer to destination, but rather create
	 *        an intermediate frame using the interpolation table. Used in MM.
	 *       The next frame then is compression 6, i.e. blits the actual decoding
	 *        result to main buffer.
	 * 0x20: unknown, checked in error path in MM
	 * 0x30: unknown, same as 0x20 but checked for MotS error path.
	 */

	if (decsize > ctx->rt.fbsize)
		decsize = ctx->rt.fbsize;
	if (pktsize > ctx->rt.fbsize)
		pktsize = ctx->rt.fbsize;

	if (seq == 0) {
		/* keep buf0 for comp == 6 */
		memset(ctx->rt.buf2, 0, decsize);
	}

	src += 16;
	size -= 16;
	if (flag & 8) {
		if (size < 0x8080)
			return -81;
		codec47_itable(ctx, src);
		src += 0x8080;
		size -= 0x8080;
	}

	switch (comp) {
	case 0:	if (size < pktsize)
			return -82;
		memcpy(ctx->rt.buf0, src, pktsize);
		break;
	case 2: codec47_comp5(src, size, ctx->rt.buf0, decsize); break;
	case 3: if ((seq == 0) || (seq == ctx->rt.lastseq + 1)) {
			if ((seq & 1) || ((flag & 1) == 0) || (flag & 0x10)) {
				c47_swap_bufs(ctx, 1);	/* swap 0 and 2 */
			}
			if (((seq & 1) != 0) && ((flag & 1) != 0)
			    && ((anm_flags & ANM_FLAG_DECODE_PRETEND) != 0)
			    && ((((flag & 0x10) == 0) && ((flag & 0x20) == 0))	/* MM */
			         || ((flag & 0x30) == 0))) {	/* MotS */
				ctx->rt.lastseq = seq;
				return 0;	/* original does return error here */
			}
			if (0 != codec48_comp3(src, ctx->rt.buf0, ctx->rt.buf2, ctx->rt.c47ipoltbl, w, h, size))
				return -84;
		}
		break;
	case 5: if (size < ((w * h) / 4))
			return -83;
		codec47_comp1(src, ctx->rt.buf0, ctx->rt.c47ipoltbl, w, h);
		break;
	default:
		break;
	}

	if (seq > 0)
		ctx->rt.can_ipol = 1;
	ctx->rt.lastseq = seq;

	if ((flag & 2) == 0) {
		if (flag & 0x10) {
			blt_ipol(dbuf, ctx->rt.buf0, ctx->rt.buf2, xoff, yoff,
				 0, 0, w, h, w, ctx->rt.pitch, ctx->rt.bufh,
				 ctx->rt.c47ipoltbl);
			ctx->rt.can_ipol = 0;
			return 0;
		}
		blt_solid(dbuf, ctx->rt.buf0, xoff, yoff, 0, 0, w, h, w, ctx->rt.pitch,
			  ctx->rt.bufh);
	} else {
		blt_mask(dbuf, ctx->rt.buf0, xoff, yoff, 0, 0, w, h, w, ctx->rt.pitch,
			 ctx->rt.bufh, 0);
	}

	return 0;
}

/******************************************************************************/

static void codec37_comp1(uint8_t * __restrict src, uint32_t size, uint8_t * __restrict dst, uint8_t * __restrict db,
			  uint16_t w, uint16_t h, uint8_t mvidx)
{
	uint8_t opc, run, skip;
	int32_t mvofs, ofs;
	int i, j, k, l, len;

	run = 0;
	len = -1;
	opc = 0;
	for (i = 0; i < h; i += 4) {
		for (j = 0; j < w; j += 4) {
			if (len < 0) {
				if (size < 1)
					return;
				len = (*src) >> 1;
				run = !!((*src++) & 1);
				skip = 0;
				size--;
			} else {
				skip = run;
			}

			if (!skip) {
				if (size < 1)
					return;
				opc = *src++;
				size--;
				if (opc == 0xff) {
					len--;
					for (k = 0; k < 4; k++) {
						ofs = j + (k * w);
						for (l = 0; l < 4; l++) {
							if (len < 0) {
								if (size < 1)
									return;
								len = (*src) >> 1;
								run = !!((*src++) & 1);
								size--;
								if (run && size) {
									opc = *src++;
									size--;
								}
							}
							if (!run){
								if (size < 1) {
									return;
								} else {
									*(dst + ofs + l) = *src++;
									size--;
								}
							} else {
								*(dst + ofs + l) = opc;
							}
							len--;
						}
					}
					continue;
				}
			}
			/* 4x4 block copy from prev with MV */
			mvofs = c37_mv[mvidx][opc*2] + (c37_mv[mvidx][opc*2 + 1] * w);
			for (k = 0; k < 4; k++) {
				ofs = j + (k * w);
				for (l = 0; l < 4; l++)
					*(dst + ofs + l) = *(db + ofs + l + mvofs);
			}
			len -= 1;
		}
		dst += w * 4;
		db += w * 4;
	}
}

static void codec37_comp3(uint8_t * __restrict src, uint32_t size,
			  uint8_t * __restrict dst, uint8_t * __restrict db,
			  uint16_t w, uint16_t h, uint8_t mvidx,
			  const uint8_t f4, const uint8_t c4)
{
	uint8_t opc, c, copycnt;
	int32_t ofs, mvofs;
	int i, j, k, l;

	copycnt = 0;
	for (i = 0; i < h; i += 4) {
		for (j = 0; j < w; j += 4) {

			/* copy a 4x4 block from the previous frame from same spot */
			if (copycnt > 0) {
				for (k = 0; k < 4; k++) {
					ofs = j + (k * w);
					for (l = 0; l < 4; l++) {
						*(dst + ofs + l) = *(db + ofs + l);
					}
				}
				copycnt--;
				continue;
			}

			if (size < 1)
				return;
			opc = *src++;
			size--;
			if (opc == 0xff) {
				/* 1 4x4 block, per-pixel data from source */
				if (size < 16)
					return;
				for (k = 0; k < 4; k++) {
					ofs = j + (k * w);
					for (l = 0; l < 4; l++)
						*(dst + ofs + l) = *src++;
				}
				size -= 16;
			} else if (f4 && (opc == 0xfe)) {
				/* 4 2x2 blocks, per-block color from source */
				if (size < 4)
					return;
				for (k = 0; k < 4; k += 2) {
					for (l = 0; l < 2; l++) {
						*(dst + j + ((k + l) * w) + 0) = src[0];
						*(dst + j + ((k + l) * w) + 1) = src[0];
						*(dst + j + ((k + l) * w) + 2) = src[1];
						*(dst + j + ((k + l) * w) + 3) = src[1];
					}
					src += 2;
				}
				size -= 4;
			} else if (f4 && (opc == 0xfd)) {
				/* 1 4x4 block, block color from source */
				if (size < 1)
					return;
				c = *src++;
				for (k = 0; k < 4; k++) {
					ofs = j + (k * w);
					for (l = 0; l < 4; l++)
						*(dst + ofs + l) = c;
				}
				size--;
			} else {
				/* 4x4 block copy from prev with MV */
				mvofs = c37_mv[mvidx][opc*2] + (c37_mv[mvidx][opc*2 + 1] * w);
				for (k = 0; k < 4; k++) {
					ofs = j + (k * w);
					for (l = 0; l < 4; l++)
						*(dst + ofs + l) = *(db + ofs + l + mvofs);
				}
				/* comp 4 opcode 0 indicates run start */
				if (c4 && (opc == 0)) {
					if (size < 1)
						return;
					copycnt = *src++;
					size--;
				}
			}
		}
		dst += w * 4;
		db += w * 4;
	}
}

static int codec37(struct sanctx *ctx, uint8_t *dbuf, uint8_t *src, uint16_t w,
		   uint16_t h, int16_t xoff, int16_t yoff, uint32_t size,
		   const uint16_t anm_flags)
{
	uint8_t comp, mvidx, flag;
	uint32_t decsize;
	uint16_t seq;

	if (size < 16)
		return -70;

	comp = src[0];
	mvidx = src[1];
	if (mvidx > 2)
		return -21;
	seq = le16_to_cpu(*(uint16_t *)(src + 2));
	decsize = le32_to_cpu(ua32(src + 4));
	flag = src[12];

	if (decsize > ctx->rt.fbsize)
		decsize = ctx->rt.fbsize;

	if (comp == 0 || comp == 2)
		memset(ctx->rt.buf2, 0, decsize);

	src += 16;
	size -= 16;

	switch (comp) {
	case 0: memcpy(ctx->rt.buf0, src, _min(size, decsize)); break;
	case 2: codec47_comp5(src, size, ctx->rt.buf0, decsize); break;
	case 1: /* fallthrough */
	case 3: /* fallthrough */
	case 4: if ((seq == 0) || (seq == ctx->rt.lastseq + 1)) {
			if (((seq & 1) == 0) || ((flag & 1) == 0) || ((anm_flags & ANM_FLAG_DECODE_PRETEND) == 0)) {
				if (((seq & 1) != 0) || ((flag & 1) == 0)) {
					c47_swap_bufs(ctx, 1);	/* swap 0 and 2 */
				}
			} else {
				c47_swap_bufs(ctx, 1);	/* swap 0 and 2 */
				return 0;	/* original does return error here */
			}
			if (comp == 1)
				codec37_comp1(src, size, ctx->rt.buf0, ctx->rt.buf2, w, h, mvidx);
			else
				codec37_comp3(src, size, ctx->rt.buf0, ctx->rt.buf2, w, h, mvidx, flag & 4, comp == 4);
		}
		break;
	default: break;
	}

	ctx->rt.lastseq = seq;

	if (ctx->rt.mortimer) {
		blt_upscale_2x2(dbuf, ctx->rt.buf0, xoff, yoff, 0, 0, w, h, w, ctx->rt.pitch,
				ctx->rt.bufh);
	} else if ((flag & 2) == 0) {
		blt_solid(dbuf, ctx->rt.buf0, xoff, yoff, 0, 0, w, h, w, ctx->rt.pitch,
			  ctx->rt.bufh);
	} else {
		blt_mask(dbuf, ctx->rt.buf0, xoff, yoff, 0, 0, w, h, w, ctx->rt.pitch,
			 ctx->rt.bufh, 0);
	}

	return 0;
}

/******************************************************************************/

static void codec45(struct sanctx *ctx, uint8_t *dst_in, uint8_t *src, uint16_t w, uint16_t h,
		    int16_t xoff, int16_t yoff, uint16_t size, uint8_t param,
		    uint16_t param2)
{
	uint8_t *pal = ctx->c45tbl1, *rgb2pal = ctx->c45tbl2, *dst;
	const uint16_t pitch = ctx->rt.pitch;
	unsigned int seq, c1, c2, r, g, b;
	int i, runlen;

	/* The main purpose of this codec is to slightly blur the outline of the "opening
	 *  aperture" effect in the LEVxx/xxRETRY.SAN files (i.e. antialiasing).
	 *
	 * This seems to be an early version of the interpolation table system of codec47/48:
	 * It comes with its own palette, and the indidividual r/g/b components of the four
	 *  surrounding pixels need to be summed up and combined into rgb555 to create an
	 *  index into the 15-bit "average color" rgb table to get the final palette value
	 *  of the center pixel.
	 *
	 * RA2MEM 001f8b00-001f8bb8 (dispatcher), 002183ea-00218603 (main worker).
	 */
	if ((size < 6) || (src[4] != 1))
		return;

	seq = *(uint16_t *)(src + 2);
	src += 6;
	size -= 6;
	if (seq == 0) {
		if (size < 0x300)
			return;
		memcpy(pal, src, 0x300);
		src += 0x300;
		size -= 0x300;
		i = 0;
		while ((size > 1) && (i < 0x8000)) {
			runlen = *src++;
			if ((runlen + i) > 0x8000)
				runlen = 0x8000 - i;
			memset(rgb2pal + i, *src++, runlen);
			i += runlen;
			size -= 2;
		}
	}

	if (!dst_in)
		return;

	while (size > 3) {
		xoff += le16_to_cpu(*(int16_t *)(src + 0));
		src += 2;
		yoff += *src++;
		runlen = *src++;
		size -= 4;
		do {
			if (xoff > 0 && yoff > 0 && xoff < (ctx->rt.bufw - 1)) {
				if (yoff >= (ctx->rt.bufh - 1))
					return;

				dst = dst_in + xoff + (yoff * pitch);
				c1 = *(dst - 1) * 3;
				c2 = *(dst + 1) * 3;
				r =  *(pal + c1 + 0) + *(pal + c2 + 0);
				g =  *(pal + c1 + 1) + *(pal + c2 + 1);
				b =  *(pal + c1 + 2) + *(pal + c2 + 2);
				c1 = *(dst - pitch) * 3;
				c2 = *(dst + pitch) * 3;
				r += *(pal + c1 + 0) + *(pal + c2 + 0);
				g += *(pal + c1 + 1) + *(pal + c2 + 1);
				b += *(pal + c1 + 2) + *(pal + c2 + 2);
				*dst = *(rgb2pal + ((((r << 5) & 0x7c00) + (g & 0x3e0) + (b >> 5)) & 0x7fff));
			}
			xoff++;
		} while (runlen-- > 0);
		xoff--;
	}
}

/******************************************************************************/

static void codec23(struct sanctx *ctx, uint8_t *dst, uint8_t *src, uint16_t w,
		    uint16_t h, int16_t xoff, int16_t yoff, uint16_t size,
		    uint8_t param, int16_t param2)
{
	const uint16_t mx = ctx->rt.bufw, my = ctx->rt.bufh, p = ctx->rt.pitch;
	int skip, i, j, ls, pc, y, wrlen, skip_left;
	uint8_t lut[256], *d;

	if (ctx->rt.version < 2) {
		/* RA1 */
		for (i = 0; i < 256; i++)
			lut[i] = (i + param + 0xd0) & 0xff;
	} else {
		/* RA2 FUN_00032350: first c23 has this LUT (param2 == 256),
		 * later frames reuse it (param2 == 257). param2 < 256 indicates
		 * this is a delta value to apply to the color instead.
		 */
		if (param2 == 256) {
			if (size < 256)
				return;
			memcpy(ctx->c23lut, src, 256);
			src += 256;
			size -= 256;
		} else if (param2 < 256) {
			/* create a lut with constant delta */
			for (i = 0; i < 256; i++)
				lut[i] = (i + param2) & 0xff;
		} else {
			for (i = 0; i < 256; i++)
				lut[i] = ctx->c23lut[i];
		}
	}

	if ((size < 1) || ((yoff + h) < 0) || (yoff >= my) || (xoff + w < 0) || (xoff >= mx))
		return;

	if (yoff < 0) {
		y = -yoff;
		while (y-- && size > 1) {
			ls = le16_to_cpu(ua16(src));
			size -= 2;
			if (size < ls)
				return;
			size -= ls;
			src += 2 + ls;
		}
		h += yoff;
		yoff = 0;
	}

	y = yoff;
	for (; (size > 1) && (h > 0) && (y < my); h--, y++) {
		ls = le16_to_cpu(ua16(src));
		src += 2;
		size -= 2;
		skip = 1;
		pc = xoff;
		while ((size > 0) && (ls > 0) && (pc <= (w + xoff))) {
			j = *src++;
			size--;
			ls--;
			if (!skip) {
				skip_left = (pc < 0) ? -pc : 0;
				if (skip_left >= j) {
					pc += j;
					j = 0;
				} else {
					pc += skip_left;
					j -= skip_left;
				}

				wrlen = (pc + j > mx) ? (mx - pc) : j;

				if (wrlen > 0) {
					d = dst + (y * p) + pc;
					for (i = 0; i < wrlen; i++) {
						d[i] = lut[d[i]];
					}

					pc += wrlen;
					j -= wrlen;
				}

				if (j > 0)
					pc += j;

			} else {
				pc += j;
			}
			skip ^= 1;
		}
	}
}

/* codec23 upscaling as used in Mortimer only. */
static void codec23_2x2(struct sanctx *ctx, uint8_t *dst, uint8_t *src, uint16_t w,
			uint16_t h, int16_t xoff, int16_t yoff, uint16_t size,
			uint8_t param, int16_t param2)
{
	const uint16_t mx = ctx->rt.bufw, my = ctx->rt.bufh, p = ctx->rt.pitch;
	int skip, i, j, ls, pc, y, wrlen, skip_left;
	uint8_t lut[256], *d;

	/* Mortimer 00410770: first c23 has this LUT (param2 == 256),
	 * later frames reuse it (param2 == 257). param2 < 256 indicates
	 * this is a delta value to apply to the color instead.
	 */
	if (param2 == 256) {
		if (size < 256)
			return;
		memcpy(ctx->c23lut, src, 256);
		src += 256;
		size -= 256;
	} else if (param2 < 256) {
		/* create a lut with constant delta */
		for (i = 0; i < 256; i++)
			lut[i] = (i + param2) & 0xff;
	} else {
		for (i = 0; i < 256; i++)
			lut[i] = ctx->c23lut[i];
	}

	if ((size < 1) || ((yoff + h) * 2 <= 0) || (yoff * 2 >= my) || ((xoff + w) * 2 <= 0) || (xoff * 2 >= mx))
		return;

	/* Mortimer 00410400+ */
	if (yoff < 0) {
		y = -yoff;
		while (y-- && size > 1) {
			ls = le16_to_cpu(ua16(src));
			size -= 2;
			if (size < ls)
				return;
			size -= ls;
			src += 2 + ls;
		}
		h += yoff;
		yoff = 0;
	}

	y = yoff;
	for (; (size > 1) && (h > 0) && (y * 2 < my); h--, y++) {
		ls = le16_to_cpu(ua16(src));
		src += 2;
		size -= 2;
		if (size < ls)
			return;

		uint8_t *next_src = src + ls;
		size -= ls;
		skip = 1;
		pc = xoff;
		while ((src < next_src) && (pc < (w + xoff))) {
			j = *src++;
			if (!skip) {
				skip_left = (pc < 0) ? -pc : 0;
				if (skip_left >= j) {
					pc += j;
					j = 0;
				} else {
					pc += skip_left;
					j -= skip_left;
				}
				wrlen = j;
				if (pc + wrlen > (mx / 2)) {
					wrlen = (mx / 2) - pc;
				}

				if (wrlen > 0) {
					uint32_t dy = y * 2;
					uint32_t dx = pc * 2;
					d = dst + (dy * p) + dx;

					for (i = 0; i < wrlen; i++) {
						uint8_t color = lut[d[0]];

						d[0] = color;
						d[1] = color;
						if (dy + 1 < my) {
							d[p] = color;
							d[p + 1] = color;
						}
						d += 2;
					}
					pc += wrlen;
					j -= wrlen;
				}
				if (j > 0)
					pc += j;
			} else {
				pc += j;
			}
			skip ^= 1;
		}
		src = next_src;
	}
}

static void codec44(struct sanctx *ctx, uint8_t *dst, uint8_t *src, uint16_t w,
		    uint16_t h, int16_t xoff, int16_t yoff, uint16_t size,
		    uint8_t fgcol)
{
	const uint16_t mx = ctx->rt.bufw, my = ctx->rt.bufh, p = ctx->rt.pitch;
	int j, y, pc, skip, ls;
	uint8_t c, *nsrc;

	if ((size < 1) || ((yoff + h) < 0) || (yoff >= my) || (xoff + w < 0) || (xoff >= mx))
		return;

	nsrc = src;
	y = yoff;
	for (; (size > 2) && (h > 0) && (y < my); y++, h--) {
		src = nsrc;
		ls = le16_to_cpu(ua16(src));
		src += 2;
		size -= 2;
		nsrc = src + ls;
		if (y < 0) {
			if (ls > size)
				break;
			size -= ls;
			continue;
		}
		skip = 1;
		pc = xoff;
		while ((size > 1) && (ls > 1) && (pc <= (w + xoff))) {
			j = le16_to_cpu(ua16(src));
			src += 2;
			size -= 2;
			ls -= 2;
			if (!skip) {
				while ((size-- > 0) && (ls-- > 0) && (j-- >= 0)) {
					c = *src++;
					if ((pc >= 0) && (pc < mx)) {
						c = (c == 0xff) ? 0 : (c + fgcol) - 1;
						*(dst + (y * p) + pc) = c;
					}
					pc++;
				}
			} else {
				pc += j;
			}
			skip ^= 1;
		}
	}
}

static void codec21(struct sanctx *ctx, uint8_t *dst, uint8_t *src, uint16_t w,
		    uint16_t h, int16_t xoff, int16_t yoff, uint16_t size,
		    uint8_t param)
{
	const uint16_t mx = ctx->rt.bufw, my = ctx->rt.bufh, p = ctx->rt.pitch;
	int j, y, pc, skip, ls;
	uint8_t c, *nsrc;

	if ((size < 1) || ((yoff + h) < 0) || (yoff >= my) || (xoff + w < 0) || (xoff >= mx))
		return;

	nsrc = src;
	y = yoff;
	for (; (size > 2) && (h > 0) && (y < my); y++, h--) {
		src = nsrc;
		ls = le16_to_cpu(ua16(src));
		src += 2;
		size -= 2;
		nsrc = src + ls;
		if (y < 0) {
			if (ls > size)
				break;
			size -= ls;
			continue;
		}
		skip = 1;
		pc = xoff;
		while ((size > 1) && (ls > 1) && (pc <= (w + xoff))) {
			j = le16_to_cpu(ua16(src));
			src += 2;
			size -= 2;
			ls -= 2;
			if (!skip) {
				while ((size-- > 0) && (ls-- > 0) && (j-- >= 0)) {
					c = *src++;
					if ((pc >= 0) && (pc < mx)) {
						*(dst + (y * p) + pc) = c;
					}
					pc++;
				}
			} else {
				pc += j;
			}
			skip ^= 1;
		}
	}
}

static void codec20(struct sanctx *ctx, uint8_t * __restrict dst,
		    uint8_t * __restrict src, const uint16_t w, const uint16_t h,
		    const int16_t xoff, const int16_t yoff, uint32_t size,
		    const uint16_t srcstride)
{
	int16_t hh = h;

	if (((xoff + w) < 0) || (xoff >= ctx->rt.bufw) || ((yoff + h) < 0)
	    || (yoff >= ctx->rt.bufh) || (w < 1) || (h < 1))
		return;

	if ((w * h) > size)
		hh = size / w;

	blt_solid(dst, src, xoff, yoff, 0, 0, w, hh, srcstride, ctx->rt.pitch,
		  ctx->rt.bufh);
}

static void codec4_main(struct sanctx *ctx, uint8_t *dst, uint8_t *src,
			const uint16_t w, const uint16_t h, const int16_t xoff,
			const int16_t yoff, uint32_t size, const uint8_t param,
			const uint16_t param2, const int codec)
{
	const uint16_t p = ctx->rt.pitch, mx = ctx->rt.bufw, my = ctx->rt.bufh;
	uint8_t mask, bits, *gs, idx, c4t;
	const int sega = (codec >= 33);
	const int c5 = ((codec == 5) || (codec == 34));
	uint32_t dstoff;
	int i, j, k, l, bit, x, y;

	c4t = ctx->c4tblparam & 0xff;
	if (param2 > 0) {
		if (size < param2 * 8)
			return;
		c4_5_param2(ctx, src, param2, c4t);
		src += param2 * 8;
		size -= param2 * 8;
	}

	for (j = 0; j < w; j += 4) {
		mask = bits = 0;
		x = xoff + j;
		for (i = 0; i < h; i += 4) {
			y = yoff + i;
			if (param2 > 0) {
				if (bits == 0) {
					if (!size--)
						return;
					mask = *src++;
					bits = 8;
				}
				bit = !!(mask & 0x80);
				mask <<= 1;
				bits -= 1;
			} else {
				bit = 0;
			}

			if (!size--)
				return;
			idx = *src++;
			if (!bit && idx == 0x80 && !c5)
				continue;

			if ((y >= my) || ((y + 4) < 0) || ((x + 4) < 0) || (x >= mx))
				continue;
			/* render the 4x4 block */
			gs = &(ctx->c4tbl[bit][idx][0]);
			if ((y >= 0) && ((y + 4) < my) && (x >= 0) && ((x + 4) < mx)) {
				for (k = 0; k < 4; k++, gs += 4)
					memcpy(dst + x + (y + k) * p, gs, 4);
			} else {
				for (k = 0; k < 4; k++) {
					for (l = 0; l < 4; l++, gs++) {
						const int yo = y + k, xo = x + l;
						if ((yo >= 0) && (yo < my) && (xo >= 0) && (xo < mx))
							*(dst + yo * p + xo) = *gs;
					}
				}
			}

			/* post processing to smooth out block borders a bit.
			 * ASSAULT.EXE 121e8 - 12242 for the (c4t&0x80)==0 case.
			 * SEGA codec33/34 do not do post-processing at all.
			 */
			if (x <= 0 || y <= 0 || x >= mx || y >= my || sega)
				continue;	/* skip unreachable edges */
			dstoff = y * p +  x;
			if (c4t & 0x80) {
				for (k = 0; k < 4; k++)
					*(dst + dstoff + k) = ((*(dst + dstoff + k) + *(dst + dstoff + k - p)) >> 1) | 0x80;
				*(dst + dstoff + 1 * p) = ((*(dst + dstoff + 1 * p) + *(dst + dstoff + 1 * p - 1)) >> 1) | 0x80;
				*(dst + dstoff + 2 * p) = ((*(dst + dstoff + 2 * p) + *(dst + dstoff + 2 * p - 1)) >> 1) | 0x80;
				*(dst + dstoff + 3 * p) = ((*(dst + dstoff + 3 * p) + *(dst + dstoff + 3 * p - 1)) >> 1) | 0x80;
			} else {
				for (k = 0; k < 4; k++)
					*(dst + dstoff + k) = ((*(dst + dstoff + k) + *(dst + dstoff + k - p)) >> 1) & 0x7f;
				*(dst + dstoff + 1 * p) = ((*(dst + dstoff + 1 * p) + *(dst + dstoff + 1 * p - 1)) >> 1) & 0x7f;
				*(dst + dstoff + 2 * p) = ((*(dst + dstoff + 2 * p) + *(dst + dstoff + 2 * p - 1)) >> 1) & 0x7f;
				*(dst + dstoff + 3 * p) = ((*(dst + dstoff + 3 * p) + *(dst + dstoff + 3 * p - 1)) >> 1) & 0x7f;
			}
		}
	}
}

static void codec33(struct sanctx *ctx, uint8_t *dst, uint8_t *src, uint16_t w,
		    uint16_t h, int16_t xoff, int16_t yoff, uint32_t size,
		    uint8_t param, uint16_t param2, int codec)
{
	if (ctx->c4tblparam != (param + 0x100))
		c33_34_tilegen(&(ctx->c4tbl[0][0][0]), param);
	ctx->c4tblparam = param + 0x100;
	codec4_main(ctx, dst, src, w, h, xoff, yoff, size, param, param2, codec);
}

static void codec4(struct sanctx *ctx, uint8_t *dst, uint8_t *src, uint16_t w,
		   uint16_t h, int16_t xoff, int16_t yoff, uint32_t size,
		   uint8_t param, uint16_t param2, int codec)
{
	if (ctx->c4tblparam != param)
		c4_5_tilegen(&(ctx->c4tbl[0][0][0]), param);
	ctx->c4tblparam = param;
	codec4_main(ctx, dst, src, w, h, xoff, yoff, size, param, param2, codec);
}

static void codec1(struct sanctx *ctx, uint8_t *dst_in, uint8_t *src, uint16_t w,
		   uint16_t h, int16_t xoff, int16_t yoff, uint32_t size, int transp)
{
	const uint16_t mx = ctx->rt.bufw, my = ctx->rt.bufh;
	uint8_t *dst, code, col;
	uint16_t rlen, dlen;
	int j, x, y;

	if (((yoff + h) < 0) || (yoff >= my) || (xoff + w < 0) || (xoff >= mx))
		return;
	if (yoff < 0) {
		y = -yoff;
		while (y-- && size > 1) {
			dlen = le16_to_cpu(ua16(src));
			size -= 2;
			if (size < dlen)
				return;
			size -= dlen;
			src += 2 + dlen;
		}
		h += yoff;
		yoff = 0;
	}

	y = yoff;
	for (; (size > 1) && (h > 0) && (y < my); h--, y++) {
		dlen = le16_to_cpu(ua16(src));
		src += 2;
		size -= 2;
		x = xoff;
		while (dlen && size) {
			code = *src++;
			dlen--;
			size--;
			rlen = (code >> 1) + 1;
			if (code & 1) {
				if (size < 1)
					return;
				col = *src++;
				dlen--;
				size--;
				if (x >= mx)
					continue;
				if (x < 0) {
					int dff = _min(-x, rlen);
					rlen -= dff;
					x += dff;
				}
				if (x + rlen > mx)
					rlen = mx - x;
				if (rlen < 1)
					continue;

				dst = (uint8_t *)dst_in + (y * ctx->rt.pitch) + x;
				if (col || !transp) {
					for (j = 0; j < rlen; j++)
						*(dst + j) = col;
				}
				x += rlen;
			} else {
				if (size < rlen)
					return;
				if (x >= mx) {
					dlen -= rlen;
					size -= rlen;
					src += rlen;
					continue;
				}
				if (x < 0) {
					int dff = _min(-x, rlen);
					src += dff;
					size -= dff;
					dlen -= dff;
					rlen -= dff;
					x += dff;
				}
				dst = (uint8_t *)dst_in + (y * ctx->rt.pitch) + x;
				for (j = 0; j < rlen; j++, x++) {
					col = *src++;
					if ((col || !transp) && (x >= 0) && (x < mx))
						*(dst + j) = col;
				}
				dlen -= rlen;
				size -= rlen;
			}
		}
	}
}

static void codec1_flipx(struct sanctx *ctx, uint8_t *dst, uint8_t *src, uint16_t w,
			 uint16_t h, int16_t xoff, int16_t yoff, uint32_t size, int transp)
{
	const uint16_t mx = ctx->rt.bufw, my = ctx->rt.bufh;
	uint8_t code, col;
	uint16_t rlen, dlen;
	int j, x_rel, y;

	for (y = 0; (size > 1) && (y < h); y++) {
		dlen = le16_to_cpu(ua16(src));
		src += 2;
		size -= 2;
		int draw_y = yoff + y;

		if (draw_y < 0 || draw_y >= my) {
			src += dlen;
			size -= dlen;
			continue;
		}

		x_rel = 0;
		while (dlen && size) {
			code = *src++;
			dlen--;
			size--;
			rlen = (code >> 1) + 1;
			if (code & 1) {
				col = *src++;
				dlen--;
				size--;
				if (col || !transp) {
					for (j = 0; j < rlen; j++) {
						int draw_x = (xoff + w - 1) - (x_rel + j);
						if (draw_x >= 0 && draw_x < mx)
							dst[draw_y * ctx->rt.pitch + draw_x] = col;
					}
				}
				x_rel += rlen;
			} else {
				for (j = 0; j < rlen; j++) {
					col = *src++;
					int draw_x = (xoff + w - 1) - (x_rel + j);
					if ((col || !transp) && draw_x >= 0 && draw_x < mx)
						dst[draw_y * ctx->rt.pitch + draw_x] = col;
				}
				dlen -= rlen;
				size -= rlen;
				x_rel += rlen;
			}
		}
	}
}

static void codec1_flipy(struct sanctx *ctx, uint8_t *dst, uint8_t *src, uint16_t w,
			 uint16_t h, int16_t xoff, int16_t yoff, uint32_t size, int transp)
{
	const uint16_t mx = ctx->rt.bufw, my = ctx->rt.bufh;
	uint8_t code, col;
	uint16_t rlen, dlen;
	int j, x, y;

	for (y = 0; (size > 1) && (y < h); y++) {
		dlen = le16_to_cpu(ua16(src));
		src += 2; size -= 2;
		int draw_y = (yoff + h - 1) - y;

		if (draw_y < 0 || draw_y >= my) {
			src += dlen;
			size -= dlen;
			continue;
		}

		x = xoff;
		while (dlen && size) {
			code = *src++;
			dlen--;
			size--;
			rlen = (code >> 1) + 1;
			if (code & 1) {
				col = *src++;
				dlen--;
				size--;
				int draw_rlen = rlen;
				int draw_x = x;
				if (draw_x < mx && draw_x + draw_rlen > 0) {
					if (draw_x < 0) {
						int off = -draw_x;
						draw_rlen -= off;
						draw_x = 0;
					}
					if (draw_x + draw_rlen > mx)
						draw_rlen = mx - draw_x;
					if (draw_rlen > 0 && (col || !transp))
						memset(dst + (draw_y * ctx->rt.pitch) + draw_x, col, draw_rlen);
				}
				x += rlen;
			} else {
				for (j = 0; j < rlen; j++, x++) {
					col = *src++;
					if ((col || !transp) && x >= 0 && x < mx)
						dst[draw_y * ctx->rt.pitch + x] = col;
				}
				dlen -= rlen;
				size -= rlen;
			}
		}
	}
}

static void codec1_flipxy(struct sanctx *ctx, uint8_t *dst, uint8_t *src, uint16_t w,
			  uint16_t h, int16_t xoff, int16_t yoff, uint32_t size, int transp)
{
	const uint16_t mx = ctx->rt.bufw, my = ctx->rt.bufh;
	uint8_t code, col;
	uint16_t rlen, dlen;
	int j, x_rel, y;

	for (y = 0; (size > 1) && (y < h); y++) {
		dlen = le16_to_cpu(ua16(src));
		src += 2;
		size -= 2;
		int draw_y = (yoff + h - 1) - y;

		if (draw_y < 0 || draw_y >= my) {
			src += dlen;
			size -= dlen;
			continue;
		}

		x_rel = 0;
		while (dlen && size) {
			code = *src++;
			dlen--;
			size--;
			rlen = (code >> 1) + 1;
			if (code & 1) {
				col = *src++;
				dlen--;
				size--;
				if (col || !transp) {
					for (j = 0; j < rlen; j++) {
						int draw_x = (xoff + w - 1) - (x_rel + j);
						if (draw_x >= 0 && draw_x < mx)
							dst[draw_y * ctx->rt.pitch + draw_x] = col;
					}
				}
				x_rel += rlen;
			} else {
				for (j = 0; j < rlen; j++) {
					col = *src++;
					int draw_x = (xoff + w - 1) - (x_rel + j);
					if ((col || !transp) && draw_x >= 0 && draw_x < mx)
						dst[draw_y * ctx->rt.pitch + draw_x] = col;
				}
				dlen -= rlen;
				size -= rlen;
				x_rel += rlen;
			}
		}
	}
}

/* Transparent codec1 where every source pixel is a 2x2 destination pixel block.
 * Used in Mortimer, as codec3 in the *H.SAN files.
 */
static void codec1_2x2(struct sanctx *ctx, uint8_t *dst, uint8_t *src,
			uint16_t fobw, uint16_t fobh, int16_t xoff, int16_t yoff)
{
	const uint16_t stride = ctx->rt.pitch;

	if (yoff < 0) {
		yoff = -yoff;
		if (fobh <= yoff)
			return;
		fobh -= yoff;
		while (yoff--)
			src += 2 + le16_to_cpu(ua16(src));
	}

	int16_t bo = yoff + fobh - ctx->rt.bufh;
	if (bo > 0) {
		if (fobh <= bo)
			return;
		fobh -= bo;
	}

	uint16_t skipx = 0;
	if (xoff < 0) {
		skipx = -xoff;
		if (fobw <= skipx)
			return;
		fobw -= skipx;
		xoff = 0;
	}

	int16_t ro = (xoff + fobw) - ctx->rt.bufw;
	if (ro > 0) {
		if (fobw <= ro)
			return;
		fobw -= ro;
	}

	dst += (yoff * stride * 2) + (xoff * 2);
	for (uint16_t y = 0; y < fobh; y++) {
		uint16_t dlen = le16_to_cpu(ua16(src));
		const uint8_t *src2 = src + 2;
		src += 2 + dlen;

		uint8_t *dst2 = dst;
		uint16_t skipx2 = skipx;
		uint16_t drawx = fobw;
		while (drawx > 0) {
			uint8_t code = *src2++;
			int rlen = (code >> 1) + 1;

			if (skipx2 > 0) {
				if (code & 1) {
					if (skipx2 >= rlen) {
						skipx2 -= rlen;
						src2++;
						continue;
					} else {
						rlen -= skipx2;
						skipx2 = 0;
					}
				} else {
					if (skipx2 >= rlen) {
						skipx2 -= rlen;
						src2 += rlen;
						continue;
					} else {
						src2 += skipx2;
						rlen -= skipx2;
						skipx2 = 0;
					}
				}
			}

			int draw_len = rlen;
			if (draw_len > drawx) {
				draw_len = drawx;
			}
			drawx -= rlen;

			if (code & 1) {
				uint8_t color = *src2++;
				if (color != 0) {
					for (int i = 0; i < draw_len; i++) {
						dst2[i * 2] = color;
						dst2[i * 2 + 1] = color;
						dst2[stride + i * 2] = color;
						dst2[stride + i * 2 + 1] = color;
					}
				}
				dst2 += draw_len * 2;
			} else {
				for (int i = 0; i < draw_len; i++) {
					uint8_t color = *src2++;
					if (color != 0) {
						dst2[0] = color;
						dst2[1] = color;
						dst2[stride] = color;
						dst2[stride + 1] = color;
					}
					dst2 += 2;
				}

				if (rlen > draw_len) {
					src2 += (rlen - draw_len);
				}
			}

			if (drawx <= 0)
				break;
		}

		dst += stride * 2;
	}
}

static void codec2(struct sanctx *ctx, uint8_t *dst, uint8_t *src, uint16_t w,
		   uint16_t h, int16_t xoff, int16_t yoff, uint32_t size,
		   uint8_t param, uint16_t param2)
{
	const uint16_t pitch = ctx->rt.pitch, maxx = ctx->rt.bufw, maxy = ctx->rt.bufh;

	/* RA2 31a10; but there are no codec2 fobjs in RA2 at all.. */
	if (param2 != 0 && ctx->rt.version == 2) {
		codec1(ctx, dst, src, w, h, xoff, yoff, size, 1);
		return;
	}

	/* ASSAULT.EXE 110f8 */
	while (size > 3) {
		xoff += (int16_t)le16_to_cpu(ua16(src));
		yoff += (int8_t)src[2];
		if (xoff >= 0 && yoff >= 0 && xoff < maxx && yoff < maxy) {
			*(dst + xoff + yoff * pitch) = src[3];
		}
		src += 4;
		size -= 4;
	}
}

static void codec31(struct sanctx *ctx, uint8_t *dst, uint8_t *src, uint16_t w,
		    uint16_t h, int16_t xoff, int16_t yoff, uint32_t size, uint8_t p1,
		    int opaque)
{
	const uint16_t mx = ctx->rt.bufw, my = ctx->rt.bufh, p = ctx->rt.pitch;
	uint8_t code, col;
	uint16_t rlen, dlen;
	int j, x, y;

	if (((yoff + h) < 0) || (yoff >= my) || (xoff + w < 0) || (xoff >= mx))
		return;
	if (yoff < 0) {
		y = -yoff;
		while (y-- && size > 1) {
			dlen = le16_to_cpu(ua16(src));
			size -= 2;
			if (size < dlen)
				return;
			size -= dlen;
			src += 2 + dlen;
		}
		h += yoff;
		yoff = 0;
	}

	y = yoff;
	for (; (size > 1) && (h > 0) && (y < my); h--, y++) {
		dlen = le16_to_cpu(ua16(src));
		src += 2;
		size -= 2;
		x = xoff;
		while (dlen && size) {
			code = *src++;
			dlen--;
			size--;
			rlen = (code >> 1) + 1;
			if (code & 1) {
				if (size < 1)
					return;
				col = *src++;
				dlen--;
				size--;

				for (j = 0; j < rlen; j++) {
					uint8_t c1 = col & 0xf;
					if ((c1 || opaque) && (x >= 0) && (x < mx))
						*(dst + y * p + x) = p1 + c1;
					x++;
					c1 = col >> 4;
					if ((c1 || opaque) && (x >= 0) && (x < mx))
						*(dst + y * p + x) = p1 + c1;
					x++;
				}
			} else {
				if (size < rlen)
					rlen = size;
				for (j = 0; (j < rlen) && (size > 0); j++) {
					col = *src++;
					uint8_t c1 = col & 0xf;
					if ((c1 || opaque) && (x >= 0) && (x < mx))
						*(dst + y * p + x) = p1 + c1;
					x++;
					c1 = col >> 4;
					if ((c1 || opaque) && (x >= 0) && (x < mx))
						*(dst + y * p + x) = p1 + c1;
					x++;
				}
				dlen -= rlen;
				size -= rlen;
			}
		}
	}
}

static void codec31_flipx(struct sanctx *ctx, uint8_t *dst, uint8_t *src, uint16_t w,
			  uint16_t h, int16_t xoff, int16_t yoff, uint32_t size,
			  uint8_t p1, int opaque)
{
	const uint16_t mx = ctx->rt.bufw, my = ctx->rt.bufh, p = ctx->rt.pitch;
	uint8_t code, col;
	uint16_t rlen, dlen;
	int j, x, y;

	if (((yoff + h) < 0) || (yoff >= my) || (xoff + w < 0) || (xoff >= mx))
		return;

	if (yoff < 0) {
		y = -yoff;
		while (y-- && size > 1) {
			dlen = le16_to_cpu(ua16(src));
			size -= 2;
			if (size < dlen)
				return;
			size -= dlen;
			src += 2 + dlen;
		}
		h += yoff;
		yoff = 0;
	}

	y = yoff;
	for (; (size > 1) && (h > 0) && (y < my); h--, y++) {
		dlen = le16_to_cpu(ua16(src));
		src += 2;
		size -= 2;
		x = xoff + w - 1;
		while (dlen && size) {
			code = *src++;
			dlen--;
			size--;
			rlen = (code >> 1) + 1;

			if (code & 1) {
				if (size < 1)
					return;
				col = *src++;
				dlen--;
				size--;

				for (j = 0; j < rlen; j++) {
					uint8_t c1 = col & 0xf;
					if ((c1 || opaque) && (x >= 0) && (x < mx))
						*(dst + y * p + x) = p1 + c1;
					x--;
					c1 = col >> 4;
					if ((c1 || opaque) && (x >= 0) && (x < mx))
						*(dst + y * p + x) = p1 + c1;
					x--;
				}
			} else {
				if (size < rlen)
					rlen = size;
				for (j = 0; (j < rlen) && (size > 0); j++) {
					col = *src++;

					uint8_t c1 = col & 0xf;
					if ((c1 || opaque) && (x >= 0) && (x < mx))
						*(dst + y * p + x) = p1 + c1;
					x--;
					c1 = col >> 4;
					if ((c1 || opaque) && (x >= 0) && (x < mx))
						*(dst + y * p + x) = p1 + c1;
					x--;
				}
				dlen -= rlen;
				size -= rlen;
			}
		}
	}
}

/******************************************************************************/

static int fob_decode_render(struct sanctx *ctx, uint8_t *dst, uint8_t *src,
			     uint32_t size, int16_t xoff, int16_t yoff,
			     uint16_t anm_flags, uint8_t fgc, uint8_t bgc)
{
	uint16_t fobw, fobh, param2;
	uint8_t codec, param;
	int16_t left, top;
	int ret;

	if (size < 14)
		return 18;

	codec  = src[0];
	param  = src[1];
	left   = le16_to_cpu(ua16(src + 2));
	top    = le16_to_cpu(ua16(src + 4));
	fobw   = le16_to_cpu(ua16(src + 6));
	fobh   = le16_to_cpu(ua16(src + 8));
	param2 = le16_to_cpu(ua16(src + 12));

	if ((anm_flags & ANM_FLAG_IGN_FOB_OFS) == 0) {
		xoff += left;
		yoff += top;
	}

	if (anm_flags & ANM_FLAG_ORIGIN_CENTER) {
		xoff -= fobw >> 1;
		yoff -= fobh >> 1;
	}

	/* based on the GOST parameter in the v1 engine, I think these flipped
	 * variants of codec1 were codecs28-30.
	 */
	if (codec == 1) {
		switch (anm_flags & (ANM_FLAG_FLIPX | ANM_FLAG_FLIPY)) {
		case 0: break;
		case ANM_FLAG_FLIPX: codec = 28; break;
		case ANM_FLAG_FLIPY: codec = 29; break;
		default: codec = 30; break;
		}
	} else if (((codec == 31 || codec == 32))
		   && (anm_flags & (ANM_FLAG_FLIPX | ANM_FLAG_FLIPY))) {
			codec += 4;	/* 35/36 for flipped */
	}

	if (anm_flags & ANM_FLAG_CODEC_OPAQUE) {
		if (codec == 1)
			codec = 3;
		else if (codec == 4)
			codec = 5;
		else if (codec == 31)
			codec = 32;
		else if (codec == 44) {
			/* black background for NUT fonts */
			fillrect(dst, left, top, fobw, fobh, ctx->rt.bufw, ctx->rt.bufh, bgc);
		}
	}

	src += 14;
	size -= 14;

	ret = 0;
	switch (codec) {
	case 1:  codec1(ctx, dst, src, fobw, fobh, xoff, yoff, size, 1); break;
	case 2:  codec2(ctx, dst, src, fobw, fobh, xoff, yoff, size, param, param2); break;
	case 3: if (ctx->rt.mortimer == 0) {
			codec1(ctx, dst, src, fobw, fobh, xoff, yoff, size, 0);
		} else {
			codec1_2x2(ctx, dst, src, fobw, fobh, xoff, yoff);
		}
		break;
	case 4:
	case 5:  codec4(ctx, dst, src, fobw, fobh, xoff, yoff, size, param, param2, codec); break;
	case 20: codec20(ctx, dst, src, fobw, fobh, xoff, yoff, size, fobw); break;
	case 21: codec21(ctx, dst, src, fobw, fobh, xoff, yoff, size, param); break;
	case 23: if (ctx->rt.mortimer == 0) {
			codec23(ctx, dst, src, fobw, fobh, xoff, yoff, size, param, param2);
		} else {
			codec23_2x2(ctx, dst, src, fobw, fobh, xoff, yoff, size, param, param2);
		}
		break;
	case 28: codec1_flipx (ctx, dst, src, fobw, fobh, xoff, yoff, size, 1); break;
	case 29: codec1_flipy (ctx, dst, src, fobw, fobh, xoff, yoff, size, 1); break;
	case 30: codec1_flipxy(ctx, dst, src, fobw, fobh, xoff, yoff, size, 1); break;
	case 31:
	case 32: codec31(ctx, dst, src, fobw, fobh, xoff, yoff, size, param, codec == 32); break;
	case 33:
	case 34: codec33(ctx, dst, src, fobw, fobh, xoff, yoff, size, param, param2, codec); break;
	case 35:
	case 36: codec31_flipx(ctx, dst, src, fobw, fobh, xoff, yoff, size, param, codec == 36); break;
	case 37: ret = codec37(ctx, dst, src, fobw, fobh, xoff, yoff, size, anm_flags); break;
	case 44: codec44(ctx, dst, src, fobw, fobh, xoff, yoff, size, fgc); break;
	case 45: codec45(ctx, dst, src, fobw, fobh, xoff, yoff, size, param, param2); break;
	case 47: ret = codec47(ctx, dst, src, fobw, fobh, xoff, yoff, size, anm_flags); break;
	case 48: ret = codec48(ctx, dst, src, fobw, fobh, xoff, yoff, size, anm_flags); break;
	default: ret = 18; break;
	}

	return ret;
}

static int handle_FOBJ(struct sanctx *ctx, uint32_t size, uint8_t *src,
		       int16_t xoff, int16_t yoff, uint16_t anm_flags)
{
	struct sanrt *rt = &ctx->rt;
	uint16_t w, h, wr, hr, param2;
	int16_t left, top, v1skip;
	uint8_t codec, param;
	int ret;

	/* FOBJ header 14 bytes */
	codec = src[0];
	param = src[1];
	left = le16_to_cpu(ua16(src + 2));
	top  = le16_to_cpu(ua16(src + 4));
	w = wr = le16_to_cpu(ua16(src + 6));
	h = hr = le16_to_cpu(ua16(src + 8));
	v1skip = le16_to_cpu(ua16(src + 10));	/* ANIMv1 version of SKIP chunk */
	param2 = le16_to_cpu(ua16(src + 12));

	/* ignore nonsensical dimensions in frames, happens with
	 * some Full Throttle and RA2 videos.
	 * Do pass the data to codec45 though, it might have the tabledata in it!
	 */
	if ((w < 2) || (h < 2) || (w > FOBJ_MAXX) || (h > FOBJ_MAXY)) {
		if (codec == 45) {
			codec45(ctx, NULL, src + 14, 0, 0, 0, 0, size - 14, param, param2);
			return 0;
		}
		if (!rt->have_vdims)
			return 0;
	}

	/* SotE: all videos have top==60 to center the video in the
	 * 640x400 game window.  We don't need that.
	 */
	if ((w == 640) && (h == 272) && (top == 60) && (codec == 47)) {
		left = top = 0;
		anm_flags |= ANM_FLAG_IGN_FOB_OFS;
	}

	/* All videos with codec37/47/48 have the FOBJ cover the full frame, except
	 * for Mortimer, which requires a 2x2 upscale, and MM which embeds the
	 * smaller c48 object in a full-frame c3 object; but this c3 object is
	 * first in every FRME, so we can forego the clearing of the dest buffer
	 * if any of c37/47/48 are used.
	 */
	if (codec == 37 || codec == 47 || codec == 48) {
		rt->def_anm_flags |= ANM_FLAG_SKIP_CLR_DST;
	}

	/* guess the canvas size */
	if (!rt->have_vdims) {
		if (rt->version < 2) {
			/* RA1: 384x242 source video size, 320x200 display */
			wr = 384;
			hr = 242;
			rt->have_vdims = 1;
			rt->bufw = wr;
			rt->bufh = hr;
		} else {
			/* RA2+: detect commonly used video resolutions */
			wr = w + left;
			hr = h + top;
			if (((wr == 424) && (hr == 260)) ||	/* RA2 */
			    ((wr == 320) && (hr == 200)) ||	/* FT/DIG/.. */
			    ((wr == 640) && (hr == 272)) ||	/* SotE */
			    ((wr == 640) && (hr == 350)) ||	/* MotS */
			    ((wr == 640) && (hr == 480)) ||	/* COMI/OL/.. */
			    ((left == 0) && (top == 0) && (codec == 20)
			     && (w > 3) && (h > 3))) {		/* FT remaster */
				rt->have_vdims = 1;
			}
		}
		rt->pitch = _max(wr, rt->pitch);
		if (!rt->fbsize || (wr > rt->bufw) || (hr > rt->bufh)) {
			rt->bufw = _max(rt->bufw, wr);
			rt->bufh = _max(rt->bufh, hr);
			rt->fbsize = rt->bufw * rt->bufh * 1;
		}
	}

	/* IACT 8/././x>0 is used by c47/48 titles for Palette crossfading */
	if ((rt->version > 1) && (codec >= 47) && (rt->iact8c4x == 0)) {
		rt->iact8c4x = 1;
	}

	if (rt->to_store) {
		/* STOR: save the whole FOBJ */
		rt->to_store = 0;
		if (((size + 4) > size) && ((size + 4) <= rt->fbsize)) {
			*(uint32_t *)rt->buf3 = size;
			memcpy(rt->buf3 + 4, src, size);
		}
	}

	(void)v1skip;

	rt->vbuf = rt->fbuf;
	ret = fob_decode_render(ctx, rt->fbuf, src, size, xoff, yoff, anm_flags, 1, 0);

	if (ret == 0) {
		rt->have_frame = 1;
		rt->last_fobj = src;
		rt->last_fobj_size = size;
	}

	return ret;
}

/* like c47_comp5 but adjusted for tbl2 lookups */
static void bl16_comp8(uint16_t *dst, uint8_t *src, uint32_t left, const uint16_t *tbl2)
{
	uint8_t opc, rlen, j;
	uint16_t col;

	left >>= 1;	/* 16bit pixels */
	while (left) {
		opc = *src++;
		rlen = (opc >> 1) + 1;
		if (rlen > left)
			rlen = left;
		if (opc & 1) {
			col = le16_to_cpu(tbl2[*src++]);
			for (j = 0; j < rlen; j++)
				*dst++ = col;
		} else {
			for (j = 0; j < rlen; j++)
				*dst++ = le16_to_cpu(tbl2[*src++]);
		}
		left -= rlen;
	}
}

/* TGSMUSH.DLL 1000c690 */
static inline uint16_t bl16_c7_avg_col(const uint16_t c1, const uint16_t c2)
{
	return	(((c2 & 0x07e0) + (c1 & 0x07e0)) & 0x00fc0) |
		(((c2 & 0xf800) + (c1 & 0xf800)) & 0x1f000) |
		(((c2 & 0x001f) + (c1 & 0x001f))) >> 1;
}

/* this is basically codec47_comp1(), but for 16bit colors, with color averaging
 * instead of the interpolation table.
 * TGSMUSH.DLL c6f0
 */
static void bl16_comp7(uint16_t *dst, uint8_t *src, const uint16_t w,
		       const uint16_t h, const uint16_t *tbl2)
{
	uint16_t hh, hw, c1, c2;
	uint8_t *dst1, *dst2;

	hh = (h + 1) >> 1;
	dst1 = (uint8_t *)(dst + (w * 2));
	while (hh--) {
		dst2 = dst1 + 4;
		c1 = le16_to_cpu(tbl2[*src++]);
		*(uint16_t *)(dst1 + 0) = c1;
		*(uint16_t *)(dst1 + 2) = c1;
		hw = (w - 1) >> 1;
		while (--hw) {
			c2 = le16_to_cpu(tbl2[*src++]);
			*(uint16_t *)dst2 = bl16_c7_avg_col(c1, c2);
			dst2 += 2;
			*(uint16_t *)dst2 = c2;
			dst2 += 2;
			c1 = c2;
		}
		dst1 += w * 2;	/* next line */
	}

	/* top row is a copy of 2nd row */
	memcpy(dst, dst + w * 2, w * 2);

	dst1 = (uint8_t *)(dst + (w * 4));
	hh = (h - 1) >> 1;
	while (hh--) {
		hw = w;				/* width is pixels! */
		while (hw--) {
			c1 = *(uint16_t *)(dst1 - (w * 2)); /* above */
			c2 = *(uint16_t *)(dst1 + (w * 2)); /* below */
			*(uint16_t *)dst1 = bl16_c7_avg_col(c1, c2);
			dst1 += 2;		/* 16 bit pixel */
		}
	}
}

/* TGSMUSH.DLL c0b4 */
static void bl16_comp6(uint16_t *dst, uint8_t *src, const uint16_t w,
		       const uint16_t h, const uint16_t *tbl2)
{
	int i;
	for (i = 0; i < w * h; i++) {
		*dst++ = le16_to_cpu(tbl2[*src++]);
	}
}

/* TGSMUSH.DLL c5a0 */
static void bl16_comp1(uint16_t *dst, uint8_t *src, const uint16_t w, const uint16_t h)
{
	const uint32_t stride = 2 * w;
	uint8_t *dst1, *dst2;
	uint16_t hh, hw, c1, c2;

	if (h > 0) {
		hh = (h + 1) >> 1;
		dst1 = ((uint8_t *)dst) + stride;
		while (hh--) {
			c1 = le16_to_cpu(*(uint16_t *)src);
			src += 2;
			*(uint16_t *)(dst1 + 0) = c1;	/* first 2 pixels in row */
			*(uint16_t *)(dst1 + 2) = c1;
			dst2 = dst1 + 4;		/* 2 16bit pixels */
			if (w - 2 > 0) {
				hw = (w - 1) >> 1;
				while (--hw) {
					c2 = le16_to_cpu(*(uint16_t *)src);
					src += 2;
					*(uint16_t *)dst2 = bl16_c7_avg_col(c1, c2);
					dst2 += 2;
					*(uint16_t *)dst2 = c2;
					dst2 += 2;
					c1 = c2;
				}
			}
			dst1 += 2 * stride;	/* start of 2nd next line */
		}

	}
	memcpy(dst, dst + stride, stride);
	dst1 = ((uint8_t *)dst) + (2 * stride);
	if (h - 2 > 0) {
		hh = (h - 1) >> 1;
		while (hh--) {
			hw = w;
			while (hw--) {
				c1 = *(uint16_t *)(dst1 + stride);
				c2 = *(uint16_t *)(dst1 - stride);
				*(uint16_t *)dst1 = bl16_c7_avg_col(c1, c2);
				dst1 += 2;	/* 1 16bit pixel */
			}
			dst1 += stride;
		}
	}
}

static uint8_t* bl16_block(uint8_t *src, uint8_t *dst, uint8_t *db1, uint8_t *db2,
			   const uint16_t *tbl1, const uint16_t *tbl2, const uint16_t w,
			   const uint32_t stride, uint8_t blksize, struct sanctx *ctx)
{
	int32_t mvofs, ofs;
	uint8_t *pglyph, opc;
	uint16_t c[2];
	int16_t o2;
	int i, j;

	opc = *src++;
	switch (opc) {
	case 0xff:
		if (blksize == 2) {
			*(uint16_t *)(dst + 0      + 0) = le16_to_cpu(ua16(src));
			src += 2;
			*(uint16_t *)(dst + 0      + 2) = le16_to_cpu(ua16(src));
			src += 2;
			*(uint16_t *)(dst + stride + 0) = le16_to_cpu(ua16(src));
			src += 2;
			*(uint16_t *)(dst + stride + 2) = le16_to_cpu(ua16(src));
			src += 2;
		} else {
			src = bl16_block(src, dst, db1, db2, tbl1, tbl2,
					 w, stride, blksize >> 1, ctx);
			src = bl16_block(src, dst + blksize, db1 + blksize, db2 + blksize,
					 tbl1, tbl2, w, stride, blksize >> 1, ctx);
			dst += stride * (blksize >> 1);
			db1 += stride * (blksize >> 1);
			db2 += stride * (blksize >> 1);
			src = bl16_block(src, dst, db1, db2, tbl1, tbl2,
					 w, stride, blksize >> 1, ctx);
			src = bl16_block(src, dst + blksize, db1 + blksize, db2 + blksize,
					 tbl1, tbl2, w, stride, blksize >> 1, ctx);
		}
		break;
	case 0xfe:
		/* fill a block with a color value from the stream */
		c[0] = le16_to_cpu(ua16(src));
		src += 2;
		for (i = 0; i < blksize; i++) {
			ofs = i * stride;
			for (j = 0; j < blksize; j++) {
				*(uint16_t *)(dst + ofs + j*2) = c[0];
			}
		}
		break;
	case 0xfd:
		/* fill a block using tbl2 color, index from next byte */
		c[0] = le16_to_cpu(tbl2[*src++]);
		for (i = 0; i < blksize; i++) {
			ofs = i * stride;
			for (j = 0; j < blksize; j++) {
				*(uint16_t *)(dst + ofs + j*2) = c[0];
			}
		}
		break;
	case 0xfc:
	case 0xfb:
	case 0xfa:
	case 0xf9:
		/* fill a block using tbl1 color */
		c[0] = le16_to_cpu(tbl1[(opc - 0xf9)]);
		for (i = 0; i < blksize; i++) {
			ofs = i * stride;
			for (j = 0; j < blksize; j++) {
				*(uint16_t *)(dst + ofs + j*2) = c[0];
			}
		}
		break;
	case 0xf8:
		if (blksize == 2) {
			*(uint16_t *)(dst + 0      + 0) = le16_to_cpu(ua16(src));
			src += 2;
			*(uint16_t *)(dst + 0      + 2) = le16_to_cpu(ua16(src));
			src += 2;
			*(uint16_t *)(dst + stride + 0) = le16_to_cpu(ua16(src));
			src += 2;
			*(uint16_t *)(dst + stride + 2) = le16_to_cpu(ua16(src));
			src += 2;
		} else {
			opc = *src++;
			c[1] = le16_to_cpu(ua16(src));
			src += 2;
			c[0] = le16_to_cpu(ua16(src));
			src += 2;
			pglyph = (blksize == 8) ? ctx->c47_glyph8x8[opc] : ctx->c47_glyph4x4[opc];
			for (i = 0; i < blksize; i++) {
				ofs = i * stride;
				for (j = 0; j < blksize; j++) {
					*(uint16_t *)(dst + ofs + j*2) = c[(*pglyph++)];
				}
			}
		}
		break;
	case 0xf7:
		if (blksize == 2) {
			*(uint16_t *)(dst + 0      + 0) = le16_to_cpu(tbl2[*src++]);
			*(uint16_t *)(dst + 0      + 2) = le16_to_cpu(tbl2[*src++]);
			*(uint16_t *)(dst + stride + 0) = le16_to_cpu(tbl2[*src++]);
			*(uint16_t *)(dst + stride + 2) = le16_to_cpu(tbl2[*src++]);
		} else {
			opc = *src++;
			c[1] = le16_to_cpu(tbl2[*src++]);
			c[0] = le16_to_cpu(tbl2[*src++]);
			pglyph = (blksize == 8) ? ctx->c47_glyph8x8[opc] : ctx->c47_glyph4x4[opc];
			for (i = 0; i < blksize; i++) {
				ofs = i * stride;
				for (j = 0; j < blksize; j++) {
					*(uint16_t *)(dst + ofs + j*2) = c[!!(*pglyph++)];
				}
			}
		}
		break;
	case 0xf6:	/* copy from db1 at same spot */
		for (i = 0; i < blksize; i++) {
			ofs = i * stride;
			for (j = 0; j < blksize; j++) {
				*(uint16_t *)(dst + ofs + j*2) = *(uint16_t *)(db1 + ofs + j*2);
			}
		}
		break;
	case 0xf5:	/* copy from db2, mvec from source */
		o2 = le16_to_cpu((int16_t)ua16(src));
		src += 2;
		mvofs = o2 * 2;  /* since stride = w*2 */
		for (i = 0; i < blksize; i++) {
			ofs = i * stride;
			for (j = 0; j < blksize; j++) {
				*(uint16_t *)(dst + ofs + j*2) = *(uint16_t *)(db2 + ofs + j*2 + mvofs);
			}
		}

		break;
	default:
		/* opc is index into c47 mv table, copy 8x8 block from db2.
		 * IMPORTANT: with width 800, for opc 1-4, the calculation will
		 * overflow the int16, turning the large negative values into
		 * large positive values.  This is by design, and exploited by
		 * the 800x600 jonesopn_8.snm video from "Indiana Jones and
		 *  the Infernal Machine".
		 * tl;dr: the cast to int16_t is essential for this to work.
		 * But signed integer overflow is UB according to C standard,
		 *  so we first calculate an int32_t, cast to an uint16_t (which
		 *  is not UB) to truncate, then cast to signed int16_t.
		 */
		mvofs = (c47_mv[opc][1] * w + c47_mv[opc][0]);	/* not overflowing */
		uint16_t u16 = (uint16_t)mvofs;			/* truncate, no UB */
		int16_t i16 = (int16_t)u16;
		mvofs = i16 * 2;
		for (i = 0; i < blksize; i++) {
			ofs = i * stride;
			for (j = 0; j < blksize; j++) {
				*(uint16_t *)(dst + ofs + j*2) = *(uint16_t *)(db2 + ofs + j*2 + mvofs);
			}
		}
		break;
	}
	return src;
}

static void bl16_comp2(uint8_t *dst, uint8_t *src, uint16_t w, uint16_t h,
		       uint8_t *db1, uint8_t *db2, const uint16_t *tbl1, const uint16_t *tbl2,
		       struct sanctx *ctx)
{
	const uint32_t stride = w * 2;
	int i, j;

	h = (h + 7) & ~7;
	w = (w + 7) & ~7;

	for (j = 0; j < h; j += 8) {
		for (i = 0; i < 2 * w; i += 8 * 2) {
			src = bl16_block(src, dst + i, db1 + i , db2 + i, tbl1,
					 tbl2, w, stride, 8, ctx);
		}
		dst += stride * 8;
		db1 += stride * 8;
		db2 += stride * 8;
	}
}

static void handle_BL16(struct sanctx *ctx, uint32_t size, uint8_t *src)
{
	struct sanrt *rt = &ctx->rt;
	uint16_t *dst, *db1, *db2, width, height, seq;
	uint16_t *tbl1, *tbl2, bgc;
	uint8_t codec, newrot;
	uint32_t decsize;
	int i;

	if (size < 0x230)
		return;

	/* source pixel format. 3 seems to indicate 16bpp RGB565; the decoder code
	 * in tgsmush.dll supports nothing else.
	 */
	if (src[0x228] != 3)
		return;

	dst = (uint16_t *)rt->buf0;
	db1 = (uint16_t *)rt->buf1;
	db2 = (uint16_t *)rt->buf2;

	width  = le16_to_cpu(*(uint16_t *)(src + 8));
	height = le16_to_cpu(*(uint16_t *)(src + 12));
	seq    = le16_to_cpu(*(uint16_t *)(src + 16));
	codec  = src[18];
	newrot = src[19];
	tbl1 = (uint16_t *)(src + 24);
	bgc = le16_to_cpu(*(uint16_t *)(src + 32));
	decsize = le32_to_cpu(*(uint32_t *)(src + 36));
	tbl2 = (uint16_t *)(src + 40);

	if (seq == 0) {
		rt->lastseq = -1;
		for (i = 0; i < width * height; i++) {
			*db1++ = bgc;
			*db2++ = bgc;
		}
		db1 = (uint16_t *)rt->buf1;
		db2 = (uint16_t *)rt->buf2;
	}

	src += 0x230;
	size -= 0x230;
	switch (codec) {
	case 0: for (i = 0; i < width * height; i++, src += 2)
			*dst++ = le16_to_cpu(*(uint16_t *)src);
		break;
	case 1: bl16_comp1(dst, src, width, height); break;
	case 2: if (seq == rt->lastseq + 1)
			bl16_comp2((uint8_t *)dst, src, width, height,
				   (uint8_t *)db1, (uint8_t *)db2, tbl1, tbl2, ctx);
		break;
	case 3:	memcpy(dst, db2, width * height * 2); break;
	case 4: memcpy(dst, db1, width * height * 2); break;
	case 5: codec47_comp5(src, size, (uint8_t *)dst, decsize); break;
	case 6: bl16_comp6(dst, src, width, height, tbl2); break;
	case 7: bl16_comp7(dst, src, width, height, tbl2); break;
	case 8: bl16_comp8(dst, src, decsize, tbl2); break;
	}

	rt->vbuf = rt->buf0;
	rt->have_frame = 1;
	rt->palette = NULL;
	if (seq == rt->lastseq + 1)
		c47_swap_bufs(ctx, newrot);
	rt->lastseq = seq;
}

static void handle_NPAL(struct sanctx *ctx, uint32_t size, uint8_t *src)
{
	if (size >= 768)
		read_palette(ctx, src);
}

static inline uint8_t _u8clip(int a)
{
	if (a > 255) return 255;
	else if (a < 0) return 0;
	else return a;
}

static void handle_XPAL(struct sanctx *ctx, uint32_t size, uint8_t *src)
{
	uint32_t *pal = ctx->rt.palette;
	int16_t *sp = ctx->rt.shiftpal;
	int16_t *dp = ctx->rt.deltapal;
	int i, j, t2[3];
	uint16_t cmd;

	if (size < 4)
		return;

	cmd = be16_to_cpu(*(uint16_t *)(src + 2));
	src += 4;

	if (cmd == 0 || cmd == 2) {
		if (cmd == 2) {
			if (size < (768 * 3 + 4))
				return;
			read_palette(ctx, src + (768 * 2));
		}
		if (size < (768 * 2 + 4))
			return;

		for (i = 0; i < 768; i += 3) {
			dp[i + 0] = le16_to_cpu(*(int16_t *)(src + 0));
			dp[i + 1] = le16_to_cpu(*(int16_t *)(src + 2));
			dp[i + 2] = le16_to_cpu(*(int16_t *)(src + 4));
			src += 6;
			sp[i + 0] = ((*pal >>  0) & 0xff) << 7;
			sp[i + 1] = ((*pal >>  8) & 0xff) << 7;
			sp[i + 2] = ((*pal >> 16) & 0xff) << 7;
			pal++;
		}
	} else {
		for (i = 0; i < 768; i += 3) {
			for (j = 0; j < 3; j++) {
				sp[i + j] += dp[i + j];
				t2[j] = _u8clip(sp[i + j] >> 7);
			}
			*pal++ = 0xffU << 24 | t2[2] << 16 | t2[1] << 8 | t2[0];
		}
	}
}

/*******************************************************************************
 *
 *  AUDIO
 *
 ******************************************************************************/

static void atrk_setdamp(struct sanmsa *msa, uint16_t dampmin, uint16_t dampmax,
			 uint16_t diprate, uint16_t riserate)
{
	if (dampmax > 127)
		dampmax = 127;
	if (dampmin > dampmax)
		dampmin = 96;
	if (dampmax < dampmin)
		dampmax = 127;
	if (riserate > 127 || riserate < 1)
		riserate = 32;
	if (diprate > 127 || diprate < 1)
		diprate = 24;

	msa->sou_vol_damp = dampmax;
	msa->sou_damp_min = dampmin;
	msa->sou_damp_max = dampmax;
	msa->sou_damp_dip_rate = diprate;
	msa->sou_damp_rise_rate = riserate;
}

static void atrk_init_volumes(struct sanmsa *msa)
{
	msa->sou_vol_sfx = 127;
	msa->sou_vol_voice = 127;
	msa->sou_vol_music = 127;
	msa->sou_vol_global = 127;
	atrk_setdamp(msa, 114, 127, 24, 32);
}

static uint32_t atrk_bytes_to_dstframes(struct sanatrk *atrk, uint32_t avail)
{
	uint32_t den = atrk->src_cnvrate >> 8;

	if (atrk->flags & ATRK_SRC8BIT) {
		avail = avail * 1;		/* 1 in byte -> 1 out frame   */
	} else if (atrk->flags & ATRK_SRC12BIT) {
		avail = (avail * 2) / 3;	/* 3 in bytes -> 2 out frames */
	} else {
		avail = avail >> 1;		/* 2 in bytes -> 1 out frame  */
	}
	if (0 == (atrk->flags & ATRK_1CH)) {
		avail /= 2;			/* 2ch: double input bytes reqd. */
	}

	if (den == 0) {
		avail = (avail == 0) ? 0 : 4096;
	} else {
		uint32_t num = avail << 8;
		if (num > (4096 * den))
			avail = 4096;
		else
			avail = num / den;
	}
	return avail;
}

static inline uint32_t atrk_bufbytes(struct sanatrk *atrk)
{
	if (atrk->rdptr <= atrk->wrptr)
		return atrk->wrptr - atrk->rdptr;
	return atrk->wrptr + ATRK_DATSZ - atrk->rdptr;
}

static void atrk_update_dstframes_avail(struct sanatrk *atrk)
{
	atrk->dstfavail = atrk_bytes_to_dstframes(atrk, atrk_bufbytes(atrk));
	atrk->dstpavail = atrk_bytes_to_dstframes(atrk, atrk->playlen);
}

static uint32_t atrk_resample_8(struct sanatrk *atrk, int16_t *dst, uint32_t count)
{
	const uint32_t mask = ATRK_DATMASK;
	const uint32_t chm = (atrk->flags & ATRK_1CH) ? 1 : 2;
	uint32_t acc = atrk->src_accum;
	int16_t s1, s2;

	for (uint32_t i = 0; i < count; i++) {
		uint32_t isidx = acc >> 16;
		uint32_t frac = acc & 0xFFFF;
		uint32_t pos = (atrk->rdptr + (isidx * chm)) & mask;

		if (atrk->flags & ATRK_1CH) {
			/* Mono 8-bit */
			s1 = (atrk->data[pos] << 8) ^ 0x8000;
			if (frac && (((pos + 1) & mask) < atrk->wrptr)) {
				int16_t next = (atrk->data[(pos + 1) & mask] << 8) ^ 0x8000;
				s1 += ((next - s1) * (int32_t)frac) >> 16;
			}
			dst[0] = dst[1] = s1;
		} else {
			/* Stereo 8-bit */
			s1 = (atrk->data[pos] << 8) ^ 0x8000;
			s2 = (atrk->data[(pos + 1) & mask] << 8) ^ 0x8000;
			if (frac && (((pos + 4) & mask) < atrk->wrptr)) {
				int16_t n1 = (atrk->data[(pos + 2) & mask] << 8) ^ 0x8000;
				int16_t n2 = (atrk->data[(pos + 3) & mask] << 8) ^ 0x8000;
				s1 += ((n1 - s1) * (int32_t)frac) >> 16;
				s2 += ((n2 - s2) * (int32_t)frac) >> 16;
			}
			dst[0] = s1;
			dst[1] = s2;
		}
		dst += 2;
		acc += atrk->src_cnvrate;
	}

	atrk->src_accum = acc & 0xFFFF;

	return ((acc >> 16) * chm);	/* source bytes consumed */
}

static uint32_t atrk_resample_16(struct sanatrk *atrk, int16_t *dst, uint32_t count)
{
	const uint32_t mask = ATRK_DATMASK;
	const uint32_t chm = (atrk->flags & ATRK_1CH) ? 1 : 2;
	uint32_t acc = atrk->src_accum;
	int16_t s1, s2;

	for (uint32_t i = 0; i < count; i++) {
		uint32_t isidx = acc >> 16;
		uint32_t frac = acc & 0xFFFF;
		uint32_t pos = (atrk->rdptr + (isidx * chm * 2)) & mask;

		if (atrk->flags & ATRK_1CH) {
			/* Mono 16-bit Little Endian */
			s1 = (int16_t)(atrk->data[pos] | (atrk->data[(pos + 1) & mask] << 8));
			if (frac && (((pos + 3) & mask) < atrk->wrptr)) {
				uint32_t pos2 = (pos + 2) & mask;
				int16_t next = (int16_t)(atrk->data[pos2] | (atrk->data[(pos2 + 1) & mask] << 8));
				s1 += ((next - s1) * (int32_t)frac) >> 16;
			}
			dst[0] = dst[1] = s1;
		} else {
			/* Stereo 16-bit Little Endian */
			s1 = (int16_t)(atrk->data[pos] | (atrk->data[(pos + 1) & mask] << 8));
			uint32_t posR = (pos + 2) & mask;
			s2 = (int16_t)(atrk->data[posR] | (atrk->data[(posR + 1) & mask] << 8));

			if (frac && (((pos + 7) & mask) < atrk->wrptr)) {
				/* Next frame Left */
				uint32_t posN = (pos + 4) & mask;
				int16_t n1 = (int16_t)(atrk->data[posN] | (atrk->data[(posN + 1) & mask] << 8));
				/* Next frame Right */
				uint32_t posNR = (pos + 6) & mask;
				int16_t n2 = (int16_t)(atrk->data[posNR] | (atrk->data[(posNR + 1) & mask] << 8));

				s1 += ((n1 - s1) * (int32_t)frac) >> 16;
				s2 += ((n2 - s2) * (int32_t)frac) >> 16;
			}
			dst[0] = s1;
			dst[1] = s2;
		}
		dst += 2;
		acc += atrk->src_cnvrate;
	}

	atrk->src_accum = acc & 0xFFFF;

	return ((acc >> 16) * 2 * chm);	/* source bytes consumed */
}

static int16_t atrk_decode_12bit(const uint8_t *data, uint32_t ptr, int hinib, const uint32_t wp)
{
	const uint32_t p0 = ptr & ATRK_DATMASK;
	const uint32_t p1 = (p0 + 1) & ATRK_DATMASK;
	const uint32_t p2 = (p0 + 2) & ATRK_DATMASK;

	const uint8_t b0 = data[p0];
	const uint8_t b1 = data[p1];
	const uint8_t b2 = data[p2];

	if (!hinib) {
		if (b1 < wp)
			return ((((b1 & 0x0f) << 8) | b0) << 4) - 0x8000;
		else
			return 0;
	} else {
		if (b2 < wp)
			return ((((b1 & 0xf0) << 4) | b2) << 4) - 0x8000;
		else
			return 0;
	}
}

static uint32_t atrk_resample_12(struct sanatrk *atrk, int16_t *dst, uint32_t count)
{
	uint32_t acc = atrk->src_accum;
	const uint32_t wp = atrk->wrptr;
	int16_t s1, s2;

	for (uint32_t i = 0; i < count; i++) {
		uint32_t isidx = acc >> 16;
		uint32_t frac = acc & 0xFFFF;

		if (atrk->flags & ATRK_1CH) {
			/* Mono 12-bit: 3 bytes = 2 samples */
			uint32_t block = isidx / 2;
			uint32_t rem = isidx & 1;
			uint32_t pos = (atrk->rdptr + (block * 3));

			s1 = atrk_decode_12bit(atrk->data, pos, rem, wp);

			if (frac) {
				int16_t next;
				/* If rem was 0 (low nibble), next is same block rem 1 (high nibble)
				 * If rem was 1 (high nibble), next is next block (block+1) rem 0 */
				if (rem == 0) {
					next = atrk_decode_12bit(atrk->data, pos, 1, wp);
				} else {
					next = atrk_decode_12bit(atrk->data, pos + 3, 0, wp);
				}
				s1 += ((next - s1) * (int32_t)frac) >> 16;
			}
			dst[0] = dst[1] = s1;
		} else {
			/* Stereo 12-bit: 3 bytes = 1 frame (L+R) */
			uint32_t pos = (atrk->rdptr + (isidx * 3));

			/* L is always low nibble type, R is always high nibble type */
			s1 = atrk_decode_12bit(atrk->data, pos, 0, wp);
			s2 = atrk_decode_12bit(atrk->data, pos, 1, wp);

			if (frac) {
				int16_t n1 = atrk_decode_12bit(atrk->data, pos + 3, 0, wp);
				int16_t n2 = atrk_decode_12bit(atrk->data, pos + 3, 1, wp);

				s1 += ((n1 - s1) * (int32_t)frac) >> 16;
				s2 += ((n2 - s2) * (int32_t)frac) >> 16;
			}
			dst[0] = s1;
			dst[1] = s2;
		}
		dst += 2;
		acc += atrk->src_cnvrate;
	}

	atrk->src_accum = acc & 0xFFFF;

	/* calculate source bytes consumed */
	uint32_t bytes_consumed;
	if (atrk->flags & ATRK_1CH) {
		bytes_consumed = (((acc >> 16) + 1) / 2) * 3;
	} else {
		bytes_consumed = (acc >> 16) * 3;
	}

	return bytes_consumed;
}

static inline void atrk_reset(struct sanatrk *atrk)
{
	/* first and last member of struct sanatrk are void* and need to be kept */
	memset(&atrk->rdptr, 0, sizeof (struct sanatrk) - 2*sizeof(void *));
}

static void atrk_set_srate(struct sanatrk *atrk, uint32_t rate)
{
	const uint32_t destrate = atrk->msa->destrate;
	atrk->srate = rate;

	/* find out whether the rate is a truncated integer ratio
	 * of the destination rate, to fix consumption accounting,
	 *  e.g. in RA2 LEV03/03PLAY2.SAN laser blasts at 5512Hz.
	 */
	const uint32_t div = (destrate + (rate / 2)) / rate;
	if ((div > 0) && (destrate / div) == rate)
		atrk->src_cnvrate = (1U << 16) / div;
	else
		atrk->src_cnvrate = ((uint64_t)rate << 16) / destrate;
}

static void atrk_set_playpos(struct sanatrk *atrk, uint32_t ofs, uint32_t len)
{
	atrk->rdptr = ofs & ATRK_DATMASK;
	atrk->playlen = len;
	atrk->src_accum = 0;
	atrk_update_dstframes_avail(atrk);
	if (atrk->state == STATE_MIXED)
		atrk->state = STATE_NEWDATA;
}

/* create a default STRK script which plays the whole stream, then terminates,
 * for the iMUSE (The Dig) tracks.
 */
static void atrk_set_default_strk(struct sanatrk *atrk, uint32_t len)
{
	uint8_t *stp = atrk->strk;

	/* set cmd1, play offset 0, play length 'len' */
	stp[0] = 1;
	stp[1] = 8;
	stp[2] = stp[3] = stp[4] = stp[5] = 0;
	stp[6] = (len >> 24) & 0xff;
	stp[7] = (len >> 16) & 0xff;
	stp[8] = (len >>  8) & 0xff;
	stp[9] = (len >>  0) & 0xff;
	/* cmd5: terminate stream */
	stp[10] = 5;
	atrk->strksz = 11;
}

/* STRK script processor, called when a stream has run out of data to play. */
static void atrk_process_strk(struct sanatrk *atrk)
{
	uint8_t *s, *r = &atrk->msa->sou_hooks[0xff];
	uint32_t v1, v2, v3, v4;
	int j;

	j = atrk->strksz - atrk->strkptr;
	if (j < 1) {
		atrk_reset(atrk);
		return;
	}

	do {
		s = atrk->strk + atrk->strkptr;
		switch (s[0]) {
		case 1:					/* set play offset + length */
			if (j < 10)
				goto out_err;

			v1 = be32_to_cpu(ua32(s + 2 + 0));
			v2 = be32_to_cpu(ua32(s + 2 + 4));
			atrk_set_playpos(atrk, v1, v2);
			atrk->strkptr += s[1] + 2;
			atrk->af4 = 0;			/* for case 7 */
			goto out_ok;			/* DONE: playlen set */
		case 3:					/* set channel params/hooks */
			if (j < 4)
				goto out_err;

			if (s[2] == 0xff) {
				*r = s[3];
			} else if (s[2] == 0xfe) {
				if ((s[3] >= 0) && (s[3] < 128))
					atrk->vol = s[3];
			} else if (s[2] == 0xfd) {
				if (((int8_t)s[3] > -128) && (s[3] < 128))
					atrk->pan = s[3];
			} else {
				atrk->msa->sou_hooks[s[2]] = s[3];
			}
			atrk->strkptr += s[1] + 2;
			break;
		case 4:					/* change channel params */
			if (j < 4)
				goto out_err;

			if (s[2] == 0xff) {
				*r += (int8_t)s[3];	/* "register" */
			} else if (s[2] == 0xfe) {
				atrk->vol += (int8_t)s[3];
				if (atrk->vol > ATRK_VOL_MAX)
					atrk->vol = ATRK_VOL_MAX;
			} else if (s[2] == 0xfd) {
				atrk->pan += (int8_t)s[3];
				if (atrk->pan & 0x80)
					atrk->pan = 0;
			} else {
				atrk->msa->sou_hooks[s[2]] += (int8_t)s[3];
			}
			atrk->strkptr += s[1] + 2;
			break;
		case 6:			/* set play offset, length, samplerate */
			if (j < 14)
				goto out_err;

			v1 = be32_to_cpu(ua32(s + 2 + 0));
			v2 = be32_to_cpu(ua32(s + 2 + 4));
			v3 = be32_to_cpu(ua32(s + 2 + 8));
			if ((v3 < 1000) || (v3 > 48000))
				goto out_err;
			atrk_set_playpos(atrk, v1, v2);
			atrk_set_srate(atrk, v3);
			atrk->strkptr += s[1] + 2;
			atrk->af4 = 0;			/* for case 7 */
			goto out_ok;			/* DONE: playlen set */
		case 2:
		case 8:
		case 9:
		case 10:
		case 11:	/* advance STRK ptr based on params + op */
			if (j < 6)
				goto out_err;

			/* gather params */
			if (s[4] == 0xff) {
				/* do nothing: *r = *r */
			} else if (s[4] == 0xfe) {
				*r = atrk->vol;
			} else if (s[4] == 0xfd) {
				*r = atrk->pan;
			} else {
				*r = atrk->msa->sou_hooks[s[4]];
			}
			/* execute logical op */
			switch (s[0]) {
			case 2:	 *r =((*r != 0) || (s[4] == 0)) ? 1 : 0; break;
			case 8:  *r = (s[5] < *r) ? 1 : 0; break;
			case 9:  *r = (*r < s[5]) ? 1 : 0; break;
			case 10: *r = (*r == s[5]) ? 1 : 0; break;
			case 11: *r = (*r == s[5]) ? 0 : 1; break;
			}
			if (*r == 0) {
				atrk->strkptr += s[1] + 2;
			} else {
				atrk->strkptr += (int16_t)be16_to_cpu(ua16(s + 2));
			}
			break;
		case 7:		/* XXX: not exactly sure yet */
			if (j < 20)
				goto out_err;

			v1 = be32_to_cpu(ua32(s + 2 +  0));
			v2 = be32_to_cpu(ua32(s + 2 +  4));
			v3 = be32_to_cpu(ua32(s + 2 +  8));
			v4 = be32_to_cpu(ua32(s + 2 + 12));
			if ((v3 < 1000) || (v3 > 48000))
				goto out_err;
			/* SHELL.EXE 41cb5e+ */
			if (atrk->af4 == 0) {
				atrk->af4 = v2;
				atrk->af0 = 0;
			}
			if (atrk->af4 <= v4)
				v4 = atrk->af4;
			atrk_set_playpos(atrk, v1 + atrk->af0, v4);
			atrk_set_srate(atrk, v3);
			atrk->af4 -= v4;
			atrk->af0 += v4;
			if (atrk->af4 == 0)
				atrk->strkptr += (int16_t)be16_to_cpu(ua16(s + 2 + 16));
			else
				atrk->strkptr += s[1] + 2;
			goto out_ok;			/* DONE: playlen set */
		default:
		case 5:					/* reset channel */
			atrk_reset(atrk);
			goto out_ok;
		}

		j = atrk->strksz - atrk->strkptr;
	} while (j > 0);
	/* no more STRK bytes, but still here? Just terminate the stream then */
out_err:
	atrk_reset(atrk);
out_ok:
	return;
}

static void atrk_set_srcfmt(struct sanatrk *atrk, uint16_t rate,
			    uint8_t bits, uint8_t ch, uint8_t vol, int8_t pan)
{
	atrk->flags &= ~(ATRK_1CH | ATRK_SRC8BIT | ATRK_SRC12BIT);
	if (bits == 8) {
		atrk->flags |= ATRK_SRC8BIT;
		atrk->resample = atrk_resample_8;
	} else if (bits == 12) {
		atrk->flags |= ATRK_SRC12BIT;
		atrk->resample = atrk_resample_12;
	} else {
		atrk->resample = atrk_resample_16;
	}
	if (ch < 2)
		atrk->flags |= ATRK_1CH;

	atrk->vol = vol;
	atrk->pan = pan;
	atrk_set_srate(atrk, rate);
}

/* find a trkid in the santrak list. Default allocate a new if the trkid is
 * not yet valid, or optionally return NULLptr if trkid is unknown.
 */
static struct sanatrk *atrk_find_trkid(struct sanmsa *msa, uint16_t trkid,
				       int16_t idx, int16_t maxidx, int fnf)
{
	struct sanatrk *atrk;
	int i, newid;

	newid = -1;
	for (i = 0; i < msa->numtrk; i++) {
		atrk = &(msa->atrk[i]);
		if ((atrk->state > STATE_UNUSED) && (trkid == atrk->trkid)) {
			if ((idx == -1) || ((atrk->maxidx == maxidx) && (atrk->curridx + 1 == idx)))
				return atrk;
		}
		if ((newid < 0) && (atrk->state == STATE_UNUSED))
			newid = i;
	}
	if ((newid > -1) && (!fnf)) {
		atrk = &(msa->atrk[newid]);
		atrk_reset(atrk);
		atrk->vol = 127;
		return atrk;
	}
	return NULL;
}

static void atrk_reset_mixed(struct sanmsa *msa, int cleanup)
{
	struct sanatrk *atrk;
	for (int i = 0; i < msa->numtrk; i++) {
		atrk = &(msa->atrk[i]);
		if (cleanup && (atrk_bufbytes(atrk) == 0) && (atrk->dataleft == atrk->playlen)
		    && (atrk->rdptr > 0) && (atrk->wrptr > 0)
		    && (atrk->state == STATE_MIXABLE)) {
			/* RA1 SEGA-CD: does sound data accounting a bit differently.
			 * all tracks end with still 8 bytes left to fill+play but no more
			 * data coming in.  However sice the engine does age-based
			 * assignment of the 3 non-music channels, this is no problem
			 * there.  We however need to kill these stale channels.
			 */
			atrk_reset(atrk);
		} else {
			if (atrk->state >= STATE_MIXED)
				atrk->state = STATE_MIXABLE;
		}
	}
}

/* get the first not yet mixed track */
static struct sanatrk *atrk_get_next_mixable(struct sanmsa *msa)
{
	struct sanatrk *atrk;
	for (int i = 0; i < msa->numtrk; i++) {
		atrk = &(msa->atrk[i]);
		if ((atrk->state == STATE_MIXABLE) && atrk->dstfavail)
			return atrk;
	}
	return NULL;
}

/* count all mixable tracks, with shortest buffer size returned too */
static int atrk_count_mixable(struct sanmsa *msa, uint32_t *minlen)
{
	struct sanatrk *atrk;
	int i, mixable;
	uint32_t ml, df;

	ml = ~0U;
	mixable = 0;
	for (i = 0; i < msa->numtrk; i++) {
		atrk = &(msa->atrk[i]);
		df = _min(atrk->dstpavail, atrk->dstfavail);
		if ((atrk->state == STATE_MIXABLE) && df) {
			mixable++;
			if (ml > df) {
				ml = df;
			}
		}
	}

	if (minlen)
		*minlen = ml;
	return mixable;
}

/* count all active tracks: tracks which are still allocated */
static int atrk_count_active(struct sanmsa *msa, int *voice)
{
	struct sanatrk *atrk;
	int i, active;

	active = 0;
	if (voice)
		*voice = 0;
	for (i = 0; i < msa->numtrk; i++) {
		atrk = &(msa->atrk[i]);
		if (voice && (SAUD_FLAG_TRK_VOICE == (atrk->flags & SAUD_FLAG_TRK_MASK)))
			*voice = 1;
		if (atrk->state > STATE_HEADER)
			active++;
	}
	return active;
}

static inline void atrk_finish_all(struct sanmsa *msa)
{
	struct sanatrk *atrk;
	int i;

	/* let tracks with up to 10 seconds of playback data left play out,
	 * but terminate all longer streams.
	 */
	for (i = 0; i < msa->numtrk; i++) {
		atrk = &(msa->atrk[i]);
		if (atrk->state < STATE_MIXABLE)
			continue;
		if ((atrk->playlen / atrk->srate) > 10)
			atrk_reset(atrk);
		else
			atrk->strksz = 0;
	}
}

/* buffer the incoming data to the ringbuffer */
static void atrk_read_pcmsrc(struct sanatrk *atrk, uint32_t size, uint8_t *src)
{
	uint32_t toend;

	toend = ATRK_DATSZ - atrk->wrptr;
	if (size <= toend) {
		memcpy(atrk->data + atrk->wrptr, src, size);
	} else {
		memcpy(atrk->data + atrk->wrptr, src, toend);
		memcpy(atrk->data + 0, src + toend, size - toend);
	}
	atrk->dataleft -= size;
	atrk->wrptr += size;
	atrk->wrptr &= ATRK_DATMASK;
	if (atrk->state > STATE_HEADER)
		atrk_update_dstframes_avail(atrk);
}

static void aud_mixs16(uint8_t *ds1, uint8_t *s1, uint8_t *s2, int bytes,
		       uint8_t vol1, int8_t pan1, uint8_t vol2, int8_t pan2)
{
	/* my attempt at a autovect-friendly mixing function. use __restrict to
	 * indicate to the compiler that these buffers don't overlap
	 */
	int16_t * __restrict src1 = (int16_t *)s1;
	int16_t * __restrict src2 = (int16_t *)s2;
	int16_t * __restrict dst  = (int16_t *)ds1;
	int32_t v1l, v1r, v2l, v2r;

	/* calculate volume/pan */
	if (pan1 == 0) {
		v1l = v1r = vol1;
	} else if (pan1 < 0) {
		v1l = vol1;
		v1r = (vol1 * (128 + pan1)) >> 7;
	} else {
		v1l = (vol1 * (128 - pan1)) >> 7;
		v1r = vol1;
	}

	if (pan2 == 0) {
		v2l = v2r = vol2;
	} else if (pan2 < 0) {
		v2l = vol2;
		v2r = (vol2 * (128 + pan2)) >> 7;
	} else {
		v2l = (vol2 * (128 - pan2)) >> 7;
		v2r = vol2;
	}

	/* loop in sample pairs */
	for (int i = 0; i < bytes/4; i++) {
		int32_t raw1_L = src1 ? src1[0] : 0;
		int32_t raw1_R = src1 ? src1[1] : 0;
		int32_t raw2_L = src2 ? src2[0] : 0;
		int32_t raw2_R = src2 ? src2[1] : 0;

		if (src1)
			src1 += 2;
		if (src2)
			src2 += 2;

		/* apply volume */
		int32_t d1_Ls = (raw1_L * v1l) >> 7;
		int32_t d1_Rs = (raw1_R * v1r) >> 7;
		int32_t d2_Ls = (raw2_L * v2l) >> 7;
		int32_t d2_Rs = (raw2_R * v2r) >> 7;

		/* s16 to u16 */
		uint32_t d1_L = d1_Ls + 32768;
		uint32_t d1_R = d1_Rs + 32768;
		uint32_t d2_L = d2_Ls + 32768;
		uint32_t d2_R = d2_Rs + 32768;

		/* both silent path */
		uint32_t dark_L = (d1_L * d2_L) >> 15;
		uint32_t dark_R = (d1_R * d2_R) >> 15;

		/* one louder path */
		uint32_t light_L = ((d1_L + d2_L) << 1) - dark_L - 65536;
		uint32_t light_R = ((d1_R + d2_R) << 1) - dark_R - 65536;

		/* selection condition */
		int cond_L = (d1_L < 32768) && (d2_L < 32768);
		int cond_R = (d1_R < 32768) && (d2_R < 32768);

		/* and select */
		uint32_t res_L = cond_L ? dark_L : light_L;
		uint32_t res_R = cond_R ? dark_R : light_R;

		/* clamp */
		if (res_L > 65535)
			res_L = 65535;
		if (res_R > 65535)
			res_R = 65535;

		/* mixed u16 back to s16 */
		dst[0] = (int16_t)(res_L - 32768);
		dst[1] = (int16_t)(res_R - 32768);

		dst += 2;
	}
}

static void atrk_resample(struct sanatrk *atrk, int16_t *destbuf, uint32_t len)
{
	uint32_t bytes;

	bytes = atrk->resample(atrk, destbuf, len);
	atrk->state = STATE_MIXED;

	/* update read/play pointers */
	atrk->rdptr += bytes;
	atrk->rdptr &= ATRK_DATMASK;
	if (bytes > atrk->playlen)
		bytes = atrk->playlen;
	atrk->playlen -= bytes;
	atrk_update_dstframes_avail(atrk);
	if (atrk->dstpavail < 1)
		atrk_process_strk(atrk);
}

static int aud_mix_tracks(struct sanctx *ctx)
{
	struct sanmsa *msa = ctx->msa;
	int16_t *trkobuf = (int16_t *)msa->audrsb1;
	int active1, active2, mixable, voice;
	uint32_t minlen1, dstlen, dff;
	uint8_t *dstptr, *aptr;
	struct sanatrk *atrk;

	dstlen = 0;
	dstptr = aptr = ctx->adstbuf1;
	memset(dstptr, 0, msa->audminframes * 4);

	dff = msa->audminframes;	/* amount of target samples to generate */

	active1 = atrk_count_active(msa, &voice);
	while ((active1 != 0) && (dff != 0)) {
		atrk_reset_mixed(msa, 0);
		mixable = atrk_count_mixable(msa, &minlen1);
		if ((mixable < 1) || (minlen1 == -1))
			break;

		if (minlen1 > dff)
			minlen1 = dff;

		while (NULL != (atrk = atrk_get_next_mixable(msa))) {
			const int m = atrk->flags & SAUD_FLAG_TRK_MASK;
			int vol = 0, pan = (atrk->flags & ATRK_1CH) ? atrk->pan : 0;

			if (m == 0) {
				vol = (atrk->vol * msa->sou_vol_sfx) >> 7;
			} else if (m == SAUD_FLAG_TRK_VOICE) {
				vol = (atrk->vol * msa->sou_vol_voice) >> 7;
			} else if (m == SAUD_FLAG_TRK_MUSIC) {
				vol = (atrk->vol * msa->sou_vol_music) >> 7;
			}
			vol = (vol * msa->sou_vol_global) >> 7;
			if (m == SAUD_FLAG_TRK_MUSIC)
				vol = ((vol * msa->sou_vol_damp) >> 7) & 0xff;

			atrk_resample(atrk, trkobuf, minlen1);
			aud_mixs16(dstptr, (uint8_t *)trkobuf, dstptr,
				   minlen1 * 4, vol, pan, ATRK_VOL_MAX, 0);
		}
		dstlen += minlen1 * 4;
		dstptr += dstlen;
		dff -= minlen1;
		if (dff) {
			active2 = atrk_count_active(msa, NULL);
			if (active2 < active1) {
				/* the short track was done and freed, try
				 * again to get the missing rest from the
				 * other still active tracks.
				 */
				active1 = active2;
				continue;
			}
			/* see if any of the tracks had a reset of their play
			 * position/length (STRK processing), indicated by
			 * STATE_NEWDATA, we reconsider these for further
			 * processing as well.
			 */
			for (int i = 0; i < msa->numtrk; i++) {
				atrk = &(msa->atrk[i]);
				if (atrk->state == STATE_NEWDATA)
					atrk->state = STATE_MIXED;
			}
		}
	}
	if (dstlen)
		ctx->io->queue_audio(ctx->io->userctx, aptr, dstlen);

	if (voice) {
		if (msa->sou_vol_damp != msa->sou_damp_min)
			msa->sou_vol_damp -= msa->audminframes / msa->sou_damp_dip_rate;
		if (msa->sou_vol_damp < msa->sou_damp_min)
			msa->sou_vol_damp = msa->sou_damp_min;
	} else {
		if (msa->sou_vol_damp != msa->sou_damp_max)
			msa->sou_vol_damp += msa->audminframes / msa->sou_damp_rise_rate;
		if (msa->sou_vol_damp > msa->sou_damp_max)
			msa->sou_vol_damp = msa->sou_damp_max;
	}

	atrk_reset_mixed(msa, 1);
	return (dstlen != 0);
}

static int iact_audio_imuse(struct sanmsa *msa, uint32_t size, uint8_t *src,
			    uint16_t trkid, uint16_t uid)
{
	uint32_t cid, csz, mapsz;
	uint16_t rate, bits, chnl, vol;
	struct sanatrk *atrk;

	if (size > (_ATRK_DATSZ / 2))
		return 1;

	vol = ATRK_VOL_MAX;
	if (uid == 1)
		trkid += 100;
	else if (uid == 2)
		trkid += 200;
	else if (uid == 3)
		trkid += 300;
	else if ((uid >= 100) && (uid <= 163)) {
		trkid += 400;
		vol = uid * 2 - 200;
	} else if ((uid >= 200) && (uid <= 263)) {
		trkid += 500;
		vol = uid * 2 - 400;
	} else if ((uid >= 300) && (uid <= 363)) {
		trkid += 600;
		vol = uid * 2 - 600;
	}

	atrk = atrk_find_trkid(msa, trkid, -1, 0, 0);
	if (!atrk)
		return 1;

	if (vol > ATRK_VOL_MAX)
		vol = ATRK_VOL_MAX;
	rate = msa->srcrate;
	bits = 12;
	chnl = 1;

	/*
	 * read header of new track. NOTE: subchunks aren't 16bit aligned!
	 */
	if (atrk->state == STATE_UNUSED) {
		if (size < 24)
			return 1;
		cid = le32_to_cpu(ua32(src + 0));
		if (cid != iMUS)
			return 1;

		cid = le32_to_cpu(ua32(src + 8));
		mapsz = be32_to_cpu(ua32(src + 12));

		size -= 16;
		src += 16;

		if (cid != MAP_ || mapsz > size)
			return 1;

		/* the MAP_ chunk again has a few subchuks, need the FRMT tag */
		while (mapsz > 7 && size > 7) {
			cid = le32_to_cpu(ua32(src + 0));
			csz = be32_to_cpu(ua32(src + 4));

			size -= 8;
			mapsz -= 8;
			src += 8;

			if (cid == FRMT) {
				bits = be16_to_cpu(ua16(src + 10));
				rate = be16_to_cpu(ua16(src + 14));
				chnl = be16_to_cpu(ua16(src + 18));
			}

			src += csz;
			size -= csz;
			mapsz -= csz;
		}

		/* now there should be "DATA" with the TOTAL len of sound of the whole track */
		if (size < 8)
			return 1;
		cid = le32_to_cpu(ua32(src + 0));
		csz = be32_to_cpu(ua32(src + 4));
		src += 8;
		size -= 8;
		if (cid != DATA)
			return 1;

		atrk->state = STATE_BLOCKED;
		atrk->trkid = trkid;
		atrk->dataleft = csz;
		atrk_set_srcfmt(atrk, rate, bits, chnl, vol, 0);
		atrk_set_default_strk(atrk, csz);
		atrk_process_strk(atrk);
	}

	if (atrk->state < STATE_BLOCKED)
		return 1;
	if (size > atrk->dataleft)
		size = atrk->dataleft;
	atrk_read_pcmsrc(atrk, size, src);
	if ((atrk->state < STATE_MIXABLE) &&
	    ((atrk->dataleft < 1) || (atrk->dstfavail >= msa->audminframes))) {
		atrk->state = STATE_MIXABLE;
	}
	return 0;
}

static void iact_audio_scaled(struct sanctx *ctx, uint32_t size, uint8_t *src)
{
	uint8_t v1, v2, v3, *src2, *ib = ctx->rt.iactbuf;
	struct sanatrk *atrk = &(ctx->msa->atrk[0]);
	uint16_t count, len;
	int16_t *dst;

	if (atrk->state < STATE_MIXABLE) {
		/* Droidworks is the only valid user of 11kHz; COMI uses 22kHz,
		 * Fortunately, they can be identified using the video framerate.
		 * COMI uses 12fps, while Droidworks uses 15fps.
		 */
		if ((ctx->rt.framedur == 1000000 / 12) && (ctx->msa->srcrate == 11025))
			ctx->msa->srcrate = 22050;
		atrk_set_srcfmt(atrk, ctx->msa->srcrate, 16, 2, ATRK_VOL_MAX, 0);
		atrk->state = STATE_MIXABLE;
	}

	/* LECSMUSH.DLL 10002030 */
	while (size > 0) {
		if (ctx->rt.iactpos >= 2) {
			len = be16_to_cpu(*(uint16_t *)ib) + 2 - ctx->rt.iactpos;
			if (len > size) {  /* continued in next IACT chunk. */
				if (ctx->rt.iactpos + size > SZ_IACT)
					return;
				memcpy(ib + ctx->rt.iactpos, src, size);
				ctx->rt.iactpos += size;
				size = 0;
			} else {
				if (ctx->rt.iactpos + len > SZ_IACT)
					return;
				memcpy(ib + ctx->rt.iactpos, src, len);
				dst = (int16_t *)ctx->adstbuf1;
				src2 = ib + 2;
				v1 = *src2++;
				v2 = v1 >> 4;
				v1 &= 0x0f;
				count = 1024 * 2;
				do {
					v3 = *src2++;
					if (v3 == 0x80) {
						*dst++ = cpu_to_le16(src2[0] << 8 | src2[1]);
						src2 += 2;
					} else {
						*dst++ = cpu_to_le16((int8_t)v3) << ((count & 1) ? v1 : v2);
					}
				} while (--count);
				atrk->dataleft += 4096;
				atrk->playlen += 4096;
				atrk_read_pcmsrc(atrk, 4096, ctx->adstbuf1);
				size -= len;
				src += len;
				ctx->rt.iactpos = 0;
			}
		} else {
			if (size > 1 && ctx->rt.iactpos == 0) {
				*ib = *src++;
				ctx->rt.iactpos++;
				size--;
			}
			*(ib + ctx->rt.iactpos) = *src++;
			ctx->rt.iactpos++;
			size--;
		}
	}
}

/* JKM.EXE 005656a3, LECSMUSH.DLL 100018d0 */
static void iact_pal_do_crossfade(struct sanctx *ctx)
{
	struct sanrt *rt = &ctx->rt;
	uint16_t elapsed, remaining;
	uint32_t *pal, *ipa;
	int v[3], w[3];

	if (rt->iactpal1 == 0)
		return;
	elapsed = rt->currframe - rt->iactpalfrme;
	remaining = rt->iactpal2 - elapsed;
	pal = rt->palette;
	ipa = rt->iactpal;

	for (int i = 0; i < 256; i++) {
		v[0] = (*pal >>  0) & 0xff;
		v[1] = (*pal >>  8) & 0xff;
		v[2] = (*pal >> 16) & 0xff;
		w[0] = (*ipa >>  0) & 0xff;
		w[1] = (*ipa >>  8) & 0xff;
		w[2] = (*ipa >> 16) & 0xff;
		for (int j = 0; j < 3; j++)
			v[j] = (((v[j] * remaining) + (w[j] * elapsed)) / rt->iactpal2);

		*pal++ = 0xffU << 24 | v[2] << 16 | v[1] << 8 | v[0];
		ipa++;
	}
	rt->iactpal1--;
	if (rt->iactpal1 == 0) {
		memcpy(rt->palette, rt->iactpal, 768);
	}
}

static void handle_IACT(struct sanctx *ctx, uint32_t size, uint8_t *src)
{
	uint16_t p[7];
	int i, ret;

	for (i = 0; i < 7; i++)
		p[i] = le16_to_cpu(*(uint16_t*)(src + (i<<1)));

	if (p[0] == 8 && p[1] == 46) {

		if (p[3] == 0) {
			/* subchunkless scaled IACT audio codec47/48 videos,
			 * LECSMUSH.DLL 10001a50
			 */
			if (ctx->io->flags & SANDEC_FLAG_NO_AUDIO)
				return;
			iact_audio_scaled(ctx, size - 18, src + 18);
		} else {

			if ((ctx->rt.iact8c4x == 0) && (ctx->rt.iactimus > 0)) {
				if (ctx->io->flags & SANDEC_FLAG_NO_AUDIO)
					ret = 1;
				else
					ret = iact_audio_imuse(ctx->msa, size - 18, src + 18, p[4], p[3]);

				if (ctx->rt.iactimus == 2) {
					ctx->rt.iactimus = ret ? 0 : 1;
				} else if (ctx->rt.iactimus == 1)
					return;
			}

			if ((ctx->rt.iactimus == 0) && (ctx->rt.iact8c4x) &&
			    (p[3] > 0) && (p[3] < 6)) {
				/* palette crossfading: p[3] == number of steps,
				 * see JKM.EXE 10565deb, LECSMUSH.DLL 10001ba4
				 */

				memcpy(ctx->rt.iactpal, ctx->rt.palette, SZ_PAL);
				if (size >= 768) {
					uint32_t *ip = ctx->rt.iactpal;
					for (int i = 0; i < 256; i++) {
						*ip++ = 0xffU | src[0] << 16 | src[1] << 8 | src[2];
						src += 3;
					}
					ctx->rt.iactpal1 = ctx->rt.iactpal2 = p[3];
					ctx->rt.iactpalfrme = ctx->rt.currframe;
				}
			}
		}
	}
}

static void handle_SAUD(struct sanatrk *atrk, const uint16_t rate)
{
	uint32_t cid, csz, size = atrk_bufbytes(atrk);
	uint8_t *src = atrk->data;

	if ((atrk->state > STATE_HEADER) || (size < 16))
		return;

	/* need to find: SAUD____STRK____<ssss>SDAT____  */
	cid = le32_to_cpu(ua32(src + 0));
	if (cid != SAUD)
		return;

	src += 8;
	size -= 8;
	cid = le32_to_cpu(ua32(src + 0));
	csz = be32_to_cpu(ua32(src + 4));
	if (cid != STRK)
		return;

	src += 8;
	size -= 8;
	/* don't copy unless the source has space for the SDAT____ as well */
	if ((csz > size + 9) || (csz >= ATRK_MAX_STRK_SIZE))
		return;

	memcpy(atrk->strk, src, csz);
	atrk->strksz = csz;
	src += csz;
	size -= csz;

	cid = le32_to_cpu(ua32(src + 0));	/* SDAT */
	csz = be32_to_cpu(ua32(src + 4));	/* total PCM bytes */
	if (cid != SDAT)
		return;

	src += 8;
	size -= 8;

	atrk->state = (atrk->maxidx == 1) ? STATE_MIXABLE : STATE_BLOCKED;

	/* Move the remaining PCM data to the start of the buffer */
	size = (size > csz) ? csz : size;
	memmove(atrk->data, src, size);
	atrk->wrptr = size;
	atrk->dataleft = csz - size;

	atrk_set_srcfmt(atrk, rate, 8, 1, atrk->vol, atrk->pan);
	atrk_process_strk(atrk);
	atrk_update_dstframes_avail(atrk);
}

static void handle_PSAD(struct sanctx *ctx, uint32_t size, uint8_t *src, uint8_t v1flag)
{
	uint32_t tid, idx, vol, pan, mid, flg;
	struct sanmsa *msa = ctx->msa;
	struct sanatrk *atrk;

	if (ctx->io->flags & SANDEC_FLAG_NO_AUDIO)
		return;

	if (size > (_ATRK_DATSZ / 2))
		return;

	/* dig.exe 4332f */
	if ((src[0] == 0) && (src[1] == 0) && (src[4] == 0) &&
	    (src[5] == 0) && (src[8] == 0) && (src[9] == 0)) {
		/* PSADv1, as in Rebel Assault 1 / ANIMv1 */
		tid = be32_to_cpu(ua32(src + 0));
		idx = be32_to_cpu(ua32(src + 4));
		mid = be32_to_cpu(ua32(src + 8));
		flg = v1flag;
		vol = ATRK_VOL_MAX;	/* maximum */
		pan = 0;		/* centered */
		src += 12;
		size -= 12;
	} else {
		/* PSADv2, as in Rebel Assault 2+ / ANIMv2 */
		tid = le16_to_cpu(ua16(src + 0));
		idx = le16_to_cpu(ua16(src + 2));
		mid = le16_to_cpu(ua16(src + 4));
		flg = le16_to_cpu(ua16(src + 6));
		vol = src[8];

		if (vol > ATRK_VOL_MAX)
			vol = ATRK_VOL_MAX;

		/* RA2 mono sound supposed to be played on both channels.
		 * used for the music mostly. Set pan to zero since the source
		 * is expanded to both channels if necessary when read.
		 */
		if (src[9] == 0x80)
			pan = 0;
		else
			pan = (int8_t)src[9];

		src += 10;
		size -= 10;
	}

	if (idx == 0) {
		atrk = atrk_find_trkid(msa, tid, -1, 0, 0);
		if (!atrk)
			return;		/* too many active tracks, bail */
		/* RA1 sometimes has identical TIDs for different tracks */
		if (atrk->state != STATE_UNUSED) {
			atrk = atrk_find_trkid(msa, tid, 0, mid, 0);
			if (!atrk)
				return;
		}
		atrk->trkid = tid;
		atrk->maxidx = mid;
		atrk->flags = flg & SAUD_FLAG_TRK_MASK;
		atrk->state = STATE_HEADER;
	} else {
		atrk = atrk_find_trkid(ctx->msa, tid, idx, mid, 1);
		if (!atrk || (atrk->state < STATE_HEADER))
			return;
	}

	atrk->curridx = idx;
	atrk->pan = pan;
	atrk->vol = vol;
	if (atrk->state > STATE_BLOCKED) {
		if (size > atrk->dataleft)
			size = atrk->dataleft;
	}
	atrk_read_pcmsrc(atrk, size, src);
	if (atrk->state < STATE_BLOCKED)
		handle_SAUD(atrk, msa->srcrate);

	if (atrk->state == STATE_BLOCKED) {
		if ((atrk->dataleft < 1) || (atrk->maxidx < 2)
		    || (atrk->dstfavail >= ctx->msa->audminframes)) {
			atrk->state = STATE_MIXABLE;
		}
	}
}

static int handle_IMA4(struct sanctx *ctx, uint32_t size, uint8_t *src,
			uint32_t samples, int ch)
{
	int16_t *dst = (int16_t *)ctx->adstbuf1;
	struct sanatrk *atrk = &(ctx->msa->atrk[0]);
	int i, j, nibsel, tblidx, adpcm_step, dat, delt;
	uint8_t in, nib;

	dat = 0;
	nibsel = 0;
	in = 0;

	if (size < 3)
		return 89;
	dat = (int16_t)le16_to_cpu(ua16(src));
	src += 2;
	tblidx = *src++;
	size -= 3;
	i = 0;
	/* this is IMA ADPCM QT */
	while ((size > 0) && (i < samples)) {
		if (nibsel == 0) {
			if (size < 1)
				break;
			in = *src++;
			nib = in >> 4;
			size--;
		} else {
			nib = in & 0x0f;
		}
		nibsel = !nibsel;

		if (tblidx < 0)
			tblidx = 0;
		else if (tblidx > 88)
			tblidx = 88;
		adpcm_step = adpcm_step_table[tblidx];
		tblidx += adpcm_index_table[nib];
		delt = adpcm_step >> 3;
		if (nib & 4)
			delt += adpcm_step;
		if (nib & 2)
			delt += (adpcm_step >> 1);
		if (nib & 1)
			delt += (adpcm_step >> 2);
		if (nib & 8)
			dat -= delt;
		else
			dat += delt;

		if (dat < -0x8000)
			dat = -0x8000;
		else if (dat > 0x7fff)
			dat = 0x7fff;

		for (j = 0; j < ch; j++)
			*dst++ = dat;

		i++;
	}
	atrk->playlen += i * 2 * ch;
	atrk->dataleft += i * 2 * ch;
	atrk_read_pcmsrc(atrk, i * 2 * ch, ctx->adstbuf1);
	return 0;
}

static int handle_VIMA(struct sanctx *ctx, uint32_t size, uint8_t *src)
{
	int i, j, v1, data, ch, inbits, numbits, bitsize;
	struct sanatrk *atrk = &(ctx->msa->atrk[0]);
	int  hibit, lobits, tblidx, idx2, delt;
	int16_t startdata[2], *dst;
	uint8_t startpos[2];
	uint32_t samples, sig;

	if (size < 16)
		return 86;

	if (atrk->state < STATE_MIXABLE) {
		atrk_set_srcfmt(atrk, 22050, 16, 2, ATRK_VOL_MAX, 0);
		atrk->state = STATE_MIXABLE;
	}

	samples = be32_to_cpu(ua32(src));
	src += 4;
	size -= 4;
	if ((int32_t)samples < 0) {
		samples = be32_to_cpu(ua32(src + 4));
		src += 8;
		size -= 8;
	}

	ch = 1;
	startpos[0] = *src++;
	size--;
	if (startpos[0] & 0x80) {
		startpos[0] = ~startpos[0];
		ch = 2;
	}

	startdata[0] = be16_to_cpu(ua16(src));
	src += 2;
	size -= 2 ;
	if (ch > 1) {
		startpos[1] = *src++;
		size--;
		startdata[1] = be16_to_cpu(ua16(src));
		src += 2;
		size -= 2;
	}

	if ((samples * 2 * ch) > SZ_ADSTBUF)
		return 87;

	/* here could be IMA4 */
	sig = be32_to_cpu(ua32(src));
	if (sig == IMA4) {
		src += 4;
		size -= 4;
		handle_IMA4(ctx, size, src, samples, ch);
		return 0;
	}

	inbits = be16_to_cpu(ua16(src));
	src += 2;
	size -= 2;
	numbits = 0;

	memset(ctx->adstbuf1, 0, samples * 2 * ch);
	for (i = 0; i < ch; i++) {
		dst = (int16_t *)ctx->adstbuf1 + i;
		tblidx = startpos[i];
		data = startdata[i];

		for (j = 0; j < samples; j++) {
			bitsize = vima_size_table[tblidx];
			numbits += bitsize;
			hibit = 1 << (bitsize - 1);
			lobits = hibit - 1;
			v1 = (inbits >> (16 - numbits)) & (hibit | lobits);

			if (numbits > 7) {
				if (!size)
					break;
				inbits = ((inbits & 0xff) << 8) | *src++;
				numbits -= 8;
				size--;
			}

			if (v1 & hibit)
				v1 ^= hibit;
			else
				hibit = 0;

			if (v1 == lobits) {
				data = ((int16_t)(inbits << numbits) & 0xffffff00);
				inbits = ((inbits & 0xff) << 8) | *src++;
				data |= ((inbits >> (8 - numbits)) & 0xff);
				inbits = ((inbits & 0xff) << 8) | *src++;
				size -= 2;
			} else {
				idx2 = (v1 << (7 - bitsize)) | (tblidx << 6);
				delt = ctx->vima_pred_tbl[idx2];

				if (v1)
					delt += (adpcm_step_table[tblidx] >> (bitsize - 1));
				if (hibit)
					delt = -delt;

				data += delt;
				if (data < -0x8000)
					data = -0x8000;
				else if (data > 0x7fff)
					data = 0x7fff;
			}

			*((uint16_t *)dst) = data;
			dst += ch;

			tblidx += vima_itbls[bitsize - 2][v1];
			if (tblidx < 0)
				tblidx = 0;
			else if (tblidx > (ADPCM_STEP_COUNT - 1))
				tblidx = ADPCM_STEP_COUNT - 1;

		}
		if (!size)
			break;
	}
	atrk->playlen += samples * 2 * ch;
	atrk->dataleft += samples * 2 * ch;
	atrk_read_pcmsrc(atrk, samples * 2 * ch, ctx->adstbuf1);
	return 0;
}

/* subtitles: index of message in the Outlaws LOCAL.MSG file, 10000 - 12001.
 * As long as subid is set to non-zero, the subtitle needs to be overlaid
 * over the image.  The chunk also provides hints about where to place
 * the subtitle, which we ignore here.
 */
static void handle_TRES(struct sanctx *ctx, uint32_t size, uint8_t *src)
{
	uint16_t *tres = (uint16_t *)src;
	ctx->rt.subid = size >= 10 ? le16_to_cpu(tres[8]) : 0;
}

static void handle_STOR(struct sanctx *ctx, uint32_t size, uint8_t *src)
{
	/* STOR stores the next FOBJ to the anm_overlay_buf, to be replayed
	 * later.  The SMUSH engines can store all objects except IACT, but on
	 * FTCH the data is only passed to fob_decode_render() so we just store
	 * the FOBJs.
	 */
	ctx->rt.to_store = 1;

	/* For posterity: in RA1, if src[0] == 3, the engine does the following:
	 *  - copy fobj header (22 bytes) to anm_overlay_buf
	 *  - fob_decode_render(anm_overlay_buf+22, src, ..., anm_flags | 8)
	 *     the 0x08 flag is a shortcut to a codec3 decoder which can only
	 *     handle 320x200 images but faster than the arbitrary-size one.
	 *  - change the fobj header to codec20, 320x200
	 * Probably a good perf improvement on 1992/1993 PC hardware.
	 */
}

static void handle_FADE(struct sanctx *ctx, uint32_t size, uint8_t *src)
{
	uint32_t csz, cid, count, remaining;
	uint8_t *vga = ctx->rt.buf5;
	int32_t dstoff, dstoff2;
	int16_t fadestride;
	int8_t c;

	if (size < 8)
		return;

	/* FDHD header first */
	cid = le32_to_cpu(ua32(src + 0));
	csz = be32_to_cpu(ua32(src + 4));
	size -= 8;
	src += 8;
	if ((csz > size) || (csz < 8) || (cid != FDHD))
		return;
	/* 2 unknown 16bit values, xres, yres in the header */
	fadestride = le16_to_cpu(*(uint16_t *)(src + 4));	/* xres, 320 */

	/* followed by FFRM chunk with the actual copy instructions */
	size -= csz;
	src += csz;
	if (size < 8)
		return;
	cid = le32_to_cpu(ua32(src + 0));
	csz = be32_to_cpu(ua32(src + 4));
	size -= 8;
	src += 8;
	if ((csz > size) || (cid != FFRM))
		return;

	dstoff = 0;
	while (size > 0) {
		c = *src++;
		size--;

		if ((c & 0x7f) == 0) {
			if (size < 2)
				break;
			count = le16_to_cpu(ua16(src));
			src += 2;
			size -= 2;
		} else {
			count = c & 0x7f;
		}

		if (c & 0x80) {
			dstoff += count;
		} else {
			remaining = count;
			while (remaining > 0) {
				/* translate from source stride to our buffer stride */
				uint32_t x = dstoff % fadestride;
				uint32_t y = dstoff / fadestride;
				uint32_t can_copy = fadestride - x;
				uint32_t chunk = (remaining < can_copy) ? remaining : can_copy;
				dstoff2 = (y * ctx->rt.bufw) + x;
				memcpy(vga + dstoff2, ctx->rt.fbuf + dstoff2, chunk);
				dstoff += chunk;
				remaining -= chunk;
			}
		}
	}

	ctx->rt.vbuf = ctx->rt.buf5;
	return;
}

static int handle_FTCH(struct sanctx *ctx, uint32_t size, uint8_t *src, uint16_t anm_flags)
{
	uint8_t *vb = ctx->rt.buf3;
	int16_t xoff, yoff;
	uint32_t sz;
	int ret;

	if ((size == 6) && (ctx->rt.version == 2)) {
		xoff = le16_to_cpu(*(int16_t *)(src + 2));
		yoff = le16_to_cpu(*(int16_t *)(src + 4));
	} else if ((size == 12) && (ctx->rt.version < 2)) {
		xoff = (int16_t)be32_to_cpu(ua32(src + 4));
		yoff = (int16_t)be32_to_cpu(ua32(src + 8));
		/* RA1 also does anm_flags |= 0x800, but this only disables the
		 * application of camera shake offsets, which we don't support.
		 */
	} else {
		xoff = yoff = 0;
	}

	ret = 0;
	sz = *(uint32_t *)(vb + 0);
	if (sz > 0 && sz <= ctx->rt.fbsize) {
		ret = fob_decode_render(ctx, ctx->rt.fbuf, vb + 4, sz, xoff, yoff, anm_flags, 1, 0);
	}
	ctx->rt.can_ipol = 0;
	if (ret == 0)
		ctx->rt.have_frame = 1;
	return ret;
}

static void handle_GOST(struct sanctx *ctx, uint32_t size, uint8_t *src, uint16_t anm_flags)
{
	int16_t xoff, yoff;

	if ((0 == ctx->rt.last_fobj) || (0 == ctx->rt.last_fobj_size))
		return;

	if (size == 12) {
		/* ASSAULT.EXE 18e7d, for FNFINAL.ANM and a few others */
		uint32_t cmd = be32_to_cpu(ua32(src + 0));
		xoff = (int16_t)(be32_to_cpu(ua32(src + 4)));
		yoff = (int16_t)(be32_to_cpu(ua32(src + 8)));

		if (cmd == 28)
			anm_flags |= ANM_FLAG_FLIPX;
		else if (cmd == 29)
			anm_flags |= ANM_FLAG_FLIPY;
		else if (cmd == 30)
			anm_flags |= ANM_FLAG_FLIPX | ANM_FLAG_FLIPY;
	} else if (size == 6) {
		/* RA2.EXE 37596 */
		uint16_t cmd = le16_to_cpu(*(uint16_t *)(src + 0));
		xoff = (int16_t)(le16_to_cpu(*(uint16_t *)(src + 2)));
		yoff = (int16_t)(le16_to_cpu(*(uint16_t *)(src + 4)));
		if (cmd == 0)
			anm_flags |= ANM_FLAG_FLIPX;
		else if (cmd == 1)
			anm_flags |= ANM_FLAG_FLIPY;
		else if (cmd == 2)
			anm_flags |= ANM_FLAG_FLIPX | ANM_FLAG_FLIPY;
	} else
		return;

	fob_decode_render(ctx, ctx->rt.fbuf, ctx->rt.last_fobj, ctx->rt.last_fobj_size,
			  xoff, yoff, anm_flags, 1, 0);
}

/* allocate memory for a full FRME */
static int allocfrme(struct sanctx *ctx, uint32_t sz)
{
	if (sz > FRME_MAX_SIZE)	/* cap at 4MB */
		return 99;
	sz = (sz + 4095) & ~4095U;
	if (sz > ctx->rt.frmebufsz) {
		if (ctx->rt.fcache)
			free(ctx->rt.fcache);
		ctx->rt.fcache = (uint8_t *)malloc(sz);
		if (!ctx->rt.fcache) {
			ctx->rt.frmebufsz = 0;
			return 1;
		}
		ctx->rt.frmebufsz = sz;
	}
	return 0;
}

static int handle_FRME(struct sanctx *ctx, uint32_t size)
{
	struct sanrt *rt = &ctx->rt;
	uint32_t cid, csz;
	uint16_t anm_flags;
	uint8_t *src;
	int ret;

	ret = allocfrme(ctx, size);
	if (ret)
		return ret;

	src = rt->fcache;
	if (read_source(ctx, src, size))
		return 14;

	ret = 0;
	anm_flags = rt->def_anm_flags;

	if ((rt->version < 3) && (0 == (anm_flags & ANM_FLAG_SKIP_CLR_DST))) {
		memset(rt->fbuf, 0, rt->fbsize ? rt->fbsize : FOBJ_MAXX * FOBJ_MAXY);
	}

	rt->last_fobj = NULL;
	rt->last_fobj_size = 0;
	while ((size > 7) && (ret == 0)) {

		/* some blocks like IACT have odd size, and RA1 L2PLAY.ANM
		 * has a few unaligned (not at 2 byte boundary) FOBJs.
		 * This is how the smush game engine deals with that.
		 */
		if (((uintptr_t)src & 1) && (*src == 0)) {
			src++;
			size--;
		}

		cid = le32_to_cpu(ua32(src + 0));
		csz = be32_to_cpu(ua32(src + 4));

		src += 8;
		size -= 8;

		if (csz > size)
			return 15;

		if (rt->version > 2) {
			switch (cid) {
			case WAVE: ret = handle_VIMA(ctx, csz, src); break;
			case BL16: handle_BL16(ctx, csz, src); break;
			default:   ret = 0;
			}
		} else {
			switch (cid) {
			case NPAL: handle_NPAL(ctx, csz, src); break;
			case FOBJ: ret = handle_FOBJ(ctx, csz, src, 0, 0, anm_flags); break;
			case GOST: handle_GOST(ctx, csz, src, anm_flags); break;
			case IACT: handle_IACT(ctx, csz, src); break;
			case TRES: handle_TRES(ctx, csz, src); break;
			case STOR: handle_STOR(ctx, csz, src); break;
			case FTCH: ret = handle_FTCH(ctx, csz, src, anm_flags); break;
			case XPAL: handle_XPAL(ctx, csz, src); break;
			case PVOC: handle_PSAD(ctx, csz, src, SAUD_FLAG_TRK_VOICE); break;
			case PSD2: handle_PSAD(ctx, csz, src, SAUD_FLAG_TRK_SFX);   break;
			case PSAD: handle_PSAD(ctx, csz, src, SAUD_FLAG_TRK_MUSIC); break;
			case FADE: handle_FADE(ctx, csz, src); break;
			default:   ret = 0;		/* unknown chunk, ignore */
			}
		}
		src += csz;
		size -= csz;
	}

	/* OK case: all usable bytes of the FRME read, no errors */
	rt->currframe++;
	if (ret == 0) {
		/* resample+mix audio track(s) and queue it up */
		if (!(ctx->io->flags & SANDEC_FLAG_NO_AUDIO) && (ctx->msa))
			aud_mix_tracks(ctx);

		if (rt->version > 2) {
			ctx->io->queue_video(ctx->io->userctx, rt->vbuf,
					     rt->pitch * rt->bufh * 1,
					     rt->bufw, rt->bufh, rt->pitch,
					     NULL, 0, rt->framedur);
			return 0;
		}

		if (rt->iact8c4x) {
			iact_pal_do_crossfade(ctx);
		}

		if (ctx->rt.have_frame) {
			/* if possible, interpolate a frame using the itable,
			 * and queue that plus the decoded one.
			 */
			if (ctx->io->flags & SANDEC_FLAG_DO_FRAME_INTERPOLATION
			    && rt->have_itable
			    && rt->can_ipol) {
				interpolate_frame(rt->buf5, rt->buf4, rt->vbuf,
						  rt->c47ipoltbl, rt->bufw, rt->bufh);
				rt->have_ipframe = 1;
				rt->can_ipol = 0;
				memcpy(rt->buf4, rt->vbuf, rt->bufw * rt->bufh * 1);
				ctx->io->queue_video(ctx->io->userctx, rt->buf5,
					     rt->pitch * rt->bufh,
					     rt->bufw, rt->bufh, rt->pitch, rt->palette,
					     rt->subid, rt->framedur / 2);
			} else {
				ctx->io->queue_video(ctx->io->userctx, rt->vbuf,
					     rt->pitch * rt->bufh * 1,
					     rt->bufw, rt->bufh, rt->pitch, rt->palette,
					     rt->subid, rt->framedur);
				/* save frame as possible interpolation source */
				if (rt->have_itable)
					memcpy(rt->buf4, rt->vbuf, rt->bufw * rt->bufh * 1);
			}
		}


		rt->subid = 0;
		rt->have_frame = 0;
	}

	return ret;
}

static void vima_init(struct sanctx *ctx)
{
	int i, j, k, l, m, n;

	for (i = 0; i < 64; i++) {
		for (j = 0, k = i; j < ADPCM_STEP_COUNT; j++, k += 64) {
			n = 0;
			l = adpcm_step_table[j];
			for (m = 32; m != 0; m >>= 1) {
				if (i & m)
					n += l;
				l >>= 1;
			}
			ctx->vima_pred_tbl[k] = n;
		}
	}
}

static void sandec_free_memories(struct sanctx *ctx)
{
	if (ctx->msa) {
		free(ctx->msa);
		ctx->msa = NULL;
	}
	/* delete existing FRME buffer */
	if (ctx->rt.fcache)
		free(ctx->rt.fcache);
	/* delete work + video buffers, iactbuf is entry point */
	if (ctx->rt.membase)
		free(ctx->rt.membase);
	memset(&ctx->rt, 0, sizeof(struct sanrt));
}

static int sandec_alloc_msa(struct sanmsa **msa_out, uint8_t num_trks,
			    uint32_t audminframes)
{
	struct sanmsa *msa;
	uint32_t mem, grsb;
	uint8_t *m;
	int i;

	/* ANM audio track buffers, resample structures */
	mem = sizeof(struct sanmsa);
	/* channels buffer memories */
	mem += num_trks * ATRK_DATSZ;

	/* resampling/mixing buffer */
	grsb = 4 * audminframes;
	mem += grsb;

	m = malloc(mem);
	if (!m)
		return 1;
	memset(m, 0, mem);

	msa = (struct sanmsa *)m;
	m += sizeof(struct sanmsa);

	msa->numtrk = num_trks;
	msa->audminframes = audminframes;

	/* PSAD/iMUS audio track buffers */;
	for (i = 0; i < msa->numtrk; i++) {
		msa->atrk[i].msa = msa;
		msa->atrk[i].data = m;
		m += ATRK_DATSZ;
	}
	msa->audrsb1 = (uint8_t *)m;	m += grsb;

	atrk_init_volumes(msa);
	*msa_out = msa;
	return 0;
}

static int sandec_alloc_vidmem(struct sanctx *ctx, const uint16_t maxx,
			        const uint16_t maxy, const int sanm)
{
	uint32_t vmem, cmem, mem, gb;
	struct sanrt *rt = &ctx->rt;
	uint8_t *m;

	mem = 0;
	vmem = 0;

	/* codec buffers: 43 lines (max. c47 mv) top + bottom as guard bands
	 * for stray motion vectors.
	 */
	cmem = (sanm ? 2 : 1) * maxx * (maxy + 88);
	cmem = (cmem + 63) & ~63;		/* 4k align */
	gb = (sanm ? 2 : 1) * 43 * maxx;	/* guard band size */
	gb = (gb + 63) & ~63;			/* align */

	/* we need 3 private buffers for codec37/47/48 and bl16 */
	mem += (cmem * 3);

	/* BL16 uses its own active buffer to display the image, since
	 * there are no other codecs which can modify its image.
	 * For ANM however we need: 1 image buffer for codecs1-33,
	 * STOR buffer and 2 image buffers to interpolate frames,
	 * plus space for the PSAD/iMUS audio tracks.
	 */
	if (!sanm) {
		/* Palettes/Interpolation Table/IACT-scaled-audio buf */
		mem += SZ_ANMBUFS;
		/* ANM aux buffers: fbuf, buf3/4/5 */
		vmem = maxx * maxy;
		vmem = (vmem + 63) & ~63;		/* align */
		mem += vmem * 4;
		/* STOR buffer c20 FOBJ header + data size */
		mem += 32;
		mem = (mem + 4095) & ~4095;
	}

	/* allocate memory for work buffers */
	m = (uint8_t *)malloc(mem);
	if (!m)
		return 1;
	rt->membase = m;
	memset(m, 0, mem);

	/* align start to cacheline size */
	m = (uint8_t *)((((uintptr_t)m) + 63) & ~63);

	if (!sanm) {
		/* ANIM/ANM misc buffers */
		rt->iactbuf = (uint8_t *)m;	m += SZ_IACT;
		rt->palette = (uint32_t *)m; 	m += SZ_PAL;
		rt->iactpal = (uint32_t *)m;	m += SZ_PAL;
		rt->deltapal = (int16_t *)m;	m += SZ_DELTAPAL;
		rt->shiftpal = (int16_t *)m;	m += SZ_SHIFTPAL;
		rt->c47ipoltbl = (uint8_t *)m;	m += SZ_C47IPTBL;

		/* set up video buffers for ANM */
		m = (uint8_t *)((((uintptr_t)m) + 63) & ~63);
		rt->fbuf = m;			/* front buffer 		*/
		rt->buf0 = rt->fbuf + vmem;	/* codec37/47/48/bl16 main buf	*/
		rt->buf1 = rt->buf0 + cmem;	/* delta buf 2 (c47/bl16),	*/
		rt->buf2 = rt->buf1 + cmem;	/* delta buf 1 (c37/47/48/bl16)	*/
		rt->buf4 = rt->buf2 + cmem;	/* interpolated frame buffer	*/
		rt->buf5 = rt->buf4 + vmem;	/* interpolation last frame buf */
		rt->buf3 = rt->buf5 + vmem;	/* STOR buffer			*/
	} else {
		/* set up buffers for SNM: 3 video buffers for BL16 */
		rt->fbuf = NULL;
		rt->buf0 = m;			/* bl16 main 			*/
		rt->buf1 = rt->buf0 + cmem;	/* bl16 delta buffer 1		*/
		rt->buf2 = rt->buf1 + cmem;	/* bl16 delta buffer 2		*/
	}
	/* offset the starts of the codec buffers with guard bands, so
	 * each one has 43 lines before start and after end to account
	 * for stray/invalid motion vectors.
	 */
	rt->buf0 += gb;
	rt->buf1 += gb;
	rt->buf2 += gb;

	return 0;
}

static int handle_AHDR(struct sanctx *ctx, uint32_t size)
{
	struct sanrt *rt = &ctx->rt;
	uint32_t maxframe, audminframes, srate;
	uint8_t *ahbuf, fps;

	if (size > 794)
		return 14;
	if (size < 774)
		return 15;

	ahbuf = (uint8_t *)malloc(size);
	if (!ahbuf)
		return 11;
	if (read_source(ctx, ahbuf, size)) {
		free(ahbuf);
		return 12;
	}

	rt->version = le16_to_cpu(*(uint16_t *)(ahbuf + 0));
	rt->FRMEcnt = le16_to_cpu(*(uint16_t *)(ahbuf + 2));

	if (0 != sandec_alloc_vidmem(ctx, FOBJ_MAXX, FOBJ_MAXY, 0)) {
		free(ahbuf);
		return 4;
	}

	read_palette(ctx, ahbuf + 6);	/* 768 bytes */

	if (rt->version > 1) {
		rt->framedur  =  le32_to_cpu(*(uint32_t *)(ahbuf + 6 + 768 + 0));
		fps = rt->framedur;
		maxframe =       le32_to_cpu(*(uint32_t *)(ahbuf + 6 + 768 + 4));
		srate	       = le32_to_cpu(*(uint32_t *)(ahbuf + 6 + 768 + 8));

		/* "maxframe" indicates the maximum size of one FRME object
		 * including chunk ID and chunk size in the stream (usually the first)
		 * plus 1 byte.
		 */
		if ((maxframe > 9) && (maxframe <= FRME_MAX_SIZE)) {
			if (allocfrme(ctx, maxframe)) {
				free(ahbuf);
				return 13;
			}
		}

		/* since IACT 8/x/x/>0 can be either TheDig Audio or something else,
		 * we initially assume imuse, since it can tell us when it is
		 * definitely not it.  The Dig uses IACT for its iMUSE audio only.
		 */
		rt->iactimus = 2;
	} else {
		fps = 15;			/* ANIMv1 default */
		srate = 11025;			/* ANIMv1 default */
		rt->frmebufsz = 0;
	}
	free(ahbuf);

	if ((fps < 10) || (fps > 15))
		fps = 15;
	rt->framedur = 1000000 / fps;	/* frame duration in microseconds */

	/* minimum number of samples to generate when resampling a stream to the
	 * output rate to supply enough data for the duration of a single frame.
	 */
	audminframes = ((ctx->adestrate / fps) + 1) & ~1U;

	/* for Full Throttle: the incoming audio data rate is not not enough
	 * to sustain click-free playback at the requested 10fps.  It starts
	 * to work at 10.3 fps; use 10.5fps since it divides the incoming sample-
	 * rate without a remainder and the 95ms frame time keeps good lip sync
	 * in all videos.
	 */
	if (fps < 11) {
		rt->framedur = 10000000 / 105;
		audminframes = (((ctx->adestrate * 10) / 105) + 1) & ~1U;
	}

	if (sandec_alloc_msa(&ctx->msa, ATRK_MAX, audminframes))
		return 14;
	ctx->msa->srcrate = srate;
	ctx->msa->destrate = ctx->adestrate;

	return 0;
}

static int handle_SHDR(struct sanctx *ctx, uint32_t csz)
{
	struct sanrt *rt = &ctx->rt;
	uint16_t maxx, maxy, t16;
	uint32_t c[2], sz, srate, achans;
	uint8_t *src, *sb;
	int ret;

	if (csz > 4096)
		return 57;

	/* even the odds */
	if (csz & 1)
		csz += 1;

	src = malloc(csz);
	if (!src)
		return 50;
	if (read_source(ctx, src, csz)) {
		free(src);
		return 51;
	}

	rt->version = 3;
	rt->FRMEcnt = le32_to_cpu(ua32(src + 2));
	rt->bufw = le16_to_cpu(*(uint16_t *)(src + 8));
	rt->bufh = le16_to_cpu(*(uint16_t *)(src + 10));
	rt->framedur = le32_to_cpu(ua32(src + 14));
	maxx = rt->bufw;
	maxy = rt->bufh;
	free(src);

	/* there's now >1kB of data left, no idea what it's for.
	 * at the end there should be FLHD
	 */
	if (read_source(ctx, c, 8)) {
		return 52;
	}
	if (c[0] != le32_to_cpu(FLHD)) {
		return 53;
	}
	sz = be32_to_cpu(c[1]);
	if (sz > 4096)
		return 58;
	src = malloc(sz);
	if (!src)
		return 54;
	if (read_source(ctx, src, sz)) {
		free(src);
		return 55;
	}

	/* FLHD has again a subchunks for video and audio format.
	 *  Audio is most interesting, but parse video as well in
	 *  case the SANM header and the BL16 chunk(s) disagree.
	 */
	ret = 0;
	sb = src;
	srate = 22050;		/* default */
	achans = 2;		/* default */
	while ((sz > 7) && (ret == 0)) {
		c[0] = le32_to_cpu(ua32(src + 0));
		c[1] = be32_to_cpu(ua32(src + 4));
		src += 8;
		sz -= 8;
		if (c[1] > sz)
			break;
		switch (c[0]) {
		case BL16:
			if (c[1] != 8)
				break;
			t16 = le16_to_cpu(*(uint16_t *)(src + 2));
			if (t16 > maxx)
				maxx = t16;
			t16 = le16_to_cpu(*(uint16_t *)(src + 4));
			if (t16 > maxy)
				maxy = t16;
			break;
		case WAVE:
			srate  = le32_to_cpu(ua32(src + 0));
			achans = le32_to_cpu(ua32(src + 4));
			if ((achans < 1) || (achans > 2))
				ret = 58;
			c[1] = 12;
			break;
		default:
			ret = 56;
		}
		if (c[1] & 1)
			c[1]++;

		sz -= c[1];
		src += c[1];
	}
	free(sb);

	if ((maxx > 1024) || (maxy > 768))
		return 59;
	if (0 != sandec_alloc_vidmem(ctx, maxx, maxy, 1))
		return 4;

	const uint32_t audminframes = (((srate * 10 * rt->framedur) / 1000000) + 1) & ~1U;
	if (sandec_alloc_msa(&ctx->msa, 1, audminframes))
		return 60;
	ctx->msa->srcrate = srate;
	ctx->msa->destrate = ctx->adestrate;

	rt->bufw = maxx;
	rt->bufh = maxy;
	rt->pitch = 2 * rt->bufw;

	vima_init(ctx);

	return ret;
}

/******************************************************************************/
/* public interface */

int sandec_decode_next_frame(void *sanctx)
{
	struct sanctx *ctx = (struct sanctx *)sanctx;
	uint32_t c[2];
	int ret, b;

	if (!ctx)
		return 1;
	/* in case of previous error, don't continue, just return it again */
	if (ctx->errdone)
		return ctx->errdone;

	/* interpolated frame: was queued first, now queue the decoded one */
	if (ctx->rt.have_ipframe) {
		struct sanrt *rt = &ctx->rt;
		rt->have_ipframe = 0;
		ctx->io->queue_video(ctx->io->userctx, rt->vbuf, rt->pitch * rt->bufh,
				     rt->bufw, rt->bufh, rt->pitch, rt->palette,
				     rt->subid, rt->framedur / 2);
		return SANDEC_OK;
	}

again:
	ret = read_source(ctx, c, 8);
	if (ret) {
		if (ctx->rt.currframe >= ctx->rt.FRMEcnt) {
			ret = SANDEC_DONE;	/* seems we reached file end */
			if (ctx->msa) {
				atrk_finish_all(ctx->msa);
				b = 1;
				while (b && atrk_count_active(ctx->msa, NULL) &&
					!(ctx->io->flags & SANDEC_FLAG_NO_AUDIO)) {
					b = aud_mix_tracks(ctx);
				}
			}
		}
		goto out;
	}

	c[1] = be32_to_cpu(c[1]);
	if (c[0] == FRME) {
		/* default case */
		ret = handle_FRME(ctx, c[1]);

	} else if (c[0] == ANNO) {
		/* some annotation, found esp. in Grim Fandango files. just skip it */
		uint8_t buf[128], rs;
		while (c[1] > 0) {
			rs = c[1] >= 128 ? 128 : c[1];
			ret = read_source(ctx, buf, rs);
			if (ret) {
				ret = 11;
				goto out;
			}
			c[1] -= rs;
		}
		goto again;

	} else if (atrk_count_active(ctx->msa, NULL) && !(ctx->io->flags & SANDEC_FLAG_NO_AUDIO)) {
		aud_mix_tracks(ctx);

	} else {
		ret = 10;
	}

out:
	ctx->errdone = ret;
	return ret;
}

int sandec_init(void **ctxout)
{
	struct sanctx *ctx;

	if (!ctxout)
		return 1;
	ctx = (struct sanctx *)malloc(sizeof(struct sanctx));
	if (!ctx)
		return 2;
	memset(ctx, 0, sizeof(struct sanctx));
	ctx->adstbuf1 = (uint8_t *)malloc(SZ_ADSTBUF);
	if (!ctx->adstbuf1) {
		free(ctx);
		return 2;
	}

	/* set to error state initially until a valid file has been opened */
	ctx->errdone = 44;

	c47_make_glyphs(&ctx->c47_glyph4x4[0][0], c47_glyph4_x, c47_glyph4_y, 4);
	c47_make_glyphs(&ctx->c47_glyph8x8[0][0], c47_glyph8_x, c47_glyph8_y, 8);
	ctx->adestrate = SANDEC_AUDIO_SRATE;
	*ctxout = ctx;

	return 0;
}

int sandec_open(void *sanctx, struct sanio *io)
{
	struct sanctx *ctx = (struct sanctx *)sanctx;
	struct sanatrk *atrk;
	uint32_t c[2];
	int ret;

	if (!io || !sanctx) {
		ret = 3;
		goto out;
	}
	ctx->io = io;

	sandec_free_memories(ctx);

	/* impossible value in case a FOBJ with param1 == 0 comes first */
	ctx->c4tblparam = 0xffff;

	/* files can either start with "ANIM____AHDR___", "SANM____SHDR____" or
	 * "SAUD____".
	 */
	ret = read_source(ctx, &c[0], 4 * 2);
	if (ret) {
		ret = 5;
		goto out;
	}
	if ((c[0] == ANIM) || (c[0] == SANM)) {
		ret = read_source(ctx, &c[0], 4 * 2);
		if (c[0] == AHDR) {
			ret = handle_AHDR(ctx, be32_to_cpu(c[1]));
		} else if (c[0] == SHDR) {
			ret = handle_SHDR(ctx, be32_to_cpu(c[1]));
		} else {
			ret = 7;
		}

	} else if (c[0] == SAUD) {
		uint32_t csz = be32_to_cpu(c[1]);
		ret = 8;
		if ((csz < 8) || (csz > (1 << 20))) {
			goto out;
		} else {
			if (sandec_alloc_msa(&ctx->msa, 1, ATRK_DEST_RATE / 15))
				goto out;
			if (csz > ATRK_DATSZ)
				goto out;
			atrk = atrk_find_trkid(ctx->msa, 1, 0, 1, 0);
			if (!atrk)
				goto out;
			memcpy(atrk->data, c, 8);
			if (read_source(ctx, atrk->data + 8, csz))
				goto out;
			atrk->vol = ATRK_VOL_MAX;
			atrk->pan = 0;
			atrk->wrptr = csz + 8;
			atrk->trkid = 1;
			atrk->maxidx = 1;
			ctx->msa->srcrate = 11025;
			ctx->msa->destrate = ctx->adestrate;
			handle_SAUD(atrk, ctx->msa->srcrate);
			ret = atrk_count_active(ctx->msa, NULL) > 0 ? 0 : 8;
		}

	} else {
		ret = 9;
	}

out:
	ctx->errdone = ret;
	return ret;
}

void sandec_exit(void **sanctx)
{
	struct sanctx *ctx;

	if (!sanctx)
		return;
	ctx = *(struct sanctx **)sanctx;
	if (!ctx)
		return;

	sandec_free_memories(ctx);

	if (ctx->adstbuf1)
		free(ctx->adstbuf1);
	free(ctx);
	*sanctx = NULL;
}

int sandec_get_framecount(void *sanctx)
{
	struct sanctx *ctx = (struct sanctx *)sanctx;
	return ctx ? ctx->rt.FRMEcnt : 0;
}

int sandec_get_currframe(void *sanctx)
{
	struct sanctx *ctx = (struct sanctx *)sanctx;
	return ctx ? ctx->rt.currframe : 0;
}

int sandec_set_params(void *sanctx, int16_t xres, int16_t yres, int8_t mortimermode)
{
	struct sanctx *ctx = (struct sanctx *)sanctx;
	if (!ctx)
		return 110;
	if (ctx->rt.version < 3) {
		if ((xres != -1) || (xres != -1)) {
			if ((xres > 0) && (xres <= FOBJ_MAXX)) {
				ctx->rt.bufw = xres;
			} else {
				return 111;
			}
			if ((yres > 0) && (yres <= FOBJ_MAXY)) {
				ctx->rt.bufh = yres;
			} else {
				return 112;
			}
			ctx->rt.pitch = ctx->rt.bufw;
			ctx->rt.fbsize = ctx->rt.bufw * ctx->rt.bufh * 1;
			ctx->rt.have_vdims = 1;
		}
		if (mortimermode != -1) {
			if (mortimermode == 0) {
				ctx->rt.mortimer = 0;
			} else if ((ctx->rt.bufw == 640) && (ctx->rt.bufh == 480) && (mortimermode != 0)) {
				ctx->rt.mortimer = 1;
			}
		}
	}

	return 0;
}
