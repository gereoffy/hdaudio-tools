/****************************************************************************
*     optimizations and FFTW support by A'rpi (C)2008
*****************************************************************************
*
* NAME: smbPitchShift.cpp
* VERSION: 1.2
* HOME URL: http://www.dspdimension.com
* KNOWN BUGS: none
*
* SYNOPSIS: Routine for doing pitch shifting while maintaining
* duration using the Short Time Fourier Transform.
*
* DESCRIPTION: The routine takes a pitchShift factor value which is between 0.5
* (one octave down) and 2. (one octave up). A value of exactly 1 does not change
* the pitch. numSampsToProcess tells the routine how many samples in indata[0...
* numSampsToProcess-1] should be pitch shifted and moved to outdata[0 ...
* numSampsToProcess-1]. The two buffers can be identical (ie. it can process the
* data in-place). fftFrameSize defines the FFT frame size used for the
* processing. Typical values are 1024, 2048 and 4096. It may be any value <=
* MAX_FRAME_LENGTH but it MUST be a power of 2. osamp is the STFT
* oversampling factor which also determines the overlap between adjacent STFT
* frames. It should at least be 4 for moderate scaling ratios. A value of 32 is
* recommended for best quality. sampleRate takes the sample rate for the signal 
* in unit Hz, ie. 44100 for 44.1 kHz audio. The data passed to the routine in 
* indata[] should be in the range [-1.0, 1.0), which is also the output range 
* for the data, make sure you scale the data accordingly (for 16bit signed integers
* you would have to divide (and multiply) by 32768). 
*
* COPYRIGHT 1999-2006 Stephan M. Bernsee <smb [AT] dspdimension [DOT] com>
*
* 						The Wide Open License (WOL)
*
* Permission to use, copy, modify, distribute and sell this software and its
* documentation for any purpose is hereby granted without fee, provided that
* the above copyright notice and this license appear in all source copies. 
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF
* ANY KIND. See http://www.dspguru.com/wol.htm for more information.
*
*****************************************************************************/ 

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
static float gSynFreq[MAX_FRAME_LENGTH];
static float gSynMagn[MAX_FRAME_LENGTH];
static float gWindow[MAX_FRAME_LENGTH];
static long gRover = 0, gInit = 0;

// -----------------------------------------------------------------------------------------------------------------


void smbPitchShift(float pitchShift, long numSampsToProcess, long fftFrameSize, long osamp, float *indata, float *outdata, int min, int max)
/*
	Routine smbPitchShift(). See top of file for explanation
	Purpose: doing pitch shifting while maintaining duration using the Short
	Time Fourier Transform.
	Author: (c)1999-2006 Stephan M. Bernsee <smb [AT] dspdimension [DOT] com>
*/
{

	double magn, phase, tmp, real, imag;
	long i,k, qpd, index;

	/* set up some handy variables */
	long fftFrameSize2 = fftFrameSize/2;
	long stepSize = fftFrameSize/osamp;
//	double freqPerBin = sampleRate/(double)fftFrameSize;
//	double expct = 2.*M_PI*(double)stepSize/(double)fftFrameSize;
	double expct = 2.*M_PI/(double)osamp;
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

			memset(gSynMagn, 0, fftFrameSize*sizeof(float));
			memset(gSynFreq, 0, fftFrameSize*sizeof(float));

			/* this is the analysis step */
//			for (k = 0; k <= fftFrameSize2; k++) {
			for (k = min; k <= max; k++) {

				/* de-interlace FFT buffer */
				real = fft_f[k][0];
				imag = fft_f[k][1];

				/* compute magnitude and phase */
				magn = 2.*sqrt(real*real + imag*imag);
				phase = atan2(imag,real);

				/* compute phase difference */
				tmp = phase - gLastPhase[k];
				gLastPhase[k] = phase;

#if 1
				/* subtract expected phase difference */
				tmp -= (double)k*expct;

				/* map delta phase into +/- Pi interval */
				qpd = tmp/M_PI;
				if (qpd >= 0) qpd += qpd&1;
				else qpd -= qpd&1;
				tmp -= M_PI*(double)qpd;

				/* compute the k-th partials' true frequency */
				tmp += (double)k*expct;
#endif

				/* get deviation from bin frequency from the +/- Pi interval */
//				tmp /= expct;
//				tmp *= freqPerBin;

				/* store magnitude and true frequency in analysis arrays */
//				gAnaMagn[k] = magn;
//				gAnaFreq[k] = tmp;

				/* this does the actual pitch shifting */
				index = k*pitchShift;
				if (index <= fftFrameSize2) { 
					gSynMagn[index] += magn;
					gSynFreq[index] = tmp * pitchShift;
				}

			}

			/* ***************** SYNTHESIS ******************* */
			memset(fft_f,0,sizeof(fftwf_complex)*fftFrameSize);

			/* this is the synthesis step */
			for (k = 0; k <= fftFrameSize2; k++) {

				/* get magnitude and true frequency from synthesis arrays */
				magn = gSynMagn[k];
				tmp = gSynFreq[k];

				/* get bin deviation from freq deviation */
//				tmp /= freqPerBin;

				/* take osamp into account */
//				tmp *= expct;

				/* subtract bin mid frequency */
//				tmp -= (double)k*expct;

				/* add the overlap phase advance back in */
//				tmp += (double)k*expct;

				/* accumulate delta phase to get bin phase */
				tmp += gSumPhase[k];
				gSumPhase[k] = tmp;
				
				if(magn==0.0) continue;

				/* get real and imag part and re-interleave */
				fft_f[k][0] = magn*cos(tmp);
				fft_f[k][1] = magn*sin(tmp);
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

