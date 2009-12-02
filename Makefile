
# ppc MacOSX:
#CFLAGS = -O3 -Wall -DBSWAP -DDTS -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -I/opt/local/include/ -L/opt/local/lib -lm
#SDL = -lSDLmain -lSDL -Wl,-framework,Cocoa

# x86 MacOSX:
#CFLAGS = -g -O3 -march=core2 -malign-double -ffast-math -Wall -DDTS -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -I/sw/include/ -L/sw/lib -lm
CFLAGS = -g -O3 -march=core2 -ffast-math -Wall -DDTS -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -I/sw/include/ -L/sw/lib -lm
SDL = -lSDLmain -lSDL -Wl,-framework,Cocoa

## x86 Linux / cygwin:
#-mfpmath=sse
##CFLAGS = -O3 -march=i686 -Wall -DDTS -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -lm
#CFLAGS = -O3 -march=i686 -malign-double -ffast-math -Wall -DDTS -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -lm
#SDL = -lSDL

LOADERFLAGS=-Iloader -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer -Ddbg_printf=__vprintf -DTRACE=__vprintf

LOADER=  loader/elfdll.c \
         loader/ext.c \
         loader/ldt_keeper.c \
         loader/module.c \
         loader/pe_image.c \
         loader/pe_resource.c \
         loader/registry.c \
         loader/resource.c \
         loader/win32.c \
         loader/wrapper.S \
         loader/osdep/mmap_anon.c

##cc -march=i686 -mtune=i686  -g -o tf -O3 $CFLAGS tf.c $LOADER -ldl -lm -lpthread -lfftw3f

#FFTW = -lfftw3
FFTWF = -lfftw3f

AC3 = -la52 -ldca
MP3 = -lmad
#SRC = -lsamplerate
SRC = samplerate.c src_sinc.c
APPS=resample tstretch tstretchsmp cut copypart dgindex dgfix dgparse dtspack dvddelay agm2dts mp3towav compare2

all: $(APPS)

remake: clean all

clean:
	rm -f $(APPS) *.exe

tstretch: resample.c mpex2v3.c mpex2_bin.c mpex2_c.c mpex2_conv.c
	$(CC) -DMPEX2 -DFFTW $(CFLAGS) -o $@ $^ $(AC3) $(FFTWF)

tstretchsmp: resample.c mpex2v3.c mpex2_bin.c mpex2_c.c mpex2_conv.c
	$(CC) -DMPEX2 -DFFTW -DSMP $(CFLAGS) -o $@ $^ $(AC3) $(FFTWF) -lpthread

agm2dts: agm2dts.c $(LOADER)
	$(CC) $(CFLAGS) $(LOADERFLAGS) -o $@ $^ -ldl -lpthread

dtsma: $(LOADER) dtsma.c
	$(CC) $(CFLAGS) $(LOADERFLAGS) -o $@ $^ -ldl -lpthread

resample: resample.c $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(AC3)

compare2: compare2.c pitch.c filter.c
	$(CC) $(CFLAGS) -o $@ $^ $(FFTWF) $(AC3) $(SDL)

cut: cut.c
	$(CC) $(CFLAGS) -o $@ $^ $(AC3)

copypart: copypart.c
	$(CC) $(CFLAGS) -o $@ $^ $(AC3)

dgindex: dgindex.c
	$(CC) $(CFLAGS) -o $@ $^

dgparse: dgparse.c
	$(CC) $(CFLAGS) -o $@ $^

dgfix: dgfix.c
	$(CC) $(CFLAGS) -o $@ $^

dtspack: dtspack.c
	$(CC) $(CFLAGS) -o $@ $^ $(AC3)

dvddelay: dvddelay.c
	$(CC) $(CFLAGS) -o $@ $^

mp3towav: mp3towav.c
	$(CC) $(CFLAGS) -o $@ $^ $(MP3)

