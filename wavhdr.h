
#define WAV_ID_RIFF 0x46464952 /* "RIFF" */
#define WAV_ID_WAVE 0x45564157 /* "WAVE" */
#define WAV_ID_FMT  0x20746d66 /* "fmt " */
#define WAV_ID_DATA 0x61746164 /* "data" */
#define WAV_ID_PCM  0x0001
#define WAV_ID_FLOAT 0x0003

struct WaveHeader
{
	uint32_t riff;
	uint32_t file_length;
	uint32_t wave;
	uint32_t fmt;

	uint32_t fmt_length;
	uint16_t fmt_tag;
	uint16_t channels;
	uint32_t sample_rate;
	uint32_t bytes_per_second;
	
	uint16_t block_align;
	uint16_t bits;
	
	uint8_t extradata[256];

//	uint32_t data;
//	uint32_t data_length;
};
static struct WaveHeader wavhdr;
static int wavhdr_len=0;
static int wav_channels=0;
static int wav_samplerate=0;
static int wav_blocksize=1;
static int wav_format=0;

static void read_wavhdr(FILE* f1){
    fread(&wavhdr,sizeof(wavhdr)-256,1,f1);
    if(bswap_32(wavhdr.riff)!=WAV_ID_RIFF ||
       bswap_32(wavhdr.wave)!=WAV_ID_WAVE ||
       bswap_32(wavhdr.fmt)!=WAV_ID_FMT){
        fprintf(stderr,"Not WAV file format!!!!\n");
//        printf("%.4s %.4s\n",&wavhdr.riff,&wavhdr.wave);
        return;
    }
    wavhdr_len=0x14+bswap_32(wavhdr.fmt_length)+8;
//    printf("wavhdr_len=%d sizeof=%d\n",wavhdr_len,sizeof(wavhdr)-256);
    fread(wavhdr.extradata,wavhdr_len-(sizeof(wavhdr)-256),1,f1);
    //
    wav_blocksize=bswap_16(wavhdr.channels)*(bswap_16(wavhdr.bits)/8);
    wav_channels=bswap_16(wavhdr.channels);
    if(bswap_32(wavhdr.sample_rate)==50050){ // fixup resampling!
	wavhdr.sample_rate=bswap_32(48000);
	wavhdr.bytes_per_second=bswap_32(48000*bswap_16(wavhdr.block_align));
    }
    wavhdr.file_length=0;
    memset(&wavhdr.extradata[wavhdr_len-(sizeof(wavhdr)-256)-4],0,4);
    wav_samplerate=bswap_32(wavhdr.sample_rate);
    wav_format=bswap_16(wavhdr.fmt_tag);
    fprintf(stderr,"WAV: headerlen=%d blocksize=%d channels=%d rate=%d format=%d\n",
	wavhdr_len,wav_blocksize,wav_channels,wav_samplerate,wav_format);
}

static void make_wavhdr(short fmt,short bits,int channels,int samplerate){
memset(&wavhdr,0,sizeof(wavhdr));
wavhdr.riff=bswap_32(WAV_ID_RIFF);
wavhdr.wave=bswap_32(WAV_ID_WAVE);
wavhdr.fmt=bswap_32(WAV_ID_FMT);
wavhdr.fmt_length=bswap_32(16);
wavhdr.fmt_tag=bswap_16(fmt);
wavhdr.channels=bswap_16(channels);
wavhdr.sample_rate=bswap_32(samplerate);
wavhdr.bits=bswap_16(bits);
bits=(bits+7)/8;
wavhdr.block_align=bswap_16(bits*channels);
wavhdr.bytes_per_second=bswap_32(bits*channels*samplerate);
memcpy(wavhdr.extradata,"data",4);
wavhdr_len=sizeof(wavhdr)+8-256;
}

static void fix_wavhdr(unsigned int filesize){
    wavhdr.file_length=bswap_32(filesize-8);
    *((unsigned int*)(wavhdr.extradata+4))=bswap_32(filesize-wavhdr_len);
}
