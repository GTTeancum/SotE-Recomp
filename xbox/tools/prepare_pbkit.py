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
Path(sys.argv[2]).write_text(source.replace(needle,replacement))
