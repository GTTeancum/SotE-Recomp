/* Xbox-local PDCLib override: release dynamically created stream mutexes.
 * Derived from staged NXDK PDCLib fclose.c (public domain, notice below).
 * Keep static stream mutexes for CRT shutdown; do not patch the shared SDK. */
#include <stdint.h>
volatile uint32_t sote_stdio_cleanup[2]; /* dynamic closes, mutexes destroyed */
/* fclose( FILE * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include <stdlib.h>

#ifndef REGTEST

#include "pdclib/_PDCLIB_glue.h"

#ifndef __STDC_NO_THREADS__
#include <threads.h>
extern mtx_t _PDCLIB_filelist_mtx;
#endif

extern struct _PDCLIB_file_t * _PDCLIB_filelist;

int fclose( struct _PDCLIB_file_t * stream )
{
    int result = 0;
    _PDCLIB_LOCK( stream->mtx );

    /* Remove stream from list */
    if ( _PDCLIB_getstream( stream ) )
    {
        _PDCLIB_UNLOCK( stream->mtx );
        return EOF;
    }


    /* Flush buffer */
    if ( stream->status & _PDCLIB_FWRITE )
    {
        if ( _PDCLIB_flushbuffer( stream ) == EOF )
        {
            /* Closing consumes the stream even on write failure. Preserve
               the failure result, but still release its handle and storage. */
            result = EOF;
        }
    }

    /* Close handle */
    if ( _PDCLIB_close( stream->handle ) != 0 ) result = EOF;

    /* Delete tmpfile() */
    if ( stream->status & _PDCLIB_DELONCLOSE )
    {
        remove( stream->filename );
    }

    /* Free user buffer (setvbuf allocated) */
    if ( stream->status & _PDCLIB_FREEBUFFER )
    {
        free( stream->buffer );
    }

    /* Free filename (freopen allocated) */
    if ( stream->status & _PDCLIB_FREENAME )
    {
        free( stream->filename );
    }

    _PDCLIB_UNLOCK( stream->mtx );

    /* Free stream */
    if ( ! ( stream->status & _PDCLIB_STATIC ) )
    {
#ifndef __STDC_NO_THREADS__
        /* fopen creates this kernel mutex. Unlock before destroying it;
           no access to this FILE is valid after fclose starts disposal. */
        mtx_destroy( &stream->mtx );
        sote_stdio_cleanup[1]++;
#endif
        sote_stdio_cleanup[0]++;
        free( stream );
    }

    return result;
}

#endif

