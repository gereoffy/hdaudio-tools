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

//0x100011b0 info
//0x100011e0 debug?
//0x10001210 warning
//0x10001240 error

void abort_error(void* caller,void* data,char* msg){
    printf("ERROR:  (caller: %p  data: %p  msg: %p)\n",caller,data,msg);
    printf(msg,data);
    printf("\n");
    exit(1);
}

void print_info(void* caller,void* data,char* msg){
    printf("INFO:  (caller: %p  data: %p  msg: %p)\n",caller,data,msg);
    printf(msg,data);
    printf("\n");
}
void print_debug(void* caller,void* data,char* msg){
    printf("DEBUG:  (caller: %p  data: %p  msg: %p)\n",caller,data,msg);
//    printf(msg,data);
//    printf("\n");
}
void print_warning(void* caller,void* data,char* msg){
    printf("WARN:  (caller: %p  data: %p  msg: %p)\n",caller,data,msg);
    printf(msg,data);
    printf("\n");
}

static void set_hook_call(void* dll,unsigned int addr,void* proc){
    // call hook:
    addr+=(unsigned int)dll;
#if 0
    ((unsigned char*)(addr))[0]=0xC3;
#else
    ((unsigned char*)(addr))[0]=0xE8;  // E8=call E9=jmp
    ((unsigned int*)(addr+1))[0]=((unsigned int)proc) - (addr+5);
    ((unsigned char*)(addr))[5]=0xC3;  // ret
//    ((unsigned char*)(addr))[6]=8;  // ret
//    ((unsigned char*)(addr))[7]=0;  // ret
#endif
}



void* (*initfunc) (void* abc) = NULL;
int (*parsefunc) (void* handle,char* data) = NULL;
int (*validfunc) (void* handle) = NULL;
char (*islossless) (void* handle) = NULL;


unsigned char temp[65536]={0x77,};

int main(int argc,char* argv[]){

fprintf(stderr,"\n" VERSION "\n\n");

int bitrate=768000;
int dialnorm=31;
int dtsflags=0;

#if !defined(__CYGWIN__) && !defined(__MINGW32__)
extern int LOADER_DEBUG;
    LOADER_DEBUG=0;

    Setup_LDT_Keeper();
#endif

    HMODULE dll = LoadLibraryA("DTS-MA/DTSEncSubBandProc.dll");
    if(!dll){
	fprintf(stderr,"unable to load DTS.dll or MSVCIRT.dll\n");
	return 0;
    }

    initfunc = GetProcAddress(dll,"DTSEnc_CreateEncoder");
    parsefunc= GetProcAddress(dll,"DTSEnc_ParseConfigStream");
    validfunc= GetProcAddress(dll,"DTSEnc_ValidateConfigStream");
    islossless= GetProcAddress(dll,"DTSEnc_IsLossLessEncode");
    
    fprintf(stderr,"dll=%p init=%p next=%p  temp=%p\n",dll,initfunc,islossless,temp);

    set_hook_call(dll,0x1240,abort_error);
    set_hook_call(dll,0x1210,print_warning);
    set_hook_call(dll,0x11e0,print_debug);
    set_hook_call(dll,0x11b0,print_info);

//    printf("&env=%p env=%p fnames: %p %p\n",&env, env, fname1,fname2);

    void* handledts=initfunc(0x99ABCDEF);
    if(!handledts){
	fprintf(stderr,"DTS encoder initialization failed!!! check .log file!\n");
	return 1;
    }
    fprintf(stderr,"init okay, handle=%p\n",handledts);

    int ret=islossless(handledts);
    printf("ret=%p\n",ret);
    
    int ize2[]={0x99123456,0,0,0,0};
    void* ize[8]={ize2,0xAAAABBBB,0,0,};

//72746962
//    ret=parsefunc(handledts,"bitrates=1509,1344,1152,960,768,639,510,447,384,318,255");
    ret=parsefunc(handledts,ize);
    printf("ret=%p\n",ret);

    return 0;
}
