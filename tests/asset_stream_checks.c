/* Cooperative reader test. Executes the real parser/queue/ownership code;
 * fake files and a bounded allocator replace Vita OS I/O and threading. */
#include "asset_stream.h"
#include <psp2/kernel/threadmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "generated_caption_fixture.h"
#include "caption_index.h"
#include "generated_subtitle_pack.h"
extern int asset_stream_test_step(void);
enum {OK,MISSING,BAD_MAGIC,SHORT_HEADER,ZERO_WIDTH,SHORT_PIXELS,OVERSIZE,BIG,CAPTION,BAD_CAPTION,SEEK_FAILED,GRADIENT,INDEX};
typedef struct {int active,handle;unsigned position;} FakeFile;
static FakeFile files[16];static unsigned modes[ASSET_SLOTS],reads[ASSET_SLOTS];
static unsigned char index_data[12+sizeof(packed_subtitles)/sizeof(*packed_subtitles)*12];
static unsigned char heap[10][2*1024*1024];static unsigned allocated[10],open_handle,create_calls,free_calls;
int snprintf(char *out,size_t size,const char *format,...){va_list args;va_start(args,format);if(format[0]=='a')open_handle=(unsigned)va_arg(args,int);else{(void)va_arg(args,const char*);open_handle=(unsigned)va_arg(args,int);}va_end(args);if(size)out[0]=0;return 0;}
static int contains(const char *s,const char *n){for(;*s;++s){unsigned i=0;while(n[i]&&s[i]&&n[i]==s[i])++i;if(!n[i])return 1;}return 0;}
FILE *fopen(const char *p,const char *m){(void)m;if(contains(p,"subtitles.idx"))open_handle=3500;else if(contains(p,"subtitles.pack"))open_handle=1024;if(modes[open_handle]==MISSING)return NULL;for(int i=0;i<16;++i)if(!files[i].active){files[i]=(FakeFile){1,(int)open_handle,0};return (FILE*)&files[i];}return NULL;}
int fclose(FILE *fp){((FakeFile*)fp)->active=0;return 0;}
int fseek(FILE *fp,long offset,int origin){FakeFile *f=(FakeFile*)fp;(void)origin;if(modes[f->handle]==SEEK_FAILED)return -1;f->position=origin==SEEK_END?(unsigned)sizeof(caption_fixture)+(unsigned)offset:(unsigned)offset;return 0;}
long ftell(FILE *f){return (long)((FakeFile*)f)->position;}
int setvbuf(FILE *f,char *b,int mode,size_t n){(void)f;(void)b;(void)mode;(void)n;return 0;}
size_t fread(void *dest,size_t size,size_t count,FILE *fp){
    FakeFile *f=(FakeFile*)fp;unsigned mode=modes[f->handle];unsigned char header[12]={'F','S','P','I',4,0,4,0,0,0,0,0};unsigned length=76;
    if(mode==INDEX){unsigned n=(unsigned)(size*count);if(n>sizeof(index_data)-f->position)n=sizeof(index_data)-f->position;memcpy(dest,index_data+f->position,n);f->position+=n;++reads[f->handle];return n/size;}
    if(mode==CAPTION||mode==BAD_CAPTION){unsigned n=(unsigned)(size*count);if(n>sizeof(caption_fixture)-f->position)n=sizeof(caption_fixture)-f->position;
        unsigned char *p=dest;for(unsigned i=0;i<n;++i)p[i]=mode==BAD_CAPTION?0:caption_fixture[f->position+i];f->position+=n;++reads[f->handle];return n/size;}
    if(mode==BAD_MAGIC)header[0]='X';
    if(mode==SHORT_HEADER)length=8;
    if(mode==ZERO_WIDTH)header[4]=0;
    if(mode==SHORT_PIXELS)length=30;
    if(mode==OVERSIZE){header[4]=0;header[5]=16;header[6]=0;header[7]=8;}
    if(mode==BIG){header[4]=0;header[5]=4;header[6]=0;header[7]=2;length=12+2*1024*1024;}
    unsigned n=(unsigned)(size*count);if(n>length-f->position)n=length-f->position;
    unsigned char *p=dest;for(unsigned i=0;i<n;++i){unsigned pos=f->position++;p[i]=pos<12?header[pos]:(unsigned char)f->handle;
        if(pos>=12&&mode==GRADIENT){unsigned pixel=(pos-12)/4,ch=(pos-12)%4;p[i]=ch==0?(pixel%4)*60:ch==1?(pixel/4)*60:ch==2?0:255;}}
    ++reads[f->handle];return n/size;
}
void *malloc(size_t n){if(n>sizeof(heap[0]))return NULL;for(int i=0;i<10;++i)if(!allocated[i]){allocated[i]=1;return heap[i];}return NULL;}
void free(void *p){if(!p)return;for(int i=0;i<10;++i)if(p==heap[i]){allocated[i]=0;++free_calls;return;}}
SceUID sceKernelCreateThread(const char *name,SceKernelThreadEntry entry,int priority,SceSize size,SceUInt attr,int affinity,const SceKernelThreadOptParam *option){(void)name;(void)entry;(void)priority;(void)size;(void)attr;(void)option;++create_calls;return affinity?-1:21;}
int sceKernelStartThread(SceUID id,SceSize args,void *p){(void)id;(void)args;(void)p;return 0;}
int sceKernelWaitThreadEnd(SceUID id,int *status,SceUInt *timeout){(void)id;(void)status;(void)timeout;return 0;}
int sceKernelDeleteThread(SceUID id){(void)id;return 0;}
int sceKernelDelayThread(SceUInt n){(void)n;return 0;}
SceInt64 sceKernelGetSystemTimeWide(void){static SceInt64 clock;return ++clock;}
#define CHECK(c,n) do{if(!(c))return 600+(n);}while(0)
static void put32(unsigned char *p,unsigned v){for(int i=0;i<4;++i)p[i]=(unsigned char)(v>>(i*8));}
int run_checks(void){
    memcpy(index_data,"FCIX",4);put32(index_data+4,1);put32(index_data+8,sizeof(packed_subtitles)/sizeof(*packed_subtitles));modes[3500]=INDEX;
    for(unsigned i=0;i<sizeof(packed_subtitles)/sizeof(*packed_subtitles);++i){unsigned char *p=index_data+12+i*12;put32(p,0);put32(p+4,sizeof(caption_fixture));p[8]=CAPTION_WIDTH&255;p[9]=CAPTION_WIDTH>>8;p[10]=CAPTION_HEIGHT&255;p[11]=CAPTION_HEIGHT>>8;}
    CaptionEntry parsed[1];unsigned char tiny[24]={'F','C','I','X',1,0,0,0,1,0,0,0,0,0,0,0,5,0,0,0,20,0,30,0};
    CHECK(!caption_index_decode(tiny,sizeof(tiny),parsed,1)&&parsed[0].width==20&&parsed[0].height==30&&parsed[0].size==5,31);
    tiny[0]='X';CHECK(caption_index_decode(tiny,sizeof(tiny),parsed,1)<0,32);tiny[0]='F';
    CHECK(caption_index_decode(tiny,sizeof(tiny)-1,parsed,1)<0,33);tiny[20]=0;CHECK(caption_index_decode(tiny,sizeof(tiny),parsed,1)<0,34);

    CHECK(!asset_stream_start()&&create_calls==2,1);AssetBuffer b={0};
    asset_stream_request(1,0);asset_stream_request(2,1);CHECK(asset_stream_pending()==2,2);CHECK(asset_stream_test_step()==1&&!reads[1]&&reads[2],3);
    CHECK(asset_stream_take(2,&b)&&b.width==4&&b.height==4&&b.bytes==64&&b.pixels[0]==2,4);CHECK(!asset_stream_take(2,&b),5);asset_stream_release(&b);CHECK(!b.pixels,6);
    CHECK(asset_stream_test_step()==1&&asset_stream_take(1,&b),7);asset_stream_release(&b);
    asset_stream_request(4,0);asset_stream_request(3,1);asset_stream_request(4,1);asset_stream_test_step();CHECK(reads[4]&&!reads[3],8);CHECK(asset_stream_take(4,&b),9);asset_stream_release(&b);asset_stream_test_step();CHECK(asset_stream_take(3,&b),10);asset_stream_release(&b);
    modes[31]=GRADIENT;asset_stream_request(31,1);asset_stream_test_step();CHECK(asset_stream_take(31,&b)&&b.width==3&&b.height==3&&b.bytes==36,20);
    CHECK(b.pixels[0]==10&&b.pixels[1]==10&&b.pixels[3]==255&&b.pixels[16]==90&&b.pixels[17]==90&&b.pixels[32]==170&&b.pixels[33]==170&&b.pixels[35]==255,21);asset_stream_release(&b);
    for(int mode=MISSING;mode<=OVERSIZE;++mode){int h=100+mode;modes[h]=(unsigned)mode;asset_stream_request(h,1);asset_stream_test_step();CHECK(!asset_stream_take(h,&b)&&asset_stream_errors()==(unsigned)mode,11);asset_stream_request(h,1);CHECK(!asset_stream_pending()&&!asset_stream_test_step(),12);}
    for(int i=200;i<208;++i){modes[i]=BIG;asset_stream_request(i,0);asset_stream_test_step();}
    asset_stream_request(200,1);modes[208]=BIG;asset_stream_request(208,0);asset_stream_test_step();
    CHECK(!asset_stream_take(201,&b)&&asset_stream_take(200,&b)&&b.bytes==2*1024*1024,13);asset_stream_release(&b);
    CHECK(asset_stream_take(208,&b)&&b.pixels[b.bytes-1]==208,14);asset_stream_release(&b);
    CHECK(free_calls>0&&asset_stream_read_us()>0,15);
    modes[1024]=CAPTION;asset_stream_request(1024,1);asset_stream_test_step();
    CHECK(asset_stream_take(1024,&b)&&b.width==CAPTION_WIDTH&&b.height==CAPTION_HEIGHT&&b.bytes==CAPTION_WIDTH*CAPTION_HEIGHT*4,17);
    CHECK(b.pixels[3]==223&&b.pixels[0]==0&&b.pixels[CAPTION_WHITE*4]==255&&b.pixels[CAPTION_WHITE*4+3]==255,18);asset_stream_release(&b);
    unsigned prior=asset_stream_errors();modes[1024]=BAD_CAPTION;asset_stream_request(1024,1);asset_stream_test_step();CHECK(!asset_stream_take(1024,&b)&&asset_stream_errors()==prior+1,19);
    modes[12]=GRADIENT;asset_stream_request(12,1);asset_stream_test_step();CHECK(asset_stream_take(12,&b)&&b.width==4&&b.height==4&&b.bytes==16,22);
    CHECK(b.pixels[0]==0&&b.pixels[1]==18&&b.pixels[4]==35&&b.pixels[15]==160,23);asset_stream_release(&b);
    asset_stream_request(300,0);asset_stream_request(301,1);asset_stream_request(302,2);asset_stream_test_step();CHECK(reads[302]&&!reads[301]&&!reads[300],24);
    CHECK(asset_stream_take(302,&b),25);asset_stream_release(&b);
    static unsigned char wanted[ASSET_SLOTS];wanted[300]=1;asset_stream_retain_requests(wanted);CHECK(asset_stream_pending()==1,26);
    asset_stream_test_step();CHECK(reads[300]&&!reads[301]&&asset_stream_take(300,&b),27);asset_stream_release(&b);
    asset_stream_request(303,2);asset_stream_request(304,2);memset(wanted,0,sizeof(wanted));wanted[303]=3;wanted[304]=4;
    asset_stream_retain_requests(wanted);asset_stream_test_step();CHECK(reads[304]&&!reads[303],28);
    CHECK(asset_stream_take(304,&b),29);asset_stream_release(&b);asset_stream_test_step();CHECK(asset_stream_take(303,&b),30);asset_stream_release(&b);
    asset_stream_stop();for(int i=0;i<10;++i)CHECK(!allocated[i],16);return 0;
}
