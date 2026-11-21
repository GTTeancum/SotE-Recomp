#include <stdio.h>
#include <string.h>
#include <hal/video.h>
#include <xboxkrnl/xboxkrnl.h>
#include "display_mode.h"
unsigned xb_display_width=640,xb_display_height=480,xb_display_wide,xb_display_bpp=32;
extern volatile unsigned sote_display_achieved[8];
/* requested,effective,status,encoder,enumerated mask,total/used-before bytes,
 * realized width/height/bpp/hz,width-mode progressive,wanted-wide,model bytes. */
volatile uint32_t sote_display_mode[14];
static unsigned boot_request(void) {
    /* Private opt-in sidecar only; no UI/save or production default change. */
    FILE* f=fopen("D:\\xbox-display.cfg","rb");
    if(!f)f=fopen("D:\\xbox-test.cfg","rb");
    if(!f)return XB_DISPLAY_DEFAULT;
    char l[96];unsigned q=XB_DISPLAY_DEFAULT;
    while(fgets(l,sizeof(l),f)){
        l[strcspn(l,"\r\n")]=0;
        if(!strcmp(l,"mode=480i-4x3"))q=XB_DISPLAY_480I;
        else if(!strcmp(l,"mode=480p-16x9"))q=XB_DISPLAY_480W;
        else if(!strcmp(l,"mode=720p-16x9"))q=XB_DISPLAY_720W;
    }
    fclose(f);return q;
}
int xb_display_boot(int hd_wanted) {
    unsigned encoder=XVideoGetEncoderSettings(),a=0,total=0,used=0, q=boot_request();
    MM_STATISTICS m={0};m.Length=sizeof m;
    if(MmQueryStatistics(&m)>=0 && m.AvailablePages<=m.TotalPhysicalPages){
        total=m.TotalPhysicalPages*4096u;used=(m.TotalPhysicalPages-m.AvailablePages)*4096u;
    }
    VIDEO_MODE v;void* i=NULL;
    while(XVideoListModes(&v,32,REFRESH_DEFAULT,&i)){
        if(v.width==640&&v.height==480)a|=XB_DISP_AVAILABLE_480;
        if(v.width==1280&&v.height==720)a|=XB_DISP_AVAILABLE_720;
    }
    xb_display_mode d=xb_mode(q,encoder,a,total,hd_wanted);
    unsigned bpp=(d.request==XB_DISPLAY_720W)?16:32;
    if(!XVideoSetMode(d.w,d.h,bpp,REFRESH_DEFAULT)){
        if(d.w==640 || !XVideoSetMode(640,480,32,REFRESH_DEFAULT))return 0;
        d=xb_mode(XB_DISPLAY_DEFAULT,encoder,a,total,hd_wanted);d.reason=XB_DISPLAY_HAL_REJECTED;bpp=32;
    }
    v=XVideoGetMode();
    if(v.width!=d.w||v.height!=d.h||v.bpp!=(int)bpp)return 0;
    xb_display_bpp=bpp;xb_display_width=d.w;xb_display_height=d.h;xb_display_wide=d.wide;
    const unsigned s[]={q,d.request,d.reason,encoder,a,total,used,d.w,d.h,v.bpp,v.refresh,d.progressive,d.wide,
        bpp==16?7380992u:xb_display_surface_bytes(d.w,d.h)};
    for(unsigned j=0;j<sizeof(s)/sizeof(*s);j++)sote_display_mode[j]=s[j];
    return 1;
}
