
#define BUFSIZE 8192
#define MAXCH 8

//double scale=(24/1.001)/25.0;   // ntsc->pal
//double resample=25.0/(24/1.001); // pal->ntsc
double resample=1.0; //25.0/(24/1.001); // pal->ntsc
#ifdef MPEX2
double pitch=1.0;
int quality=3; // 1..4, 4=hq
int hook=1;
int debug=0;
#endif
float volume=1.0;
int format=1; // 0=pcm 1=float
int split=0; // output: multiple wavs
int wavs=0;  // input:  multiple wavs
int channels=0;
int out_bitrate=0;
int agm=0; // .agm format (for DTS encode)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

#include "version.h"

#ifdef MPEX2
#include "mpex2v3.h"
#else
#include "samplerate.h"
#endif

#include <a52dec/a52.h>
#ifdef DTS
#include <dca.h>
#endif

#include "bswap.h"
#include "wavhdr.h"
#include "ac3hdr.h"
#include "agmhdr.h"

a52_state_t* ac3_state=NULL;
#ifdef DTS
dca_state_t* dca_state=NULL;
#endif
sample_t* ac3_samples=NULL;

float inbuf[8*0x2000];
FILE* f1=NULL;
FILE* f1s[8]={NULL,};
FILE* f2=NULL;
FILE* f2s[8]={NULL,};

    // WAV: FL FR FC LFE RL RR
//	           0    1     2   3     4    5
//  char* id[6]={"LFE","FL","C" ,"FR","SL","SR"}; // AC3
//  char* id[6]={"C"  ,"FL","FR","SL","SR","LFE"}; // dts
//unsigned int orig_order[]={256*0,256*1,256*2,256*3,256*4,256*5,256*6,256*7};
//unsigned int ac3_order[]={256*1,256*3,256*2,256*0,256*4,256*5,256*6,256*7};
//unsigned int dts_order[]={256*1,256*2,256*0,256*5,256*3,256*4,256*6,256*7};


char* get_dts_order(int flags){
//   1, 2, 2, 2, 2, 3, 3, 4, 4, 5, 6, 6, 6, 7, 8, 8
    switch (flags & (DCA_CHANNEL_MASK|DCA_LFE)) {
    case DCA_MONO:	return "0";
    case DCA_CHANNEL:
    case DCA_STEREO:
    case DCA_STEREO_SUMDIFF:
    case DCA_STEREO_TOTAL:	return "01";
    case DCA_3F:	return "120";
    case DCA_2F1R:	return "012";
    case DCA_3F1R:	return "1203";
    case DCA_2F2R:	return "0123";
    case DCA_3F2R:	return "12034";
    // LFE:
    case DCA_LFE|DCA_MONO:	return "01";
    case DCA_LFE|DCA_CHANNEL:
    case DCA_LFE|DCA_STEREO:
    case DCA_LFE|DCA_STEREO_SUMDIFF:
    case DCA_LFE|DCA_STEREO_TOTAL:	return "012";
    case DCA_LFE|DCA_3F:	return "1203";
    case DCA_LFE|DCA_2F1R:	return "0132";
    case DCA_LFE|DCA_3F1R:	return "12043";
    case DCA_LFE|DCA_2F2R:	return "01423";
    case DCA_LFE|DCA_3F2R:	return "120534";
    }

    return NULL;
}


char* get_ac3_order(int flags){
    switch (flags&(A52_CHANNEL_MASK|A52_LFE)) {
    case A52_MONO:
    case A52_CHANNEL1:
    case A52_CHANNEL2:	return "0";
    case A52_CHANNEL:
    case A52_STEREO:
    case A52_DOLBY:	return "01";
    case A52_3F:	return "021";
    case A52_2F1R:	return "012";
    case A52_3F1R:	return "0213";
    case A52_2F2R:	return "0123";
    case A52_3F2R:	return "02134";
    case A52_MONO | A52_LFE:
    case A52_CHANNEL1 | A52_LFE:
    case A52_CHANNEL2 | A52_LFE:	return "10";
    case A52_CHANNEL | A52_LFE:
    case A52_STEREO | A52_LFE:
    case A52_DOLBY | A52_LFE:	return "120";
    case A52_3F | A52_LFE:	return "1320";
    case A52_2F1R | A52_LFE:	return "1203";
    case A52_3F1R | A52_LFE:	return "13204";
    case A52_2F2R | A52_LFE:	return "12034";
    case A52_3F2R | A52_LFE:	return "132045";
    }
    return NULL;
}


long (*callback)(void *cb_data, float **data)=NULL;


long callback_wav(void *cb_data, float **data){
    unsigned char buffer[8192];
    int frlen=fread(buffer,1,sizeof(buffer),f1);
//    printf("frlen=%d\n",frlen);
    frlen/=wav_blocksize;
    if(frlen<=0) return 0; // EOF
    //
    float* ptr=inbuf;
    int i,j;
    if(wav_format==WAV_ID_FLOAT){
	// float
        for(j=0;j<frlen;j++){
	    unsigned int* src=(unsigned int*)(buffer+wav_blocksize*j);
	    for(i=0;i<channels;i++){
		union {
		    unsigned int i;
		    float f;
		} x;
		x.i=bswap_32(src[i]);
	        *ptr++=x.f*volume;
	    }
	}
    } else {
	// int16
        for(j=0;j<frlen;j++){
	    unsigned short* src=(unsigned short*)(buffer+wav_blocksize*j);
	    for(i=0;i<channels;i++){
		short x=bswap_16(src[i]);
	        *ptr++=x*(volume/32768.0);
	    }
	}
    }
    *data=inbuf;
    return frlen; // len/channels;
}


long callback_wavs(void *cb_data, float **data){
  int c,frlen=0;
  for(c=0;c<channels;c++){
    unsigned char buffer[8192];
    frlen=fread(buffer,1,sizeof(buffer),f1s[c]);
//    printf("frlen=%d\n",frlen);
    frlen/=wav_blocksize;
    if(frlen<=0) return 0; // EOF
    //
    float* ptr=inbuf+c;
    int j;
    if(wav_format==WAV_ID_FLOAT){
	// float
        for(j=0;j<frlen;j++){
	    unsigned int* src=(unsigned int*)(buffer+wav_blocksize*j);
	    union {
	        unsigned int i;
	        float f;
	    } x;
	    x.i=bswap_32(*src);
	    *ptr=x.f*volume;
	    ptr+=channels;
	}
    } else {
	// int16
        for(j=0;j<frlen;j++){
	    unsigned short* src=(unsigned short*)(buffer+wav_blocksize*j);
	    short x=bswap_16(*src);
	    *ptr=x*(volume/32768.0);
	    ptr+=channels;
	}
    }
  }
  *data=inbuf;
  return frlen; // len/channels;
}




long callback_ac3(void *cb_data, float **data){
//    printf("callback!\n");
    unsigned char buffer[8192];
retry:
    if(fread(buffer,ac3_blocksize,1,f1)<=0) return 0;

    sample_t level=1;
    int flags,ret;
    int samplerate,bitrate,size,frlen=0x600;
#ifdef DTS
	if(ac3_channels&256)	// DTS!!!
	    size=dca_syncinfo(dca_state, buffer,&flags,&samplerate,&bitrate,&frlen);
	else
#endif
	    size=a52_syncinfo(buffer,&flags,&samplerate,&bitrate);
	if(size<=0){
	    fprintf(stderr,"AC3/DTS stream sync error!!!\n");
	    fseek(f1,-(ac3_blocksize-1),SEEK_CUR);
	    goto retry; //	    return 0; // error!!!!!!!
	}
	if(size>ac3_blocksize || frlen!=ac3_framelen){
	    fprintf(stderr,"VBR file? aborting...  (%d != %d)\n",size,ac3_blocksize);
	    return 0; // not yet supported
	}
//	printf("syncinfo: %d  (%d Hz, %d bps)  %d\n",size,samplerate,bitrate,frlen);

    char* order_str;

#ifdef DTS
    if(ac3_channels&256){
	    if(channels==2) flags=DCA_STEREO;
//	    if(channels>4) order=dts_order;
//	    flags=DCA_STEREO|DCA_ADJUST_LEVEL; level=32768;
//	    flags=-1; level=1;
	    ret=dca_frame(dca_state, buffer, &flags, &level, 0);
	    order_str=get_dts_order(flags);
    } else
#endif
    {
	    if(channels==2) flags=A52_STEREO;
//	    if(channels>4) order=ac3_order;
//	    flags=A52_STEREO|A52_ADJUST_LEVEL; level=32768;
//	    flags=-1; level=1;
	    ret=a52_frame(ac3_state, buffer, &flags, &level, 0);
	    order_str=get_ac3_order(flags);
    }
    
//    printf("ac3/dts decoder ret=%d\n",ret);

//    printf("ret=%d channels=%d flags=%d order=%s\n",ret,channels,flags,order_str);
    
    int b,i,j;
    unsigned int order[8]={-1,-1,-1,-1,-1,-1,-1,-1};
    for(i=0;i<strlen(order_str);i++) order[i]=(order_str[i]-'0')*256;

    int len=0;
    for(b=0;b<frlen/256;b++){
#ifdef DTS
	if(ac3_channels&256){
		dca_block(dca_state);
		ac3_samples=dca_samples(dca_state);
	} else
#endif
		a52_block(ac3_state);

//	printf("len=%d\n",len);

	for(j=0;j<256;j++)
	    for(i=0;i<channels;i++)
		inbuf[len++]=(order[i]<0) ? 0 : volume*ac3_samples[order[i]+j];
	
    }
    
    *data=inbuf;
    return frlen; // len/channels;
}


void write_wav(float* outbuf,int ret){
    int i;
    if(format&1){
	// float!
	unsigned int outbuf2[MAXCH*BUFSIZE];
	for(i=0;i<ret*channels;i++){
	    outbuf2[i]=bswap_32(*((unsigned int*)(&outbuf[i])));
	}
	fwrite(outbuf2,4,i,f2);
    } else {
	unsigned char outbuf2[2*MAXCH*BUFSIZE];
	for(i=0;i<ret*channels;i++){
	    int x=outbuf[i]*32768.0;
	    if(x<-32767) x=-32767; else
	    if(x>32767) x=32767;
//#ifdef BSWAP
	    outbuf2[2*i+0]=x&255;
	    outbuf2[2*i+1]=(x>>8)&255;
//#else
//	    outbuf2[2*i+0]=(x>>8)&255;
//	    outbuf2[2*i+1]=x&255;
//#endif
	}
	fwrite(outbuf2,2,i,f2);
    }
    fflush(f2);
}

void write_wavs(float* outbuf,int ret){
    int i,c;
    if(format&1){
	// float!
	unsigned int outbuf2[BUFSIZE];
	for(c=0;c<channels;c++){
	    unsigned int* p=((unsigned int*)(&outbuf[c]));
	    for(i=0;i<ret;i++){
		outbuf2[i]=bswap_32(p[0]);
		p+=channels;
	    }
	    fwrite(outbuf2,4,i,f2s[c]);
	    fflush(f2s[c]);
	}
    } else {
	unsigned char outbuf2[2*BUFSIZE];
	for(c=0;c<channels;c++){
	    for(i=0;i<ret;i++){
		unsigned int x=outbuf[i*channels+c]*32768.0;
		outbuf2[2*i+0]=x&255;
		outbuf2[2*i+1]=x>>8;
	    }
	    fwrite(outbuf2,2,i,f2s[c]);
	    fflush(f2s[c]);
	}
    }
}

FILE* my_popen(char* cmd,char* args[]){
    int pipe_to[2];  // [0]=read [1]=write
    pipe(pipe_to);
    int pid=fork();
    if(pid==0){
	// child
	close(pipe_to[1]);
	close(0); dup(pipe_to[0]);
	close(pipe_to[0]);
	execvp(cmd,args);
//	execl("/bin/sh", "sh", "-c", cmd, NULL);
//	system(cmd);
	perror("execlp");
	_exit(1);
    }
    close(pipe_to[0]);
//    fcntl(pipe_to[1], F_SETFL, fcntl(pipe_to[1],F_GETFL) | O_NONBLOCK);
    return fdopen(pipe_to[1],"w");
}


// Returns current time in microseconds
unsigned int GetTimer(void){
  struct timeval tv;
//  float s;
  gettimeofday(&tv,NULL);
//  s=tv.tv_usec;s*=0.000001;s+=tv.tv_sec;
  return (tv.tv_sec*1000000+tv.tv_usec);
}  




int main(int argc,char* argv[]){

fprintf(stderr,"\n" VERSION "\n\n");

#if 0

int i;
for(i=0;i<10;i++){
    char* dts=get_dts_order(i);
    if(!dts) continue;
    int len=strlen(dts);
    char jo[10];
    int j;
    for(j=0;j<len;j++) jo[dts[j]-'0']='0'+j;
    jo[len]=0;
    printf("%d: %s -> %s\n",i,dts,jo);
}
return;

#endif



int param=0;
while(++param<argc){
    if(argv[param][0]!='-') break;
    // option
    if(argv[param][1]==0) continue; // - -filename
    else if(!strcasecmp(argv[param]+1,"split"))
        split=1; // multiple wavs
    else if(!strcasecmp(argv[param]+1,"wavs"))
        wavs=1; // multiple wavs
    else if(!strcasecmp(argv[param]+1,"int"))
        format=0; // int16 (no float)
    else if(!strcasecmp(argv[param]+1,"pal"))
        resample=25.0/(24.0/1.001); // ntsc2pal
    else if(!strcasecmp(argv[param]+1,"ntsc"))
        resample=(24.0/1.001)/25.0; // pal2ntsc
    else if(!strcasecmp(argv[param]+1,"resample"))
        resample=atof(argv[++param]); // pal2ntsc (resample)
#ifdef MPEX2
    else if(!strcasecmp(argv[param]+1,"pitch"))
        pitch=atof(argv[++param]);
    else if(!strcasecmp(argv[param]+1,"slow"))
        hook=0;
    else if(!strcasecmp(argv[param]+1,"hq"))
        quality=4;
//    else if(!strcasecmp(argv[param]+1,"debug"))
//        debug=1;
#endif
    else if(!strcasecmp(argv[param]+1,"volume"))
        volume=atof(argv[++param]);
    else if(!strcasecmp(argv[param]+1,"b"))
        out_bitrate=atoi(argv[++param]);
    else if(!strcasecmp(argv[param]+1,"ch") || !strcasecmp(argv[param]+1,"channels"))
        channels=atoi(argv[++param]);
    else {
        fprintf(stderr,"Unknow option: %s\n",argv[param]);
        return 1;
    }
}

if(param>=argc){
    fprintf(stderr,"Missing filename!\n");
    return 1;
}

int samplerate=0;
off_t f1size=0;

if(!wavs){

f1=fopen(argv[param++],"rb");
if(!f1) return 1;

if(!fseeko(f1,0,SEEK_END)) f1size=ftello(f1);
rewind(f1);
read_wavhdr(f1);
if(!wav_channels){
    rewind(f1);
    read_ac3hdr(f1);
    if(!ac3_channels) return 1;
    rewind(f1);
    // AC3!!!
    callback=callback_ac3;
    samplerate=ac3_samplerate;
    if(!channels) channels = (ac3_channels&255)<=3 ? 2 : (ac3_channels&255);
#ifdef DTS
    if(ac3_channels&256){
	dca_state=dca_init(0);
	ac3_samples=dca_samples(dca_state);
    } else
#endif
    {
	ac3_state=a52_init(1);
	ac3_samples=a52_samples(ac3_state);
    }
} else {
    // WAV!!!
    callback=callback_wav;
    samplerate=wav_samplerate;
    if(!channels) channels = wav_channels;
}

} else {
    // INPUT: multiple WAVs:
    int c;
    for(c=0;c<channels;c++){
	f1s[c]=fopen(argv[param++],"rb");
	if(!fseeko(f1s[c],0,SEEK_END)) f1size=ftello(f1s[c]);
	rewind(f1s[c]);
	read_wavhdr(f1s[c]);
	if(!wav_channels) return 1;
    }
    callback=callback_wavs;
    samplerate=wav_samplerate;
}


if(resample>10) resample/=samplerate;

fprintf(stderr,"resample: %s, %s, channels=%d, scale=%5.12f\n",
    format ? "float" : "int", split ? "multiple wavs" : "single wav",
    channels,resample);

if(param>=argc) return 0;


#ifdef MPEX2
if(!MPEX2_load(hook)) return 0;
#else
int error;
SRC_STATE* state=src_callback_new(callback,1,channels,&error,NULL);
//SRC_STATE* state=src_callback_new(callback,1,2,&error,NULL);
#endif

if(split){
    make_wavhdr((format&1) ? WAV_ID_FLOAT : WAV_ID_PCM,(format&1)?32:16,1,samplerate);
    int c;
    for(c=0;c<channels;c++){
	char name[1024];
    	if(channels==6){
	    // WAV: FL FR FC LFE RL RR
	    char* id[6]={"FL","FR","C","LFE","SL","SR"}; // WAV6
	    sprintf(name,"%s-%s.wav",argv[param],id[c]);
	} else
    	if(channels==2)
	    sprintf(name,"%s-%c.wav",argv[param],c?'R':'L');
	else
	    sprintf(name,"%s-%d.wav",argv[param],c);
	f2s[c]=fopen(name,"wb");
	if(!f2s[c]) return 2;
	fwrite(&wavhdr,wavhdr_len,1,f2s[c]);
    }
} else {
    char* p=strstr(argv[param],".ac3");
    if(!p)p=strstr(argv[param],".AC3");
    if(p && p[4]==0){
	// AC3!
	char br[100]; sprintf(br,"%d",out_bitrate?out_bitrate:ac3_bitrate);
	char* args[]={"aften","-b",br,"-",argv[param],NULL};
//	sprintf(cmd,"aften -b %d - '%s'",out_bitrate?out_bitrate:ac3_bitrate,argv[param]);
//	printf("Sending output to filter: %s\n",cmd);
//	f2=popen(cmd,"w");
	f2=my_popen("aften",args);
	if(!f2) perror("Failed to open pipe to aften");
    } else {
	p=strstr(argv[param],".agm");
	if(!p)p=strstr(argv[param],".AGM");
	if(p && p[4]==0){ agm=1; format=0; }
	p=strstr(argv[param],".dts");
	if(!p)p=strstr(argv[param],".DTS");
	if(!p)p=strstr(argv[param],".cpt");
	if(!p)p=strstr(argv[param],".CPT");
        if(p && p[4]==0){
    	    // DTS!
    	    agm=1; format=0;
	    char br[100]; sprintf(br,"%d",out_bitrate?out_bitrate:ac3_bitrate);
	    char* args[]={"agm2dts","-b",br,"-",argv[param],NULL};
//	    char* args[]={"agm2dts","-",argv[param],NULL};
	    f2=my_popen("agm2dts",args);
	    if(!f2) perror("Failed to open pipe to agm2dts");
        } else
	f2=strcmp(argv[param],"-") ? fopen(argv[param],"wb") : stdout;
    }
    if(!f2) return 2;
    if(agm) {
	fwrite(agmhdr_51,sizeof(agmhdr_51),1,f2);
    } else {
	make_wavhdr((format&1) ? WAV_ID_FLOAT : WAV_ID_PCM,(format&1)?32:16,channels,samplerate);
	fwrite(&wavhdr,wavhdr_len,1,f2);
    }
}

unsigned int f1time=GetTimer();

while(1){
    long ret;
    float outbuf_[MAXCH*BUFSIZE];
    float* outbuf=outbuf_;
#ifdef MPEX2
    ret=MPEX2_process(resample, pitch, quality, samplerate, channels, callback, outbuf);
#else
    if(resample==1.0)
	ret=callback(NULL,&outbuf); // copy
    else
	ret=src_callback_read(state, resample, BUFSIZE, outbuf); // resample
#endif

//    printf("->%d\n",(int)ret);
    if(ret<=0) break;
    if(split)
	write_wavs(outbuf,ret);
    else
	write_wav(outbuf,ret);
    if(f1size){
	off_t f1pos=ftello(wavs ? f1s[0] : f1);
	unsigned int t=GetTimer();t-=f1time;
	fprintf(stderr,"\r%5.2f%% %4.1fmin",f1pos*100.0/f1size,
	    (f1size-f1pos) * ((t/60000000.0)/f1pos) );fflush(stderr);
    }
}

off_t filesize=ftello(split?f2s[0]:f2);
printf("WAV filesize: %lld\n",(long long)filesize);
if(agm){
    // fixup agm header
    if(!fseek(f2,sizeof(agmhdr_51)-8,SEEK_SET)) fwrite(&filesize,8,1,f2);
} else
if(filesize<=0x7fffffff){
    fix_wavhdr(filesize);
    if(split){
        int c;
	for(c=0;c<channels;c++)
	    if(!fseek(f2s[c],0,SEEK_SET)) fwrite(&wavhdr,wavhdr_len,1,f2s[c]);
    } else
	if(!fseek(f2,0,SEEK_SET)) fwrite(&wavhdr,wavhdr_len,1,f2);
}

#ifndef MPEX2
state=src_delete(state);
#endif

return 0;
}
