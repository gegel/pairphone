

CFLAGS = -Wall -O4


LOBJECTS = modem.o 
LSRC = $(LOBJECTS:.o=.c)

LIBS = -lm

libaudio.a: $(LOBJECTS)
	/bin/rm -f libmodem.a
	ar cr libmodem.a $(LOBJECTS)
	-if test -s /bin/ranlib; then /bin/ranlib libmodem.a; \
      else if test -s /usr/bin/ranlib; then /usr/bin/ranlib libmodem.a; \
	else exit 0; fi; fi


clean:
	rm -f $(PROGRAM) $(LOBJECTS) $(ARCH) core *.bak *.a

depend:
	makedepend -- $(CFLAGS) -- $(LSRC)


# DO NOT DELETE
