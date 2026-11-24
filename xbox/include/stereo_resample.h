#ifndef SOTE_XBOX_STEREO_RESAMPLE_H
#define SOTE_XBOX_STEREO_RESAMPLE_H

/* The title supplies 22,050 Hz stereo; the Xbox plays 48,000 Hz stereo.
   SDL restarts its source clock at zero for each conversion and supplies both
   padding buffers. Precompute the weights for those same output positions. */
enum { XB_AUDIO_FRAMES = 2048, XB_AUDIO_TAPS = 6, XB_AUDIO_PHASES = 320 };
typedef struct {
    unsigned count;
    double weight[XB_AUDIO_TAPS];
} xb_filter_wing;
/* 22050/48000 reduces to 147/320. At an exact filter-table boundary SDL's
   double clock can fall on either side. Keep both versions of that wing. */
static xb_filter_wing xb_filter_wings[XB_AUDIO_PHASES+1][2];
static struct { unsigned short index,wing[2]; } xb_stereo_phases[XB_AUDIO_FRAMES];

static void xb_prepare_stereo_filter(const float* filter,const float* delta,int stride,int size) {
    double time=0;
    for (unsigned i=0;i<XB_AUDIO_FRAMES;++i,time+=1.0/48000) {
        unsigned index=(unsigned)(time*22050);
        xb_stereo_phases[i].index=(unsigned short)index;
        double start=index/22050.0, end=(index+1)/22050.0;
        double left=1.0-(end-time)/(end-start);
        for (unsigned wing=0;wing<2;++wing) {
            double fraction=wing?1.0-left:left;
            int tap=(int)(fraction*stride);
            unsigned phase=(unsigned)(fraction*XB_AUDIO_PHASES+0.5);
            unsigned below=tap<(int)(phase*stride/XB_AUDIO_PHASES);
            xb_stereo_phases[i].wing[wing]=(unsigned short)(2*phase+below);
            xb_filter_wing* coeff=&xb_filter_wings[phase][below];
            unsigned count=0;
            for (;tap<size;tap+=stride)
                coeff->weight[count++]=filter[tap]+fraction*delta[tap];
            coeff->count=count;
        }
    }
}

static int xb_resample_stereo(const float* before,const float* after,const float* input,
                             int frames,int padding,float* output,int count) {
    for (int i=0;i<count;++i) {
        int index=xb_stereo_phases[i].index;
        float left=0,right=0;
        for (int wing=0;wing<2;++wing) {
            unsigned id=xb_stereo_phases[i].wing[wing];
            const xb_filter_wing* coeff=&xb_filter_wings[id>>1][id&1];
            for (int j=0;j<(int)coeff->count;++j) {
                int frame=wing?index+1+j:index-j;
                const float* sample=frame<0?before+2*(padding+frame):
                                    frame>=frames?after+2*(frame-frames):input+2*frame;
                double weight=coeff->weight[j];
                left+=(float)(sample[0]*weight);
                right+=(float)(sample[1]*weight);
            }
        }
        *output++=left;
        *output++=right;
    }
    return count*2*(int)sizeof(float);
}
#endif
