MANDIR=/usr/local/man/man1
BINDIR=/usr/local/bin

# If your sound device is not /dev/dsp, you should uncomment the line
# below, replacing /dev/dsp with your sound device file.

# CUSTFLAGS=-DSOUNDDEV='"/dev/dsp"'

all: wav2prg prg2wav
wav2prg: wav2prg.c
	gcc wav2prg.c -o wav2prg $(CUSTFLAGS)
prg2wav: prg2wav.c
	gcc prg2wav.c -o prg2wav $(CUSTFLAGS)
install: wav2prg prg2wav
	install -o root -g root -m 755 wav2prg $(BINDIR)
	install -o root -g root -m 755 prg2wav $(BINDIR)
	install -o root -g root -m 755 d642wav $(BINDIR)
	install -o root -g root -m 644 man/prg2wav.1 $(MANDIR)
	install -o root -g root -m 644 man/wav2prg.1 $(MANDIR)
uninstall:
	rm -f $(BINDIR)/wav2prg $(BINDIR)/prg2wav $(MANDIR)/prg2wav.1 $(MANDIR)/wav2prg.1
clean:
	rm -f wav2prg prg2wav
