VORBIS_ORIG = $(VORBIS)tremor/

FINAL_CFLAGS += -I$(VORBIS_ORIG)

LIB_VORBIS = \
    bitwise.o \
    codebook.o \
    dsp.o \
    floor0.o \
    floor1.o \
    floor_lookup.o \
    framing.o \
    info.o \
    mapping0.o \
    mdct.o \
    misc.o \
    res012.o \
    vorbisfile.o \

VORBIS_HEADERS =

bitwise.o: $(VORBIS_ORIG)bitwise.c $(VORBIS_HEADERS) $(STD_INCS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)bitwise.c

codebook.o: $(VORBIS_ORIG)codebook.c $(VORBIS_HEADERS) $(STD_INCS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)codebook.c

dsp.o: $(VORBIS_ORIG)dsp.c $(VORBIS_HEADERS) $(STD_INCS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)dsp.c

floor0.o: $(VORBIS_ORIG)floor0.c $(VORBIS_HEADERS) $(STD_INCS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)floor0.c

floor1.o: $(VORBIS_ORIG)floor1.c $(VORBIS_HEADERS) $(STD_INCS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)floor1.c

floor_lookup.o: $(VORBIS_ORIG)floor_lookup.c $(VORBIS_HEADERS) $(STD_INCS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)floor_lookup.c

framing.o: $(VORBIS_ORIG)framing.c $(VORBIS_HEADERS) $(STD_INCS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)framing.c

info.o: $(VORBIS_ORIG)info.c $(VORBIS_HEADERS) $(STD_INCS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)info.c

mapping0.o: $(VORBIS_ORIG)mapping0.c $(VORBIS_HEADERS) $(STD_INCS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)mapping0.c

mdct.o: $(VORBIS_ORIG)mdct.c $(VORBIS_HEADERS) $(STD_INCS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)mdct.c

misc.o: $(VORBIS_ORIG)misc.c $(VORBIS_HEADERS) $(STD_INCS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)misc.c

res012.o: $(VORBIS_ORIG)res012.c $(VORBIS_HEADERS) $(STD_INCS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)res012.c

vorbisfile.o: $(VORBIS_ORIG)vorbisfile.c $(VORBIS_HEADERS) $(STD_INCS)
	$(CC) $(FINAL_CFLAGS) -c $(VORBIS_ORIG)vorbisfile.c
