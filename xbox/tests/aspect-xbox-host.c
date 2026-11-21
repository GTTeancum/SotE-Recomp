#include "aspect_xbox.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static int same(double a,double b){return fabs(a-b)<1e-5;}
static void rect(int w,int h,int cent,int x0,int x1){
    const int f[4]={0,0,320,240};
    const xb_pixel_rect r=xb_aspect_pixels(f,w,h,cent);
    assert(r.left==x0&&r.top==0&&r.right==x1&&r.bottom==h);
    const int a[4]={0,0,157,240},b[4]={157,0,320,240};
    assert(xb_aspect_pixels(a,w,h,cent).right==xb_aspect_pixels(b,w,h,cent).left);
    const int o[4]={-4,-8,328,245};
    if(!cent){const xb_pixel_rect c=xb_aspect_pixels(o,w,h,0);assert(c.left==0&&c.top==0&&c.right==w&&c.bottom==h);}
}
static void compare(int w,int h,double display_sx){
    /* One unit of camera-plane displacement projects the same as vertically
       after display correction. Stock NDC X ratio for a square is 3:4. */
    const double x=xb_aspect_clip_x(.75f,1,1)*w/2*display_sx;
    const double y=(double)h/2;
    assert(same(x,y));
    assert(same((xb_aspect_ui_x(200,1)-xb_aspect_ui_x(160,1))*w/320*display_sx,
                (40.0/240)*h));
    assert(same(xb_aspect_ui_x(160,1),160));
}
int main(void){
    assert(xb_aspect_perspective(0,0,-1));
    assert(xb_aspect_perspective(.0625f,0,0));
    assert(!xb_aspect_perspective(0,-0.0f,0));
    assert(xb_aspect_clip_x(5,1,0)==5);
    assert(xb_aspect_clip_x(5,0,1)==5);
    assert(xb_aspect_ui_x(55,0)==55);
    float left_edge[]={-1.25f,.125f,.25f,1};
    assert(xb_aspect_outcode(left_edge)==1);
    left_edge[0]=xb_aspect_clip_x(left_edge[0],1,1);
    assert(xb_aspect_outcode(left_edge)==0);
    float right_edge[]={1.5f,.125f,.25f,1};
    assert(xb_aspect_outcode(right_edge)==2);
    right_edge[0]=xb_aspect_clip_x(right_edge[0],1,1);
    assert(xb_aspect_outcode(right_edge)==2);
    assert(right_edge[1]==.125f&&right_edge[2]==.25f&&right_edge[3]==1);
    rect(640,480,0,0,640);rect(640,480,1,80,560);
    rect(1280,720,0,0,1280);rect(1280,720,1,160,1120);
    compare(640,480,4.0/3);compare(1280,720,1.0);
    puts("PASS: preclip expansion, stock identity, perspective/orthographic classification, 480p anamorphic/720p square pixel proportions, scissor bounds/edges");
    return 0;
}
