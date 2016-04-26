

CFLAGS = -Wall -O4


LOBJECTS = audio.o 
LSRC = $(LOBJECTS:.o=.c)

LIBS = -lm

libaudio.a: $(LOBJECTS)
	/bin/rm -f libaudio.a
	ar cr libaudio.a $(LOBJECTS)
	-if test -s /bin/ranlib; then /bin/ranlib libaudio.a; \
      else if test -s /usr/bin/ranlib; then /usr/bin/ranlib libaudio.a; \
	else exit 0; fi; fi


clean:
	rm -f $(PROGRAM) $(LOBJECTS) $(ARCH) core *.bak *.a

depend:
	makedepend -- $(CFLAGS) -- $(LSRC)


# DO NOT DELETE
