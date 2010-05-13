#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#ifdef DTS
#include <dca.h>
#endif

#include "bswap.h"
#include "wavhdr.h"
#include "ac3hdr.h"

#define BUFFERSIZE 65536
unsigned char buffer[BUFFERSIZE];

void filecopy(FILE* f2,FILE* f1,long long len){
#if 1
    int len2=0;
    while(len>0){
	int l2=(len<BUFFERSIZE) ? len : BUFFERSIZE;
	l2=fread(buffer,1,l2,f1);
//	printf("read l2=%d\n",l2);
	if(l2<=0) break; // EOF/error
	l2=fwrite(buffer,1,l2,f2);
	if(l2<=0){ printf("Write error!\n");break; }
//	printf("write l2=%d\n",l2);
	len-=l2;len2+=l2;
    }
    printf("%d bytes copied\n",len2);
#endif
}

int main(int argc,char* argv[]){

FILE* f1=fopen(argv[1],"rb");
if(!f1) return 1;
FILE* f2=fopen(argv[2],"r+");
if(!f2) f2=fopen(argv[2],"ab");
if(!f2) return 1;

double fpms=1.0; // frames/milisec
static int chunksize=1;

read_wavhdr(f1);
if(wav_channels){
    // WAV !!!
    chunksize=wav_blocksize;
    fpms=wav_samplerate*0.001;
} else {
    rewind(f1); //fseek(f1,0,SEEK_SET);
    if(!read_ac3hdr(f1)){
	printf("Unknown/unsupported audio format...\n");
	return 1;
    }
    // AC3 !!!
    chunksize=ac3_blocksize;
    fpms=0.001*ac3_samplerate/ac3_framelen;
}

int pos1=atoi(argv[3]);
int pos2=atoi(argv[4]);

    off_t fpos=pos1*fpms+0.5;
    long long len=pos2*fpms+0.5;
    len-=fpos;
    if(len<=0){
	printf("negative cut!!! ABORTING!\n");
	return 1;
    }
    
	// copy big area from orig dub:
	int ret=fseeko(f2,fpos*chunksize+wavhdr_len,SEEK_SET);
	if(ret<0){ printf("Seek error!!!!!!\n"); return 1;}
	ret=fseeko(f1,fpos*chunksize+wavhdr_len,SEEK_SET);
	if(ret<0){ printf("Seek error!!!!!!\n");}
	printf("COPY_ORIG %4.3fs to %4.3fs  (%d frames to %d)\n",len*0.001/fpms,fpos*0.001/fpms,(int)len,(int)fpos);
	printf("Copying %lld bytes to %lld...\n",len*chunksize,fpos*chunksize+wavhdr_len);
	filecopy(f2,f1,len*chunksize);

fclose(f2);
fclose(f1);
return 0;
}
