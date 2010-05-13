
#if 1
#define COLOR_B 24
#define COLOR_G 16
#define COLOR_R 8
#else
#define COLOR_B 0
#define COLOR_G 8
#define COLOR_R 16
#endif

#define BLOCKS 512
#define MAXBLOCKS (16*BLOCKS)

#define BLOCKSIZE 512
#define FFTSIZE (2*BLOCKSIZE)

#if 1
#define VIDEO_W 256
#define VIDEO_H 128
#define VIDEO_SCALE 2
#else
#define VIDEO_W 64
#define VIDEO_H 32
#define VIDEO_SCALE 8
#endif

#define KOZ 8

#define SEARCHRANGE1 100
#define SEARCHRANGE2 25000

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <stdint.h>
#include <math.h>


#ifdef SMP
#if !defined(__CYGWIN__) && !defined(__MINGW32__)
#include <pthread.h>
#else
#include <windows.h>
#endif
#endif


#include <fftw3.h>

#include <a52dec/a52.h>
#ifdef DTS
#include <dca.h>
#endif

#include "bswap.h"
#include "wavhdr.h"
#include "ac3hdr.h"

#include "version.h"

#include <SDL/SDL.h>
#include <SDL/SDL_main.h>
SDL_Surface *screen=NULL;

int noiselimit=5;

typedef struct {
    float s_max;
    float fft_avg;
    float fft_max;
} envdata_t;


typedef struct buffer_s {
    struct buffer_s* ref;
    unsigned char* fft;
#ifdef CEPSTRUM
    unsigned char* cft;
    float cft_max=0;
#endif
    envdata_t* env;
    envdata_t env_max;
    long pos;
    long len;
    int step;
    float avg;    // FFT "brightness", 32.0 default
    double fftavg[FFTSIZE];
    double total_avg;
    long total_cnt;
//    double* fftavg_ref;
//    float scale;  // playback volume
    //
    int b_pos;
    int b_min;
    int b_max;
    int maxblocks;
    //
    int rs_mode;
    FILE* wavfile;
    int samplerate;
    int ac3_blocksize;
    int ac3_framelen;
    int ac3_channels;
    //
    double fps;
    int frames;   // total frames in file
    int framepos; // first frame in buffer->video[]
    FILE* rawfile;// FILE fp
    unsigned char* video; // frame data
    //
    void* state;      // a52/dca state
    sample_t* samples;// a52/dca samples[]
    unsigned int id;  // cache ID
    //
    float* fft_in;
    fftwf_complex* fft_out;
    fftwf_plan plan;
} buffer_t;


int search_aud(buffer_t* buffer0,buffer_t* buffer1){
    int best_e=2000000000,best_p=0;
    int pos1=buffer0->b_pos-BLOCKS/2;
    int pos2=buffer1->b_pos-BLOCKS/2;
    int min=pos2-BLOCKS/2;
    int max=pos2+BLOCKS/2;
    int p;
    for(p=min;p<=max;p++){
	long long e=0;
	int j;
	for(j=0;j<BLOCKS;j++){
	    int i;
	    unsigned char* ptr1=&buffer0->fft[BLOCKSIZE*(pos1+j)];
	    unsigned char* ptr2=&buffer1->fft[BLOCKSIZE*(p+j)];
	    unsigned int e1=0;
	    for(i=0;i<BLOCKSIZE;i+=4){
		int t0=(int)(ptr1[i+0])-(int)(ptr2[i+0]);
		int t1=(int)(ptr1[i+1])-(int)(ptr2[i+1]);
		int t2=(int)(ptr1[i+2])-(int)(ptr2[i+2]);
		int t3=(int)(ptr1[i+3])-(int)(ptr2[i+3]);
		e1+=t0*t0+t1*t1+t2*t2+t3*t3;
	    }
	    e+=e1;
	}
	
	e/=BLOCKSIZE*BLOCKS;
	if(e<best_e){ best_p=p-pos2; best_e=e; }
    }
    printf("... range %d - %d  BEST: %d (E: %d)\n",min,max,best_p,(int)sqrt(best_e));
    return best_p;
}

int search_vid(buffer_t* buffer0,buffer_t* buffer1,int step){
    if(!buffer0->rawfile || !buffer1->rawfile) return 0;
    int best_e=2000000000,best_p=0;
    int p;
    for(p=0;p<BLOCKS;p++){
//printf("search_vid! p=%d\n",p);
	long long e=0;
	int j;
	for(j=0;j<BLOCKS;j++){
	    unsigned char* ptr1=&buffer0->video[VIDEO_W*VIDEO_H*(j+BLOCKS/2)];
	    unsigned char* ptr2=&buffer1->video[VIDEO_W*VIDEO_H*(j+p)];
	    unsigned int e1=0;
	    int y;
	    for(y=step/2;y<VIDEO_H;y+=step){
		int i;
		for(i=0;i<VIDEO_W;i+=4){
//printf("search_vid! p=%d y=%d i=%d p1=%p p2=%p\n",p,y,i,ptr1,ptr2);
		    int t0=(int)(ptr1[i+0])-(int)(ptr2[i+0]);
		    int t1=(int)(ptr1[i+1])-(int)(ptr2[i+1]);
		    int t2=(int)(ptr1[i+2])-(int)(ptr2[i+2]);
		    int t3=(int)(ptr1[i+3])-(int)(ptr2[i+3]);
		    e1+=t0*t0+t1*t1+t2*t2+t3*t3;
		}
		ptr1+=VIDEO_W*step;
		ptr2+=VIDEO_W*step;
	    }
	    e+=e1;
	}
	e/=VIDEO_W*VIDEO_H*BLOCKS;
	if(e<best_e){ best_p=p-BLOCKS/2; best_e=e; }
    }
    printf("... range %d - %d  BEST: %d (E: %d)\n",-BLOCKS/2,BLOCKS/2,best_p,(int)sqrt(best_e));
    return best_p;
}



#if 0
void draw(buffer_t* buffer,int pos,int y0,int showdelay){
    int p1= pos - (screen->w - BLOCKS)/2;
    int y;
    for(y=0;y<256;y++){
	unsigned int* ptr= screen->pixels + (y0+y)*screen->pitch;
	unsigned char* src= &buffer->fft[p1*BLOCKSIZE+y];
	int d1=screen->w/2,d2=screen->w/2;
	if(showdelay<0) d1+=showdelay; else d1-=showdelay;
//	printf("delay area: %d .. %d\n",d1,d2);
	int x;
	for(x=0;x<screen->w;x++){
	    unsigned int c = (x+p1<0 || x+p1>=buffer->len) ? 0 : *src;
	    c*=0x01010101;
//	    if(x>=d1 && x<=d2) c|=(showdelay<0) ? 0x400000 : 0x4000; else
	    if((x+p1<pos)||(x+p1>=pos+BLOCKS)) c|=0x80;
	    ptr[x]=c;
	    
//	    ptr[x]=((x+p1<pos)||(x+p1>=pos+BLOCKS)||(x>=d1 && x<=d2)) ? ((0x01010101*c)|0x80) : 0x01010101*c;
//	    ptr[x]=((x+p1<pos)||(x+p1>=pos+BLOCKS)) ? 0xFFFF00 : 0x01010101*c;
	    src+=BLOCKSIZE;
	}
    }
}
#endif

void draw(buffer_t* buffer1,int y0){
    int y;
    for(y=0;y<BLOCKSIZE/2;y++){
	int y1=(y<BLOCKSIZE/4)?y:BLOCKSIZE/4+(y-BLOCKSIZE/4)*2;
	unsigned int* ptr= screen->pixels + (y0+y)*screen->pitch;
	unsigned char* src=&(buffer1->fft[(buffer1->b_pos-BLOCKS)*BLOCKSIZE+y1]);
	int x;
	for(x=0;x<2*BLOCKS;x+=4){
	    ptr[x+0]= src[0*BLOCKSIZE]*0x01010101;
	    ptr[x+1]= src[1*BLOCKSIZE]*0x01010101;
	    ptr[x+2]= src[2*BLOCKSIZE]*0x01010101;
	    ptr[x+3]= src[3*BLOCKSIZE]*0x01010101;
	    src+=BLOCKSIZE*4;
	}
    }
}

#ifdef CEPSTRUM
void draw_cft(buffer_t* buffer1,int y0){
    int y;
    for(y=0;y<BLOCKSIZE/2;y++){
	int y1=(y<BLOCKSIZE/4)?y:BLOCKSIZE/4+(y-BLOCKSIZE/4)*2;
	unsigned int* ptr= screen->pixels + (y0+y)*screen->pitch;
	unsigned char* src=&(buffer1->cft[(buffer1->b_pos-BLOCKS)*BLOCKSIZE+y1]);
	int x;
	for(x=0;x<2*BLOCKS;x+=4){
	    ptr[x+0]= src[0*BLOCKSIZE]*0x01010101;
	    ptr[x+1]= src[1*BLOCKSIZE]*0x01010101;
	    ptr[x+2]= src[2*BLOCKSIZE]*0x01010101;
	    ptr[x+3]= src[3*BLOCKSIZE]*0x01010101;
	    src+=BLOCKSIZE*4;
	}
    }
}
#endif

//#ifdef FILTER
static unsigned char fft_filter[2*BLOCKS*BLOCKSIZE];
//#endif

static inline int gauss(unsigned char *p){
    return (p[-1]*0.3 + p[0] + p[1]*0.3)/1.6;
//    return p[-2]*0.2 + p[-1]*0.5 + p[0] + p[1]*0.5 + p[2]*0.2;
//    return (p[-3]*0.2 + p[-2]*0.5 + p[-1]*0.8 + p[0] + p[1]*0.8 + p[2]*0.5 + p[3]*0.2) / (0.2+0.5+0.8+1+0.8+0.5+0.2);
}

#define SQR(x) ((x)*(x))

void draw_diff(buffer_t* buffer1,buffer_t* buffer2,int y0,int mode){
    int y;
#ifdef FILTER
    unsigned long filtertemp[2*BLOCKS];
    memset(filtertemp,0,sizeof(unsigned long)*2*BLOCKS);
#endif
    for(y=0;y<BLOCKSIZE;y++){
	unsigned int* ptr= screen->pixels + (y0+y)*screen->pitch;
	unsigned char* src1=&(buffer1->fft[(buffer1->b_pos-BLOCKS)*BLOCKSIZE+y]);
	unsigned char* src2=&(buffer2->fft[(buffer2->b_pos-BLOCKS)*BLOCKSIZE+y]);
	if(mode==0){
	  // "yellow"
	  int x;
	  for(x=0;x<2*BLOCKS;x+=4){
	    ptr[x+0]= (src1[0*BLOCKSIZE]<<COLOR_R) | (src2[0*BLOCKSIZE]<<COLOR_G);
	    ptr[x+1]= (src1[1*BLOCKSIZE]<<COLOR_R) | (src2[1*BLOCKSIZE]<<COLOR_G);
	    ptr[x+2]= (src1[2*BLOCKSIZE]<<COLOR_R) | (src2[2*BLOCKSIZE]<<COLOR_G);
	    ptr[x+3]= (src1[3*BLOCKSIZE]<<COLOR_R) | (src2[3*BLOCKSIZE]<<COLOR_G);
#ifdef FILTER
	    filtertemp[x+0]+=SQR(src1[0*BLOCKSIZE]-src2[0*BLOCKSIZE]);
	    filtertemp[x+1]+=SQR(src1[1*BLOCKSIZE]-src2[1*BLOCKSIZE]);
	    filtertemp[x+2]+=SQR(src1[2*BLOCKSIZE]-src2[2*BLOCKSIZE]);
	    filtertemp[x+3]+=SQR(src1[3*BLOCKSIZE]-src2[3*BLOCKSIZE]);
#endif
	    src1+=BLOCKSIZE*4; src2+=BLOCKSIZE*4;
	  }
	} else
	if(mode==1){
	  // diff. only!
	  int x;
	  for(x=0;x<2*BLOCKS;x++){
	    int a=src1[-1*BLOCKSIZE]; a-=(int)src2[-1*BLOCKSIZE];
	    int b=src1[0*BLOCKSIZE]; b-=(int)src2[0*BLOCKSIZE];
	    int c=src1[1*BLOCKSIZE]; c-=(int)src2[1*BLOCKSIZE];
	    if(a<0) a=-a;
	    if(b<0) b=-b;
	    if(c<0) c=-c;
	    if(a*3<b && c*3<b) b=(a+c)/2;
	    if(b>255) b=255;
	    fft_filter[x*BLOCKSIZE+y]=b;
	    ptr[x]= b*0x01010101;
	    src1+=BLOCKSIZE; src2+=BLOCKSIZE;
	  }
	} else
	if(mode==2 || mode==3){
	  // smoothed diff. only!
	  int x;
	  for(x=0;x<2*BLOCKS;x++){
	    int b=gauss(src1) - gauss(src2);
	    if(mode==3) b=(b*b)>>6; else if(b<0) b=-b;
	    if(b>255) b=255;
	    fft_filter[x*BLOCKSIZE+y]=b;
	    ptr[x]= b*0x01010101;
	    src1+=BLOCKSIZE; src2+=BLOCKSIZE;
	  }
	} else
	if(mode==4 || mode==5){
	  // smoothed diff. only!
	  int x;
	  for(x=0;x<2*BLOCKS;x++){
//	    int a=gauss(src1) + 0.8*gauss(src1-BLOCKSIZE) + 0.8*gauss(src1+BLOCKSIZE);
//	    int b=gauss(src2) + 0.8*gauss(src2-BLOCKSIZE) + 0.8*gauss(src2+BLOCKSIZE);
//	    b=(a-b)/(1+0.8+0.8);
	    int a=gauss(src1) + 0.8*gauss(src1-BLOCKSIZE) + 0.8*gauss(src1+BLOCKSIZE) + 0.5*gauss(src1-2*BLOCKSIZE) + 0.5*gauss(src1+2*BLOCKSIZE);
	    int b=gauss(src2) + 0.8*gauss(src2-BLOCKSIZE) + 0.8*gauss(src2+BLOCKSIZE) + 0.5*gauss(src2-2*BLOCKSIZE) + 0.5*gauss(src2+2*BLOCKSIZE);
	    b=(a-b)/(1+0.8+0.8+0.5+0.5);
	    if(mode==5) b=(b*b)>>6; else if(b<0) b=-b;
	    if(b>255) b=255;
	    fft_filter[x*BLOCKSIZE+y]=b;
	    ptr[x]= b*0x01010101;
	    src1+=BLOCKSIZE; src2+=BLOCKSIZE;
	  }
	}
    }
#ifdef FILTER
    if(mode==0){
	int x;
	for(x=0;x<2*BLOCKS;x++){
	    unsigned long res=filtertemp[x] + filtertemp[x>0 ? x-1 : 0] + filtertemp[x<2*BLOCKS-1 ? x+1 : 2*BLOCKS-1];
//	    unsigned long res=filtertemp[x];
	    int b=sqrt(res)-1100;
	    if(b<0) b=0; else if(b>255) b=255;
//	    unsigned char b=(res<SQR(1000))?0:255;
//	    printf("%4d: %8ld -> %d\n",x,res,b);
	    memset(&fft_filter[x*BLOCKSIZE], b ,BLOCKSIZE);
	}
    }
#endif
}


void draw_env(buffer_t* buffer,int y0,int type,unsigned int color){
    if(!(type&3)) return; // disabled
    int x;
    for(x=0;x<2*BLOCKS;x+=1){
	int c=0;
	switch(type&3){
	case 1: c = buffer->env[x+buffer->b_pos-BLOCKS].s_max * 256.0 / buffer->env_max.s_max;break;
	case 2: c = buffer->env[x+buffer->b_pos-BLOCKS].fft_max * 256.0 / buffer->env_max.fft_max;break;
	case 3: c = buffer->env[x+buffer->b_pos-BLOCKS].fft_avg * 256.0 / buffer->env_max.fft_avg;break;
	}
	if(c<0) c=0;
	if(c>255) c=255;
	int y=y0-(c&0xFE);
	for(;y<y0;y+=2){
	    unsigned int* ptr= screen->pixels + y*screen->pitch;
	    ptr[x]|=color;
	}
    }
}

void draw_frame(buffer_t* buffer,int x0,int y0,int frameno){
    unsigned char frame[VIDEO_H][VIDEO_W];
    
    if(frameno<0 || frameno>=buffer->frames) return;
        
    fseeko(buffer->rawfile,(VIDEO_W*VIDEO_H)*(off_t)frameno,SEEK_SET);
    fread(&frame[0][0],VIDEO_W*VIDEO_H,1,buffer->rawfile);
    int y;
    for(y=0;y<VIDEO_H*VIDEO_SCALE;y++){
	unsigned int* ptr= screen->pixels + (y0+y)*screen->pitch;
	unsigned char* src=&frame[y/VIDEO_SCALE][0];
	ptr+=x0;
	int x;
	for(x=0;x<VIDEO_W;x++){
	    if(VIDEO_SCALE==2) ptr[2*x+0]= ptr[2*x+1]= src[x]*0x01010101;
	    if(VIDEO_SCALE==4) ptr[4*x+0]= ptr[4*x+1]= ptr[4*x+2]= ptr[4*x+3]= src[x]*0x01010101;
	    if(VIDEO_SCALE==8) ptr[8*x+0]= ptr[8*x+1]= ptr[8*x+2]= ptr[8*x+3]= ptr[8*x+4]= ptr[8*x+5]= ptr[8*x+6]= ptr[8*x+7]= src[x]*0x01010101;
	}
    }

}

void draw_frame_diff(buffer_t* buffer1,buffer_t* buffer2,int x0,int y0,int frameno1,int frameno2){
    unsigned char frame1[VIDEO_H][VIDEO_W];
    unsigned char frame2[VIDEO_H][VIDEO_W];
    
//    if(frameno<0 || frameno>=buffer->frames) return;

    fseeko(buffer1->rawfile,(VIDEO_W*VIDEO_H)*(off_t)frameno1,SEEK_SET);
    fread(&frame1[0][0],VIDEO_W*VIDEO_H,1,buffer1->rawfile);

    fseeko(buffer2->rawfile,(VIDEO_W*VIDEO_H)*(off_t)frameno2,SEEK_SET);
    fread(&frame2[0][0],VIDEO_W*VIDEO_H,1,buffer2->rawfile);

    int y;
    for(y=0;y<VIDEO_H*VIDEO_SCALE;y++){
	unsigned int* ptr= screen->pixels + (y0+y)*screen->pitch;
	unsigned char* src1=&frame1[y/VIDEO_SCALE][0];
	unsigned char* src2=&frame2[y/VIDEO_SCALE][0];
	ptr+=x0;
	int x;
	for(x=0;x<VIDEO_W;x++){
	    if(VIDEO_SCALE==2) ptr[2*x+0]= ptr[2*x+1]= (src1[x]<<COLOR_R) | (src2[x]<<COLOR_G);
	    if(VIDEO_SCALE==4) ptr[4*x+0]= ptr[4*x+1]= ptr[4*x+2]= ptr[4*x+3]=  (src1[x]<<COLOR_R) | (src2[x]<<COLOR_G);
	    if(VIDEO_SCALE==8) ptr[8*x+0]= ptr[8*x+1]= ptr[8*x+2]= ptr[8*x+3]= ptr[8*x+4]= ptr[8*x+5]= ptr[8*x+6]= ptr[8*x+7]=  (src1[x]<<COLOR_R) | (src2[x]<<COLOR_G);
	}
    }

}

void draw_video(buffer_t* buffer,int y0){
    int y;
    for(y=0;y<256;y++){
	unsigned int* ptr= screen->pixels + (y0+y)*screen->pitch;
//	unsigned char* src=&(buffer1->fft[(buffer1->b_pos-BLOCKS)*BLOCKSIZE+y1]);
	// 4, 12, 20, 28
	unsigned char* src= &buffer->video[(y&63)+((VIDEO_H/8)+(y/64)*(VIDEO_H/4))*VIDEO_W];
	int x;
	for(x=0;x<2*BLOCKS;x+=4){
	    ptr[x+0]= src[0*VIDEO_W*VIDEO_H]*0x01010101;
	    ptr[x+1]= src[1*VIDEO_W*VIDEO_H]*0x01010101;
	    ptr[x+2]= src[2*VIDEO_W*VIDEO_H]*0x01010101;
	    ptr[x+3]= src[3*VIDEO_W*VIDEO_H]*0x01010101;
	    src+=VIDEO_W*VIDEO_H*4;
	}
    }
}

void draw_video_diff(buffer_t* buffer1,buffer_t* buffer2,int y0){
    int y;
    for(y=0;y<512;y++){
	unsigned int* ptr= screen->pixels + (y0+y)*screen->pitch;
	unsigned char* src1=&buffer1->video[(y&63)+((VIDEO_H/8)+(y/64)*(VIDEO_H/4))*VIDEO_W];
	unsigned char* src2=&buffer2->video[(y&63)+((VIDEO_H/8)+(y/64)*(VIDEO_H/4))*VIDEO_W];
	int x;
	for(x=0;x<2*BLOCKS;x+=4){
	    ptr[x+0]= (src1[0*VIDEO_W*VIDEO_H]<<COLOR_R) | (src2[0*VIDEO_W*VIDEO_H]<<COLOR_G);
	    ptr[x+1]= (src1[1*VIDEO_W*VIDEO_H]<<COLOR_R) | (src2[1*VIDEO_W*VIDEO_H]<<COLOR_G);
	    ptr[x+2]= (src1[2*VIDEO_W*VIDEO_H]<<COLOR_R) | (src2[2*VIDEO_W*VIDEO_H]<<COLOR_G);
	    ptr[x+3]= (src1[3*VIDEO_W*VIDEO_H]<<COLOR_R) | (src2[3*VIDEO_W*VIDEO_H]<<COLOR_G);
	    src1+=VIDEO_W*VIDEO_H*4; src2+=VIDEO_W*VIDEO_H*4;
	}
    }
}

void draw_cut(int delay,int side,int y0){
    int min,max;
    min=max=screen->w/2;
    int adelay=(delay<0)?-delay:delay;
    int color=(delay<0)?0x404040|(0xff<<COLOR_G):0x404040|(0xff<<COLOR_R);
    if(!side){
	min-=adelay; if(min<0) min=0;
    } else {
	max+=adelay; if(max>=screen->w) max=screen->w-1;
    }
    int x,y;
    for(y=0;y<KOZ;y++){
	unsigned int* ptr= screen->pixels + (y0+y)*screen->pitch;
	for(x=min;x<=max;x++) ptr[x]=color;
    }
}



void alloc_buffer(buffer_t* buffer,int maxblocks){
    buffer->maxblocks=maxblocks;
    buffer->fft=malloc(maxblocks*BLOCKSIZE);
    memset(buffer->fft,0,maxblocks*BLOCKSIZE);
#ifdef CEPSTRUM
    buffer->cft=malloc(maxblocks*BLOCKSIZE);
    memset(buffer->cft,0,maxblocks*BLOCKSIZE);
#endif
    buffer->env=malloc(maxblocks*sizeof(envdata_t));
    memset(buffer->env,0,maxblocks*sizeof(envdata_t));
}

buffer_t* load_buffer(char* name){
    if(!name) return NULL;
    buffer_t* buffer=malloc(sizeof(buffer_t));
    memset(buffer,0,sizeof(buffer_t));
    alloc_buffer(buffer,MAXBLOCKS);
    // open WAV
    buffer->wavfile=fopen(name,"rb");
    ac3_blocksize=0;wav_channels=0;
    if(buffer->wavfile){
	read_wavhdr(buffer->wavfile);
	if(wav_channels){
	    buffer->ac3_channels=wav_channels;
	    buffer->ac3_framelen=wav_blocksize;
	} else {
	    rewind(buffer->wavfile);
	    read_ac3hdr(buffer->wavfile);
	    if(!ac3_blocksize){
		fclose(buffer->wavfile);
		buffer->wavfile=NULL;
		printf("Not audio file: %s\n",name);
	    }
	    buffer->ac3_channels=ac3_channels;
	    buffer->ac3_framelen=ac3_framelen;
	}
    }
    buffer->ac3_blocksize=ac3_blocksize;
    // get size:
    if(buffer->wavfile){
      fseeko(buffer->wavfile,0,SEEK_END);
      if(ac3_blocksize){
	buffer->len=ftell(buffer->wavfile)/ac3_blocksize*ac3_framelen;
	buffer->samplerate=ac3_samplerate;
#ifdef DTS
	if(ac3_channels&256){	// DTS!!!
	    buffer->state=dca_init(0);
	    buffer->samples=dca_samples(buffer->state);
	} else
#endif
	{   buffer->state=a52_init(1);
	    buffer->samples=a52_samples(buffer->state);
	}
      } else {
	buffer->len=ftello(buffer->wavfile)/wav_blocksize;
	buffer->samplerate=wav_samplerate;
      }
    } else buffer->samplerate=48000;
    //
    char name2[strlen(name)+10]; strcpy(name2,name);
    char* np=strrchr(name2,'.'); if(!np) np=name2+strlen(name2);
    strcpy(np,".raw");
    printf("Trying to open video file '%s'...\n",name2);
    buffer->rawfile=fopen(name2,"rb");
    if(buffer->rawfile){
        fseeko(buffer->rawfile,0,SEEK_END);
        buffer->frames=ftello(buffer->rawfile)/(VIDEO_W*VIDEO_H);
	buffer->framepos=-10000;
        printf("video len: %d frames\n",buffer->frames);
	buffer->video=malloc(2*BLOCKS*VIDEO_W*VIDEO_H); //2MB
    }
    //
    static int id=0;
    buffer->id=id++;
    return buffer;
}

buffer_t* dupe_buffer(buffer_t* orig,int maxblocks){
    if(!orig) return NULL;
    buffer_t* buffer=malloc(sizeof(buffer_t));
    memcpy(buffer,orig,sizeof(buffer_t));
    alloc_buffer(buffer,maxblocks);
    return buffer;
}


#define CACHESIZE 16

volatile unsigned int cache_serial=0;
volatile unsigned int cache_last[CACHESIZE]={0,};
volatile int cache_block[CACHESIZE]={-1,};
sample_t cache_samples[CACHESIZE][6][256];

// channel order is LFE, left, center, right, left surround, right surround.
int ch=-1; // -1 = mix all channels  0..5 = use only this channel

//float s_max=0;

int read_ac3_block(buffer_t* buf,unsigned int f_pos,float* fft_in,int maxlen,float* s_max){
    unsigned char buffer[4096];
    unsigned int block=f_pos / buf->ac3_framelen;
    unsigned int skip=f_pos - block*buf->ac3_framelen; // samples to skip before read
    int i=0;
    while(i<maxlen){
      int cache;
      ++cache_serial;
      for(cache=0;cache<CACHESIZE;cache++)
        if(cache_block[cache]==(block|(buf->id<<30))) break;
      if(cache<CACHESIZE){
	cache_last[cache]=cache_serial; // update!
	int b,j;
	for(b=0;b<buf->ac3_framelen/256;b++){
	    if(skip>=256){ skip-=256; continue; }
	    for(j=skip;j<256;j++){
		if(i>=maxlen) break;
		// process sample
		fft_in[i]=cache_samples[cache][b][j];
		if(fft_in[i]>*s_max) *s_max=fft_in[i];
		++i;
	    }
	    skip=0;
	}
      } else {
//        printf("Decoding block %d\n",block);
	fseek(buf->wavfile,buf->ac3_blocksize*block,SEEK_SET);
retry:
	if(fread(buffer,buf->ac3_blocksize,1,buf->wavfile)<=0) return 0;
	sample_t level;
	int flags;
	int samplerate,bitrate,size,frlen=0x600;
#ifdef DTS
	if(buf->ac3_channels&256)	// DTS!!!
	    size=dca_syncinfo(buf->state, buffer,&flags,&samplerate,&bitrate,&frlen);
	else
#endif
	    size=a52_syncinfo(buffer,&flags,&samplerate,&bitrate);
//	printf("syncinfo: %d  (%d Hz, %d bps)  %d\n",size,samplerate,bitrate,frlen);
	if(size<=0){
//	    printf("AC3/DTS stream sync error!!!\n");
	    fseek(buf->wavfile,1-buf->ac3_blocksize,SEEK_CUR);
	    goto retry;
	}
	if(frlen!=buf->ac3_framelen || size!=buf->ac3_blocksize){
	    printf("ERROR: block/frame size changed!!!\n");
	}

#ifdef DTS
	if(buf->ac3_channels&256){
	    if(ch<0) flags=DCA_STEREO;
	    level=1;
	    dca_frame(buf->state, buffer, &flags, &level, 0);
//	    printf("dca_frame -> %d (flag=%d level=%f)\n",ret,flags,level);
	} else
#endif
	{
	    if(ch<0) flags=A52_STEREO;
	    level=1;   //flags|=A52_ADJUST_LEVEL;
	    a52_frame(buf->state, buffer, &flags, &level, 0);
//	    printf("a52 flags = %d\n",flags);
	}

	int b;

	int oldest=cache_last[0]; cache=0;
	for(b=0;b<CACHESIZE;b++)
	    if(cache_last[b]<oldest){ oldest=cache_last[b]; cache=b; }
	cache_last[cache]=cache_serial;
	cache_block[cache]=(block|(buf->id<<30));
	
	for(b=0;b<buf->ac3_framelen/256;b++){
#ifdef DTS
	    if(buf->ac3_channels&256)
		dca_block(buf->state);
	    else
#endif
		a52_block(buf->state);
	    int j;
	    for(j=0;j<256;j++){
		float x=(ch>=0) ? (buf->samples[256*ch+j]) :
			(buf->samples[j]+buf->samples[256+j]); // all float!!!
		cache_samples[cache][b][j]=x;
		if(skip) --skip; else
		if(i<maxlen){
		    fft_in[i]=x;
		    if(x>*s_max) *s_max=x;
		    ++i;
		}
	    }
	}
      }
      ++block;
    }
    return 1;
}

int read_wav_block(buffer_t* buf,unsigned int f_pos,float* fft_in,int maxlen,float* s_max){
	if(!buf->wavfile) return 0; // no audio
	signed short buffer[maxlen][buf->ac3_channels];
	fseeko(buf->wavfile,wavhdr_len+buf->ac3_framelen*(off_t)f_pos,SEEK_SET);
	int len=fread(&buffer[0][0],2*buf->ac3_channels,maxlen,buf->wavfile);
	if(len<maxlen) return 0; //break;
//	printf("len=%d\n",len);

	int i;
	for(i=0;i<len;i++){
	    //  bswap!!!!!!
	    buffer[i][0]=bswap_16(buffer[i][0]);
	    int x=buffer[i][0];
	    if(buf->ac3_channels>1){
		buffer[i][1]=bswap_16(buffer[i][1]);
		x+=buffer[i][1];
	    }
	    fft_in[i]=x*(1.0/32768.0); // remap to -1.0 .. 1.0
	    if(fft_in[i]>*s_max) *s_max=fft_in[i];
	}
//	while(i<maxlen) fft_in[i]=0;
        return 1;
}



static signed short audiobuffer[BLOCKSIZE*BLOCKS][2];
static float audiotemp[BLOCKSIZE*BLOCKS*2]; // 2MByte ram
static int audio_pos=0;
static unsigned int audio_playing=0;

//extern void smbPitchShift(float pitchShift, long numSampsToProcess, long fftFrameSize, long osamp, float sampleRate, double *indata, double *outdata);
extern void smbPitchShift(float pitchShift, long numSampsToProcess, long fftFrameSize, long osamp, float *indata, float *outdata, int min, int max);

extern void FFTfilter(long numSampsToProcess, long fftFrameSize, long osamp, float *indata, float *outdata, unsigned char* filter,int step,int invert);

void mix_audio(buffer_t* buffer,unsigned int f_pos,int ch,double resample,int pitch,int filter){
    // load
    int len=BLOCKSIZE*BLOCKS*resample+4;
    float s_max=0;
    printf("MIX ch%d: reading %d samples from %d  (scale %5.3f)\n",ch,len,f_pos,resample);
    if(!buffer->ac3_blocksize){
	if(!read_wav_block(buffer,f_pos,audiotemp,len,&s_max)) return; // EOF
    } else {
	if(!read_ac3_block(buffer,f_pos,audiotemp,len,&s_max)) return; // EOF
    }
    // pitch correction?
    if(pitch && resample!=1.0){
	if(pitch==1)
	    smbPitchShift(1.0/resample, len, 1024, 4, audiotemp, audiotemp,0,512);
	else {
	    float temp2[len];
	    memcpy(temp2,audiotemp,len*sizeof(float));
	    switch(pitch){
	    case 2:
		smbPitchShift(1.0/resample, len, 4096, 8, audiotemp, audiotemp,0,4096/16-1);
		smbPitchShift(1.0/resample, len, 1024, 4, temp2, audiotemp,1024/16,1024/2);
		break;
	    case 3:
		smbPitchShift(1.0/resample, len, 4096, 4, audiotemp, audiotemp,0,4096/256-1);
		smbPitchShift(1.0/resample, len, 1024, 4, temp2, audiotemp,1024/256,1024/32-1);
		smbPitchShift(1.0/resample, len, 256,  4, temp2, audiotemp,256/32,256/2);
		break;
	    case 4:
		smbPitchShift(1.0/resample, len, 4096, 8, audiotemp, audiotemp,0,4096/512-1);
		smbPitchShift(1.0/resample, len, 2048, 8, temp2, audiotemp,2048/512,2048/128-1);
		smbPitchShift(1.0/resample, len, 1024, 4, temp2, audiotemp,1024/128,1024/32-1);
		smbPitchShift(1.0/resample, len, 512,  4, temp2, audiotemp,512/32,512/2);
		break;
	    }
	}
    }
    // filter?
    if(filter){
	FFTfilter(len, FFTSIZE, 4, audiotemp, audiotemp, fft_filter+BLOCKS*BLOCKSIZE, buffer->step, filter);
    }
    // scale?
    float scale=16384.0/buffer->env_max.s_max;
    // let's resample!  (linear interpolation...)
    int i;
    for(i=0;i<BLOCKSIZE*BLOCKS;i++){  // optimize!!!?
	float a=i*resample;
	int x=a; a-=x;
	if(ch<=1)
	    audiobuffer[i][ch]=scale*(audiotemp[x] + (audiotemp[x+1]-audiotemp[x])*a);
	else
	    audiobuffer[i][0]=audiobuffer[i][1]=scale*(audiotemp[x] + (audiotemp[x+1]-audiotemp[x])*a);
    }
}

// SDL Callback function
void outputaudio(void *userdata, unsigned char* stream, int space){
    int len=space/4;
    if(len>(BLOCKSIZE*BLOCKS-audio_pos)) len=audio_pos-BLOCKSIZE*BLOCKS;
    if(len<=0){ len=0; audio_playing=0; }
//    printf("outputaudio! space=%d len=%d\n",space,len);
    if(len) memcpy(stream,&audiobuffer[audio_pos][0],4*len);
    audio_pos+=len;
    if(4*len<space) memset(stream+4*len,0,space-4*len); // pad with zero
}

void play_audio_diff(buffer_t* buffer1,buffer_t* buffer2,long pos1,long pos2,double resample,int pitch,int filter){
    SDL_PauseAudio(1);
    SDL_LockAudio();
    mix_audio(buffer1,pos1,0,1.0,0,filter);
    mix_audio(buffer2,pos2,1,resample,pitch,filter);
    audio_pos=0; //audio_playing=buffer1->pos+1;
    SDL_UnlockAudio();
    SDL_PauseAudio(0);
}

void play_audio(buffer_t* buffer1,long pos1,double resample,int pitch,int filter){
    SDL_PauseAudio(1);
    SDL_LockAudio();
    mix_audio(buffer1,pos1,2,resample,pitch,filter);
    audio_pos=0; //audio_playing=buffer1->pos+1;
    SDL_UnlockAudio();
    SDL_PauseAudio(0);
}



// FFT data
//float* fft_in=NULL;
//fftwf_complex* fft_out=NULL;
//fftwf_plan plan;

void fft_init(buffer_t* b){
    b->fft_in=fftwf_malloc(sizeof(float) * FFTSIZE);
    b->fft_out=fftwf_malloc(sizeof(fftwf_complex) * FFTSIZE);
    b->plan = fftwf_plan_dft_r2c_1d(FFTSIZE, b->fft_in, b->fft_out, FFTW_ESTIMATE|FFTW_DESTROY_INPUT);
}

void update_fft_line(buffer_t* b,int n,int f_pos,double resample,int rs_mode){
	float s_max=0;
	if(f_pos<0){
clear:	    memset(b->fft+BLOCKSIZE*n,0,BLOCKSIZE);
	    memset(b->env+n,0,sizeof(envdata_t));
	    return;
	}
	if(rs_mode>0 && resample!=1.0){
	    // resample
	    int len=FFTSIZE*resample+4;
	    float temp[len];
	    if(!b->ac3_blocksize){
		if(!read_wav_block(b,f_pos,temp,len,&s_max)) goto clear; // EOF
	    } else {
		if(!read_ac3_block(b,f_pos,temp,len,&s_max)) goto clear; // EOF
	    }
	    // let's resample!  (linear interpolation...)
	    int i;
	    for(i=0;i<FFTSIZE;i++){
		float a=i*resample;
		int x=a; a-=x;
		b->fft_in[i]=temp[x] + (temp[x+1]-temp[x])*a;
	    }
	} else {
	    // time-stretch
	    if(!b->ac3_blocksize){
		if(!read_wav_block(b,f_pos,b->fft_in,FFTSIZE,&s_max)) goto clear; // EOF
	    } else {
		if(!read_ac3_block(b,f_pos,b->fft_in,FFTSIZE,&s_max)) goto clear; // EOF
	    }
	}

//	printf("FFT:\n");
	fftwf_execute(b->plan);
//	printf("OK!\n");

	float fft_max=0;
	float fft_avg=0;
	float fft_res[BLOCKSIZE]={0,};
//	float cft_min=0,cft_max=0;
	int i;
#ifdef CEPSTRUM
	for(i=0;i<FFTSIZE;i++){
	    double cc=b->fft_out[i][0]*b->fft_out[i][0] + b->fft_out[i][1]*b->fft_out[i][1];
	    b->fft_in[i]=log1p(cc); // for cepstrum
//	    b->fft_in[i]=cc; // for cepstrum
//	    if(b->fft_in[i]<cft_min) cft_min=b->fft_in[i];
//	    if(b->fft_in[i]>cft_max) cft_max=b->fft_in[i];
	    if(i>=BLOCKSIZE) continue;
	    double c=sqrt(cc);
#else
	for(i=0;i<BLOCKSIZE;i++){
	    double c=sqrt(b->fft_out[i][0]*b->fft_out[i][0] + b->fft_out[i][1]*b->fft_out[i][1]);
#endif
	    b->fftavg[i]+=c;
//	    b->fftavg[i]=b->fftavg[i]*0.999+c;
//	    b->fftavg[i]=b->fftavg[i]*0.999+0.001*c;
	    if(b->ref) c*=b->ref->fftavg[i]/b->fftavg[i]; // freq band normalization
	    fft_res[i]=c; fft_avg+=c;
	    if(c>fft_max) fft_max=c;
	}
	
//	printf("CFTin: min=%10.5f max=%10.5f\n",cft_min,cft_max);

	b->env[n].s_max=s_max;
	b->env[n].fft_avg=fft_avg;
	b->env[n].fft_max=fft_max;
	
	if(s_max>b->env_max.s_max) b->env_max.s_max=s_max;
	if(fft_max>b->env_max.fft_max) b->env_max.fft_max=fft_max;
	if(fft_avg>b->env_max.fft_avg) b->env_max.fft_avg=fft_avg;
	
	b->total_avg+=fft_avg;b->total_cnt++;
	if(noiselimit)
	    if(fft_avg<(b->total_avg/b->total_cnt)/noiselimit)
		fft_avg=(b->total_avg/b->total_cnt)/noiselimit;

#if 0
	if(b->ref){
	    int ndiff=(b->pos - b->ref->pos)/b->step + b->b_pos-b->ref->b_pos;
	    printf("ndiff=%d\n",ndiff);
	    fft_avg=b->ref->env[n-ndiff].fft_avg;
	    double scale1=b->avg/(fft_avg/BLOCKSIZE);
	    for(i=0;i<BLOCKSIZE;i++){
		double scale=scale1*b->ref->fftavg[i]/b->fftavg[i];
		int c=scale*fft_res[i];
	        if(c>255) c=255;
		b->fft[BLOCKSIZE*n+i]=c;
	    }
	} else 
#endif
	{
	    float scale=b->avg/(fft_avg/BLOCKSIZE);
	    for(i=0;i<BLOCKSIZE;i++){
		int c=scale*fft_res[i];
	        if(c>255) c=255;
		b->fft[BLOCKSIZE*n+i]=c;
	    }
	}

#ifdef CEPSTRUM
//	cepstrum calc:
	fftwf_execute(b->plan);
	fft_max=0;fft_avg=0;
	for(i=0;i<BLOCKSIZE;i++){
	    double c=b->fft_out[i][0]*b->fft_out[i][0] + b->fft_out[i][1]*b->fft_out[i][1];
	    if(i==0) c=0; // skip DC
	    fft_res[i]=c;
	    fft_avg+=c;
	    if(c>fft_max) fft_max=c;
	}
	{
	    if(fft_max>cft_max) cft_max=fft_max;
//	    float scale=b->avg/(fft_avg/BLOCKSIZE);
	    float scale=1024.0/cft_max;
//	    float scale=255.0/fft_max;
//	    printf("CFT res: avg=%10.8f  max=%10.8f   scale=%8.5f\n",fft_avg,fft_max,scale);
	    for(i=0;i<BLOCKSIZE;i++){
		int c=scale*fft_res[i/4];
	        if(c>255) c=255;
		b->cft[BLOCKSIZE*n+i]=c;
	    }
	}
#endif	
	
}

#define dprintf if(0) printf

void reset_buffer(buffer_t* b){
	// drop buffer!
	b->b_pos=b->maxblocks/2; // FIXME: OPTIMIZE!
	b->b_min=b->b_max=b->b_pos;
	dprintf("RESET! ");
}

// step = samples per block
void update_fft(buffer_t* b,long pos,double resample,int rs_mode,int step,float avg){
    if(b->rs_mode!=rs_mode || b->step!=step || b->avg!=avg){
	b->rs_mode=rs_mode; b->step=step; b->avg=avg;
	reset_buffer(b);
    }
    int diff=(pos-b->pos)/step;
    b->pos=pos;
    // update b_pos
    b->b_pos+=diff;
    dprintf("POS=%d  b_pos=%d",(int)pos,b->b_pos);
    // move buffer?
    if(b->b_pos<BLOCKS){
	int diff=2*BLOCKS-b->b_pos;
	if(b->b_min+diff<b->maxblocks){
	    dprintf(" MOVE[%d>>]",diff);
	    b->b_pos+=diff;
	    b->b_min+=diff;
	    b->b_max+=diff; if(b->b_max>b->maxblocks) b->b_max=b->maxblocks;
	    int len=b->b_max-b->b_min;
	    memmove(b->fft+b->b_min*BLOCKSIZE,b->fft+(b->b_min-diff)*BLOCKSIZE,BLOCKSIZE*len);
#ifdef CEPSTRUM
	    memmove(b->cft+b->b_min*BLOCKSIZE,b->cft+(b->b_min-diff)*BLOCKSIZE,BLOCKSIZE*len);
#endif
	    memmove(b->env+b->b_min,b->env+b->b_min-diff,sizeof(envdata_t)*len);
	} else {
	    dprintf(" ->RESET!");
	    b->b_pos=b->maxblocks/2;
	    b->b_min=b->b_max=b->b_pos;
	}
    }
    if(b->b_pos>b->maxblocks-BLOCKS){
	int diff=b->b_pos-(b->maxblocks-BLOCKS*2);
	if(b->b_max-diff>0){
	    dprintf(" MOVE[<<%d]",diff);
	    b->b_pos-=diff;
	    b->b_min-=diff; if(b->b_min<0) b->b_min=0;
	    b->b_max-=diff;
	    int len=b->b_max-b->b_min;
	    memmove(b->fft+b->b_min*BLOCKSIZE,b->fft+(b->b_min+diff)*BLOCKSIZE,BLOCKSIZE*len);
#ifdef CEPSTRUM
	    memmove(b->cft+b->b_min*BLOCKSIZE,b->cft+(b->b_min+diff)*BLOCKSIZE,BLOCKSIZE*len);
#endif
	    memmove(b->env+b->b_min,b->env+b->b_min+diff,sizeof(envdata_t)*len);
	} else {
	    dprintf(" ->RESET!");
	    b->b_pos=b->maxblocks/2;
	    b->b_min=b->b_max=b->b_pos;
	}
    }
    // update min buffer:
    if(b->b_min>b->b_pos-BLOCKS){
	int n=b->b_pos-BLOCKS-64;
	if(n<0) n=0;
	int nmax=b->b_min;
	dprintf("  MIN[%d..%d]",n,nmax);
	b->b_min=n;
	for(;n<nmax;n++){
	    int f_pos=(pos+(n-b->b_pos)*step)*resample+0.5;
	    update_fft_line(b,n,f_pos,resample,rs_mode);
	}
    }
    // update max buffer:
    if(b->b_max<b->b_pos+BLOCKS){
	int nmax=b->b_pos+BLOCKS+64;
	if(nmax>b->maxblocks) nmax=b->maxblocks;
	int n=b->b_max;
	dprintf("  MAX[%d..%d]",n,nmax);
	b->b_max=nmax;
	for(;n<nmax;n++){
	    int f_pos=(pos+(n-b->b_pos)*step)*resample+0.5;
	    update_fft_line(b,n,f_pos,resample,rs_mode);
	}
    }
    dprintf("\n");
}


void update_video(buffer_t* b,long pos){
    if(!b->rawfile) return;
    int n=0;
    int nmax=2*BLOCKS;
    {
	// optimization:
	if(b->framepos==pos) return; // nothing to do!
	int diff=pos-b->framepos;
	if(diff>0 && diff<2*BLOCKS && b->framepos>=0){
	    // scroll balra
	    int len=2*BLOCKS-diff;
	    memmove(b->video,b->video+VIDEO_W*VIDEO_H*diff,VIDEO_W*VIDEO_H*len);
	    n=len;
//		printf("optimized! diff=%d len=%d n=%d\n",diff,len,n);
        } else
	if(diff<0 && -diff<2*BLOCKS && b->framepos>=0){
	    // scroll jobbra
	    diff=-diff;
	    int len=2*BLOCKS-diff;
	    memmove(b->video+VIDEO_W*VIDEO_H*diff,b->video,VIDEO_W*VIDEO_H*len);
	    nmax=diff;
//		printf("optimized! diff=%d len=%d nmax=%d\n",diff,len,nmax);
	}
    }
    
    while((pos+n-BLOCKS)<0 && n<nmax){
	// negative position!
	memset(b->video+VIDEO_W*VIDEO_H*n,0,VIDEO_W*VIDEO_H);
	++n;
    }
    
    if(n<nmax){
	fseeko(b->rawfile,VIDEO_W*VIDEO_H*((off_t)pos+n-BLOCKS),SEEK_SET);
	int len=fread(b->video+VIDEO_W*VIDEO_H*n,nmax-n,VIDEO_W*VIDEO_H,b->rawfile);
	if(len>0) n+=len;
    }
    if(n<nmax) memset(b->video+VIDEO_W*VIDEO_H*n,0,VIDEO_W*VIDEO_H*(nmax-n));
    b->framepos=pos;
}




//void print_stat(buffer_t* b,char* name){
//    printf("%s: ENV s_max=%5.3f f_avg=%5.3f f_max=%5.3f\n",name,
//	b->env_max.s_max,b->env_max.fft_avg,b->env_max.fft_max);
//}


void cuthere(char* string){
    FILE* cutlist=fopen("cutlist.txt","a");
    fprintf(cutlist,"%s\n",string);
    fflush(cutlist);
    fclose(cutlist);
}




int main(int argc,char* argv[]){
double resample=(24000.0/1001)/25.0;
double timescale=1.0;
double fps=24/1.001;
int zoom=0;

fprintf(stderr,"\n" VERSION "\n\n");

int param=0;
while(++param<argc){
    if(argv[param][0]!='-') break;
    // option
    if(argv[param][1]==0) continue; // - -filename
//    else if(!strcasecmp(argv[param]+1,"samplerate"))
//        timescale=BLOCKSIZE/atof(argv[++param]);
    else if(!strcasecmp(argv[param]+1,"resample"))
        resample=1.0/atof(argv[++param]);
    else if(!strcasecmp(argv[param]+1,"fps"))
        fps=atof(argv[++param]);
    else if(!strcasecmp(argv[param]+1,"pal"))
        resample=(24000.0/1001)/25.0;
    else if(!strcasecmp(argv[param]+1,"ntsc"))
        resample=25.0/(24000.0/1001);
    else if(!strcasecmp(argv[param]+1,"tb2"))
        timescale=resample;
    else if(!strcasecmp(argv[param]+1,"ch"))
        ch=atoi(argv[++param]);
    else {
        printf("Unknown option: %s\n",argv[param]);
        return 1;
    }
}

if(param+1>=argc){
    printf("Usage: %s [options] file1 file2 [file3]\n",argv[0]);
    return 1;
}


//fft_init();

static buffer_t* buffers[3]={NULL,};
static buffer_t* buffers_temp[3]={NULL,};

#ifdef SMP
#if !defined(__CYGWIN__) && !defined(__MINGW32__)
pthread_t threads[3];
#else
HANDLE threads[3];
#endif
#endif

buffers[0]=load_buffer(argv[param++]); if(!buffers[0]) return 1;
buffers[1]=load_buffer(argv[param++]); if(!buffers[1]) return 1;
buffers[2]=load_buffer(argv[param++]);
fft_init(buffers[0]);
fft_init(buffers[1]);
if(buffers[2]) fft_init(buffers[2]);
#if 1
// igy tobb memoriat eszik, visoznt nem dobja el a cache-t shift/ctrl+S -nel
buffers_temp[0]=dupe_buffer(buffers[0],2*BLOCKS);
buffers_temp[1]=dupe_buffer(buffers[1],2*BLOCKS);
buffers_temp[2]=dupe_buffer(buffers[2],2*BLOCKS);
#else
buffers_temp[0]=buffers[0];
buffers_temp[1]=buffers[1];
buffers_temp[2]=buffers[2];
#endif

timescale=timescale/buffers[0]->samplerate;

if(resample<0.1) resample*=buffers[0]->samplerate;

int video_y0=0;
if(buffers[0]->rawfile || buffers[1]->rawfile) video_y0=VIDEO_H*VIDEO_SCALE+KOZ;


//if( SDL_Init(SDL_INIT_VIDEO) <0 ){
if( SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) <0 ){
    printf("Unable to init SDL: %s\n", SDL_GetError());
    return 2;
}
atexit(SDL_Quit);
SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, 100 /*SDL_DEFAULT_REPEAT_INTERVAL*/);
//screen= SDL_SetVideoMode(1024,buffers[2] ? (6*KOZ+3*256+2*64) : (4*KOZ+2*256+1*64), 32, SDL_ANYFORMAT|SDL_HWSURFACE);
screen= SDL_SetVideoMode(1024,video_y0 + 2*KOZ+BLOCKSIZE + (buffers[2]?256+KOZ:0), 32, SDL_ANYFORMAT|SDL_HWSURFACE);
if ( screen == NULL ){
    printf("Unable to set video: %s\n", SDL_GetError());
    return 2;
}
printf("SDL screen BPP = %d\n",screen->format->BytesPerPixel);
SDL_LockSurface(screen);
memset(screen->pixels,128,screen->h*screen->pitch);
SDL_UnlockSurface(screen);
SDL_UpdateRect(screen, 0, 0, screen->w, screen->h);


/* SDL Audio Specifications */
SDL_AudioSpec aspec, obtained;
aspec.format = AUDIO_S16LSB;
aspec.freq     = 48000;
aspec.channels = 2;
/* The desired size of the audio buffer in samples. This number should be a power of two, and may be adjusted by the audio driver to a value more suitable for the hardware. Good values seem to range between 512 and 8192 inclusive, depending on the application and CPU speed. Smaller values yield faster response time, but can lead to underflow if the application is doing heavy processing and cannot fill the audio buffer in time. A stereo sample consists of both right and left channels in LR ordering. Note that the number of samples is directly related to time by the following formula: ms = (samples*1000)/freq */
aspec.samples  = 2048;
/* The callback prototype is:
   void callback(void *userdata, Uint8 *stream, int len);
   userdata is the pointer stored in userdata field of the SDL_AudioSpec.
   stream is a pointer to the audio buffer you want to fill with information
   and len is the length of the audio buffer in bytes. */
aspec.callback = outputaudio;
/* This pointer is passed as the first parameter to the callback function. */
aspec.userdata = NULL;
/* Open the audio device and start playing sound! */
printf("SDL audio wanted: %d Hz, %d channels, format=%d, buffer=%d\n",
    aspec.freq,aspec.channels,aspec.format,aspec.samples);
if(SDL_OpenAudio(&aspec, &obtained) < 0) {
    printf("Unable to set audio: %s\n", SDL_GetError());
//    return 3;
}
printf("SDL audio inited: %d Hz, %d channels, format=%d, buffer=%d\n",
    obtained.freq,obtained.channels,obtained.format,obtained.samples);



int pos1=0;
int b2_delay=0;
int b3_delay=0;
int v2_delay_corr=0;
int curr_delay2=0;
int curr_delay3=0;
int curr_delay2_pos=0;

int rs_mode=1;    // resample-mode  (-1=timescale)
int pitch_mode=1; // pitch correction mode (0=off 1=singleband 2/3/4=multiband)
float avg=32.0;

int matching=0;
int env_type=3;
int draw_mode=0;
int diff_mode=0;
int video_mode=(buffers[0]->rawfile && buffers[1]->rawfile) ? 1 : 0;
int delay_side=0;

int freqcomp=0;

int ret;
int step;

refresh:
    step=video_mode ? buffers[0]->samplerate/fps               // 1frame
			: buffers[0]->samplerate*pow(2,-zoom)/100; // 10ms/block

//    printf("step=%d\n",buffers[0]->samplerate);

    const int delay2=delay_side ? curr_delay2 : b2_delay;

    int frameno=pos1*fps/buffers[0]->samplerate;
    int framenop=audio_playing?(audio_playing+audio_pos)*fps/buffers[0]->samplerate:frameno;
    int v2_delay=v2_delay_corr+delay2*fps/buffers[0]->samplerate;


void thread_1(void* param){
    if(video_mode)
	update_video(buffers[0],frameno);
    else
	update_fft(buffers[0],pos1,1.0,0,step,avg);
}
void thread_2(void* param){
    if(video_mode)
	update_video(buffers[1],v2_delay+frameno);
    else
	update_fft(buffers[1],pos1+delay2,rs_mode ? resample : 1.0,rs_mode,step,avg);
}
void thread_3(void* param){
    update_fft(buffers[2],pos1+delay2+b3_delay,rs_mode ? resample : 1.0,rs_mode,step,avg);
}

#ifdef SMP
#if !defined(__CYGWIN__) && !defined(__MINGW32__)
    ret = pthread_create( &threads[0], NULL, thread_1, NULL);
    ret = pthread_create( &threads[1], NULL, thread_2, NULL);
    if(buffers[2]) ret = pthread_create( &threads[2], NULL, thread_3, NULL);
    pthread_join( threads[0], NULL);
    pthread_join( threads[1], NULL);
    if(buffers[2]) pthread_join( threads[2], NULL);
#else
    threads[0] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thread_1, NULL, 0, 0);
    threads[1] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thread_2, NULL, 0, 0);
    if(buffers[2]) threads[2] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thread_3, NULL, 0, 0);
    WaitForMultipleObjects(buffers[2] ? 3 : 2, threads, TRUE, INFINITE);
    CloseHandle(threads[0]);
    CloseHandle(threads[1]);
    if(buffers[2]) CloseHandle(threads[2]);
#endif
#else
    thread_1(NULL);
    thread_2(NULL);
    if(buffers[2]) thread_3(NULL);
#endif

    
    if(!video_mode)
        printf("volume ratio:  s_max: %5.3f  f_avg: %5.3f  f_max: %5.3f\n",
    	    buffers[1]->env_max.s_max/buffers[0]->env_max.s_max,
	    buffers[1]->env_max.fft_avg/buffers[0]->env_max.fft_avg,
	    buffers[1]->env_max.fft_max/buffers[0]->env_max.fft_max);

    SDL_LockSurface(screen);
//    draw(buffers[0],pos1,KOZ,0);

    if(video_mode){
	if(!draw_mode && buffers[0]->rawfile && buffers[1]->rawfile){
	    draw_video_diff(buffers[0],buffers[1],video_y0+KOZ);
	} else {
	    if(buffers[0]->rawfile) draw_video(buffers[0],video_y0+KOZ);
	    if(buffers[1]->rawfile) draw_video(buffers[1],video_y0+KOZ+256);
	}
    } else
    if(draw_mode){
	draw(buffers[0],video_y0+KOZ);
	draw(buffers[1],video_y0+KOZ+BLOCKSIZE/2);
	draw_env(buffers[0],video_y0+KOZ+BLOCKSIZE/2,env_type,0xFF<<COLOR_R);
	draw_env(buffers[1],video_y0+KOZ+BLOCKSIZE,env_type,0xFF<<COLOR_G);
    } else {
	draw_diff(buffers[0],buffers[1],video_y0+KOZ,diff_mode);
	draw_env(buffers[0],video_y0+KOZ+BLOCKSIZE,env_type,0xFF<<COLOR_R);
	draw_env(buffers[1],video_y0+KOZ+BLOCKSIZE,env_type,0xFF<<COLOR_G);
    }
    
    if(buffers[2]){
#ifdef CEPSTRUM
	draw_cft(buffers[1],video_y0+KOZ+BLOCKSIZE/2+KOZ+256);
#else
	draw(buffers[2],video_y0+KOZ+BLOCKSIZE/2+KOZ+256);
	draw_env(buffers[2],video_y0+KOZ+BLOCKSIZE+KOZ+256,env_type,0xFF<<COLOR_G);
#endif
    }
    
    if(buffers[0]->rawfile && buffers[1]->rawfile){
	if(draw_mode){
	    draw_frame(buffers[0],512-VIDEO_W*VIDEO_SCALE,KOZ,framenop);
	    draw_frame(buffers[1],512,KOZ,v2_delay+framenop);
	} else {
	    draw_frame_diff(buffers[0],buffers[1],512-VIDEO_W*VIDEO_SCALE,KOZ,
		frameno-1, v2_delay+frameno-1 );
	    draw_frame_diff(buffers[0],buffers[1],512,KOZ,
		framenop, v2_delay+framenop);
	}
    } else
    if(buffers[0]->rawfile){
	draw_frame(buffers[0],512-VIDEO_W*VIDEO_SCALE,KOZ,frameno-1);
	draw_frame(buffers[0],512,KOZ,framenop);
    } else
    if(buffers[1]->rawfile){
	draw_frame(buffers[1],512-VIDEO_W*VIDEO_SCALE,KOZ,v2_delay+frameno-1);
	draw_frame(buffers[1],512,KOZ,v2_delay+framenop);
    }
//    printf("AUDIO: playing=%d pos=%d\n",audio_playing,audio_pos);

    int i;
    for(i=0;i<screen->h;i++){
	unsigned int* ptr= screen->pixels + i*screen->pitch;
	ptr[screen->w/2]|=0x808080|(0xFF<<COLOR_B);
	ptr[screen->w/2 - BLOCKS/2]|=(0xFF<<COLOR_B);
	ptr[screen->w/2 + BLOCKS/2]|=(0xFF<<COLOR_B);
    }

    memset(screen->pixels+video_y0*screen->pitch,64,KOZ*screen->pitch);
    draw_cut((b2_delay-curr_delay2)/step,delay_side,video_y0);
//    draw_cut(b2_delay-curr_delay2,delay_side,video_y0+KOZ+BLOCKSIZE);

    SDL_UnlockSurface(screen);
    SDL_UpdateRect(screen, 0, 0, screen->w, screen->h);

    char title[500];
    sprintf(title,"pos1: %d [%4.3fs - %dh%02dm%02ds] || delay: %d [%4.3fs] (%+dms) || %s %d || zoom=%d",
	pos1,pos1*timescale,
	(int)(pos1*timescale/3600),
	(int)(pos1*timescale/60)%60,
	(int)(pos1*timescale)%60,
	curr_delay2-b2_delay,(curr_delay2-b2_delay)*timescale, (int)(b3_delay*timescale*1000),
	rs_mode<0 ? "time-stretch" : rs_mode ? "resample" : "untouched",
	pitch_mode, zoom);
    SDL_WM_SetCaption(title,title);

    SDL_Event event;
    
    if(matching){
	ret=video_mode ? search_vid(buffers[0],buffers[1],8) : search_aud(buffers[0],buffers[1]);
//	ret-=b2_delay;
//	printf("matching! ret=%d\n",ret);
//	printf("pos1=%d pos2=%d len1=%d len2=%d\n",pos1,pos1+b2_delay,buffers[0]->len,buffers[1]->len);
//	if(ret>-3 && ret<3 && pos1<buffers[0]->len && (pos1+b2_delay)<buffers[1]->len){
	if(ret>-3 && ret<3 && pos1<buffers[0]->len){
	    while(SDL_PollEvent(&event)){
		if(event.type==SDL_QUIT) goto quit;
		if(event.type==SDL_KEYDOWN)
		    if(event.key.keysym.sym=='m') goto stop_match;
	    }
	    pos1+=matching * 250*step;
	    goto refresh;
	}
	stop_match:
        matching=0;
    }
    
    while(audio_playing ? SDL_PollEvent(&event) : SDL_WaitEvent(&event)){
	if(event.type==SDL_QUIT) goto quit;
	if(event.type==SDL_KEYDOWN){
	    int shift=event.key.keysym.mod&KMOD_SHIFT;
	    int ctrl=event.key.keysym.mod&KMOD_CTRL;
	    int alt=event.key.keysym.mod&KMOD_ALT;
	    switch(event.key.keysym.sym){
	    case SDLK_ESCAPE: goto quit;
	    case 'd':
		if(shift) diff_mode=(diff_mode+1)%6; else draw_mode^=1;
		break;
	    case 'v':
		video_mode^=1; break;
	    case '-':
		if(!shift){
		    if(ctrl) avg/=1.5; else
		    if(zoom>-8) --zoom;
		    break;
		}
	    case '=':
	    case '+':
		if(ctrl) avg*=1.5; else
		if(zoom<8) ++zoom;
		break;
	    case 'e':
		env_type=(env_type+1)&3;
		printf("ENV type set to %d\n",env_type);
		break;
	    case 'f':
		freqcomp^=1;
		printf("freqcomp: %d\n",freqcomp);
		if(freqcomp)
		    buffers[1]->ref=buffers[0];
		else
		    buffers[1]->ref=NULL;
		reset_buffer(buffers[1]);
		break;
	    case 'r':
		if(ctrl) pitch_mode=(pitch_mode+1)%5; else
		if(shift)
		    rs_mode=rs_mode ? 0 : 1;
		else
		    rs_mode=-rs_mode;
		break;
	    case 'p': // play audio
		if(alt && buffers[2]) play_audio(buffers[2],(pos1+delay2+b3_delay)*resample,rs_mode ? resample : 1.0, rs_mode<0?pitch_mode:0,0);
		else if(ctrl)  play_audio(buffers[1],(pos1+delay2)*(rs_mode?resample:1.0),rs_mode?resample:1.0, rs_mode<0?pitch_mode:0,0);
		else if(shift) play_audio(buffers[0],pos1,1.0,0,0);
		else play_audio_diff(buffers[0],buffers[1],pos1,(pos1+delay2)*(rs_mode?resample:1.0),rs_mode?resample:1.0, rs_mode<0?pitch_mode:0,0);
		audio_playing=pos1;
		break;
	    case 'o': // play filtered
		if(alt && buffers[2]) play_audio(buffers[2],(pos1+delay2+b3_delay)*resample,rs_mode ? resample : 1.0, rs_mode<0?pitch_mode:0,1);
		else if(ctrl)  play_audio(buffers[1],(pos1+delay2)*(rs_mode?resample:1.0),rs_mode?resample:1.0, rs_mode<0?pitch_mode:0,1);
		else if(shift) play_audio(buffers[0],pos1,1.0,0,1);
		else play_audio_diff(buffers[0],buffers[1],pos1,(pos1+delay2)*(rs_mode?resample:1.0),rs_mode?resample:1.0, rs_mode<0?pitch_mode:0,1);
		audio_playing=pos1;
		break;
	    case 'l': // play negative filtered
		if(alt && buffers[2]) play_audio(buffers[2],(pos1+delay2+b3_delay)*resample,rs_mode ? resample : 1.0, rs_mode<0?pitch_mode:0,-1);
		else if(ctrl)  play_audio(buffers[1],(pos1+delay2)*(rs_mode?resample:1.0),rs_mode?resample:1.0, rs_mode<0?pitch_mode:0,-1);
		else if(shift) play_audio(buffers[0],pos1,1.0,0,-1);
		else play_audio_diff(buffers[0],buffers[1],pos1,(pos1+delay2)*(rs_mode?resample:1.0),rs_mode?resample:1.0, rs_mode<0?pitch_mode:0,-1);
		audio_playing=pos1;
		break;
	    case 's':
		if(shift|ctrl){
		    // zoom in! (will be restored at next refresh)
		    int step=ctrl?1:buffers_temp[0]->samplerate/1000; // 1ms/block
		    update_fft(buffers_temp[0],pos1,1.0,0,step,avg);
		    update_fft(buffers_temp[1],pos1+delay2,rs_mode ? resample : 1.0,rs_mode,step,avg);
		    printf("Searching... (step=%d)\n",buffers_temp[0]->step);
		    ret=search_aud(buffers_temp[0],buffers_temp[1]);
		    printf("done! ret=%d\n",ret);
		    b2_delay+=ret*buffers_temp[0]->step;
		} else {
		    printf("Searching... (step=%d)\n",step);
		    ret=video_mode ? search_vid(buffers[0],buffers[1],1) : search_aud(buffers[0],buffers[1]);
		    printf("done! ret=%d\n",ret);
		    b2_delay+=ret*step;
		}
		break;
	    case 'h':
		if(!buffers[2]) break;
		if(shift|ctrl){
		    // zoom in! (will be restored at next refresh)
		    int step=ctrl?1:buffers_temp[1]->samplerate/1000; // 1ms/block
		    update_fft(buffers_temp[1],pos1+delay2,rs_mode ? resample : 1.0,rs_mode,step,avg);
		    update_fft(buffers_temp[2],pos1+delay2+b3_delay,rs_mode ? resample : 1.0,rs_mode,step,avg);
		    printf("Searching... (step=%d)\n",buffers_temp[1]->step);
		    ret=search_aud(buffers_temp[1],buffers_temp[2]);
		    printf("done! ret=%d\n",ret);
		    b3_delay+=ret*buffers_temp[1]->step;
		} else {
		    printf("Searching... (step=%d)\n",step);
		    ret=search_aud(buffers[1],buffers[2]);
		    printf("done! ret=%d\n",ret);
		    b3_delay+=ret*step;
		}
		break;
	    case 'm':
		printf("Matching...\n");
		matching=shift ? -1 : 1; break;
	    case '1'...'9':
		pos1=buffers[0]->len*0.1*(event.key.keysym.sym-'0');break;
	    case '0':
		if(ctrl) v2_delay_corr=0; else
		if(shift) b2_delay=0; else
		if(alt) b3_delay=0; else
		pos1=buffers[0]->len; // same as END
		break;
	    case SDLK_TAB:
		delay_side^=1;
		int dc=b2_delay-curr_delay2;
		if(dc<0) pos1-=delay_side ? -dc : dc;
		break;
	    case 'c':
		if(delay_side){	// disable it!
		    delay_side^=1;
		    int dc=b2_delay-curr_delay2;
		    if(dc<0) pos1-=delay_side ? -dc : dc;
		}
		if(shift){
		    b2_delay=curr_delay2;
		    b3_delay=curr_delay3;
		} else {
		    char temp[100];
		    sprintf(temp,"%d,%d,%d",
			(int)(0.5+1000.0*timescale*pos1),
			(int)(0.5+1000.0*timescale*b2_delay),
			(int)(0.5+1000.0*timescale*(b2_delay+b3_delay)));
		    cuthere(temp);
		    curr_delay2=b2_delay;
		    curr_delay3=b3_delay;
		    curr_delay2_pos=pos1;
		}
		break;
	    case SDLK_BACKSPACE:
		{int temp=pos1;
		pos1=curr_delay2_pos;
		curr_delay2_pos=temp;}
		break;
	    case SDLK_HOME:
		pos1=0; break;
	    case SDLK_END:
		pos1=buffers[0]->len;
		break;
	    case '[':
		if(!shift) pos1-=10000*step; else b2_delay-=1000*step;
		break;
	    case ']':
		if(!shift) pos1+=10000*step; else b2_delay+=1000*step;
		break;
	    case SDLK_PAGEUP:
		if(alt) b3_delay-=BLOCKS*step; else
		if(!shift) pos1-=500*step; else b2_delay-=BLOCKS*step;
		break;
	    case SDLK_PAGEDOWN:
		if(alt) b3_delay+=BLOCKS*step; else
		if(!shift) pos1+=500*step; else b2_delay+=BLOCKS*step;
		break;
	    case SDLK_UP:
		if(alt) b3_delay-=10*step; else
		if(!shift) pos1-=50*step; else b2_delay-=10*step;
		break;
	    case SDLK_DOWN:
		if(alt) b3_delay+=10*step; else
	    	if(!shift) pos1+=50*step; else b2_delay+=10*step;
	    	break;
	    case SDLK_RIGHT:
		if(alt) b3_delay-=step; else
		if(ctrl) v2_delay_corr--; else
	    	if(!shift) pos1-=step; else b2_delay-=step;
	    	break;
	    case SDLK_LEFT:
		if(alt) b3_delay+=step; else
		if(ctrl) v2_delay_corr++; else
	    	if(!shift) pos1+=step; else b2_delay+=step;
	    	break;
	    default: break;
	    }
	    goto refresh;
	}
    };
    goto refresh;

quit:
printf("Done!\n");

return 0;
}
