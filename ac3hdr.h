
/**
 * Maps audio coding mode (acmod) to number of full-bandwidth channels.
 * from ATSC A/52 Table 5.8 Audio Coding Mode
 */
const uint8_t ff_ac3_channels_tab[8] = {
    2, 1, 2, 3, 3, 4, 4, 5
};

/* possible frequencies */
const uint16_t ff_ac3_sample_rate_tab[3] = { 48000, 44100, 32000 };

/* possible bitrates */
const uint16_t ff_ac3_bitrate_tab[19] = {
    32, 40, 48, 56, 64, 80, 96, 112, 128,
    160, 192, 224, 256, 320, 384, 448, 512, 576, 640
};

/**
 * Possible frame sizes.
 * from ATSC A/52 Table 5.18 Frame Size Code Table.
 */
const uint16_t ff_ac3_frame_size_tab[38][3] = {
    { 64,   69,   96   },
    { 64,   70,   96   },
    { 80,   87,   120  },
    { 80,   88,   120  },
    { 96,   104,  144  },
    { 96,   105,  144  },
    { 112,  121,  168  },
    { 112,  122,  168  },
    { 128,  139,  192  },
    { 128,  140,  192  },
    { 160,  174,  240  },
    { 160,  175,  240  },
    { 192,  208,  288  },
    { 192,  209,  288  },
    { 224,  243,  336  },
    { 224,  244,  336  },
    { 256,  278,  384  },
    { 256,  279,  384  },
    { 320,  348,  480  },
    { 320,  349,  480  },
    { 384,  417,  576  },
    { 384,  418,  576  },
    { 448,  487,  672  },
    { 448,  488,  672  },
    { 512,  557,  768  },
    { 512,  558,  768  },
    { 640,  696,  960  },
    { 640,  697,  960  },
    { 768,  835,  1152 },
    { 768,  836,  1152 },
    { 896,  975,  1344 },
    { 896,  976,  1344 },
    { 1024, 1114, 1536 },
    { 1024, 1115, 1536 },
    { 1152, 1253, 1728 },
    { 1152, 1254, 1728 },
    { 1280, 1393, 1920 },
    { 1280, 1394, 1920 },
};

// DTS:
static const uint8_t nfchans_tbl[]={ 1, 2, 2, 2, 2, 3, 3, 4, 4, 5, 6, 6, 6, 7, 8, 8 };

static unsigned char ac3_hdr[16];
static int ac3_channels=0;
static int ac3_samplerate=0;
static int ac3_bitrate=0;
static int ac3_blocksize=0;
static int ac3_framelen=0;

int read_ac3hdr(FILE* f){
    fread(ac3_hdr,16,1,f);
    if(ac3_hdr[0]!=0x0B || ac3_hdr[1]!=0x77){
	fprintf(stderr,"Not AC3 file format!!!\n");
#ifdef DTS
	// try DTS:
	dca_state_t* s=dca_init(0);
	int flags=0;
	ac3_blocksize=dca_syncinfo(s, ac3_hdr, &flags,
                  &ac3_samplerate, &ac3_bitrate, &ac3_framelen);
	if(ac3_blocksize<=0){
	    fprintf(stderr,"Not DTS file format!!!\n");
	    return 0; // not DTS
	}
        ac3_bitrate>>=10;
	ac3_channels= 256 | nfchans_tbl[flags & DCA_CHANNEL_MASK];
	if (flags & DCA_LFE) ++ac3_channels;
	fprintf(stderr,"DTS: len=%d frlen=%d (%d Hz, %d kbps)\n",
	    ac3_blocksize,ac3_framelen,ac3_samplerate,ac3_bitrate);
	// find padded blocksize:
	int i;
	for(i=16;i<ac3_blocksize;i++) fgetc(f); // skip to end of frame
	int c1=fgetc(f);
	for(i=0;i<4096;i++){
	    int c2=fgetc(f);
	    if(c1==ac3_hdr[0] && c2==ac3_hdr[1]) break;
	    c1=c2;
	}
	if(i>2048) fprintf(stderr,"Warning! can't find padded blocksize!\n");
	else ac3_blocksize+=i;
	// done
	dca_free(s);
	if(ac3_blocksize>0) return ac3_channels;
#endif
	return 0; // not ac3
    }
    ac3_channels=ff_ac3_channels_tab[ac3_hdr[6]>>5] + ((ac3_hdr[7]&0x40)?1:0);
    ac3_samplerate=ff_ac3_sample_rate_tab[ac3_hdr[4]>>6];
    ac3_bitrate=ff_ac3_bitrate_tab[(ac3_hdr[4]&0x3e)>>1];
    ac3_blocksize=ff_ac3_frame_size_tab[(ac3_hdr[4]&0x3e)][ac3_hdr[4]>>6]*2;
    ac3_framelen=6*256; // 1536 samples/block
    fprintf(stderr,"AC3: %d.%d ch, %d Hz, %d kbit/s (blocksize: %d)\n",
	ff_ac3_channels_tab[ac3_hdr[6]>>5],(ac3_hdr[7]&0x40)?1:0,
	ac3_samplerate,ac3_bitrate,ac3_blocksize);
    return ac3_channels;
}

