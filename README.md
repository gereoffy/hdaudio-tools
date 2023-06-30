# HD audio tools

Features:

- audio dub track aligning & cutting using SDL gui, displaying FFT graph of audio tracks and/or video image strips (requires MPlayer)
- command-line tools for very HQ resampling/stretching (Cinema/PAL/NTSC rate conversion), converting (wav/ac3/dts/mp3) audio tracks

Usage:

see \*.txt files!


Compile:

$ apt install liba52-0.7.4-dev libdca-dev libfftw3-dev libmad0-dev libsdl1.2-dev  
$ make


Notes:

DLL files & win32 DLL loader from MPlayer not included (used by agm2dts & dtsma tools on linux)
