#define CH 2
//#define RESAMPLE (25.0/23.976)

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
    while(len>0){
	int l2=(len<BUFFERSIZE) ? len : BUFFERSIZE;
	l2=fread(buffer,1,l2,f1);
	if(l2<=0) break; // EOF/error
	fwrite(buffer,1,l2,f2);
	len-=l2;
    }
#endif
}

int num_cuts=0;
int cuts[1000][3];

int main(int argc,char* argv[]){
int c;

printf("==============================================================================\n");
FILE* cutlist=fopen("cutlist.txt","r");
char sor[1000];
while(fgets(sor,1000,cutlist)){
    char* e=sor;
    ++num_cuts;
    cuts[num_cuts][0]=strtol(e,&e,10);
    if(*e==',') ++e;
    cuts[num_cuts][1]=strtol(e,&e,10);
    if(*e==',') ++e;
    cuts[num_cuts][2]=strtol(e,&e,10);
#ifdef RESAMPLE
    cuts[num_cuts][0]=cuts[num_cuts][0]*RESAMPLE;
    cuts[num_cuts][1]=cuts[num_cuts][1]*RESAMPLE;
    cuts[num_cuts][2]=cuts[num_cuts][2]*RESAMPLE;
#endif
    int delay=cuts[num_cuts-1][CH]-cuts[num_cuts][CH];
    int t1=(cuts[num_cuts][0]-delay)/1000;
    int t2=cuts[num_cuts][0]/1000;
    printf("%3d: %10.3fs: %5dms  %d:%02d:%02d.%03ds - %d:%02d:%02d.%03ds%s",num_cuts,cuts[num_cuts][0]*0.001,delay,
	t1/3600,(t1/60)%60,t1%60,cuts[num_cuts][0]-delay-1000*t1,
	t2/3600,(t2/60)%60,t2%60,cuts[num_cuts][0]-1000*t2,
	(cuts[num_cuts][0]<cuts[num_cuts-1][0])?"  <-- Invalid cutpoint!\n":"\n");
}
fclose(cutlist);
printf("==============================================================================\n");
printf("%d cutpoints read\n",num_cuts);


FILE* f1=fopen(argv[1],"rb");
if(!f1) return 1;
FILE* f2=fopen(argv[2],"wb");
if(!f2) return 1;
FILE* f3=fopen(argv[3],"rb");

// create first cutpoint:
cuts[0][0]=0;
cuts[0][1]=0;
cuts[0][2]=0;
// fix startpoint
if(!f3 && cuts[1][0]+cuts[1][CH]<0){
    cuts[1][0]-=cuts[1][CH];
    printf("Correcting initial delay cutpos to %5.3fs\n",cuts[1][0]*0.001);
}
//cuts[1][0]=0;
// create last cutpoint:
cuts[num_cuts+1][0]=20000000; // 20000 sec
cuts[num_cuts+1][1]=cuts[num_cuts][1];
cuts[num_cuts+1][2]=cuts[num_cuts][2];
++num_cuts;

// correcting inserts:
for(c=2;c<num_cuts;c++){
    int delay=cuts[c-1][CH]-cuts[c][CH];
    if(delay>0 && cuts[c][0]-delay>cuts[c-1][0]){
	printf("correcting position #%d at %5.3fs by %d ms\n",c,cuts[c][0]*0.001,-delay);
	cuts[c][0]-=delay;
    }
}

double fpms=1.0; // frames/milisec
static int chunksize=1;

if(f3) read_wavhdr(f3);
read_wavhdr(f1);
if(wav_channels){
    // WAV !!!
    fwrite(&wavhdr,wavhdr_len,1,f2);
    chunksize=wav_blocksize;
    fpms=wav_samplerate*0.001;
} else {
    if(f3){ rewind(f1); read_ac3hdr(f3); }
    rewind(f1); //fseek(f1,0,SEEK_SET);
    if(!read_ac3hdr(f1)){
	printf("Unknown/unsupported audio format...\n");
	return 1;
    }
    // AC3 !!!
    chunksize=ac3_blocksize;
    fpms=0.001*ac3_samplerate/ac3_framelen;
}



for(c=0;c<num_cuts;c++){

    off_t fpos=(cuts[c][0]+cuts[c][CH])*fpms+0.5;
    long long to=cuts[c][0]*fpms+0.5;
    long long len=cuts[c+1][0]*fpms+0.5;

    int delay=(c>=1) ? cuts[c-1][CH]-cuts[c][CH] : 0;
    printf("*** %4.3fs: %dms delay\n",cuts[c][0]*0.001,delay);

    if((c==1 || delay>=500) && f3 && len>=to+delay*fpms+0.5){
	// copy big area from orig dub:
	off_t fpos2=cuts[c][0]*fpms+0.5;
	int ret=fseeko(f3,fpos2*chunksize+wavhdr_len,SEEK_SET);
	if(ret<0){ printf("Seek error!!!!!!\n"); break;}
	long long len2=delay*fpms+0.5;
//	printf("len2=%d to=%d len=%d -> ",(int)(len2/fpms),(int)(to/fpms),(int)(len/fpms));
//	if(len<to+len2) len2=len-to; // prevent negative cut!
//	printf("len2=%d\n",(int)(len2/fpms));
	printf("COPY_ORIG %4.3fs to %4.3fs  (from %4.3fs .. %4.3fs)\n",
	    len2*0.001/fpms,to*0.001/fpms,fpos2*0.001/fpms,(fpos2+len2)*0.001/fpms);
	printf("Copying %lld bytes... (%lld blocks)\n",len2*chunksize,len2);
	filecopy(f2,f3,len2*chunksize);
	to+=len2;
	fpos+=len2;
    }

    len-=to;
    if(len<0){
	printf("negative cut #%d!!! ABORTING!  to=%d len=%d\n",c,cuts[c][0],cuts[c+1][0]);
	break;
    }

    int ret=fseeko(f1,fpos*chunksize+wavhdr_len,SEEK_SET);
//    printf("seek %d -> %d\n",(int)fpos,(int)ret);
    if(ret<0){ printf("Seek error!!!!!!\n"); break;}
//    if(delay>0) len-=delay;
    printf("COPY %4.3fs to %4.3fs  (from %4.3fs .. %4.3fs)\n",
	len*0.001/fpms,to*0.001/fpms,fpos*0.001/fpms,(fpos+len)*0.001/fpms);

    printf("Copying %lld bytes... (%lld blocks)\n",len*chunksize,len);
    filecopy(f2,f1,len*chunksize);

}

fclose(f2);
fclose(f1);
return 0;
}
