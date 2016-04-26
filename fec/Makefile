

CFLAGS = -Wall -O4


LOBJECTS = fec_golay2412.o 
LSRC = $(LOBJECTS:.o=.c)

LIBS = -lm

libfec.a: $(LOBJECTS)
	/bin/rm -f libfec.a
	ar cr libfec.a $(LOBJECTS)
	-if test -s /bin/ranlib; then /bin/ranlib libfec.a; \
      else if test -s /usr/bin/ranlib; then /usr/bin/ranlib libfec.a; \
	else exit 0; fi; fi


clean:
	rm -f $(PROGRAM) $(LOBJECTS) $(ARCH) core *.bak *.a

depend:
	makedepend -- $(CFLAGS) -- $(LSRC)


# DO NOT DELETE
