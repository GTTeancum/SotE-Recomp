#include "sandec.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct converter {
    FILE *input;
    FILE *output;
    const char *output_path;
    uint64_t audio_bytes;
    int failed;
};

static int read_input(void *userctx, void *dst, uint32_t size) {
    struct converter *converter = (struct converter *)userctx;
    return fread(dst, 1, size, converter->input) == size;
}

static void discard_video(void *userctx, unsigned char *data, uint32_t size,
    uint16_t width, uint16_t height, uint16_t pitch, uint32_t *palette,
    uint16_t subtitle, uint32_t duration_us) {
    (void)userctx;
    (void)data;
    (void)size;
    (void)width;
    (void)height;
    (void)pitch;
    (void)palette;
    (void)subtitle;
    (void)duration_us;
}

static void write_audio(void *userctx, unsigned char *data, uint32_t size) {
    struct converter *converter = (struct converter *)userctx;
    if (converter->failed || size == 0) {
        return;
    }
    if (converter->output == NULL) {
        converter->output = fopen(converter->output_path, "wb");
        if (converter->output == NULL) {
            converter->failed = 1;
            return;
        }
    }
    if (fwrite(data, 1, size, converter->output) != size) {
        converter->failed = 1;
    }
    converter->audio_bytes += size;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: sandec_audio_cache input.SAN output.pcm\n");
        return 2;
    }
    struct converter converter = {0};
    converter.output_path = argv[2];
    converter.input = fopen(argv[1], "rb");
    if (converter.input == NULL) {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 1;
    }
    void *decoder = NULL;
    int status = sandec_init(&decoder);
    if (status == SANDEC_OK) {
        struct sanio io = {0};
        io.ioread = read_input;
        io.queue_video = discard_video;
        io.queue_audio = write_audio;
        io.userctx = &converter;
        status = sandec_open(decoder, &io);
        while (status == SANDEC_OK && !converter.failed) {
            status = sandec_decode_next_frame(decoder);
        }
    }
    if (decoder != NULL) {
        sandec_exit(&decoder);
    }
    fclose(converter.input);
    if (converter.output != NULL && fclose(converter.output) != 0) {
        converter.failed = 1;
    }
    if (converter.failed || status != SANDEC_DONE) {
        fprintf(stderr, "SAN audio decode failed: %s (status %d)\n", argv[1], status);
        remove(argv[2]);
        return 1;
    }
    printf("SAN audio: %llu bytes at 22050 Hz stereo S16LE\n",
        (unsigned long long)converter.audio_bytes);
    return 0;
}
