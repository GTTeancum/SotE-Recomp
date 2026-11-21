#pragma once
#define _PDCLIB_FWRITE 1
#define _PDCLIB_DELONCLOSE 2
#define _PDCLIB_FREEBUFFER 4
#define _PDCLIB_FREENAME 8
#define _PDCLIB_STATIC 16
void test_lock(mtx_t*);
void test_unlock(mtx_t*);
#define _PDCLIB_LOCK(m) test_lock(&(m))
#define _PDCLIB_UNLOCK(m) test_unlock(&(m))
int _PDCLIB_getstream(struct _PDCLIB_file_t*);
int _PDCLIB_flushbuffer(struct _PDCLIB_file_t*);
int _PDCLIB_close(void*);
