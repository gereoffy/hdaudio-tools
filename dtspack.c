#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include <dca.h>

int main(int argc,char* argv[]){
dca_state_t* s=dca_init(0);

FILE* f=fopen(argv[1],"rb");
FILE* f2=fopen(argv[2],"wb");

unsigned char buffer[8192];
fread(buffer,2,1,f);

// try DTS:
while(1){
    if(fread(buffer+2,14,1,f)<=0) break;
    int flags=0,frlen=0,samplerate=0,bitrate=0;
    int len=dca_syncinfo(s, buffer, &flags, &samplerate, &bitrate, &frlen);
    printf("DTS -> len=%d frlen=%d (%d Hz, %d bps)\n",len,frlen,samplerate,bitrate);
    if(fread(buffer+16,len-16,1,f)<=0) break;
    if(f2) fwrite(buffer,len,1,f2);
    // find padded blocksize:
    int i;
    int c1=fgetc(f);
    for(i=0;i<4096;i++){
        int c2=fgetc(f);
        if(c1==buffer[0] && c2==buffer[1]) break;
        c1=c2;
    }
    if(c1<0) break; // EOF
    if(i) printf("skipped %d bytes (padded size: %d)\n",i,len+i);
}

if(f2) fclose(f2);
fclose(f);

dca_free(s);
return 0;
}

