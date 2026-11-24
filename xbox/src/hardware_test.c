/* Opt-in on-device acceptance-test output. A normal disc never writes to D:. */
#include <windows.h>
#include <pbkit/pbkit.h>
#include <xboxkrnl/xboxkrnl.h>
#include <stdio.h>
#include <stdlib.h>
#include "runtime.h"
#include "rdram_guard.h"
#include "display_mode.h"
#include "hardware_test.h"
#include "performance.h"
#include "gpu.h"
#include "texture.h"

int xb_hardware_test;
unsigned xb_audio_replay_poll;
unsigned xb_trace_preview_tiles;
static struct {float x,y;uint32_t key[17];int asset;} preview_tiles[64];
static unsigned tile_count;
void xb_hardware_tile(unsigned tile,unsigned tlut,unsigned w,unsigned h,float x,float y,int asset){
    if(tile_count>=64 || xb_texture_scene()!=2 || !xb_texture_slot_key(tile,tlut,w,h,preview_tiles[tile_count].key))return;
    preview_tiles[tile_count].x=x;preview_tiles[tile_count].y=y;
    preview_tiles[tile_count++].asset=asset;
}
static void write_preview_tiles(unsigned id){
    char path[40];snprintf(path,sizeof(path),"D:\\tiles%02u.csv",id);
    FILE* f=fopen(path,"w");if(!f)return;
    for(unsigned i=0;i<tile_count;i++){
        fprintf(f,"%d,%d,%d",(int)preview_tiles[i].x,(int)preview_tiles[i].y,preview_tiles[i].asset);
        for(unsigned k=0;k<17;k++)fprintf(f,",%u",preview_tiles[i].key[k]);
        fputc(10,f);
    }
    fclose(f);
}
static unsigned stop_poll;
static unsigned capture_polls[16],capture_count,capture_next;
static unsigned first_profile_poll, last_profile_poll;
typedef struct {
    uint32_t ms,frame,poll,event,target,paused,comm,inputs,buttons;
    int stick_x,stick_y;
    uint32_t players,vehicles,x,y,z,triangles,pcm_samples,audio_completed,shots;
} gameplay_sample;
static gameplay_sample* samples;
static unsigned sample_count,sample_capacity;

/* Timestamps are taken at the same pre-swap point on consecutive frames.
   A header records the actual counter frequency for host-side analysis. */
typedef struct { uint32_t frame,poll; int64_t ticks; } frame_timestamp;
static frame_timestamp* timestamps;
static unsigned time_first,time_last,time_count,time_capacity;
/* One writer (pbkit vblank handler). Take endpoints outside file I/O so the monitor
   measures distinct scanout addresses independently of CPU submissions. */
static volatile uint32_t scanout_retraces,scanout_changes;
static uint32_t scanout_first[3],scanout_last[3];
/* The first and last partial display intervals fall outside this histogram.
   Only the vblank callback writes its counters, on the single Xbox CPU. */
static volatile unsigned time_scanout,have_timed_flip;
static volatile unsigned last_flip_vblank,flip_gap_bins[5],longest_flip_gap;

void xb_hardware_scanout(uint32_t front) {
    static uint32_t previous;
    if(scanout_retraces && previous!=front){
        scanout_changes++;
        if(time_scanout){
            if(have_timed_flip){
                unsigned gap=scanout_retraces-last_flip_vblank;
                flip_gap_bins[gap<5?gap-1:4]++;
                if(gap>longest_flip_gap)longest_flip_gap=gap;
            }
            last_flip_vblank=scanout_retraces;
            have_timed_flip=1;
        }
    }
    previous=front;
    scanout_retraces++;
}

static void flush_timestamps(void) {
    LARGE_INTEGER frequency;
    int ok=QueryPerformanceFrequency(&frequency);
    FILE* file=fopen("D:\\frame-times.bin","wb");
    if(file){
        ok=ok && fwrite(&frequency.QuadPart,sizeof(frequency.QuadPart),1,file)==1 &&
           fwrite(timestamps,sizeof(*timestamps),time_count,file)==time_count;
        if(fclose(file))ok=0;
    }else ok=0;
    xb_hardware_note(ok?"frame-times":"frame-times-failed",time_count);
    /* Cross-check the address observations against pbkit's retrace counter. */
    xb_hardware_note("timed-vblanks",scanout_last[2]-scanout_first[2]);
    xb_hardware_note("timed-scanout-samples",scanout_last[0]-scanout_first[0]);
    xb_hardware_note("timed-front-changes",scanout_last[1]-scanout_first[1]);
    static const char* const names[]={"display-gap-1","display-gap-2","display-gap-3",
                                      "display-gap-4","display-gap-5plus"};
    for(unsigned i=0;i<5;i++)xb_hardware_note(names[i],flip_gap_bins[i]);
    xb_hardware_note("display-longest-gap",longest_flip_gap);
    free(timestamps);timestamps=NULL;
}

static int write_sample(FILE* file,const gameplay_sample* row) {
    return fprintf(file,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d,%u,%u,%08x,%08x,%08x,%u,%u,%u,%u\n",
        row->ms,row->frame,row->poll,row->event,row->target,row->paused,row->comm,row->inputs,
        row->buttons,row->stick_x,row->stick_y,row->players,row->vehicles,row->x,row->y,row->z,
        row->triangles,row->pcm_samples,row->audio_completed,row->shots)>=0;
}
static int flush_samples(void) {
    FILE* file=fopen("D:\\hardware.csv","a");
    if(!file)return 0;
    int ok=1;
    for(unsigned i=0;i<sample_count && ok;i++)ok=write_sample(file,&samples[i]);
    if(fclose(file))ok=0;
    free(samples);samples=NULL;
    return ok;
}
volatile uint32_t xb_gpu_fault[3];
extern volatile uint32_t sote_input_status[8];
static DWORD WINAPI hardware_watch(void* unused) {
    (void)unused;
    for(unsigned i=0;i<120;i++){
        Sleep(500);
        if(xb_gpu_fault[0]){
            xb_hardware_note("nv2a-method",xb_gpu_fault[1]);
            xb_hardware_note("nv2a-data",xb_gpu_fault[2]);
            xb_hardware_note("nv2a-error",xb_gpu_fault[0]);
            break;
        }
        if(i%4==3 && sote_input_status[0]<20)xb_hardware_note("input-polls",sote_input_status[0]);
    }
    return 0;
}
extern volatile uint32_t sote_input_status[8], sote_player_status[8], sote_vehicle_status[10];
extern volatile uint32_t sote_scene_fixture_status[4], sote_triangle_status[3];
extern volatile uint32_t sote_diagnostic_status[8],sote_player_trace[64][10];
extern volatile struct {uint32_t frequency,submissions,completed,callbacks,bytes,nonzero;} sote_audio_status;

static unsigned player_shots;
void xb_hardware_player_shot(uint32_t shooter){
    if(xb_hardware_test && shooter && shooter==sote_player_status[1])player_shots++;
}

void xb_hardware_note(const char* stage, unsigned value) {
    if(!xb_hardware_test)return;
    DbgPrint("SotE %s %08x\n",stage,value);
    FILE* f=fopen("D:\\hardware.log","a");
    if(f){fprintf(f,"%lu %s %08x\n",(unsigned long)GetTickCount(),stage,value);fclose(f);}
}
void xb_hardware_test_init(void) {
    FILE* flag=fopen("D:\\hwtest.cfg","rb");
    if(!flag)return;
    unsigned buffer_samples=0;
    char line[64];
    while(fgets(line,sizeof(line),flag)){
        unsigned n;
        if(sscanf(line,"trace_preview_tiles=%u",&n)==1)xb_trace_preview_tiles=n==1;
        if(sscanf(line,"capture_poll=%u",&n)==1 && n && n<=100000 &&
           capture_count<16 && (!capture_count || n>capture_polls[capture_count-1]))
            capture_polls[capture_count++]=n;
        if(sscanf(line,"end_poll=%u",&n)==1 && n>0 && n<=100000)stop_poll=n;
        if(sscanf(line,"audio_replay_poll=%u",&n)==1 && n>0 && n<=100000)xb_audio_replay_poll=n;
        if(sscanf(line,"buffer_samples=%u",&n)==1)buffer_samples=n==1;
        unsigned first,last;
        if(sscanf(line,"frame_times=%u-%u",&first,&last)==2 &&
           first>0 && first<last && last<=100000){time_first=first;time_last=last;}
        if(sscanf(line,"profile_frames=%u-%u",&first,&last)==2 &&
           first>0 && first<=last && last-first<120 && last<=100000){
            first_profile_poll=first;
            last_profile_poll=last;
        }
    }
    fclose(flag);
    if(!stop_poll)stop_poll=10000;
    xb_hardware_test=1;
    xb_hardware_note("start",0);
    if(time_first && time_last<stop_poll){
        time_capacity=time_last-time_first+1;
        timestamps=malloc(time_capacity*sizeof(*timestamps));
        if(!timestamps)xb_hardware_note("frame-times-allocation-failed",time_capacity);
    }
    if(buffer_samples){
        sample_capacity=stop_poll/12+1;
        samples=malloc(sample_capacity*sizeof(*samples));
        xb_hardware_note(samples?"buffered-samples":"sample-allocation-failed",sample_capacity);
    }
    HANDLE watch=CreateThread(NULL,16384,hardware_watch,NULL,0,NULL);
    if(watch)CloseHandle(watch);
    FILE* f=fopen("D:\\hardware.csv","w");
    if(f){
        fputs("ms,frame,poll,event,target,paused,comm,inputs,buttons,stickx,sticky,players,vehicles,x,y,z,triangles,pcm_samples,audio_completed,player_shots\n",f);
        fclose(f);
    }
}
/* A separate, untimed test can preserve one real mixer input for scalar vs.
   Xbox replay. The audio task has just finished loading its original DMEM. */
void xb_hardware_capture_audio_task(const uint8_t* rdram,const uint8_t* dmem){
    unsigned poll=sote_input_status[0];
    if(!xb_audio_replay_poll || poll<xb_audio_replay_poll)return;
    xb_audio_replay_poll=0;
    FILE* f=fopen("D:\\audio-replay.bin","wb");
    if(!f){xb_hardware_note("audio-replay-failed",poll);return;}
    unsigned header[2]={poll,xb_rdram_committed_limit};
    int ok=fwrite(header,1,sizeof(header),f)==sizeof(header) &&
           fwrite(dmem,1,4096,f)==4096 && fwrite(rdram,1,header[1],f)==header[1];
    if(fclose(f))ok=0;
    xb_hardware_note(ok?"audio-replay":"audio-replay-failed",poll);
}
/* Read the actual raster result, including sloped Z interpolation. Alternating
   the submission order distinguishes a depth failure from missing geometry. */
void xb_hardware_check_depth(void) {
    if(!xb_hardware_test)return;
    const int scissor[4]={0,0,320,240};
    const unsigned width=pb_back_buffer_width(),height=pb_back_buffer_height();
    const unsigned pitch=pb_back_buffer_pitch();
    pb_reset();pb_target_back_buffer();
    for(unsigned pass=0;pass<4;pass++){
        const float span=pass<2?.5f:.0001f;
        pb_fill(0,0,width,height,0xff000000);
        xb_gpu_clear_depth(0,0,width,height);
        for(unsigned draw=0;draw<2;draw++){
            unsigned red=draw^(pass&1);
            xb_gpu_vertex tri[3]={
                {.x=0,.y=0},{.x=320,.y=0},{.x=0,.y=240}
            };
            for(unsigned i=0;i<3;i++){
                tri[i].z=.999f-span*.5f+(red?(i==1?span*.5f:-span*.5f):0);
                tri[i].q=1;tri[i].color[red?0:1]=1;tri[i].color[3]=1;
            }
            xb_gpu_triangle(0,0,1,0x30,4,scissor,tri);
        }
        xb_gpu_flush();
        while(pb_busy())Sleep(0);
        unsigned miss=0,tested=0;
        const volatile uint8_t* fb=(const volatile uint8_t*)pb_back_buffer();
        for(unsigned y=8;y<height;y+=13)for(unsigned x=8;x<width;x+=13){
            if((x+4)*height+(y+4)*width>=width*height ||
               (x+4>=width/2 && x<=width/2+4))continue;
            unsigned c=xb_display_bpp==16?((const volatile uint16_t*)(fb+y*pitch))[x]:
                        ((const volatile uint32_t*)(fb+y*pitch))[x]&0xffffffu;
            unsigned expect=xb_display_bpp==16?(x<width/2?0xf800:0x07e0):
                            (x<width/2?0xff0000:0x00ff00);
            miss+=c!=expect;tested++;
        }
        xb_hardware_note("depth-probe",(tested<<16)|miss);
    }
    xb_gpu_clear_depth(0,0,width,height);
    pb_fill(0,0,width,height,0xff000000);
    while(pb_busy())Sleep(0);
}
uint32_t xb_hardware_rgb_hash(void) {
    while(pb_busy())Sleep(0);
    const volatile uint8_t* fb=(const volatile uint8_t*)pb_back_buffer();
    const unsigned width=pb_back_buffer_width(),height=pb_back_buffer_height();
    const unsigned pitch=pb_back_buffer_pitch();
    uint32_t hash=2166136261u;
    for(unsigned y=0;y<height;y++){
        const volatile uint8_t* row=fb+y*pitch;
        for(unsigned x=0;x<width;x++){
            uint32_t rgb=xb_display_bpp==16?((const volatile uint16_t*)row)[x]:
                         ((const volatile uint32_t*)row)[x]&0xffffff;
            hash=(hash^rgb)*16777619u;
        }
    }
    return hash;
}
static int capture_frame(unsigned event,unsigned step) {
    char name[40];
    snprintf(name,sizeof(name),"D:\\event%02u.ppm",event);
    FILE* f=fopen(name,"wb");
    if(!f)return 0;
    const unsigned width=pb_back_buffer_width(),height=pb_back_buffer_height();
    const unsigned pitch=pb_back_buffer_pitch();
    unsigned char row[1280*3];
    if(width>1280 || (width&1) || (height&1)){fclose(f);return 0;}
    while(pb_busy())Sleep(0);
    const volatile unsigned char* src=(const volatile unsigned char*)pb_back_buffer();
    fprintf(f,"P6\n%u %u\n255\n",width/step,height/step);
    int ok=1;
    for(unsigned y=0;y<height && ok;y+=step){
        for(unsigned x=0;x<width;x+=step){
            unsigned char* p=row+3*(x/step);
            if(xb_display_bpp==16){
                unsigned c=((const volatile uint16_t*)(src+y*pitch))[x];
                p[0]=((c>>11)&31)*255/31;p[1]=((c>>5)&63)*255/63;p[2]=(c&31)*255/31;
            }else{
                unsigned c=((const volatile uint32_t*)(src+y*pitch))[x];
                p[0]=c>>16;p[1]=c>>8;p[2]=c;
            }
        }
        ok=fwrite(row,1,3*(width/step),f)==3*(width/step);
    }
    return fclose(f)==0 && ok;
}
int xb_hardware_capture(unsigned event) {return capture_frame(event,2);}
void xb_hardware_frame(uint8_t* rdram,unsigned frame) {
    static unsigned next_poll,images;
    unsigned poll=sote_input_status[0];
    if(capture_next<capture_count && poll>=capture_polls[capture_next]){
        unsigned id=100+capture_next++;
        if(xb_trace_preview_tiles)write_preview_tiles(id);
        xb_hardware_note(capture_frame(id,1)?"capture":"capture-failed",id);
    }
    tile_count=0;
    if(timestamps && poll>=time_first && poll<=time_last && time_count<time_capacity){
        if(poll==time_first || poll==time_last){
            uint32_t* pair=poll==time_first?scanout_first:scanout_last;
            /* Repeat if a vblank interrupted the endpoint snapshot. */
            do {
                pair[0]=scanout_retraces;pair[1]=scanout_changes;pair[2]=pb_get_vbl_counter();
                time_scanout=(poll==time_first);
            } while(pair[0]!=scanout_retraces);
        }
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        timestamps[time_count++]=(frame_timestamp){frame,poll,now.QuadPart};
    }
    if(first_profile_poll && (!frame || (poll>=first_profile_poll && poll<=last_profile_poll))){
        unsigned event=MEM_HU(0,(int32_t)0x8013CE0E);
        xb_perf_checkpoint(frame,poll,event);
    }
    if(next_poll==UINT32_MAX || poll<next_poll)return;
    if(poll>=stop_poll){
        if(timestamps)flush_timestamps();
        if(samples && !flush_samples())xb_hardware_note("sample-write-failed",sample_count);
        xb_perf_flush_checkpoints();
        xb_hardware_note("sweep-ended",poll);
        next_poll=UINT32_MAX;
        return;
    }
    next_poll=poll+12;
    unsigned event=MEM_HU(0,(int32_t)0x8013CE0E),target=sote_scene_fixture_status[2];
    if(!first_profile_poll)xb_perf_checkpoint(frame,poll,event);
    unsigned paused=MEM_W(0,(int32_t)0x800DD2BC),comm=MEM_HU(0,(int32_t)0x800DD2D0);
    unsigned pos[3]={0};
    if(sote_vehicle_status[3]==event && sote_vehicle_status[0]){
        for(unsigned k=0;k<3;k++)pos[k]=sote_vehicle_status[7+k];
    }else if((event==4 || event==8) && sote_player_status[4]==event && sote_player_status[1]>=0x80000000u &&
             sote_player_status[1]-0x80000000u<=xb_rdram_committed_limit-0x5cu){
        for(unsigned k=0;k<3;k++)pos[k]=MEM_W(0x50+4*k,(int32_t)sote_player_status[1]);
    }else if(sote_diagnostic_status[7]==event){
        unsigned newest=0;
        for(unsigned i=0;i<64;i++){
            const volatile uint32_t* pose=sote_player_trace[i];
            if(pose[0]<=newest || poll-pose[1]>60)continue;
            for(unsigned k=0;k<3;k++)pos[k]=pose[5+k];
            newest=pose[0];
        }
    }
    gameplay_sample row={GetTickCount(),frame,poll,event,target,paused,comm,sote_input_status[5],
        sote_input_status[1],(int)sote_input_status[2],(int)sote_input_status[3],sote_diagnostic_status[3],
        sote_vehicle_status[0],pos[0],pos[1],pos[2],sote_triangle_status[0],sote_audio_status.nonzero,
        sote_audio_status.completed,player_shots};
    if(samples){
        if(sample_count<sample_capacity)samples[sample_count++]=row;
        else xb_hardware_note("sample-overflow",poll);
    }else{
        FILE* f=fopen("D:\\hardware.csv","a");
        if(f){write_sample(f,&row);fclose(f);}
    }
    if(event>0 && event<31 && target==event && poll-sote_scene_fixture_status[3]>=275 &&
       paused!=1 && comm!=1 && !(images&(1u<<event)) && xb_hardware_capture(event)){
        images|=1u<<event;xb_hardware_note("capture",event);
    }
}
