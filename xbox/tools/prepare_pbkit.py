"""Generate a local pbkit core correction without modifying the installed SDK."""
from pathlib import Path
import sys

source=Path(sys.argv[1]).read_text()
needle='''        //screen swapping has been done already, theoretically, in ISR
        pb_BackBufferbReady[next]=0;'''
replacement='''        /* FIFO processing can make a frame ready after the ISR checked
           this slot, or invoke this handler without that ISR scanout step.
           Always display the queued buffer before consuming its slot. */
        if (pb_debug_screen_active==0)
            VIDEOREG(PCRTC_START)=pb_FBAddr[pb_BackBufferIndex[next]]&0x03FFFFFF;
        pb_BackBufferbReady[next]=0;'''
if source.count(needle)!=1:
    raise SystemExit('Installed pbkit differs: review vblank fix before building')
source=source.replace(needle,replacement)
def exact(a,b):
    global source
    if source.count(a)!=1:raise SystemExit("pbkit layout mismatch: "+a[:55])
    source=source.replace(a,b)
exact("static  int         pb_running=0;", "extern unsigned xb_display_bpp;\nvolatile unsigned sote_display_buffers[12]; /* layout and owned/scanout synchronization */\nstatic unsigned sote_last_submitted; static int sote_have_submit;\nstatic  int         pb_running=0;")
exact("    int ColorBpp = 0;", "    if (vm.width==1280 && vm.height==720 && xb_display_bpp==16) pb_ColorFmt=NV097_SET_SURFACE_FORMAT_COLOR_LE_R5G6B5;\n    int ColorBpp = 0;")
exact("    BackBufferCount=2;", "    const int shared_front=(vm.width==1280 && vm.height==720 && xb_display_bpp==16);\n    BackBufferCount=shared_front?1:2;")
exact("    FBSize=Size*FrameBufferCount;", "    FBSize=Size*(shared_front?1:FrameBufferCount);")
exact("    for(i=0;i<FrameBufferCount;i++)\n    {\n        pb_FBAddr[i]=FBAddr;\n        FBAddr+=Size;\n    }","    /* HAL retains ownership of its native RGB565 scanout. Only the private back allocation is tiled/freed by pbkit. */\n    if(shared_front){\n        DWORD h=(DWORD)XVideoGetFB();\n        if(!h || (h&63u) || MmQueryAllocationSize((PVOID)h)<Size || Size!=Width*Height*2u || ((h&0x03ffffff)+Size>0x04000000)){pb_kill();return -15;}\n        pb_FBAddr[0]=h;pb_FBAddr[1]=FBAddr;pb_FBAddr[2]=0;\n    }else for(i=0;i<FrameBufferCount;i++){pb_FBAddr[i]=FBAddr;FBAddr+=Size;}\n    sote_display_buffers[0]=FrameBufferCount;sote_display_buffers[1]=pb_FBAddr[0]&0x03ffffff;sote_display_buffers[2]=pb_FBAddr[1]&0x03ffffff;\n    sote_display_buffers[3]=Size;sote_display_buffers[4]=FBSize;sote_display_buffers[5]=ColorBpp;")
exact("    pb_DSAddr=DSAddr;", "    pb_DSAddr=DSAddr;sote_display_buffers[6]=DSSize;sote_display_buffers[7]=pb_DepthStencilPitch;")
exact("    pb_back_index=(pb_back_index+1)%3;", "    if(pb_FrameBuffersCount==2){sote_last_submitted=(DWORD)pb_FBAddr[pb_back_index]&0x03ffffff;sote_have_submit=1;}\n    pb_back_index=(pb_back_index+1)%pb_FrameBuffersCount;")
exact("    pb_shutdown_registration.NotificationRoutine", "    sote_have_submit=0;\n    pb_shutdown_registration.NotificationRoutine")
# Fix upstream's five writes to three queue slots (adjacent static variables must not be clobbered).
exact("for(i=0;i<5;i++) pb_BackBufferbReady[i]=0;", "for(i=0;i<3;i++) pb_BackBufferbReady[i]=0;")
source += r"""
/* Call before touching the next physical surface. The triple-entry hardware queue is not confused with the two backing pages. */
int xb_pb_ready_for_draw(void){
    if(pb_FrameBuffersCount!=2 || !sote_have_submit)return 1;
    unsigned front=VIDEOREG(PCRTC_START)&0x03ffffff;
    unsigned back=(DWORD)pb_FBAddr[pb_back_index]&0x03ffffff;
    sote_display_buffers[8]=front;sote_display_buffers[9]=sote_last_submitted;
    if(front!=sote_last_submitted || front==back){sote_display_buffers[10]++;return 0;}
    sote_display_buffers[11]++;return 1;
}
"""
Path(sys.argv[2]).write_text(source)
