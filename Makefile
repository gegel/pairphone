      
#   Make file for PairPhone
#   Tested with gcc 4.4.3 under Ubuntu 10.04 (requires libsound2-dev)
#   and MinGW (gcc 4.8.1, make 3.81) under Windows XP SP3


# Debugging options  
DEBUG = -O -DHEXDUMP

#Full duplex:
CCFLAGS =  -DAUDIO_BLOCKING -DLINUX -DM_LITTLE_ENDIAN -DNEEDED_LINEAR -DLINUX_DSP_SMALL_BUFFER -DHAVE_DEV_RANDOM

CC = gcc -Wall # for GNU's gcc compiler
CELPFLAGS = -fomit-frame-pointer -ffast-math -funroll-loops
LFLAGS = -lm
CCFLAGS = -DLINUX_ALSA -DM_LITTLE_ENDIAN
SOUNDLIB = -lasound 

#   Compiler flags

CFLAGS = $(DEBUG) $(PKOPTS) -Iaudio -Icrypto -Imelpe -Imodem -Ifec -Ivad $(CARGS) $(DUPLEX) $(CCFLAGS) $(DOMAIN)

BINARIES = pp

PROGRAMS = $(BINARIES) $(SCRIPTS)

DIRS = audio crypto melpe modem fec vad

all:	$(PROGRAMS)

SPKROBJS = pp.o crp.o ctr.o rx.o tx.o

#Link

ifdef SYSTEMROOT
#Win32    
pp: $(SPKROBJS) audiolib.o cryptolib.o melpelib.o modemlib.o feclib.o vadlib.o 
	$(CC) $(SPKROBJS)  audio/libaudio.a crypto/libcrypto.a melpe/libmelpe.a modem/libmodem.a fec/libfec.a vad/libvad.a $(LFLAGS) -lcomctl32 -lwinmm -lws2_32 -o pp
else
   ifeq ($(shell uname), Linux)
#Linux      
pp: $(SPKROBJS) audiolib.o cryptolib.o melpelib.o modemlib.o feclib.o vadlib.o
	$(CC) $(SPKROBJS) audio/libaudio.a crypto/libcrypto.a melpe/libmelpe.a modem/libmodem.a fec/libfec.a vad/libvad.a  $(LFLAGS) $(SOUNDLIB) -o pp
   endif
endif


#	Compression and encryption libraries.  Each of these creates
#	a place-holder .o file in the main directory (which is not
#	an actual object file, simply a place to hang a time and
#	date stamp) to mark whether the library has been built.
#	Note that if you actually modify a library you'll need to
#	delete the place-holder or manually make within the library
#	directory.  This is tacky but it avoids visiting all the
#	library directories on every build and/or relying on features
#	in make not necessarily available on all platforms.


melpelib.o:
	( echo "Building MELPE library."; cd melpe ; make CC="$(CC) $(CCFLAGS) $(DEBUG) $(CELPFLAGS)" )
	echo "MELPE" >melpelib.o

vadlib.o:
	( echo "Building VAD library."; cd vad ; make CC="$(CC) $(CCFLAGS) $(DEBUG) $(CELPFLAGS)" )
	echo "VAD" >vadlib.o

modemlib.o:
	( echo "Building MODEM library."; cd modem ; make CC="$(CC) $(CCFLAGS) $(DEBUG) $(CELPFLAGS)" )
	echo "MODEM" >modemlib.o

audiolib.o:
	( echo "Building AUDIO library."; cd audio ; make CC="$(CC) $(CCFLAGS) $(DEBUG) $(CELPFLAGS)" )
	echo "AUDIO" >audiolib.o

cryptolib.o:
	( echo "Building CRYPTO library."; cd crypto ; make CC="$(CC) $(CCFLAGS) $(DEBUG)" )
	echo "CRYPTO" >cryptolib.o

feclib.o:
	( echo "Building FEC library."; cd fec ; make CC="$(CC) $(CCFLAGS) $(DEBUG)" )
	echo "FEC" >feclib.o


#   Object file dependencies

crp.o: Makefile crp.c

ctr.o: Makefile ctr.c

rx.o: Makefile rx.c

tx.o: Makefile tx.c


#	Clean everything

clean:
	find . -name Makefile.bak -exec rm {} \;
	rm -f core *.out *.o *.bak $(PROGRAMS) *.shar *.exe *.a
	@for I in $(DIRS); \
	  do (cd $$I; echo "==>Entering directory `pwd`"; $(MAKE) $@ || exit 1); done
	

# DO NOT DELETE
