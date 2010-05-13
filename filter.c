#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include <fftw3.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_FRAME_LENGTH 8192

//static void smbFft(float *fftBuffer, long fftFrameSize, long sign);
//static double smbAtan2(double x, double y);

static float* fft_t=NULL;
static fftwf_complex* fft_f=NULL;
static fftwf_plan fft_plan1;
static fftwf_plan fft_plan2;

static float gInFIFO[MAX_FRAME_LENGTH];
static float gOutFIFO[MAX_FRAME_LENGTH];
static float gLastPhase[MAX_FRAME_LENGTH/2+1];
static float gSumPhase[MAX_FRAME_LENGTH/2+1];
static float gOutputAccum[2*MAX_FRAME_LENGTH];
static float gAnaFreq[MAX_FRAME_LENGTH];
static float gAnaMagn[MAX_FRAME_LENGTH];
static float gWindow[MAX_FRAME_LENGTH];
static long gRover = 0, gInit = 0;

// -----------------------------------------------------------------------------------------------------------------


void FFTfilter(long numSampsToProcess, long fftFrameSize, long osamp, float *indata, float *outdata, unsigned char* filter,int step,int invert)
{
	long i,k;

	/* set up some handy variables */
	long fftFrameSize2 = fftFrameSize/2;
	long stepSize = fftFrameSize/osamp;
//	double expct = 2.*M_PI/(double)osamp;
	long inFifoLatency = fftFrameSize-stepSize;
	if (!gRover) gRover = inFifoLatency;

	/* initialize our static arrays */
        if(!fft_t){   // init!
	    fft_t     = fftwf_malloc(sizeof(float) * MAX_FRAME_LENGTH);
	    fft_f     = fftwf_malloc(sizeof(fftwf_complex) * MAX_FRAME_LENGTH);
    	}
    	
	fft_plan1 = fftwf_plan_dft_r2c_1d(fftFrameSize,fft_t,fft_f, FFTW_ESTIMATE|FFTW_DESTROY_INPUT);
	fft_plan2 = fftwf_plan_dft_c2r_1d(fftFrameSize,fft_f,fft_t, FFTW_ESTIMATE|FFTW_DESTROY_INPUT);
    	
	if(!gInit){
//		gInit=1;
		gRover = 0;
		memset(gInFIFO, 0, MAX_FRAME_LENGTH*sizeof(float));
		memset(gOutFIFO, 0, MAX_FRAME_LENGTH*sizeof(float));
		memset(gLastPhase, 0, (MAX_FRAME_LENGTH/2+1)*sizeof(float));
		memset(gSumPhase, 0, (MAX_FRAME_LENGTH/2+1)*sizeof(float));
		memset(gOutputAccum, 0, 2*MAX_FRAME_LENGTH*sizeof(float));
		memset(gAnaFreq, 0, MAX_FRAME_LENGTH*sizeof(float));
		memset(gAnaMagn, 0, MAX_FRAME_LENGTH*sizeof(float));
		for (k = 0; k < fftFrameSize;k++)
			gWindow[k] = -.5*cos(2.*M_PI*(double)k/(double)fftFrameSize)+.5;
	}
	
	/* main processing loop */
	for (i = 0; i < numSampsToProcess+inFifoLatency; i++){

		/* As long as we have not yet collected enough data just read in */
		gInFIFO[gRover] = (i<numSampsToProcess) ? indata[i] : 0;
		if(i>=inFifoLatency){
		    if(indata==outdata) outdata[i-inFifoLatency]=0;
		    outdata[i-inFifoLatency] += gOutFIFO[gRover-inFifoLatency];
		}
		gRover++;

		/* now we have enough data for processing */
		if (gRover >= fftFrameSize) {
			gRover = inFifoLatency;

			/* do windowing and re,im interleave */
			for (k = 0; k < fftFrameSize;k++)
				fft_t[k]=gInFIFO[k] * gWindow[k];

			/* ***************** ANALYSIS ******************* */
			/* do transform */
			fftwf_execute(fft_plan1);

			// do filtering!!!!!
			int fpos=i/step;
//			float tscale=0;
//			if(fpos<512) for (k = 0; k < 512;k++) tscale+=(filter[fpos*512 + k]>128) ? 1.0/512.0 : 0;
//			tscale*=(1.0/255.0)*(1.0/512.0);
			//tscale*=10;
//			tscale=(tscale<0.002)?0:1;
			
			for (k = 0; k < fftFrameSize2;k++){
				float scale=0;
				if(fpos<511 && k<512){
				    scale=filter[fpos*512 + k] + filter[fpos*512 + k-1] + filter[fpos*512 + k+1] + filter[fpos*512 + k-2] + filter[fpos*512 + k+2] + filter[fpos*512 + k-3] + filter[fpos*512 + k+3] + filter[fpos*512 + k-4] + filter[fpos*512 + k+4];
				    scale*=1.0/510.0;
				    if(scale>1.0) scale=1.0;
				    if(invert<0) scale=1.0-scale;
				}
				fft_f[k][0] *= scale;
				fft_f[k][1] *= scale;
				fft_f[fftFrameSize-k-1][0] *= scale;
				fft_f[fftFrameSize-k-1][1] *= scale;
			}

			/* do inverse transform */
			fftwf_execute(fft_plan2);

			/* do windowing and add to output accumulator */ 
			register double scale=1.0/(fftFrameSize2*osamp);
			for(k=0; k < fftFrameSize; k++)
				gOutputAccum[k] += gWindow[k]*fft_t[k]*scale;

			for (k = 0; k < stepSize; k++) gOutFIFO[k] = gOutputAccum[k];

			/* shift accumulator */
			memmove(gOutputAccum, gOutputAccum+stepSize, fftFrameSize*sizeof(float));

			/* move input FIFO */
			for (k = 0; k < inFifoLatency; k++) gInFIFO[k] = gInFIFO[k+stepSize];
		}
	}

    	fftwf_destroy_plan(fft_plan2);
    	fftwf_destroy_plan(fft_plan1);

}

