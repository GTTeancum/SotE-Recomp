#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include "../../src/audio.c"
static unsigned init_calls,open_calls,close_calls,worker_calls;
static int fail_init,expect_fatal; static jmp_buf fatal_env;
int SDL_InitSubSystem(unsigned f){assert(f==SDL_INIT_AUDIO);init_calls++;return fail_init?-1:0;}
void SDL_CloseAudioDevice(unsigned d){assert(d);close_calls++;}
unsigned SDL_OpenAudioDevice(const char*n,int c,const SDL_AudioSpec*d,SDL_AudioSpec*o,int f){(void)n;(void)c;(void)f;*o=*d;return ++open_calls;}
SDL_Thread* SDL_CreateThread(int(*f)(void*),const char*n,void*a){static SDL_Thread t;(void)f;(void)n;(void)a;worker_calls++;return &t;}
void SDL_Delay(unsigned x){(void)x;}
void SDL_PauseAudioDevice(unsigned d,int p){assert(d);(void)p;}
void SDL_LockAudioDevice(unsigned d){assert(d);}
void SDL_UnlockAudioDevice(unsigned d){assert(d);}
void xb_audio_dma_service(void){}
void xb_post_event(unsigned e){(void)e;}
void xb_unsupported(uint32_t id){if(expect_fatal){assert(id==0xE3000001);longjmp(fatal_env,1);}
 fprintf(stderr,"unexpected fatal %x\n",id);abort();}
int main(void){
 recomp_context c={0};
 c.r4=1;osAiSetFrequency_recomp(NULL,&c);assert(c.r2==-1&&init_calls==0);
 fail_init=expect_fatal=1;c.r4=32000;
 if(!setjmp(fatal_env)){osAiSetFrequency_recomp(NULL,&c);assert(0);}
 assert(!audio_initialized&&open_calls==0&&worker_calls==0);
 fail_init=expect_fatal=0;
 for(unsigned i=0;i<1000;i++){
  c.r4=(i&1)?22050:32000;osAiSetFrequency_recomp(NULL,&c);assert(c.r2==c.r4);
  unsigned opened=open_calls;osAiSetFrequency_recomp(NULL,&c);assert(open_calls==opened);
 }
 assert(init_calls==2&&worker_calls==1&&open_calls==1000&&close_calls==999);
 puts("1000 frequency switches: one successful subsystem init/worker; failed init retains retry state; each replaced device closed; same-frequency no-op PASS");
}
