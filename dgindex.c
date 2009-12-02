/* Copyright (C) 1996, MPEG Software Simulation Group. All Rights Reserved. */

/*
 * Disclaimer of Warranty
 *
 * These software programs are available to the user without any license fee or
 * royalty on an "as is" basis.  The MPEG Software Simulation Group disclaims
 * any and all warranties, whether express, implied, or statuary, including any
 * implied warranties or merchantability or of fitness for a particular
 * purpose.  In no event shall the copyright-holder be liable for any
 * incidental, punitive, or consequential damages of any kind whatsoever
 * arising from the use of these programs.
 *
 * This disclaimer of warranty extends to the user of these programs and user's
 * customers, employees, agents, transferees, successors, and assigns.
 *
 * The MPEG Software Simulation Group does not represent or warrant that the
 * programs furnished hereunder are free of infringement of any third-party
 * patents.
 *
 * Commercial implementations of MPEG-1 and MPEG-2 video, including shareware,
 * are subject to royalty fees to patent holders.  Many of these patents are
 * general enough such that they are unavoidable regardless of implementation
 * design.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
//#include <sys/types.h>


#define boolean char
#define true 1
#define false 0

void sequence_header();

/* code definition */
#define PICTURE_START_CODE			0x100
#define SLICE_START_CODE_MIN		0x101
#define SLICE_START_CODE_MAX		0x1AF
#define USER_DATA_START_CODE		0x1B2
#define SEQUENCE_HEADER_CODE		0x1B3
#define EXTENSION_START_CODE		0x1B5
#define SEQUENCE_END_CODE			0x1B7
#define GROUP_START_CODE			0x1B8

#define SYSTEM_END_CODE				0x1B9
#define PACK_START_CODE				0x1BA
#define SYSTEM_START_CODE			0x1BB

#define IS_NOT_MPEG 0
#define IS_MPEG1 1
#define IS_MPEG2 2

#define ELEMENTARY_STREAM 0
#define MPEG1_PROGRAM_STREAM 1
#define MPEG2_PROGRAM_STREAM 2

/* extension start code IDs */
#define SEQUENCE_EXTENSION_ID					1
#define SEQUENCE_DISPLAY_EXTENSION_ID			2
#define QUANT_MATRIX_EXTENSION_ID				3
#define COPYRIGHT_EXTENSION_ID					4
#define PICTURE_DISPLAY_EXTENSION_ID			7
#define PICTURE_CODING_EXTENSION_ID				8

#define ZIG_ZAG									0
#define MB_WEIGHT								32
#define MB_CLASS4								64

#define I_TYPE			1
#define P_TYPE			2
#define B_TYPE			3
#define D_TYPE			4

#define MACROBLOCK_INTRA				1
#define MACROBLOCK_PATTERN				2
#define MACROBLOCK_MOTION_BACKWARD		4
#define MACROBLOCK_MOTION_FORWARD		8
#define MACROBLOCK_QUANT				16

#define TOP_FIELD		1
#define BOTTOM_FIELD	2
#define FRAME_PICTURE	3

#define MC_FIELD		1
#define MC_FRAME		2
#define MC_16X8			2
#define MC_DMV			3

#define MV_FIELD		0
#define MV_FRAME		1

#define CHROMA420		1
#define CHROMA422		2
#define CHROMA444		3

#define SECTOR_SIZE				2048
#define MAX_FILE_NUMBER			512
#define MAX_PICTURES_PER_GOP	500
#define MAX_GOPS				1000000

#define IDCT_MMX		1
#define IDCT_SSEMMX		2
#define IDCT_SSE2MMX	3
#define	IDCT_FPU		4
#define IDCT_REF		5
#define IDCT_SKAL		6
#define IDCT_SIMPLE		7

#define LOCATE_INIT			0
#define LOCATE_FORWARD		1
#define LOCATE_BACKWARD		-1
#define LOCATE_SCROLL		2
#define LOCATE_RIP			3
#define LOCATE_PLAY			4
#define LOCATE_DEMUX_AUDIO	5

#define CHANNEL				8

#define TRACK_1			0
#define TRACK_2			1
#define TRACK_3			2
#define TRACK_4			3
#define TRACK_5			4
#define TRACK_6			5
#define TRACK_7			6
#define TRACK_8			7

#define FORMAT_AC3			1
#define FORMAT_MPA			2
#define FORMAT_LPCM			3
#define FORMAT_LPCM_M2TS	4
#define FORMAT_DTS			5
#define FORMAT_AAC			6

#define AUDIO_NONE			0
#define AUDIO_DEMUX			1
#define AUDIO_DEMUXALL		2
#define AUDIO_DECODE		3

#define DRC_NONE		0
#define DRC_LIGHT		1
#define DRC_NORMAL		2
#define DRC_HEAVY		3

#define FO_NONE			0
#define FO_FILM			1
#define FO_RAW			2

#define SRC_NONE		0
#define SRC_LOW			1
#define SRC_MID			2
#define SRC_HIGH		3
#define SRC_UHIGH		4

#define TRACK_PITCH		30000

#define DG_MAX_PATH 2048

#define XTN static
#define GXTN static

static int frame_rate_code;

XTN int Second_Field;
XTN int horizontal_size, vertical_size;

XTN double frame_rate;
XTN unsigned int fr_num, fr_den;

XTN int full_pel_forward_vector;
XTN int full_pel_backward_vector;
XTN int forward_f_code;
XTN int backward_f_code;

XTN int q_scale_type;
XTN int alternate_scan;
//XTN int quantizer_scale;

/* ISO/IEC 13818-2 section 6.2.2.1:  sequence_header() */
XTN int aspect_ratio_information;

/* ISO/IEC 13818-2 section 6.2.2.3:  sequence_extension() */
XTN int progressive_sequence;
XTN int chroma_format;

/* sequence_display_extension() */
XTN int display_horizontal_size;
XTN int display_vertical_size;

/* ISO/IEC 13818-2 section 6.2.2.6:  group_of_pictures_header() */
XTN int closed_gop;

/* ISO/IEC 13818-2 section 6.2.3: picture_header() */
XTN int temporal_reference;
XTN int picture_coding_type;
XTN int progressive_frame;
//XTN int PTSAdjustDone;

XTN int matrix_coefficients;
XTN boolean default_matrix_coefficients;

/* ISO/IEC 13818-2 section 6.2.3.1: picture_coding_extension() header */
XTN int f_code[2][2];
XTN int picture_structure;
XTN int frame_pred_frame_dct;
XTN int concealment_motion_vectors;
XTN int intra_dc_precision;
XTN int top_field_first;
XTN int repeat_first_field;
XTN int intra_vlc_format;

XTN boolean Stop_Flag;

int mpeg_type=0;

static int Infile=-1;

GXTN unsigned char *Rdbfr=NULL, *Rdptr, *Rdmax;
GXTN unsigned int BitsLeft, CurrentBfr, NextBfr;
//GXTN off_t CurrentPackHeaderPosition;
//unsigned char *buffer_invalid;

#define BUFFER_SIZE				65536

static unsigned int Get_Byte()
{

	if(Stop_Flag) return 0xff;

	if(Rdptr >= Rdmax){
		int Read = read(Infile, Rdbfr, BUFFER_SIZE);
		Rdptr=Rdbfr;
		Rdmax=Rdbfr+Read;
		if(Read<=0){
		    printf("EOF!!!!!!\n");
		    Stop_Flag=true; // EOF
		    return 0xFF;
		}
	}
	return *Rdptr++;
}

static void Fill_Next(){
	if(Stop_Flag)
	{
		NextBfr = 0xffffffff;
		return;
	}

	//CurrentPackHeaderPosition = PackHeaderPosition;
	if (Rdptr <= Rdmax - 4)
	{
		NextBfr = (*Rdptr << 24) + (*(Rdptr+1) << 16) + (*(Rdptr+2) << 8) + *(Rdptr+3);
		Rdptr += 4;
	}
	else
	{
		NextBfr = Get_Byte() << 24;
		NextBfr += Get_Byte() << 16;
		NextBfr += Get_Byte() << 8;
		NextBfr += Get_Byte();
	}
}

static void Init_Bits(int f){
	Infile=f;
	Stop_Flag=0;
	if(!Rdbfr) Rdbfr=malloc(BUFFER_SIZE);
	Rdmax=Rdptr=Rdbfr;
	Fill_Next();
	CurrentBfr = NextBfr;
	BitsLeft = 32;
	Fill_Next();
}

static inline unsigned int Show_Bits(unsigned int N)
{
	if (N <= BitsLeft)
		return (CurrentBfr << (32 - BitsLeft)) >> (32 - N);
	N -= BitsLeft;
	return (((CurrentBfr << (32 - BitsLeft)) >> (32 - BitsLeft)) << N) + (NextBfr >> (32 - N));
}


static inline unsigned int Get_Bits(unsigned int N)
{
	if (N < BitsLeft) {
		unsigned int Val = (CurrentBfr << (32 - BitsLeft)) >> (32 - N);
		BitsLeft -= N;
		return Val;
	}

	N -= BitsLeft;
	unsigned int Val = (CurrentBfr << (32 - BitsLeft)) >> (32 - BitsLeft);

	if (N)	Val = (Val << N) + (NextBfr >> (32 - N));

	CurrentBfr = NextBfr;
	BitsLeft = 32 - N;
	Fill_Next();
	return Val;
}

static inline void Flush_Buffer(unsigned int N)
{
	if (N < BitsLeft)
		BitsLeft -= N;
	else {
	    CurrentBfr = NextBfr;
	    BitsLeft = BitsLeft + 32 - N;
	    Fill_Next();
	}
}


static void next_start_code()
{
    // This is contrary to the spec but is more resilient to some
    // stream corruption scenarios.
    BitsLeft = ((BitsLeft + 7) / 8) * 8;

    while (1) {
	unsigned int show = Show_Bits(24);
	if (Stop_Flag == true) return;
        if (show == 0x000001) return;
	Flush_Buffer(8);
    }
}



typedef struct {
	int		gop_start;
	int		type;
	int		file;
	off_t		lba;
	off_t		position;
	boolean		pf;
	boolean		trf;
	int     	picture_structure;
}	D2VData;
XTN D2VData d2v_backward, d2v_forward, d2v_current;

XTN boolean GOPSeen;


static double frame_rate_Table[16] = 
{
	0.0,
	((24.0*1000.0)/1001.0),
	24.0,
	25.0,
	((30.0*1000.0)/1001.0),
	30.0,
	50.0,
	((60.0*1000.0)/1001.0),
	60.0,
	-1,		// reserved
	-1,
	-1,
	-1,
	-1,
	-1,
	-1
};

static unsigned int frame_rate_Table_Num[16] = 
{
	0,
	24000,
	24,
	25,
	30000,
	30,
	50,
	60000,
	60,
	0xFFFFFFFF,		// reserved
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF
};

static unsigned int frame_rate_Table_Den[16] = 
{
	0,
	1001,
	1,
	1,
	1001,
	1,
	1,
	1001,
	1,
	0xFFFFFFFF,		// reserved
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF
};

static void group_of_pictures_header(void);
static void picture_header(off_t, boolean, boolean);

static void sequence_extension(void);
static void sequence_display_extension(void);
//static void quant_matrix_extension(void);
static void picture_display_extension(void);
static void picture_coding_extension(void);
static void copyright_extension(void);
static int  extra_bit_information(void);
static void extension_and_user_data(void);
void StartVideoDemux(void);

//////////
// We index the on-disk file position for accessing each I picture. 
// We prefer to index a sequence header immediately accompanying
// the GOP, because it might carry new quant matrices that we would need
// to read if we randomly access the picture. If no sequence header is
// there, we index the I picture header start code. For ES, we index
// directly each indexed location. For PES, we index the position of
// the previous packet start code for each indexed location.
//
// There are two different indexing strategies, one for ES and one for PES.
// The ES scheme directly calculates the required file offset.
// The PES scheme uses CurrentPackHeaderPosition for the on-disk file
// offset of the last pack start code. This variable is irrelevant for ES!
// The pack header position is double-buffered through PackHeaderPosition
// and CurrentPackHeaderPosition to avoid a vulnerability in which
// PackHeaderPosition can become invalid due to a Fill_Next() call
// triggering a Next_Packet() call, which would rewrite PackHeaderPosition.
// So we record PackHeaderPosition to CurrentPackHeaderPosition as the
// first thing we do in Fill_Next().
//////////

// Decode headers up to a picture header and then return.
// There are two modes of operation. Normally, we return only
// when we get a picture header and if we hit EOF file, we
// thread kill ourselves in here. But when checking for leading
// B frames we don't want to kill in order to properly handle
// files with only one I frame. So in the second mode, we detect
// the sequence end code and return with an indication.

int Get_Hdr(int mode)
{
	int code;
	off_t position = 0;
	boolean HadSequenceHeader = false;
	boolean HadGopHeader = false;

	for (;;)
	{
		// Look for next_start_code.
		if (Stop_Flag == true)
			return 1;
		next_start_code();

		code = Show_Bits(32);
		switch (code)
		{
			case SEQUENCE_HEADER_CODE:
				// Index the location of the sequence header for the D2V file.
				// We prefer to index the sequence header corresponding to this
				// GOP, but if one doesn't exist, we index the picture header of the I frame.
//					dprintf("DGIndex: Index sequence header at %d\n", Rdptr - 8 + (32 - BitsLeft)/8);
					d2v_current.position = lseek(Infile,0,SEEK_CUR)
										   - (Rdmax - Rdptr)
										   - 8
										   + (32 - BitsLeft)/8;
				Get_Bits(32);
				sequence_header();
				HadSequenceHeader = true;
				break;

			case SEQUENCE_END_CODE:
				Get_Bits(32);
				if (mode == 1)
					return 1;
				break;

			case GROUP_START_CODE:
				Get_Bits(32);
				group_of_pictures_header();
				HadGopHeader = true;
				GOPSeen = true;
				break;

			case PICTURE_START_CODE:
				position = lseek(Infile,0,SEEK_CUR)
										   - (Rdmax - Rdptr)
										   - 8
										   + (32 - BitsLeft)/8;
//				printf("pos=%lld\n",position);
				Get_Bits(32);
				picture_header(position, HadSequenceHeader, HadGopHeader);
				return 0;
				break;

			default:
				Get_Bits(32);
				break;
		}
	}
}

/* decode sequence header */
void sequence_header()
{
	int constrained_parameters_flag;
	int bit_rate_value;
	int vbv_buffer_size;
	int i;

    // These will become nonzero if we receive a sequence display extension
	display_horizontal_size = 0;
	display_vertical_size = 0;

	horizontal_size             = Get_Bits(12);
	vertical_size               = Get_Bits(12);
	aspect_ratio_information    = Get_Bits(4);
	frame_rate_code             = Get_Bits(4);

	// This is default MPEG1 handling.
	// It may be overridden to MPEG2 if a
	// sequence extension arrives.
	frame_rate = frame_rate_Table[frame_rate_code];
	fr_num = frame_rate_Table_Num[frame_rate_code];
	fr_den = frame_rate_Table_Den[frame_rate_code];

	bit_rate_value              = Get_Bits(18);
	Flush_Buffer(1);	// marker bit
	vbv_buffer_size             = Get_Bits(10);
	constrained_parameters_flag = Get_Bits(1);

//	if (load_intra_quantizer_matrix = Get_Bits(1))
	if (Get_Bits(1))
	{
		for (i=0; i<64; i++)
			//intra_quantizer_matrix[scan[ZIG_ZAG][i]] = 
			Get_Bits(8);
	}
//	else
//	{
//		for (i=0; i<64; i++)
//			intra_quantizer_matrix[i] = default_intra_quantizer_matrix[i];
//	}

//	if (load_non_intra_quantizer_matrix = Get_Bits(1))
	if (Get_Bits(1))
	{
		for (i=0; i<64; i++)
			//non_intra_quantizer_matrix[scan[ZIG_ZAG][i]] = 
			Get_Bits(8);
	}
	else
//	{
//		for (i=0; i<64; i++)
//			non_intra_quantizer_matrix[i] = 16;
//	}

	/* copy luminance to chrominance matrices */
//	for (i=0; i<64; i++)
//	{
//		chroma_intra_quantizer_matrix[i] = intra_quantizer_matrix[i];
//		chroma_non_intra_quantizer_matrix[i] = non_intra_quantizer_matrix[i];
//	}

#if 0
	if (D2V_Flag && LogQuants_Flag)
	{
		// Log the quant matrix changes.
		// Intra luma.
		for (i=0; i<64; i++)
		{
			if (intra_quantizer_matrix[i] != intra_quantizer_matrix_log[i])
				break;
		}
		if (i < 64)
		{
			// The matrix changed, so log it.
			fprintf(Quants, "Intra Luma and Chroma Matrix at encoded frame %d:\n", Frame_Number);
			for (i=0; i<64; i++)
			{
				fprintf(Quants, "%d ", intra_quantizer_matrix[i]);
				if ((i % 8) == 7)
					fprintf(Quants, "\n");
			}
			fprintf(Quants, "\n");
			// Record the new matrix for change detection.
			memcpy(intra_quantizer_matrix_log, intra_quantizer_matrix, sizeof(intra_quantizer_matrix));
			memcpy(chroma_intra_quantizer_matrix_log, chroma_intra_quantizer_matrix, sizeof(chroma_intra_quantizer_matrix));
		}
		// Non intra luma.
		for (i=0; i<64; i++)
		{
			if (non_intra_quantizer_matrix[i] != non_intra_quantizer_matrix_log[i])
				break;
		}
		if (i < 64)
		{
			// The matrix changed, so log it.
			fprintf(Quants, "NonIntra Luma and Chroma Matrix at encoded frame %d:\n", Frame_Number);
			for (i=0; i<64; i++)
			{
				fprintf(Quants, "%d ", non_intra_quantizer_matrix[i]);
				if ((i % 8) == 7)
					fprintf(Quants, "\n");
			}
			fprintf(Quants, "\n");
			// Record the new matrix for change detection.
			memcpy(non_intra_quantizer_matrix_log, non_intra_quantizer_matrix, sizeof(non_intra_quantizer_matrix));
			memcpy(chroma_non_intra_quantizer_matrix_log, chroma_non_intra_quantizer_matrix, sizeof(chroma_non_intra_quantizer_matrix));
		}
	}
#endif

	// This is default MPEG1 handling.
	// It may be overridden to MPEG2 if a
	// sequence extension arrives.
//	matrix_coefficients = 5;
//	default_matrix_coefficients = true;

//	setRGBValues();
	// These are MPEG1 defaults. These will be overridden if we have MPEG2
	// when the sequence header extension is parsed.
	progressive_sequence = 1;
//	chroma_format = CHROMA420;

	extension_and_user_data();

}

/* decode group of pictures header */
/* ISO/IEC 13818-2 section 6.2.2.6 */
static void group_of_pictures_header()
{
	int gop_hour;
	int gop_minute;
	int gop_sec;
	int gop_frame;
	int drop_flag;
	int broken_link;

//	if (LogTimestamps_Flag && D2V_Flag && StartLogging_Flag)
//		fprintf(Timestamps, "GOP start\n");

	drop_flag   = Get_Bits(1);
	gop_hour    = Get_Bits(5);
	gop_minute  = Get_Bits(6);
	Flush_Buffer(1);	// marker bit
	gop_sec     = Get_Bits(6);
	gop_frame	= Get_Bits(6);
	closed_gop  = Get_Bits(1);
	broken_link = Get_Bits(1);

//	extension_and_user_data();
}

/* decode picture header */
/* ISO/IEC 13818-2 section 6.2.3 */
static void picture_header(off_t start, boolean HadSequenceHeader, boolean HadGopHeader)
{
	int vbv_delay;
	int Extra_Information_Byte_Count;
//	int trackpos;
//	double track;

	temporal_reference  = Get_Bits(10);
	picture_coding_type = Get_Bits(3);
#if 0
	if (LogTimestamps_Flag && D2V_Flag && StartLogging_Flag)
	{
		fprintf(Timestamps, "Decode picture: temporal reference %d", temporal_reference);
		switch (picture_coding_type)
		{
		case I_TYPE:
			fprintf(Timestamps, "[I]\n");
			break;
		case P_TYPE:
			fprintf(Timestamps, "[P]\n");
			break;
		case B_TYPE:
			fprintf(Timestamps, "[B]\n");
			break;
		}
	}
#endif

	d2v_current.type = picture_coding_type;

	if (d2v_current.type == I_TYPE)
	{
//		d2v_current.file = process.startfile = CurrentFile;
//		process.startloc = lseek(Infile,0,SEEK_CUR);
//		d2v_current.lba = process.startloc/SECTOR_SIZE - 1;
//		if (d2v_current.lba < 0)
//		{
//			d2v_current.lba = 0;
//		}
//		track = (double) d2v_current.lba;
//		track *= SECTOR_SIZE;
//		track += process.run;
//		track *= TRACK_PITCH;
//		track /= Infiletotal;
//		trackpos = (int) track;
//		SendMessage(hTrack, TBM_SETPOS, (WPARAM)true, trackpos);
#if 0
		{
			char buf[80];
			char total[80], run[80];
			_i64toa(Infiletotal, total, 10);
			_i64toa(process.run, run, 10);
			sprintf(buf, "DGIndex: d2v_current.lba = %x\n", d2v_current.lba);
			OutputDebugString(buf);
			sprintf(buf, "DGIndex: trackpos = %d\n", trackpos);
			OutputDebugString(buf);
			sprintf(buf, "DGIndex: process.run = %s\n", run);
			OutputDebugString(buf);
			sprintf(buf, "DGIndex: Infiletotal = %s\n", total);
			OutputDebugString(buf);
		}
#endif

//		if (process.locate==LOCATE_RIP)
//		{
//			process.file = d2v_current.file;
//			process.lba = d2v_current.lba;
//		}

		// This triggers if we reach the right marker position.
//		if (CurrentFile==process.endfile && process.startloc>=process.endloc)		// D2V END
//		{
//			ThreadKill(END_OF_DATA_KILL);
//		}

//		if (Info_Flag)
//			UpdateInfo();
//		UpdateWindowText();
	}

	vbv_delay = Get_Bits(16);

	if (picture_coding_type == P_TYPE || picture_coding_type == B_TYPE)
	{
		full_pel_forward_vector = Get_Bits(1);
		forward_f_code = Get_Bits(3);
	}

	if (picture_coding_type == B_TYPE)
	{
		full_pel_backward_vector = Get_Bits(1);
		backward_f_code = Get_Bits(3);
	}

	// MPEG1 defaults. May be overriden by picture coding extension.
	intra_dc_precision = 0;
	picture_structure = FRAME_PICTURE;
	top_field_first = 1;
	frame_pred_frame_dct = 1;
	concealment_motion_vectors = 0;
	q_scale_type = 0;
	intra_vlc_format = 0;
	alternate_scan = 0;
	repeat_first_field = 0;
	progressive_frame = 1;

	d2v_current.pf = progressive_frame;
	d2v_current.trf = (top_field_first<<1) + repeat_first_field;

	Extra_Information_Byte_Count = extra_bit_information();
	extension_and_user_data();

    // We prefer to index the sequence header, but if one doesn't exist,
	// we index the picture header of the I frame.
	if (HadSequenceHeader == false)
	{
		// Indexing for the D2V file.
		if (picture_coding_type == I_TYPE && !Second_Field)
		{
//			dprintf("DGIndex: Index picture header at %d\n", Rdptr - Rdbfr);
			d2v_current.position = start;
		}
	}
}

/* decode slice header */
/* ISO/IEC 13818-2 section 6.2.4 */
int slice_header()
{
	int slice_vertical_position_extension;
	int quantizer_scale_code;

	if (mpeg_type == IS_MPEG2)
		slice_vertical_position_extension = vertical_size>2800 ? Get_Bits(3) : 0;
	else
		slice_vertical_position_extension = 0;

	quantizer_scale_code = Get_Bits(5);
//	if (mpeg_type == IS_MPEG2)
//		quantizer_scale = q_scale_type ? Non_Linear_quantizer_scale[quantizer_scale_code] : quantizer_scale_code<<1;
//	else
//		quantizer_scale = quantizer_scale_code;
	while (Get_Bits(1)) Flush_Buffer(8);

	return slice_vertical_position_extension;
}

/* decode extension and user data */
/* ISO/IEC 13818-2 section 6.2.2.2 */
static void extension_and_user_data()
{
	int code, ext_ID;

	if (Stop_Flag == true)
		return;
	next_start_code();

	while ((code = Show_Bits(32))==EXTENSION_START_CODE || code==USER_DATA_START_CODE)
	{
		if (code==EXTENSION_START_CODE)
		{
			Flush_Buffer(32);
			ext_ID = Get_Bits(4);

			switch (ext_ID)
			{
				case SEQUENCE_EXTENSION_ID:
					sequence_extension();
					break;
				case SEQUENCE_DISPLAY_EXTENSION_ID:
					sequence_display_extension();
					break;
				case QUANT_MATRIX_EXTENSION_ID:
//					quant_matrix_extension();
					break;
				case PICTURE_DISPLAY_EXTENSION_ID:
					picture_display_extension();
					break;
				case PICTURE_CODING_EXTENSION_ID:
					picture_coding_extension();
					break;
				case COPYRIGHT_EXTENSION_ID:
					copyright_extension();
					break;
			}
			if (Stop_Flag == true)
				return;
			next_start_code();
		}
		else
		{
			Flush_Buffer(32);	// ISO/IEC 13818-2  sections 6.3.4.1 and 6.2.2.2.2
			if (Stop_Flag == true)
				return;
			next_start_code();	// skip user data
		}
	}
}

/* decode sequence extension */
/* ISO/IEC 13818-2 section 6.2.2.3 */
int profile_and_level_indication;
static void sequence_extension()
{
	int low_delay;
	int frame_rate_extension_n;
	int frame_rate_extension_d;
	int horizontal_size_extension;
	int vertical_size_extension;
	int bit_rate_extension;
	int vbv_buffer_size_extension;

	// This extension means we must have MPEG2, so
	// override the earlier assumption of MPEG1 for
	// transport streams.
	mpeg_type = IS_MPEG2;
//   	setRGBValues();

	profile_and_level_indication = Get_Bits(8);
	progressive_sequence         = Get_Bits(1);
	chroma_format                = Get_Bits(2);
	horizontal_size_extension    = Get_Bits(2);
	vertical_size_extension      = Get_Bits(2);
	bit_rate_extension           = Get_Bits(12);
	Flush_Buffer(1);	// marker bit
	vbv_buffer_size_extension    = Get_Bits(8);
	low_delay                    = Get_Bits(1);
 
	frame_rate_extension_n       = Get_Bits(2);
	frame_rate_extension_d       = Get_Bits(5);
	frame_rate = frame_rate_Table[frame_rate_code] * (frame_rate_extension_n+1)/(frame_rate_extension_d+1);
	fr_num = frame_rate_Table_Num[frame_rate_code] * (frame_rate_extension_n+1);
	fr_den = frame_rate_Table_Den[frame_rate_code] * (frame_rate_extension_d+1);

	horizontal_size = (horizontal_size_extension<<12) | (horizontal_size&0x0fff);
	vertical_size = (vertical_size_extension<<12) | (vertical_size&0x0fff);
    // This is the default case and may be overridden by a sequence display extension.
    if (horizontal_size > 720 || vertical_size > 576)
        // HD
	    matrix_coefficients = 1;
    else
        // SD
       	matrix_coefficients = 5;
}

/* decode sequence display extension */
static void sequence_display_extension()
{
	int video_format;  
	int color_description;
	int color_primaries;
	int transfer_characteristics;
    int matrix;

	video_format      = Get_Bits(3);
	color_description = Get_Bits(1);

	if (color_description)
	{
		color_primaries          = Get_Bits(8);
		transfer_characteristics = Get_Bits(8);
		matrix = Get_Bits(8);
        // If the stream specifies "reserved" or "unspecified" then leave things set to our default
        // based on HD versus SD.
        if (matrix == 1 || (matrix >= 4 && matrix <= 7))
        {
		    matrix_coefficients  = matrix;
            default_matrix_coefficients = false;
        }
//		setRGBValues();
	}

	display_horizontal_size = Get_Bits(14);
	Flush_Buffer(1);	// marker bit
	display_vertical_size   = Get_Bits(14);
}

#if 0
/* decode quant matrix entension */
/* ISO/IEC 13818-2 section 6.2.3.2 */
static void quant_matrix_extension()
{
	int i;

	if (load_intra_quantizer_matrix = Get_Bits(1))
		for (i=0; i<64; i++)
//			chroma_intra_quantizer_matrix[scan[ZIG_ZAG][i]]
//				= intra_quantizer_matrix[scan[ZIG_ZAG][i]] = 
			Get_Bits(8);

	if (load_non_intra_quantizer_matrix = Get_Bits(1))
		for (i=0; i<64; i++)
//			chroma_non_intra_quantizer_matrix[scan[ZIG_ZAG][i]]
//				= non_intra_quantizer_matrix[scan[ZIG_ZAG][i]] = 
			Get_Bits(8);

	if (load_chroma_intra_quantizer_matrix = Get_Bits(1))
		for (i=0; i<64; i++)
			chroma_intra_quantizer_matrix[scan[ZIG_ZAG][i]] = Get_Bits(8);

	if (load_chroma_non_intra_quantizer_matrix = Get_Bits(1))
		for (i=0; i<64; i++)
			chroma_non_intra_quantizer_matrix[scan[ZIG_ZAG][i]] = Get_Bits(8);

	if (D2V_Flag && LogQuants_Flag)
	{
		// Log the quant matrix changes.
		// Intra luma.
		for (i=0; i<64; i++)
		{
			if (intra_quantizer_matrix[i] != intra_quantizer_matrix_log[i])
				break;
		}
		if (i < 64)
		{
			// The matrix changed, so log it.
			fprintf(Quants, "Intra Luma Matrix at encoded frame %d:\n", Frame_Number);
			for (i=0; i<64; i++)
			{
				fprintf(Quants, "%d ", intra_quantizer_matrix[i]);
				if ((i % 8) == 7)
					fprintf(Quants, "\n");
			}
			fprintf(Quants, "\n");
			// Record the new matrix for change detection.
			memcpy(intra_quantizer_matrix_log, intra_quantizer_matrix, sizeof(intra_quantizer_matrix));
		}
		// Non intra luma.
		for (i=0; i<64; i++)
		{
			if (non_intra_quantizer_matrix[i] != non_intra_quantizer_matrix_log[i])
				break;
		}
		if (i < 64)
		{
			// The matrix changed, so log it.
			fprintf(Quants, "NonIntra Luma Matrix at encoded frame %d:\n", Frame_Number);
			for (i=0; i<64; i++)
			{
				fprintf(Quants, "%d ", non_intra_quantizer_matrix[i]);
				if ((i % 8) == 7)
					fprintf(Quants, "\n");
			}
			fprintf(Quants, "\n");
			// Record the new matrix for change detection.
			memcpy(non_intra_quantizer_matrix_log, non_intra_quantizer_matrix, sizeof(non_intra_quantizer_matrix));
		}
		// Intra chroma.
		for (i=0; i<64; i++)
		{
			if (chroma_intra_quantizer_matrix[i] != chroma_intra_quantizer_matrix_log[i])
				break;
		}
		if (i < 64)
		{
			// The matrix changed, so log it.
			fprintf(Quants, "Intra Chroma Matrix at encoded frame %d:\n", Frame_Number);
			for (i=0; i<64; i++)
			{
				fprintf(Quants, "%d ", chroma_intra_quantizer_matrix[i]);
				if ((i % 8) == 7)
					fprintf(Quants, "\n");
			}
			fprintf(Quants, "\n");
			// Record the new matrix for change detection.
			memcpy(chroma_intra_quantizer_matrix_log, chroma_intra_quantizer_matrix, sizeof(chroma_intra_quantizer_matrix));
		}
		// Non intra chroma.
		for (i=0; i<64; i++)
		{
			if (chroma_non_intra_quantizer_matrix[i] != chroma_non_intra_quantizer_matrix_log[i])
				break;
		}
		if (i < 64)
		{
			// The matrix changed, so log it.
			fprintf(Quants, "NonIntra Chroma Matrix at encoded frame %d:\n", Frame_Number);
			for (i=0; i<64; i++)
			{
				fprintf(Quants, "%d ", chroma_non_intra_quantizer_matrix[i]);
				if ((i % 8) == 7)
					fprintf(Quants, "\n");
			}
			fprintf(Quants, "\n");
			// Record the new matrix for change detection.
			memcpy(chroma_non_intra_quantizer_matrix_log, chroma_non_intra_quantizer_matrix, sizeof(chroma_non_intra_quantizer_matrix));
		}
	}
}
#endif

/* decode picture display extension */
/* ISO/IEC 13818-2 section 6.2.3.3. */
static void picture_display_extension()
{
	int frame_center_horizontal_offset[3];
	int frame_center_vertical_offset[3];

	int i;
	int number_of_frame_center_offsets;

	/* based on ISO/IEC 13818-2 section 6.3.12 
	   (November 1994) Picture display extensions */

	/* derive number_of_frame_center_offsets */
	if (progressive_sequence)
	{
		if (repeat_first_field)
		{
			if (top_field_first)
				number_of_frame_center_offsets = 3;
			else
				number_of_frame_center_offsets = 2;
		}
		else
			number_of_frame_center_offsets = 1;
	}
	else
	{
		if (picture_structure!=FRAME_PICTURE)
			number_of_frame_center_offsets = 1;
		else
		{
			if (repeat_first_field)
				number_of_frame_center_offsets = 3;
			else
				number_of_frame_center_offsets = 2;
		}
	}

	/* now parse */
	for (i=0; i<number_of_frame_center_offsets; i++)
	{
		frame_center_horizontal_offset[i] = Get_Bits(16);
		Flush_Buffer(1);	// marker bit

		frame_center_vertical_offset[i] = Get_Bits(16);
		Flush_Buffer(1);	// marker bit
	}
}

/* decode picture coding extension */
static void picture_coding_extension()
{
	int chroma_420_type;
	int composite_display_flag;
	int v_axis;
	int field_sequence;
	int sub_carrier;
	int burst_amplitude;
	int sub_carrier_phase;

	f_code[0][0] = Get_Bits(4);
	f_code[0][1] = Get_Bits(4);
	f_code[1][0] = Get_Bits(4);
	f_code[1][1] = Get_Bits(4);

	intra_dc_precision			= Get_Bits(2);
	picture_structure			= Get_Bits(2);
	top_field_first				= Get_Bits(1);
    frame_pred_frame_dct		= Get_Bits(1);
	concealment_motion_vectors	= Get_Bits(1);
	q_scale_type				= Get_Bits(1);
	intra_vlc_format			= Get_Bits(1);
	alternate_scan				= Get_Bits(1);
	repeat_first_field			= Get_Bits(1);
#if 0
    // Not working right
	if (D2V_Flag && progressive_sequence && repeat_first_field)
	{
		if (FO_Flag == FO_FILM)
		{
			MessageBox(hWnd, "Frame repeats were detected in the source stream.\n"
							 "Proper handling of them requires that you turn off Force Film mode.\n"
							 "After turning it off, restart this operation.", NULL, MB_OK | MB_ICONERROR);
			ThreadKill(MISC_KILL);
		}
	}
#endif
	chroma_420_type				= Get_Bits(1);
	progressive_frame			= Get_Bits(1);
	composite_display_flag		= Get_Bits(1);

	d2v_current.pf = progressive_frame;
	d2v_current.trf = (top_field_first<<1) + repeat_first_field;
    // Store just the first picture structure encountered. We'll
    // use it to report the field order for field structure clips.
    if (d2v_current.picture_structure == -1)
	    d2v_current.picture_structure = picture_structure;

	if (composite_display_flag)
	{
		v_axis            = Get_Bits(1);
		field_sequence    = Get_Bits(3);
		sub_carrier       = Get_Bits(1);
		burst_amplitude   = Get_Bits(7);
		sub_carrier_phase = Get_Bits(8);
	}
}

/* decode extra bit information */
/* ISO/IEC 13818-2 section 6.2.3.4. */
static int extra_bit_information()
{
	int Byte_Count = 0;

	while (Get_Bits(1))
	{
        if (Stop_Flag)
            break;
		Flush_Buffer(8);
		Byte_Count++;
	}

	return Byte_Count;
}

/* Copyright extension */
/* ISO/IEC 13818-2 section 6.2.3.6. */
/* (header added in November, 1994 to the IS document) */
static void copyright_extension()
{
	int copyright_flag;
	int copyright_identifier;
	int original_or_copy;
	int copyright_number_1;
	int copyright_number_2;
	int copyright_number_3;

	int reserved_data;

	copyright_flag =       Get_Bits(1); 
	copyright_identifier = Get_Bits(8);
	original_or_copy =     Get_Bits(1);
  
	/* reserved */
	reserved_data = Get_Bits(7);

	Flush_Buffer(1); // marker bit
	copyright_number_1 =   Get_Bits(20);
	Flush_Buffer(1); // marker bit
	copyright_number_2 =   Get_Bits(22);
	Flush_Buffer(1); // marker bit
	copyright_number_3 =   Get_Bits(22);
}


/* decode one frame */
struct ENTRY
{
	boolean gop_start;
	int lba;
	off_t position;
	int pct;
	int trf;
	int pf;
	int closed;
	int pseq;
	int matrix;
	int vob_id;
	int cell_id;
} gop_entries[MAX_PICTURES_PER_GOP];
int gop_entries_ndx = 0;

off_t gop_positions[MAX_GOPS];
int gop_positions_ndx = 0;

char D2VLine[2048];
FILE* D2VFile=NULL;

void WriteD2VLine(int finish)
{
	char temp[8];
	int m, r;
	int had_P = 0;
	int mark = 0x80;
	struct ENTRY ref = gop_entries [0]; // To avoid compiler warning.
	struct ENTRY entries[MAX_PICTURES_PER_GOP];
	unsigned int gop_marker = 0x800;
	int num_to_skip, ndx;

    // We need to keep a record of the history of the I frame positions
	// to enable the following trick.
	gop_positions[gop_positions_ndx] = gop_entries[0].position;
	// An indexed unit (especially a pack) can contain multiple I frames.
	// If we did nothing we would have multiple D2V lines with the same position
	// and the navigation code in DGDecode would be confused. We detect this
	// by seeing how many previous lines we have written with the same position.
	// We store that number in the D2V file and DGDecode uses that as the
	// number of I frames to skip before settling on the required one.
	// So, in fact, we are indexing position plus number of I frames to skip.
	num_to_skip = 0;
	ndx = gop_positions_ndx;
	while (ndx && gop_positions[ndx] == gop_positions[ndx-1])
	{
		num_to_skip++;
		ndx--;
	}
	gop_positions_ndx++;

	// Write the first part of the data line to the D2V file.
	if (gop_entries[0].gop_start == true) gop_marker |= 0x100;
	if (gop_entries[0].closed == 1) gop_marker |= 0x400;
	if (gop_entries[0].pseq == 1) gop_marker |= 0x200;
	sprintf(D2VLine,"%03x %d %d %lld %d %d %d",
		    gop_marker, gop_entries[0].matrix, d2v_forward.file, gop_entries[0].position, num_to_skip,
			gop_entries[0].vob_id, gop_entries[0].cell_id);

	// Reorder frame bytes for display order.
	for (m = 0, r = 0; m < gop_entries_ndx; m++)
	{
		// To speed up random access navigation, we mark frames that can
		// be accessed by decoding just the GOP that they
		// belong to. The mark is done by setting bit 0x10
		// on the trf value. This is read by MPEG2DEC3dg
		// and used to avoid having to decode the previous
		// GOP when doing random access. B frames before or
		// P frame in a non-closed GOP cannot be marked because
		// they reference a frame in the previous GOP.
		if (gop_entries[m].pct == I_TYPE)
		{
			gop_entries[m].trf |= mark;
			ref = gop_entries[m];
		}
		else if (gop_entries[m].pct == P_TYPE)
		{
			entries[r++] = ref;
			gop_entries[m].trf |= mark;
			ref = gop_entries[m];
			had_P = 1;
		}
		else if (gop_entries[m].pct == B_TYPE)
		{
			if (had_P || gop_entries[m].closed)
				gop_entries[m].trf |= mark;
			entries[r++] = gop_entries[m];
		}
	}
	entries[r++] = ref;

	for (m = 0; m < gop_entries_ndx; m++)
	{
		sprintf(temp," %02x", entries[m].trf | (entries[m].pct << 4) | (entries[m].pf << 6));
		strcat(D2VLine, temp);
	}
	if (finish) strcat(D2VLine, " ff\n"); else strcat(D2VLine, "\n");
	fprintf(D2VFile, "%s", D2VLine);
	fflush(D2VFile);

	gop_entries_ndx = 0;

	printf("%s", D2VLine);
}







int main(int argc,char* argv[]){

int f=open(argv[1],O_RDONLY);
Init_Bits(f);

D2VFile=fopen(argv[2],"wt");
fprintf(D2VFile,"DGIndexProjectFile16\n1\nfilename\n\nStream_Type=0\nMPEG_Type=2\nField_Operation=0\nFrame_Rate=29970 (30000/1001)\nLocation=0,0,0,6316f8\n\n");

while(1){
	Get_Hdr(0);
	if(Stop_Flag) break;
//    printf("pos=%lld type=%d progr=%d closed=%d rff=%d\n",d2v_current.position,picture_coding_type,progressive_sequence,closed_gop,repeat_first_field);

	// D2V file generation rewritten by Donald Graft to support IBBPBBP...
        if (picture_coding_type!=B_TYPE) {
	    d2v_forward = d2v_backward;
	    d2v_backward = d2v_current;
	}
	if ((picture_structure==FRAME_PICTURE  || !Second_Field))
	{	
		if (picture_coding_type == I_TYPE && gop_entries_ndx > 0)
		{
			WriteD2VLine(0);
		}
		if (GOPSeen == true && picture_coding_type == I_TYPE)
			gop_entries[gop_entries_ndx].gop_start = true;
		else
			gop_entries[gop_entries_ndx].gop_start = false;
		GOPSeen = false;
		gop_entries[gop_entries_ndx].lba = (int) d2v_current.lba;
		gop_entries[gop_entries_ndx].position = d2v_current.position;
		gop_entries[gop_entries_ndx].pf = progressive_frame;
		gop_entries[gop_entries_ndx].pct = picture_coding_type;
		gop_entries[gop_entries_ndx].trf = d2v_current.trf;
		gop_entries[gop_entries_ndx].closed = closed_gop;
		gop_entries[gop_entries_ndx].pseq = progressive_sequence;
		gop_entries[gop_entries_ndx].matrix = matrix_coefficients;
		gop_entries[gop_entries_ndx].vob_id = 0;//VOB_ID;
		gop_entries[gop_entries_ndx].cell_id = 0;//CELL_ID;
		gop_entries_ndx++;
		if(gop_entries_ndx>=MAX_PICTURES_PER_GOP){
			printf("Too many pictures per GOP (>= 500).\nDGIndex will terminate.\n");
			exit(1);
		}
	}

}
WriteD2VLine(1);
fprintf(D2VFile,"\nFINISHED  100%% FILM\n");
fclose(D2VFile);

return 0;
}

