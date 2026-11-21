#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define fclose tested_fclose
#define remove tested_remove
#include "../../src/stdio_close.c"
static int removed,flush_failed,close_failed,closed,destroyed,locked,unlocked;
void test_lock(mtx_t*m){assert(!m->locked&&!m->destroyed);m->locked=1;locked++;}
void test_unlock(mtx_t*m){assert(m->locked&&!m->destroyed);m->locked=0;unlocked++;}
void mtx_destroy(mtx_t*m){assert(!m->locked&&!m->destroyed);m->destroyed=1;destroyed++;}
int _PDCLIB_getstream(struct _PDCLIB_file_t*f){(void)f;return removed;}
int _PDCLIB_flushbuffer(struct _PDCLIB_file_t*f){(void)f;return flush_failed?EOF:0;}
int _PDCLIB_close(void*h){(void)h;closed++;return close_failed?-1:0;}
int tested_remove(const char*p){(void)p;return 0;}
extern int puts(const char*);
int main(void){
 for(int i=0;i<10000;i++){
  struct _PDCLIB_file_t*f=calloc(1,sizeof *f);assert(f);
  assert(tested_fclose(f)==0);
 }
 assert(closed==10000&&destroyed==10000&&locked==unlocked);
 assert(sote_stdio_cleanup[0]==10000&&sote_stdio_cleanup[1]==10000);
 struct _PDCLIB_file_t standard={0};standard.status=_PDCLIB_STATIC;
 assert(tested_fclose(&standard)==0&&!standard.mtx.destroyed&&destroyed==10000);
 struct _PDCLIB_file_t fail={0};removed=1;
 assert(tested_fclose(&fail)==EOF&&!fail.mtx.destroyed&&locked==unlocked);
 removed=0;flush_failed=1;
 struct _PDCLIB_file_t*write_fail=calloc(1,sizeof *write_fail);write_fail->status=_PDCLIB_FWRITE;
 assert(tested_fclose(write_fail)==EOF&&destroyed==10001&&closed==10002&&locked==unlocked);
 flush_failed=0;close_failed=1;
 struct _PDCLIB_file_t*close_fail=calloc(1,sizeof *close_fail);
 assert(tested_fclose(close_fail)==EOF&&destroyed==10002&&closed==10003&&locked==unlocked);
 puts("10000 dynamic fclose mutex destructions; static streams retained; unlock order/flush failure/close failure cleanup PASS");
}
