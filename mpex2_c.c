#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#ifdef SMP
#if !defined(__CYGWIN__) && !defined(__MINGW32__)
#include <pthread.h>
#else
#include <windows.h>
#endif
#endif

#ifdef FFTW
#include <fftw3.h>

#ifdef SMP

static int fftw_init[8]={0,};

static fftwf_complex* fft_in[8]={NULL,};
static fftwf_complex* fft_out[8]={NULL,};

static fftwf_plan fft_plan[8];

static void FFT_init(long fftFrameSize,int channel){
    if(fftw_init[channel]!=fftFrameSize){
	// init!
	fftw_init[channel]=fftFrameSize;
	fprintf(stderr,"**** FFTWf INIT! ****\n");
	fft_in[channel]=fftwf_malloc(sizeof(fftwf_complex) * fftFrameSize);
	fft_out[channel]=fftwf_malloc(sizeof(fftwf_complex) * fftFrameSize);
//	fft_plan[channel] = fftwf_plan_dft_1d(fftFrameSize,fft_in[channel],fft_out[channel],+1, FFTW_MEASURE|FFTW_DESTROY_INPUT);
	fft_plan[channel] = fftwf_plan_dft_1d(fftFrameSize,fft_in[channel],fft_out[channel],+1, FFTW_ESTIMATE|FFTW_DESTROY_INPUT);
    }
}

static void FFT_p1(float *fftBuffer, long fftFrameSize, long channel){
    memcpy(fft_in[channel],fftBuffer+1,fftFrameSize*sizeof(fftwf_complex));
    fftwf_execute(fft_plan[channel]);
    memcpy(fftBuffer+1,fft_out[channel],fftFrameSize*sizeof(fftwf_complex));
}

#else

static int fftw_init=0;

static fftwf_complex* fft_in=NULL;
static fftwf_complex* fft_out=NULL;

static fftwf_plan fft_plan;

static void FFT_p1(float *fftBuffer, long fftFrameSize, long channel){
    if(fftw_init!=fftFrameSize){
	// init!
	fftw_init=fftFrameSize;
	fprintf(stderr,"**** FFTWf INIT! ****\n");
	fft_in=fftwf_malloc(sizeof(fftwf_complex) * fftFrameSize);
	fft_out=fftwf_malloc(sizeof(fftwf_complex) * fftFrameSize);
	fft_plan = fftwf_plan_dft_1d(fftFrameSize,fft_in,fft_out,+1, FFTW_MEASURE|FFTW_DESTROY_INPUT);
    }
    memcpy(fft_in,fftBuffer+1,fftFrameSize*sizeof(fftwf_complex));
    fftwf_execute(fft_plan);
    memcpy(fftBuffer+1,fft_out,fftFrameSize*sizeof(fftwf_complex));
}


#endif

#else

static void FFT_p1(float *p, int n, long channel){
    
    int ecx=1;
    int edi;
    for(edi=1;edi<2*n;edi+=2){
	if(edi<ecx){
	    // cserebere:
	    float t1,t2;
	    t1=p[ecx];t2=p[edi];	  p[ecx]=t2;p[edi]=t1;
	    t1=p[ecx+1];t2=p[edi+1];  p[ecx+1]=t2;p[edi+1]=t1;
	}
	int eax=n;
	while(eax>=2){
    	    if(ecx<=eax) break;
	    ecx-=eax;
	    eax=eax/2;
	}
	ecx+=eax;     //    esi+=2;
    }


    // int ebx=2*n
    // edx=p
    int eax=2;	//    esp10=eax;
    while(eax<2*n){ // bigloop
	double esp14=2.0*M_PI/eax;
//	double esp14=sign*2.0*M_PI/eax;
//	double t1=sin( 0.5*esp14 ); t1=-2*t1*t1; // derivalt fv?
	double t1=cos(esp14);
	double t2=sin(esp14);
	double t3=1.0;
	double t4=0.0;
	
	int j;
	for(j=1;j<eax;j+=2){
	    int i;
	    for(i=j;i<=2*n;i+=2*eax){
		double a=p[eax+i] * t3 - p[eax+i+1] * t4;
		double b=p[eax+i] * t4 + p[eax+i+1] * t3;
		p[eax+i+0]=p[i+0]-a;
		p[eax+i+1]=p[i+1]-b;
		p[i+0]+=a;
		p[i+1]+=b;
	    }
	    
	    // t1 t2 t3 t4
	    double st=t3;
//	    t3+= t3*t1 - t4*t2;
//	    t4+= st*t2 + t4*t1;
	    t3= t3*t1 - t4*t2;
	    t4= st*t2 + t4*t1;

	} //if(ebp<esp10) goto loc_47a890_loop2;    loc_47a92c_skiploop2:

	eax*=2;
    }

}

#endif

//                           +28h   +2ch   +30
static void sub_47a7b0_miez8_hivja(float* p,int n,long channel){
    //if(MPEX2_debug) 
//    printf("sub_47a7b0_miez8_hivja: p=%p n=%d sign=%d\n",p,n,(int)sign);
#if 0
    printf("FFT2: ");
    int i;
    for(i=1;i<n;i++) printf(" %+6.5f",p[i]*2048);
    printf("\n");
#endif

    FFT_p1(p,n,channel);
}

static void sub_47a950_miez8_hivja_ezt_is(float* p,int n,long channel){
    //if(MPEX2_debug) 
//    printf("sub_47a7b0_miez8_hivja_ezt_is: p=%p n=%d x=%d\n",p,n,(int)sign);

#if 0
    printf("FFT1: ");
    int i;
    for(i=1;i<40;i++) printf(" %+6.5f",p[i]*2048);
    printf("\n");
#endif

    // half FFT:
    FFT_p1(p,n/2,channel);

    // valami butterfly-szeru postprocessing filter:
    
    double t1=sin(2*M_PI/n);
    double t2=-2*sin(M_PI/n)*sin(M_PI/n);
    double t3=t2+1.0;
    double t4=t1;

    int ecx=4;      // 16/4
    int esi=n/4-1;  // 511
    
    while(esi){
	int eax=n+3-ecx;
	
	float esp20 =0.5*(p[eax]+p[ecx-1]);
	float esp28 =0.5*(p[eax]-p[ecx-1]);
	float esp3c =0.5*(p[ecx]+p[eax+1]);
	float esp1c =0.5*(p[ecx]-p[eax+1]);
	
	p[ecx-1]= esp20 + (esp3c*t3-esp28*t4);
	p[eax]  = esp20 - (esp3c*t3-esp28*t4);

	p[ecx]  = esp1c + (esp3c*t4+esp28*t3);
	p[eax+1]=-esp1c + (esp3c*t4+esp28*t3);

	double st=t3;
	t3+=t3*t2-t4*t1;
	t4+=t4*t2+st*t1;

	ecx+=2;
	--esi;
    }  //if(esi>0) goto loc_47ab6a_matekloop;

    float p1=p[1];
    float p2=p[2];
    p[1]=p1+p2;
    p[2]=p1-p2;
}



//=======================================================================
//=======================================================================
//============================== MIEZ8 ==================================
//=======================================================================
//=======================================================================


static int fftsize;
static int orig_i2;
static int pow2_i2;
static int fade_level;
static int scaled_from,scaled_to;
static float* windowing;
static float* chdata[8];

static void do_channel(int channel){

float* chXdata=chdata[channel];
int half_fftsize=fftsize>>1;

//float* windowing=bigmem+0x2050;
//float* mixbuffer=bigmem+0xa050;
//float* fftbuffer=bigmem+0x2c2050;

float mixbuffer[(half_fftsize + 2*pow2_i2)];
float fftbuffer[fftsize];

memset(mixbuffer,0,4*(half_fftsize + 2*pow2_i2));

// esp2c = pass   esp28 = passes_left
int pass=0; // valami ciklusszamlalo
// overlap count:
//int passes_left = (pow2_i2 / half_fftsize)-1;
int passes_left = (orig_i2 / half_fftsize)-1;
if(passes_left<1) passes_left=1;


int esp48 = scaled_to;  // int scaled_to = param_to / (22050.0/esp78);

//printf("****** half_fftsize=%d  passes_left=%d  pass=%d  pow2_i2=%d\n",half_fftsize,passes_left,pass,pow2_i2);

float* mixbuffer_ptr=mixbuffer;
float* chXdata_ptr=chXdata;     //int esp64=(int)chXdata-(int)esp14;

//////////////////// LOOOOOOOOOOOOOOOOOOOOOOOOOOOOP! /////////////////////////

loc_479ee9_looooooop:                     //xref j47a466

//printf("channel=%d  passes_left=%d  fftsize=%d  src=%p\n",channel,passes_left,fftsize,chXdata_ptr);
//printf("SRC=%p  ",chXdata_ptr);


{ int i;
  for(i=0;i<fftsize;i++) fftbuffer[i]=chXdata_ptr[i]*(1.0/2048.0);
  fftbuffer[0]=0; // DC
}


{
    double t1=0.0;
    double t2=1.0;
    double t3=sin(M_PI/fftsize);
    double t4=-2*sin(0.5*M_PI/fftsize)*sin(0.5*M_PI/fftsize);
    float* eax=fftbuffer+1;
    float* ecx=fftbuffer+fftsize-1;
    int edx=(fftsize>>1)+1;
    while(--edx>0){
      double a=t1*t4+t3*t2;
      double b=t2*t4-t3*t1;
      t1+=a; t2+=b;
      a=(*ecx+*eax)*t1;
      b=(*eax-*ecx)*0.5;
      *eax=a+b;
      *ecx=a-b;
      ++eax; --ecx;
    }
}



sub_47a950_miez8_hivja_ezt_is(fftbuffer-1,fftsize,channel);


fftbuffer[0]*=0.5;
fftbuffer[1]=0;

{   double temp=0;
    int i;
    for(i=0;i<fftsize;i+=2){
	temp+=fftbuffer[i];
	fftbuffer[i]=fftbuffer[i+1];
	fftbuffer[i+1]=temp;
    }
}

fftbuffer[0]*=0.5;


//========================== FADE IN/OUT ========================

int esp38=esp48/fade_level;     // esp48=param_to skalazva   fade_level=fadein_level

if(esp38>0){
    int edx=esp48-esp38; if(edx<0) edx=0;
    float* ecx=fftbuffer+edx;
    int i; // esp18
    for(i=0;i<esp38;i++)  ecx[i] *= 1.0-(double)i/(double)esp38;
}

// ebx=fftsize   edi=esp48
if(esp48>fftsize) esp48=fftsize;    // 47a095-

memset(fftbuffer+esp48,0,4*(fftsize-esp48)); // OK

esp38=scaled_from/fade_level;   // scaled_from = param_from skalazva   fade_level=fadein_level
int esp50=scaled_from-esp38;

if(esp38>0){
    int edx=esp50; if(edx<0) edx=0;
    float* ecx=fftbuffer+edx; // bigmem+0x2c2050+edx*4;
    int i; // esp18
    for(i=0;i<esp38;i++)  ecx[i] *= (double)i/(double)esp38;
}

memset(fftbuffer,0,4*esp50);

fftbuffer[0]=0;

//========================================================================


{
    double t1=0.0;
    double t2=1.0;
    double t3=sin(M_PI/fftsize);
    double t4=-2*sin(0.5*M_PI/fftsize)*sin(0.5*M_PI/fftsize);
    float* eax=fftbuffer+1;
    float* ecx=fftbuffer+fftsize-1;
    int edx=(fftsize>>1)+1;
    while(--edx>0){
      double a=t1*t4+t3*t2;
      double b=t2*t4-t3*t1;
      t1+=a; t2+=b;
      a=(*ecx+*eax)*t1;
      b=(*eax-*ecx)*0.5;
      *eax=a+b;
      *ecx=a-b;
      ++eax; --ecx;
    }
}



#if 0
sub_47a950_miez8_hivja_ezt_is(fftbuffer-1,fftsize,channel);
#else
sub_47a7b0_miez8_hivja(fftbuffer-1, fftsize>>1, channel);

{
double t1=-2*sin(M_PI/fftsize)*sin(M_PI/fftsize);
double t2=sin(2.0*M_PI/fftsize);
double t3=t1+1.0;
double t4=t2;

int ecx=4;  //  mov         ecx, 10h
float* esi=fftbuffer-1; //bigmem+0x2c204c;
int ebx=(fftsize>>2);    // ciklusszam

  while(--ebx>0){		//loc_47a236_loop:                //xref j47a300

    int eax=fftsize+3-ecx;

    float esp38=0.5*(esi[ecx]+esi[eax+1]);
    float esp50=0.5*(esi[ecx]-esi[eax+1]);
    float esp4c=0.5*(esi[eax]+esi[ecx-1]);
    float esp18=0.5*(esi[eax]-esi[ecx-1]);
    
    esi[ecx-1] = esp4c+(esp38*t3-esp18*t4);
    esi[eax]   = esp4c-(esp38*t3-esp18*t4);
    esi[ecx]   = esp50+(esp18*t3+esp38*t4);
    esi[eax+1] =-esp50+(esp18*t3+esp38*t4);

    double a=t3*t1-t4*t2;
    double b=t2*t3+t1*t4;
    t3+=a; t4+=b;

    ecx+=2;
  }  //--ebx;if(ebx>0) goto loc_47a236_loop
}

fftbuffer[0]=0.5*(fftbuffer[0]+fftbuffer[1]);  //*((float*)(bigmem+0x2c2050)) = ( *((float*)(bigmem+0x2c2050)) + *((float*)(bigmem+0x2c2050+8)) )*0.5;
fftbuffer[1]=0; //*((float*)(bigmem+0x2c2050+4)) = 0;

#endif


{   double temp=0;
    int i;
    for(i=0;i<fftsize;i+=2){
	temp+=fftbuffer[i];
	fftbuffer[i]=fftbuffer[i+1];
	fftbuffer[i+1]=temp;
    }
}



//=======================================================================
//=======================================================================
//================================ MIX ==================================
//=======================================================================
//=======================================================================
//47a348:

int i;

if(passes_left!=1) goto loc_47a387; // ha nem az utolso
// utolso!
if(pass!=0) goto loc_47a3cc;        // ha nem az elso
// elso!
//printf("mix: 47a348  ---\n"); // egyetlen pass van, nincs window:
for(i=0;i<fftsize;i++) mixbuffer_ptr[i] += fftbuffer[i];
goto loc_47a43d;


loc_47a387:                     //xref j47a351
// nem utolso!
if(pass!=0) goto loc_47a40b; // de nem is az elso
// elso!
//printf("mix: 47a387  --\\\n");
for(i=0;i<fftsize;i++)  mixbuffer_ptr[i] += (i<half_fftsize) ? fftbuffer[i] : fftbuffer[i]*windowing[i];
goto loc_47a43d;


loc_47a3cc:                     //xref j47a38b
// nem elso!
//if(passes_left!=1) goto loc_47a40b; // nem az utolso
// utolso!
//printf("mix: 47a3cc  /--\n");
for(i=0;i<fftsize;i++)  mixbuffer_ptr[i] += (i<half_fftsize) ? fftbuffer[i]*windowing[i] : fftbuffer[i];
goto loc_47a43d;


loc_47a40b:                     //xref j47a357 j47a3ce
// nem elso es nem utolso: (kozepso)
//printf("mix: 47a40b  /-\\ \n");
for(i=0;i<fftsize;i++)  mixbuffer_ptr[i] += fftbuffer[i]*windowing[i];


loc_47a43d:                     //xref j47a382 j47a3ca j47a409
                                //xref j47a438




mixbuffer_ptr+=half_fftsize;		// =bigmem+0xa050 volt initkor
chXdata_ptr+=half_fftsize;		// =chXdata volt initkor

++pass;
if(--passes_left>0) goto loc_479ee9_looooooop;



// visszamasolas:  (bigmem-es bufferbol channel data-ba)
//memcpy(chXdata,mixbuffer,4*pow2_i2);
memcpy(chXdata,mixbuffer,4*orig_i2);


}



#if 0
static int (*miez8_orig)(double pitch,double stretch, int param_channels,
    int i2,int i3,int i4,
    float* ch1, float* ch2, float* ch3, float* ch4, float* ch5, float* ch6, float* ch7, float* ch8,
    void* p1,   // output???
    void* p2,   // table? (=bigmem+0x34)
    int i5,     // 0
    int from,int to,
    int i6,     // 3
    void* p3,
    void* bigmem)=NULL;
#else


extern void calc_convolution(int n, float *buffer, float *buffer3);


static int miez8_orig(double pitch,double stretch, int param_channels,
    int param_i2,int param_i3,int param_i4,
    float* ch1data, float* ch2data, float* ch3data, float* ch4data, float* ch5data, float* ch6data, float* ch7data, float* ch8data,
    void* param_p1,   // output???
    void* param_p2,   // table? (=bigmem+0x34)
    int param_i5,     // 0
    int param_from,int param_to,
    int quality,     // 3
    void* param_p3,
    void* bigmem){

//////////////////////////// PASS 2. ///////////////////////////////
//////////////////////////// PASS 2. ///////////////////////////////
//////////////////////////// PASS 2. ///////////////////////////////
//////////////////////////// PASS 2. ///////////////////////////////
//////////////////////////// PASS 2. ///////////////////////////////
// vszinu ez a fazistolas-kereso:
// (es a param_P* es param_i* ennek a parameterei)


int half_i3 = param_i3>>1;
int half_i2=  param_i2>>1;

*((int*)param_p3)+=1;
//  mov         esi, [param_i2]

*((double*)(param_p3+8)) += half_i3 / (pitch*stretch);

int ecx=1;
#if 0
int eax=(param_i2-1)>>1;
while(eax>1){
    eax=eax>>1;
    ++ecx;
}
++ecx; // fixed
//int esp50=ecx;
eax=1; while(ecx>0){ eax+=eax; --ecx; } // eax=pow(2,ecx)
int esp30=eax;  //pow2_i2=eax;
#endif


int esp68=param_i4 * param_to;
esp68=(double)param_i4*22050.0/(double)esp68;

double scale=esp68;  // 1..64
#if 1
if(quality==4) scale=half_i2/512.0;     // n = 512
//int edi=half_i2/scale;
if(quality==1 || scale<1.0) scale=1.0;
#else
if(quality==4 || quality==1 || scale<1.0) scale=1.0;
#endif
//printf("scale=%f  esp68=%d\n",scale,esp68);

int edi=half_i2/scale;

//if(quality<4) if(edi>256) edi=256; // fixme?
//if(quality<4) if(edi>128) edi=128; // fixme?

float* buffer=bigmem+0xa050;
float* buffer2=bigmem+0x2e2064;


int i;
for(i=0;i<2*edi;i++){  //loc_47a54e_loop:                //xref j47a5d0
  int ii=(int)((i+0.5)*scale);
  double x=ch1data[ii];
  if(param_i5==0){
    if(param_channels>1) x+=ch2data[ii];
    if(param_channels>2) x+=ch3data[ii];
    if(param_channels>3) x+=ch4data[ii];
    if(param_channels>4) x+=ch5data[ii];
    if(param_channels>5) x+=ch6data[ii];
    if(param_channels>6) x+=ch7data[ii];
    if(param_channels>7) x+=ch8data[ii];
    x*=(1.0/param_channels);
  }
  buffer[i]=x;
}


//printf("**** edi=%d  scale=%5.3f\n",edi,scale);

if(1 && edi==512){
  calc_convolution(edi, buffer, buffer2);
} else
for(ecx=0;ecx<edi;ecx++){  //loc_47a605:                     //xref j47a6b9
#if 1
#if 1
  int i;
  double sum=0;
  for(i=0;i<edi;i++) sum+=(buffer[i+ecx]-buffer[i])*(buffer[i+ecx]-buffer[i]);
  buffer2[ecx]=sqrt(sum);
#else
//  SSE code:
    float sum[4]={0,0,0,0};
    float* s1=buffer;
    float* s2=buffer+ecx;
    int n=edi/8;
    __asm__ volatile(
	"movups %3, %%xmm0 \n\t"
	"movups %3, %%xmm5 \n\t"
    "1: \n\t"
	"movups (%0), %%xmm1 \n\t"
	"movups 16(%0), %%xmm3 \n\t"
	"movups (%1), %%xmm2 \n\t"
	"movups 16(%1), %%xmm4 \n\t"
	"subps %%xmm2,%%xmm1 \n\t"
	"subps %%xmm4,%%xmm3 \n\t"
	"mulps %%xmm1,%%xmm1 \n\t"
	"mulps %%xmm3,%%xmm3 \n\t"
        "add $32,%0 \n\t"
        "add $32,%1 \n\t"
	"addps %%xmm1,%%xmm0 \n\t"
	"addps %%xmm3,%%xmm5 \n\t"
        "dec %2 \n\t"
    "jnz 1b \n\t"
	"addps %%xmm5,%%xmm0 \n\t"
	"movups %%xmm0,%3 \n\t"
    : "=r"(s1), "=r"(s2), "=r"(n), "=g"(sum)
    : "0"(s1), "1"(s2), "2"(n), "g"(sum)
    : "memory"
    );
    buffer2[ecx]=sqrt(sum[0]+sum[1]+sum[2]+sum[3]);
#endif
#else
  for(i=0;i<edi;i+=4){
    sum+=fabs(buffer[i+ecx+0]-buffer[i+0])
       + fabs(buffer[i+ecx+1]-buffer[i+1])
       + fabs(buffer[i+ecx+2]-buffer[i+2])
       + fabs(buffer[i+ecx+3]-buffer[i+3]);
  }
  buffer2[ecx]=sum;
#endif
}


float last_temp=0; //buffer2[0]; <- mindig 0!
for(ecx=1;ecx<edi;ecx++){
  double temp=1.0-ecx*0.02;   // esp4c==ecx
  if(temp<0) temp=0;
  temp = (2*temp*last_temp+buffer2[ecx]) / (2*temp+1.0);
  if(temp<last_temp) break; //esi=1;
  last_temp=temp;
}

// hiba-minimumkereseshez:
double best_val=10000000000.0;	// minimum ertek
int best_pos=1;                    // miniumhely

//int esi=1;
for(;ecx<edi;ecx++){
    if(buffer2[ecx]<best_val){
	best_val=buffer2[ecx];
	best_pos=ecx;
    }
}


//printf("i2=%d i3=%d i4=%d i5=%d i6=%d\n",param_i2,param_i3,param_i4,param_i5,quality);
//printf("half_i3=%d  best_pos=%d  half_i2=%d\n",half_i3,best_pos,half_i2);


float diff2= buffer2[best_pos]-buffer2[best_pos-1];
float diff = buffer2[best_pos]-buffer2[best_pos+1];
double ratio=(diff2-diff)/(diff2+diff+1e-30) + best_pos;

*((int*)param_p2) += half_i3;

int shift = *((int*)param_p2) - *((int*)(param_p3+4));
shift = *((double*)(param_p3+8))-shift;
shift = shift / (ratio*scale);
shift = shift * (ratio*scale);
if(abs(shift)>param_i4*60) *((int*)param_p2) += shift;

//printf("*** shift=%d  ratio=%f  best_pos=%d  period=%6.3f  edi=%d\n",shift, (diff2-diff)/(diff2+diff+1e-30), best_pos, ratio*scale, edi);

*((int*)param_p1) = param_i3;

return 0;
}


#endif



//                esp+8    ebp+10h         ebp+18h
static int miez8(double pitch,double stretch, int param_channels,
    int param_i2,int param_i3,int param_i4,
    float* ch1data, float* ch2data, float* ch3data, float* ch4data, float* ch5data, float* ch6data, float* ch7data, float* ch8data,
    void* param_p1,   // output???
    void* param_p2,   // table? (=bigmem+0x34)
    int param_i5,     // 0
    int param_from,int param_to,
    int quality,     // 3
    void* param_p3,
    void* bigmem){

#if 0    
    printf("\n*** miez8_A( pitch=%10.8f, stretch=%10.8f, param_channels=%d\n",pitch,stretch,param_channels);
    printf("    i2=%d i3=%d i4=%d   p1=%p p2=%p p3=%p  bigmem=%p\n",
	param_i2,param_i3,param_i4, param_p1,param_p2,param_p3,  bigmem);
    printf("    i5=%d from=%d to=%d quality=%d\n",param_i5,param_from,param_to,quality);
#endif


int ecx=1;
int eax=(param_i2-1)>>1;
while(eax>1){
    eax=eax>>1;
    ++ecx;
}
++ecx; // fixed
//int esp50=ecx;
eax=1; while(ecx>0){ eax+=eax; --ecx; } // eax=pow(2,ecx)
// esp30=eax
pow2_i2=eax;
orig_i2=param_i2;


// esp20=i4*2048  esp24=i4*1024
fftsize=param_i4*2048;	//double esp78 = fftsize;

scaled_from = param_from / (22050.0/fftsize);  // esp68
scaled_to = param_to / (22050.0/fftsize);      // esp60

double pitch_change = pitch*stretch-1.0;         // esp88
if(pitch_change<0) pitch_change=-pitch_change;

// fadeout level: (esp34)
fade_level=6;
if(pitch_change<0.1) fade_level=10;  // FIXME: < vagy > ?

windowing=bigmem+0x2050;

chdata[0]=ch1data;
chdata[1]=ch2data;
chdata[2]=ch3data;
chdata[3]=ch4data;
chdata[4]=ch5data;
chdata[5]=ch6data;
chdata[6]=ch7data;
chdata[7]=ch8data;

////////////////////// VERY BIIIIIIIIIIIG LOOOOOOOOOP !!! ////////////////
int channel; // esp44

#ifdef SMP

#ifdef FFTW
  //========= FFTw init:
  for(channel=0;channel<param_channels;channel++) FFT_init(fftsize>>1, channel);
#endif

#if !defined(__CYGWIN__) && !defined(__MINGW32__)
pthread_t threads[param_channels];
#else
HANDLE threads[param_channels];
#endif

//========= Start threads:
  for(channel=0;channel<param_channels;channel++){
#if !defined(__CYGWIN__) && !defined(__MINGW32__)
    int ret = pthread_create( &threads[channel], NULL, do_channel, (void*) channel);
#else
    threads[channel] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)do_channel, (void*) channel, 0, 0);
#endif
  }

//========= Wait for threads to finish: 
#if !defined(__CYGWIN__) && !defined(__MINGW32__)
  for(channel=0;channel<param_channels;channel++)
    pthread_join( threads[channel], NULL);
#else
//    WaitForSingleObject( threads[channel], INFINITE);
  WaitForMultipleObjects(param_channels, threads, TRUE, INFINITE);
  for(channel=0;channel<param_channels;channel++)
    CloseHandle(threads[channel]);
#endif

#else

//========= no SMP:
  for(channel=0;channel<param_channels;channel++) do_channel(channel);

#endif

#if 0
printf("pass2-:  p1=%d  p2=%d  p3: %d  %d  %f\n",
    *((int*)param_p1),
    *((int*)param_p2),
    *((int*)param_p3),
    *((int*)param_p3+4),
    *((double*)param_p3+8)  );
#endif

int mret=miez8_orig(pitch,stretch,param_channels,
	param_i2,param_i3,param_i4,
	ch1data,ch2data,ch3data,ch4data, ch5data,ch6data,ch7data,ch8data,
	param_p1,param_p2, param_i5,
	param_from, param_to, quality, param_p3,
	bigmem);

#if 0
double x=*((double*)(param_p3+8));
printf("pass2+:  p1=%d  p2=%d  p3: %d  %d  %f\n",
    *((int*)param_p1),
    *((int*)param_p2),
    *((int*)param_p3),
    *((int*)param_p3+4),
    x );
#endif

return mret;
}



static void miez2(int edx, float* eax, int edi){
//    if(MPEX2_debug) printf("miez2: edx=%d eax=%p edi=%d\n",edx,eax,edi);
    int ecx;
    for(ecx=0;ecx<edx;ecx++){
	if(ecx<=edi) eax[ecx]*= (float)ecx/(float)edi; else
	if(ecx>edx-edi) eax[ecx]*= (float)(edx-ecx)/(float)edi;
    }
}


//============================================================================//
//============================================================================//
//============================================================================//

static void set_hook(void* dll,unsigned int addr,void* proc){
    // jump hook:
    addr+=(unsigned int)dll;
    ((unsigned char*)(addr))[0]=0xE9;
    ((unsigned int*)(addr+1))[0]=((unsigned int)proc) - (addr+5);
}

static void set_hook_call(void* dll,unsigned int addr,void* proc){
    // call hook:
    addr+=(unsigned int)dll;
    ((unsigned char*)(addr))[0]=0xE8;
    ((unsigned int*)(addr+1))[0]=((unsigned int)proc) - (addr+5);
}

void MPEX2_sethook(void* dll){

	set_hook(dll,0x79920,miez2);

#if 0
	// csak az FFT-k
        set_hook(dll,0x7a7b0,sub_47a7b0_miez8_hivja);
	set_hook(dll,0x7a950,sub_47a950_miez8_hivja_ezt_is);

#else
        // miez8 callers:
        set_hook_call(dll,0x78d94,miez8);
        set_hook_call(dll,0x78e49,miez8);
	
	// miez8() eleje
//	miez8_orig=(void*)dll+0x79d20;
//	((unsigned char*)(dll+0x79dbf))[0]=0x67; // skip big loop in miez8()
//	((unsigned char*)(dll+0x79dbf))[1]=0xe9;
#endif

}

