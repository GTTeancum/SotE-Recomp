#pragma once
#include "threads.h"
#define EOF (-1)
struct _PDCLIB_file_t {mtx_t mtx;unsigned status;void*handle;char*filename;char*buffer;};
int fclose(struct _PDCLIB_file_t*);
int remove(const char*);
