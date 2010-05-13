
// TimeFactory 2.02 (MPEX2 GUI) wrapper v3 for Linux by A'rpi

// file:00078800
// 478800 - 47af00          (83 ec 30 8b 44 24 4c c7 4424100500000085c07508c744244c03000000)
// 479da8 = pow()    (e833230000)        47c0e0-


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "mpex2v3.h"

// 10k of binary code extracted from Timefactory.exe:
#include "mpex2_bin.h"

static void* my_memcpy(void* dst,void* src,int len){
    memcpy(dst,src,len);
    return dst;
}

// nagy1:        osszesen 30h local stack
static int (*process) (double stretch, double pitch, double formant, // fo parameterek!
    int quality, // 1..4, 1=legrosszabb/gyorsabb  4=legjobb/lassabb
    float samplerate,int channels,
    int zero,  int* ret2,int* ret3, // ret2 a pozicio az input streamben
    float* ch1,float* ch2,float* ch3,float* ch4,float* ch5,float* ch6,float* ch7,float* ch8, // buffer
    void* bigmem)=NULL;  // state - itt tarolja a tablakat es a buffereit

static void* bigmem=NULL;

static float buffers[8][8192*8];
static int buffer_pos=0;
static int buffer_len=0;
static int last_ret2=0;
static int init_skip=0;
static int end_skip=0;

static int ret,ret2,ret3;

static float workbuf[8][8192];


int MPEX2_process(double stretch, double pitch, int quality, int samplerate, int channels, MPEX2_callback_t callback,float* outbuf){
    int i,c;
retry:
    if(buffer_pos>3*8192){
	int diff=buffer_pos-8192;
	buffer_pos=8192;buffer_len-=diff;
	for(c=0;c<channels;c++)
	    memmove(&buffers[c][0],&buffers[c][diff],buffer_len*4);
    }
    while(buffer_pos+8192>buffer_len){
	float* data=NULL;
//	if(MPEX2_debug) printf("Calling callback!\n");
	int len=callback(NULL, &data);
	if(len<=0){
	    for(c=0;c<channels;c++)
		memset(&buffers[c][buffer_len],0,4*(end_skip+buffer_pos+8192-buffer_len));
	    buffer_len+=end_skip; end_skip=0;
	    break; // EOF!
	}
	for(i=0;i<len;i++)
	    for(c=0;c<channels;c++)
		buffers[c][buffer_len+i]=*data++; // deinterleave!
	buffer_len+=len;
    }

//    if(MPEX2_debug) printf("MPEX2: buffer  pos=%d  len=%d\n",buffer_pos,buffer_len);
    if(buffer_pos>=buffer_len) return 0; // EOF!
    for(c=0;c<channels;c++)
	memcpy(&workbuf[c][0],&buffers[c][buffer_pos],4*8192);

//    printf("Calling process! (%p)\n",process);
    ret=process(stretch,pitch,1.0, // fo parameterek!
	quality, // 1..4, 1=legrosszabb/gyorsabb  4=legjobb/lassabb
	samplerate,channels, 0,&ret2,&ret3,
	workbuf[0],workbuf[1],workbuf[2],workbuf[3],workbuf[4],workbuf[5],workbuf[6],workbuf[7],
	bigmem);
//    if(MPEX2_debug) 
//    printf("MPEX2: ret=%d ret2=%d ret3=%d\n",ret,ret2,ret3);

    if(ret>0){
	if(init_skip>0){
	    init_skip-=ret;
	    goto retry;
	}
	float* data=outbuf;
	for(i=0;i<ret;i++)
	    for(c=0;c<channels;c++)
		*data++=workbuf[c][i]; // interleave!
	buffer_pos+=ret2-last_ret2;
	last_ret2=ret2;
    }

    return ret;
}

extern void MPEX2_sethook(void* dll);

static void set_hook_call(void* dll,unsigned int addr,void* proc){
    // call hook:
    addr+=(unsigned int)dll;
    ((unsigned char*)(addr))[0]=0xE8;
    ((unsigned int*)(addr+1))[0]=((unsigned int)proc) - (addr+5);
}


#if !defined(__CYGWIN__) && !defined(__MINGW32__)
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#else
#include <windows.h>
#endif


int MPEX2_load(int hook){

#if !defined(__CYGWIN__) && !defined(__MINGW32__)
//    void* dll=mmap((void*)(0x400000+LOADBASE), LOADSIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
//            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#if defined(MAP_ANON) && !defined(MAP_ANONYMOUS)
#define MAP_ANONYMOUS MAP_ANON
#endif
    void* dll=mmap(NULL, LOADSIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#else
    void* dll = (DWORD)VirtualAlloc( NULL, LOADSIZE,
                                     MEM_RESERVE | MEM_COMMIT,
                                     PAGE_EXECUTE_READWRITE );
#endif

    fprintf(stderr,"mmap -> %p\n",dll);
    if(!dll || dll==(void*)0xffffffff){
	fprintf(stderr,"Cannot allocate exectuable memory!\n");
	return 0;
    }

    memcpy(dll,mpex2_binary,LOADSIZE);
    dll-=LOADBASE;

    process=(void*)dll+0x78800;

#define RELOC(addr,name) *((void**)(dll+addr))=&name;
#define lrint__float2int (dll+0x7AF00)
#include "mpex2_rel.h"
#undef RELOC

    // enable native version of some of the mpex2 code:
    if(hook) MPEX2_sethook(dll);

    bigmem=malloc(8000000);
    if(!bigmem){
	fprintf(stderr,"Not enough memory! (8MB required)\n");
	return 0;
    }
    memset(bigmem,0,8000000);

    buffer_pos=0;
    buffer_len=0;
    last_ret2=0;
    init_skip=2048;
    end_skip=2048;
    
    return 1;
}


