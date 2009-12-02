
typedef long (*MPEX2_callback_t)(void* cb_data, float **data);

extern int MPEX2_load(int hook); // ret: 0=error 1=ok

extern int MPEX2_process(double stretch, double pitch, int quality, int samplerate, int channels, MPEX2_callback_t callback,float* outbuf);

