#pragma once
#include <SDL.h>

/* The Xbox port reads mapped state, not the public SDL event queue. Keep
   normal event generation/watchers for mapping and hotplug. Recycle only
   pad events after their delivery; leave quit, keyboard, audio, user, etc.
   intact. In the pinned NXDK SDL, PumpEvents already updates all sticks and
   USB hub detection with the default enabled joystick events. */
static inline SDL_GameController* xb_poll_pad_state(SDL_GameController* pad) {
    SDL_PumpEvents();
    if(pad && !SDL_GameControllerGetAttached(pad)) {
        SDL_GameControllerClose(pad);pad=NULL;
    }
    if(!pad)for(int i=0;i<SDL_NumJoysticks();++i)if(SDL_IsGameController(i)) {
        pad=SDL_GameControllerOpen(i);if(pad)break;
    }
    /* This range contains only SDL joystick/controller events in the Xbox
       SDK; their scalar payloads require no per-event cleanup. */
    SDL_FlushEvents(SDL_JOYAXISMOTION,SDL_CONTROLLERDEVICEREMAPPED);
    return pad;
}
