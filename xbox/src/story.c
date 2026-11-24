#include "recomp_hooks.h"
#include "console_budget.h"
volatile struct {uint32_t event,caller,loads;} sote_story_status;
void sote_before_story_load(uint8_t* rdram,uint32_t event,uint32_t caller) {
    xb_budget_story(event);
    (void)rdram;sote_story_status.event=event;sote_story_status.caller=caller;sote_story_status.loads++;
}
