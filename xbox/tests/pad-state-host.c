/* Uses a real host SDL2 runtime (>= 2.0.14) with a deterministic virtual
   gamepad. Can compile against the staged NXDK SDL2 headers. Never opens or
   injects input into an emulator or a physical/user controller. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pad_state.h"
#if !SDL_VERSION_ATLEAST(2,0,14)
extern int SDL_JoystickAttachVirtual(SDL_JoystickType, int, int, int);
extern int SDL_JoystickDetachVirtual(int);
extern int SDL_JoystickSetVirtualAxis(SDL_Joystick*, int, Sint16);
extern int SDL_JoystickSetVirtualButton(SDL_Joystick*, int, Uint8);
#endif
static unsigned added,removed,motion;
static int SDLCALL watch(void* ud,SDL_Event* e){
    (void)ud;
    if(e->type==SDL_JOYDEVICEADDED)++added;
    if(e->type==SDL_JOYDEVICEREMOVED)++removed;
    if(e->type==SDL_CONTROLLERAXISMOTION||e->type==SDL_JOYAXISMOTION)++motion;
    return 1;
}
static int qcount(void){
    return SDL_PeepEvents(NULL,0,SDL_PEEKEVENT,SDL_JOYAXISMOTION,SDL_CONTROLLERDEVICEREMAPPED);
}
static SDL_GameController* old_poll(SDL_GameController* p){
    SDL_PumpEvents();SDL_GameControllerUpdate();
    if(p && !SDL_GameControllerGetAttached(p)){SDL_GameControllerClose(p);p=NULL;}
    if(!p)for(int i=0;i<SDL_NumJoysticks();++i)if(SDL_IsGameController(i)){
        p=SDL_GameControllerOpen(i);if(p)break;}
    return p;
}
int main(int argc,char**argv){
    assert(argc==2 && (!strcmp(argv[1],"control")||!strcmp(argv[1],"fixed")));
    int fixed=!strcmp(argv[1],"fixed");
    SDL_GameController*(*poll)(SDL_GameController*)=fixed?xb_poll_pad_state:old_poll;
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    assert(SDL_Init(SDL_INIT_GAMECONTROLLER)==0);
    /* Deliberately fail closed on a machine with a real connected pad. */
    if(SDL_NumJoysticks()!=0){fprintf(stderr,"Run only in an isolated environment without joysticks.\n");return 77;}
    SDL_version v;SDL_GetVersion(&v);
    SDL_AddEventWatch(watch,NULL);
    int idx=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,15,1);
    assert(idx>=0 && SDL_IsGameController(idx));
    SDL_GameController* p=poll(NULL);assert(p && SDL_GameControllerGetAttached(p));
    SDL_Joystick* j=SDL_GameControllerGetJoystick(p);
    SDL_Event marker;memset(&marker,0,sizeof marker);marker.type=SDL_USEREVENT;marker.user.code=0x507;
    assert(SDL_PushEvent(&marker)==1);
    for(int i=0;i<12000;++i){
        Sint16 x=(i&1)?18000:-14000;Uint8 b=(Uint8)(i&1);
        assert(!SDL_JoystickSetVirtualAxis(j,0,x) && !SDL_JoystickSetVirtualButton(j,0,b));
        p=poll(p);assert(p && SDL_GameControllerGetAttached(p));
        assert(SDL_GameControllerGetAxis(p,SDL_CONTROLLER_AXIS_LEFTX)==x);
        assert(SDL_GameControllerGetButton(p,SDL_CONTROLLER_BUTTON_A)==b);
    }
    int queued=qcount();assert(fixed ? queued==0 : queued>=12000);
    assert(SDL_PeepEvents(&marker,1,SDL_GETEVENT,SDL_USEREVENT,SDL_USEREVENT)==1 && marker.user.code==0x507);
    assert(added && motion && !SDL_JoystickDetachVirtual(idx));p=poll(p);assert(!p && removed);
    idx=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,15,1);
    p=poll(p);assert(p && added>=2);j=SDL_GameControllerGetJoystick(p);
    assert(!SDL_JoystickSetVirtualButton(j,0,SDL_PRESSED));p=poll(p);
    assert(SDL_GameControllerGetButton(p,SDL_CONTROLLER_BUTTON_A));
    assert(!SDL_JoystickDetachVirtual(idx));p=poll(p);assert(!p && removed>=2);
    if(fixed)assert(!qcount());
    printf("SDL %u.%u.%u, %s: 12000 state changes, pending pad events %d, watchers/state/marker/disconnect/reopen PASS\n",
           v.major,v.minor,v.patch,argv[1],queued);
    SDL_DelEventWatch(watch,NULL);SDL_Quit();
}
