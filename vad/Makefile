
CFLAGS = -Wall -O4

LOBJECTS = basicop2.o log2.o oper_32b.o pow2.o r_fft.o vad2.o
       
LSRC = $(LOBJECTS:.o=.c)

LIBS = -lm

libvad.a: $(LOBJECTS)
	/bin/rm -f libvad.a
	ar cr libvad.a $(LOBJECTS)
	-if test -s /bin/ranlib; then /bin/ranlib libvad.a; \
      else if test -s /usr/bin/ranlib; then /usr/bin/ranlib libvad.a; \
	else exit 0; fi; fi

$(PROGRAM): $(OBJECTS) $(ULIBS)
	$(CC) -o $(PROGRAM) $(OBJECTS) $(ULIBS) $(LIBS)

clean:
	rm -f $(PROGRAM) $(LOBJECTS) $(ARCH) core *.bak *.a

depend:
	makedepend -- $(CFLAGS) -- $(LSRC)


# DO NOT DELETE
