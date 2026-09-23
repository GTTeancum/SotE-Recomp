/*
 * Decoder library for LucasArts SMUSH/SAN videos.
 *
 * How to use:
 *
 * - Prepare some callbacks:
 *
 * Queue new audio data:  Audio is 22.05kHz 16bit Stereo LSB. This callback
 *  can be called multiple times during sandec_decode_next_frame(), always
 *  with new data, so the callback needs to either queue the data immediately,
 *  or append it to a buffer for later consumption.
 *
 * void my_queue_audio(void *userctx, char *abuf, uint32_t bufsize)
 * {
 *  userctx:  context data from struct sanio
 *  abuf, bufsize: audio data buffer and size in bytes of data in buffer.
 * }
 *
 *
 * Queue new video frame callback: Called once per frame after a new frame
 *  is available.  The pointers given to this callback are valid until
 *  the next invocation of sandec_decode_next_frame().
 * the subid parameter indicates which message id from the Outlaws LOCAL.MSG
 *  file should be displayed for subtitles. If it is zero, do not show any.
 *
 * void my_queue_videoframe(void *userctx, char *vbuf, uint32_t bufsize,
 *                          uint16_t w, uint16_t h, uint32_t* pal,
 *                          uint16_t subid, uint16_t, uint32_t frame_duration_us)
 * {
 *  userctx: context data from struct sanio
 *  vbuf, bufsize: image data and size of buffer
 *  w, h: width/height in pixels of image
 *  pitch: line size/stride/pitch in bytes
 *  pal: 256 * 4 byte buffer with palette data in ARGB format.  If pal is NULL,
 *   then pixelformat is RGB565, otherwise if PAL is set, the pixelformat is INDEX8.
 *  subid: index of the subtitle to display, or zero.
 *  frame_duration_us: how long to display the frame, in microsecods.  The frame
 *   duration _may_ vary during playback!
 * }
 *
 *
 * fetch data callback:  destbuf is always valid, amount is always 1 or more.
 * Return 1 if the requested amount of data was put in the buffer,
 *  otherwise or on error return 0.
 *
 * int my_data_read(void *userctx, char *destbuf, int amount)
 * {    // return 1 if all data was read, 0 on errors or not enough data.
 *	return (read(userctx->fhandle, destbuf, amount) == amount);
 * }
 *
 *
 * - Set up decoder context and handle frames:
 *
 * void *sancontext;
 * struct sanio myio;
 * struct my_avcontext_t my_avctx;
 *
 * int ret = sandec_init(&sancontxt);
 * if (ret != 0) { // error no memory };
 *
 * int fd = open("/path/to/OP_CR.SAN", O_RDONLY);
 * if (fd < 0) { // error file not found };
 * myavctx.fhandle = fd;
 * myio.ioread = my_data_read;
 * myio.userctx = my_avctx;
 * myio.queue_audio = my_queue_audio;
 * myio.queue_video = my_queue_video;
 *
 * int ret = sandec_open(sancontext, &myio);
 * while (ret == SANDEC_OK) {
 *  ret = sandec_decode_next_frame(sancontext);
 *  if (ret != SANDEC_OK)
 * 	break;
 *  render_video_and_audio(my_avctx);
 * }
 * if (ret != SANDEC_DONE) { // report error code please }
 * sandec_exit(&sancontext);
 * close(fd);
 *
 * NOTES:
 * - The decoder only does linear forward reads. Once data has been read, it
 *    it will not be requested again.
 * - sanio.queue_audio() can be called multiple times per frame decoding call.
 * - sanio.queue_video() is only called ONCE per frame decoding call.
 */

#ifndef _SANDEC_H_
#define _SANDEC_H_

#include <inttypes.h>

/* status codes */
#define SANDEC_OK	0
#define SANDEC_DONE	-1
/* all other positive values indicate where the error occured */

/* resample all audio to this target rate */
#define SANDEC_AUDIO_SRATE	22050


/* flags */
/* do frame interpolation if possible */
#define SANDEC_FLAG_DO_FRAME_INTERPOLATION	(1 << 0)
/* ignore audio */
#define SANDEC_FLAG_NO_AUDIO			(1 << 1)

struct sanio {
	int(*ioread)(void *userctx, void *dst, uint32_t size);
	void(*queue_video)(void *userctx, unsigned char *vdata, uint32_t size,
			  uint16_t w, uint16_t h, uint16_t pitch, uint32_t *pal,
			  uint16_t subid, uint32_t frame_duration_us);
	void(*queue_audio)(void *userctx, unsigned char *adata, uint32_t size);
	void *userctx;
	uint32_t flags;
};

/* init SAN context. Call this as step 1. */
int sandec_init(void **sanctx);

/* open/analyze a SAN file, supply with IO structure with at least "read" set. */
int sandec_open(void *sanctx, struct sanio *io);

/* Process one full frame (audio+video).
 * will call the queue_audio() callback multiple times, and queue_video()
 * callback once.
 */
int sandec_decode_next_frame(void *sanctx);

/* destroy all context. call this as last step. */
void sandec_exit(void **sanctx);

/* get the expected number of frames to render */
int sandec_get_framecount(void *sanctx);

/* get the current rendered frame number */
int sandec_get_currframe(void *sanctx);

/* override parameters; for Mortimer primarily */
int sandec_set_params(void *sanctx, int16_t xres, int16_t yres, int8_t mortimermode);

#endif
