#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "version.h"

#if !defined(__CYGWIN__) && !defined(__MINGW32__)

#include "config.h"

#include "wine/driver.h"
#include "wine/vfw.h"
#include "wine/windef.h"
#include "ldt_keeper.h"

#else
#include <windows.h>
#endif

HMODULE   WINAPI LoadLibraryA(LPCSTR);
FARPROC   WINAPI GetProcAddress(HMODULE,LPCSTR);
int       WINAPI FreeLibrary(HMODULE);



#define NO_FLAGS 0L
#define ATTENUATE_REAR 1L
#define FORMAT_INTEL 2L
#define FORMAT_MOTOROLA 4L
#define ZERO_PAD 16L
#define ES 32L
#define ES_MATRIXED 64L
#define ES_PHASE_SHIFTED 128L
#define MONITOR_ENABLED 256L
#define EXTENDED_96_24 512L

#define BITRATE_DVD_HIGH 1509750L
#define BITRATE_DVD_LOW 754500L
#define BITRATE_CD 1234800L

#define SAMPLERATE_CD_44k 44100L
#define SAMPLERATE_CD_88k 88200L
#define SAMPLERATE_DVD_48k 48000L
#define SAMPLERATE_DVD_96k 96000L

#define DIALNORM_MAX 31L
#define DIALNORM_MIN 1L



#define JNICALL __stdcall

typedef signed char jbyte;
typedef long jint;
typedef char jboolean;
typedef long long jlong;
typedef void* jobject;
typedef void* jstring;
typedef void* JNIEnv;

jint JNICALL (*initfunc) (JNIEnv *, jobject, jstring, jstring, jint, jint, jlong, jlong, jint) = NULL;
jint JNICALL (*nextfunc) (JNIEnv *, jobject, jint) = NULL;

void JNICALL ReleaseStringUTFChars(JNIEnv *env, jstring str, const char* chars){
    printf("JNI call ReleaseStringUTFChars p1=%p p2=%p\n",str,chars);
}

char* JNICALL GetStringUTFChars(JNIEnv *env, jstring str, jboolean *isCopy){
    printf("JNI call GetStringUTFChars env=%p str=%p isCopy=%p\n",env,str,isCopy);
    return str;
}



static void* expfopen(const char* path, const char* mode){
    printf("DTS_fopen: \"%s\"  mode:%s\n", path, mode);
    //return fopen(path, mode);
    if(!strcmp(path,"-")){
	if(mode[0]=='r') return stdin;
	return stdout;
    }
    return fopen(path, mode); // everything on screen
}
static int expfclose(void* f){
    printf("DTS_fclose: file: %p\n", f);
    return fclose(f);
}
static int expfread(void* ptr, int size, int nmemb, void* f){
//    printf("DTS_fread: %d x %d bytes to %p from file %p\n",size,nmemb,ptr,f);
    return fread(ptr,size,nmemb,f);
}
static int expfwrite(void* ptr, int size, int nmemb, void* f){
//    printf("DTS_fwrite: %d x %d bytes  buffer: %p  file: %p\n",size,nmemb,ptr,f);
    return fwrite(ptr,size,nmemb,f);
}
static int expfseek(void* f,int offset,int whence){
    printf("DTS_fseek: offset=%d whence=%d file %p\n",offset,whence,f);
    if(whence==1 && offset<100000){
	while(offset-->0) fgetc(f);
	return 0;
    }
    return fseek(f,offset,whence);
}


static void* ptrs[1000];
static JNIEnv env=ptrs;
static jobject obj;

int main(int argc,char* argv[]){

fprintf(stderr,"\n" VERSION "\n\n");

int bitrate=768;
int dialnorm=31;
int dtsflags=0;

int param=0;
while(++param<argc){
    if(argv[param][0]!='-') break;
    if(argv[param][1]==0) break; //continue; // - -filename
    // option
    if(!strcasecmp(argv[param]+1,"b"))
        bitrate=atoi(argv[++param]);
    else if(!strcasecmp(argv[param]+1,"dialnorm"))
        dialnorm=atoi(argv[++param]);
    else if(!strcasecmp(argv[param]+1,"flags"))
        dtsflags|=atoi(argv[++param]);
    else {
        fprintf(stderr,"Unknown option: %s\n",argv[param]);
        return 1;
    }
}

if(param+1>=argc){
    fprintf(stderr,"Usage: agm2dts [-b bitrate] input.agm output[.dts]\n");
    return 1;
}

if(bitrate>10000) bitrate/=1000;
bitrate = (bitrate<1000) ? BITRATE_DVD_LOW : BITRATE_DVD_HIGH;
fprintf(stderr,"Selected bitrate: %d bps\n",bitrate);

#if !defined(__CYGWIN__) && !defined(__MINGW32__)
extern int LOADER_DEBUG;
//    LOADER_DEBUG=1;

    Setup_LDT_Keeper();
#endif

    HMODULE dll = LoadLibraryA("DTS_rw.dll");
    if(!dll){
	fprintf(stderr,"unable to load DTS.dll or MSVCIRT.dll\n");
	return 0;
    }
    
#if 1
    // patch fopen/fread/fwrite/fseek/fclose:
    *((unsigned int*)((void*)dll+0x430c4)) = expfopen;
    *((unsigned int*)((void*)dll+0x43068)) = expfseek;
    *((unsigned int*)((void*)dll+0x43060)) = expfread;
    *((unsigned int*)((void*)dll+0x43084)) = expfwrite;
    *((unsigned int*)((void*)dll+0x43080)) = expfclose;
#endif

    initfunc = GetProcAddress(dll,"_Java_dts_Native_DtsEncoder_doInitialiseEncoder@44");
    nextfunc = GetProcAddress(dll,"_Java_dts_Native_DtsEncoder_doEncodeNextBlock@12");
//    fprintf(stderr,"dll=%p init=%p next=%p\n",dll,initfunc,nextfunc);

//    printf("&env=%p env=%p fnames: %p %p\n",&env, env, fname1,fname2);

    ptrs[676/4]=GetStringUTFChars;
    ptrs[680/4]=ReleaseStringUTFChars;

    void* handledts=initfunc(&env,obj,argv[param],argv[param+1], bitrate, dtsflags, 0, 10000000000LL, dialnorm);
    if(!handledts){
	fprintf(stderr,"DTS encoder initialization failed!!! check .log file!\n");
	return 1;
    }
    fprintf(stderr,"init okay, handle=%p\n",handledts);

    while(1){
	int ret=nextfunc(&env,obj,handledts);
//	printf("next block: %d\n",ret);
	if(ret<=0) break;
    }

    return 0;
}
