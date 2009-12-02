#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include <mad.h>

#include "bswap.h"
#include "wavhdr.h"

typedef struct mad_decoder_s {

  struct mad_synth  synth; 
  struct mad_stream stream;
  struct mad_frame  frame;
  
  int have_frame;

  int               output_sampling_rate;
  int               output_open;
  int               output_mode;

} mad_decoder_t;

unsigned char a_in_buffer[4096];
int a_in_buffer_len=0;
int skipped=0;

static int read_frame(FILE* f,mad_decoder_t *this){

while(1){
  int len=sizeof(a_in_buffer)-a_in_buffer_len; // if(len>512) len=512;
  len=fread(a_in_buffer+a_in_buffer_len,1,len,f);
//  printf("len=%d\n",len);
  if(len<=0) break;
  a_in_buffer_len+=len;
  while(1){
    int ret;
    mad_stream_buffer (&this->stream, a_in_buffer, a_in_buffer_len);
    ret=mad_frame_decode (&this->frame, &this->stream);
    if (this->stream.next_frame) {
	int num_bytes =
	    (char*)a_in_buffer+a_in_buffer_len - (char*)this->stream.next_frame;
	memmove(a_in_buffer, this->stream.next_frame, num_bytes);
	//printf("mad: %d bytes processed\n",a_in_buffer_len-num_bytes);
	if(ret!=0) skipped+=a_in_buffer_len-num_bytes;
	a_in_buffer_len = num_bytes;
    }
    if (ret == 0) return 1; // OK!!!
    // error! try to resync!
    if(this->stream.error==MAD_ERROR_BUFLEN) break;
  }
}
printf("Cannot sync MAD frame\n");
return 0;
}


/* utility to scale and round samples to 16 bits */
static inline signed int scale(mad_fixed_t sample) {
  /* round */
  sample += (1L << (MAD_F_FRACBITS - 16));

  /* clip */
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  /* quantize */
  return sample >> (MAD_F_FRACBITS + 1 - 16);
}

unsigned short outbuf[65536];

static int decode_audio(FILE* f,mad_decoder_t *this){

    if(!this->have_frame) this->have_frame=read_frame(f,this);
    if(!this->have_frame) return 0; // failed to sync... or EOF
    this->have_frame=0;

    mad_synth_frame (&this->synth, &this->frame);

	  unsigned int         nchannels, nsamples;
	  mad_fixed_t const   *left_ch, *right_ch;
	  struct mad_pcm      *pcm = &this->synth.pcm;
	  unsigned short      *output = outbuf;

	  nchannels = pcm->channels;
	  nsamples  = pcm->length;
	  left_ch   = pcm->samples[0];
	  right_ch  = pcm->samples[1];

	  int len=2*nchannels*nsamples;
	  
	  while (nsamples--) {
	    /* output sample(s) in 16-bit signed little-endian PCM */
	    
	    *output++ = bswap_16(scale(*left_ch++));
	    
	    if (nchannels == 2) 
	      *output++ = bswap_16(scale(*right_ch++));

	  }

    return len;
}


int main(int argc,char* argv[]){

  FILE* f1=fopen(argv[1],"rb");
  if(!f1){
    printf("Cannot open input file: %s\n",argv[1]);
    return 1;
  }

  mad_decoder_t *this = (mad_decoder_t *) malloc(sizeof(mad_decoder_t));
  memset(this,0,sizeof(mad_decoder_t));
  
  mad_synth_init  (&this->synth);
  mad_stream_init (&this->stream);
  mad_frame_init  (&this->frame);

  this->have_frame=read_frame(f1,this);
  if(!this->have_frame) return 0; // failed to sync...
  
  int channels=(this->frame.header.mode == MAD_MODE_SINGLE_CHANNEL) ? 1 : 2;
  int samplerate=this->frame.header.samplerate;
  int i_bps=this->frame.header.bitrate/8;

  printf("channels=%d rate=%d bps=%d skipped=%d delay=%5.3f\n",channels,samplerate,i_bps,skipped,skipped/(float)samplerate);

  FILE* f2=fopen(argv[2],"wb");
  if(!f2){
    printf("Cannot open output file: %s\n",argv[2]);
    return 1;
  }

  make_wavhdr(WAV_ID_PCM,16,channels,samplerate);
  fwrite(&wavhdr,wavhdr_len,1,f2);
  
  int i;
  for(i=0;i<skipped*2*channels;i++) fputc(0,f2);
  
  while(1){
    int len=decode_audio(f1,this);
    if(len<=0) break;
    fwrite(outbuf,len,1,f2);
  }

  fix_wavhdr(ftello(f2));
  fseeko(f2,0,SEEK_SET);
  fwrite(&wavhdr,wavhdr_len,1,f2);
  fclose(f2);
  
  mad_synth_finish (&this->synth);
  mad_frame_finish (&this->frame);
  mad_stream_finish(&this->stream);
  return 0;
}
