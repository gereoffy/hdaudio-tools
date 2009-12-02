// convolution!

#include <string.h>
#include <math.h>

#include <fftw3.h>


static float* A=NULL;
static float* B=NULL;
static fftwf_complex* fft_A=NULL;
static fftwf_complex* fft_B=NULL;

static fftwf_plan fft_planA;
static fftwf_plan fft_planB;
static fftwf_plan fft_planC;

static int inited=0;

void calc_convolution(int n, float *buffer, float *buffer3){
int N=2*n;
int i,j;

if(inited!=n){
    if(inited) printf("ERROR! calc_convolution reinited / N size changed %d->%d!\n",inited,n);
    printf("fast convolution code inited.\n");
    inited=n;
    A     = fftwf_malloc(sizeof(float) * N);
    B     = fftwf_malloc(sizeof(float) * N);
    fft_A     = fftwf_malloc(sizeof(fftwf_complex) * N);
    fft_B     = fftwf_malloc(sizeof(fftwf_complex) * N);
#if 1
    fft_planA = fftwf_plan_dft_r2c_1d(N,A,fft_A, FFTW_ESTIMATE|FFTW_DESTROY_INPUT);
    fft_planB = fftwf_plan_dft_r2c_1d(N,B,fft_B, FFTW_ESTIMATE|FFTW_DESTROY_INPUT);
    fft_planC = fftwf_plan_dft_c2r_1d(N,fft_B,B, FFTW_ESTIMATE|FFTW_DESTROY_INPUT);
#else
    fft_planA = fftwf_plan_dft_r2c_1d(N,A,fft_A, FFTW_MEASURE|FFTW_DESTROY_INPUT);
    fft_planB = fftwf_plan_dft_r2c_1d(N,B,fft_B, FFTW_MEASURE|FFTW_DESTROY_INPUT);
    fft_planC = fftwf_plan_dft_c2r_1d(N,fft_B,B, FFTW_MEASURE|FFTW_DESTROY_INPUT);
#endif
}

double sum3=0;
for(i=0;i<n;i++) sum3+=buffer[i]*buffer[i]; // a^2

//memset(B,0,sizeof(float)*N);
//for(i=0;i<n;i++) A[i]=buffer[i];
memcpy(A,buffer,sizeof(float)*n);
memset(A+n,0,sizeof(float)*n);
for(i=0;i<N;i++) B[(N-i) % N]=buffer[i];

fftwf_execute(fft_planA);
fftwf_execute(fft_planB);

for(i=0;i<N;i++){ 
    // p+iq = (a+ib) x (c+id)
    // p = ac - bd
    // q = ad + bc
    double temp = fft_A[i][0]*fft_B[i][0] - fft_A[i][1]*fft_B[i][1];
    fft_B[i][1] = fft_A[i][0]*fft_B[i][1] + fft_A[i][1]*fft_B[i][0];
    fft_B[i][0] = temp;
}

fftwf_execute(fft_planC);

buffer3[0]=0; // ez mindig 0!

double sum1=sum3;
for(j=1;j<n;j++){
    sum1 = sum1 - buffer[j-1]*buffer[j-1] + buffer[j+n-1]*buffer[j+n-1];
    double sum2=B[N-j]*(1.0/N);
    buffer3[j]=sqrt(sum1+sum3-2*sum2);
}

}



#if 0
// teszt:
int main(){
int i,j,k;
int n=512;

float buffer[1024];
float buffer2[512];
float buffer3[512];

for(i=0;i<n;i++) buffer[i]=0.1*(cos(i)+sin(i*0.23652)+sin(i*0.072632)+(i&3)/10.0);

//for(k=0;k<10000;k++)
for(j=0;j<n;j++){
  double sum=0;
//  for(i=0;i<n;i++) sum += buffer[i+j]*buffer[i];
  for(i=0;i<n;i++) sum += (buffer[i+j]-buffer[i])*(buffer[i+j]-buffer[i]);
  buffer2[j]=sqrt(sum);
}

//for(k=0;k<10000;k++)
calc_convolution(n,buffer,buffer3);

for(i=0;i<n;i++) printf(" %5.3f",buffer2[i]); printf("\n");
for(i=0;i<n;i++) printf(" %5.3f",buffer3[i]); printf("\n");

}

#endif

