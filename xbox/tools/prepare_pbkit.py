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
# pb_end advances pb_Put by the actual packet length. Wrap at submission
# boundaries, leaving room for a whole block and pb_jump_to_head's jump word.
# pb_reset waits for the DMA reader to reach the head before reusing it.
exact("uint32_t *pb_begin(void)\n{", """extern volatile unsigned sote_gpu_flushes;
uint32_t *pb_begin(void)
{
    if (pb_Tail - pb_Put < 1024) {
        pb_reset();
        ++sote_gpu_flushes;
    }""")
# NV097 triangle packets can share one write-combine flush and DMA doorbell.
# Only the title's triangle encoder uses this API. Normal pbkit submissions,
# ownership fences and pushbuffer wraps publish the entire preceding stream.
exact("void pb_reset(void)\n{", """static uint32_t *xb_first_packet;
void pb_reset(void)
{
    xb_first_packet = NULL;""")
exact("    pb_Put=pEnd;", "    xb_first_packet = NULL;\n    pb_Put=pEnd;")
exact("void pb_push_to(DWORD", """void xb_pb_end_packet(uint32_t *end)
{
#ifndef DBG
    if (!pb_trace_mode) {
        if (xb_first_packet == NULL) xb_first_packet = pb_Put;
        if (end - xb_first_packet < 512) {
            pb_Put = end;
            return;
        }
    }
#endif
    pb_end(end);
}

void xb_pb_publish(void)
{
    if (xb_first_packet != NULL) pb_end(pb_Put);
}

void pb_push_to(DWORD""")
# All supported modes use pbkit-owned, tiled color buffers. The kernel's
# linear display buffer is separate and remains available to the debug font.
exact("for(i=0;i<5;i++) pb_BackBufferbReady[i]=0;", "for(i=0;i<3;i++) pb_BackBufferbReady[i]=0;")
# pb_init can enter pb_kill before its shutdown callback has been registered.
exact("    HalRegisterShutdownNotification(&pb_shutdown_registration, FALSE);",
      "    if (pb_shutdown_registration.NotificationRoutine) {\n"
      "        HalRegisterShutdownNotification(&pb_shutdown_registration, FALSE);\n"
      "        pb_shutdown_registration.NotificationRoutine = NULL;\n    }")
# These occur once, at thread level, and are recorded only by the hardware
# test fixture. Preserve the stage even if GPU initialization stalls.
source = "extern void xb_hardware_note(const char *, unsigned);\n" + source
for anchor, name, value in (
    ("    pb_GpuInstMem=(DWORD)MmClaimGpuInstanceMemory", "gpu-instance", "0"),
    ("    pb_FrameBuffersAddr=FBAddr;", "color-allocation", "FBSize"),
    ("    pb_assign_tile( 0,", "color-tile", "Pitch"),
    ("    pb_DepthStencilAddr=DSAddr;", "depth-allocation", "DSSize"),
    ("    pb_assign_tile( 1,", "depth-tile", "Pitch"),
    ("    memset((DWORD *)pb_FBAddr[0],0,pb_FBSize);", "surface-clear", "pb_FBSize"),
):
    exact(anchor, f'    xb_hardware_note("{name}",{value});\n' + anchor)
# During physical-console acceptance tests, retain the fatal method and stop the
# title GPU queue. Stock pbkit sleeps forever in its interrupt DPC, preventing
# file output or the debug monitor from retrieving the fault.
source = "extern int xb_hardware_test;\nextern volatile unsigned xb_gpu_fault[3];\n" + source
# Sample the display address at vblank service, after any queued buffer is
# selected. A VI thread woken by the event can miss multiple retraces.
source = "extern void xb_hardware_scanout(unsigned);\n" + source
exact("    NtPulseEvent(pb_VBlankEvent, NULL);",
      "    if (xb_hardware_test) xb_hardware_scanout(VIDEOREG(PCRTC_START)&0x03ffffff);\n"
      "    NtPulseEvent(pb_VBlankEvent, NULL);")
exact("                            pb_show_debug_screen();",
      """                            if (xb_hardware_test) {
                                xb_gpu_fault[1]=trapped_address;
                                xb_gpu_fault[2]=DataLow;
                                xb_gpu_fault[0]=nsource;
                                VIDEOREG(NV_PGRAPH_INTR_EN)=0;
                                VIDEOREG(NV_PGRAPH_INTR)=status;
                                return 0;
                            }
                            pb_show_debug_screen();""")
exact("        if (status&NV_PMC_INTR_0_PGRAPH_PENDING) more|=pb_gr_handler();",
      """        if (status&NV_PMC_INTR_0_PGRAPH_PENDING) more|=pb_gr_handler();
        if (xb_gpu_fault[0]) return; /* ISR masked the title GPU IRQ on entry. */""")
# pb_kill waits for the GPU to retire its queue. A terminal QA fault has
# already stopped that queue; only bypass the wait during system shutdown.
exact("\t" + "pb_kill();\n}", "\tif (!xb_gpu_fault[0]) pb_kill();\n}")
Path(sys.argv[2]).write_text(source)
