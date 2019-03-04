PREFIX= $(HOME)

CFLAGS = -Wall -Werror -I. -I$(PREFIX)/include
LDFLAGS= -L. -L$(PREFIX)/lib
LDLIBS= -lhajime -lxutp -lrho -lutp -llz4 -lcrypto -lssl -lpthread -lrt

LIBS= libhajime.a
BINS= hajime_parse_payload hajime_getfile hajime_getpeerpubkey
OBJS= hajime.o hajime_parse_payload.o hajime_getfile.o hajime_getpeerpubkey.o

all: $(LIBS) $(BINS)

libhajime.a: hajime.o
	ar cru $@ $^
	ranlib $@

hajime_parse_payload: hajime_parse_payload.o libhajime.a
	$(CXX) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS)

hajime_getfile: hajime_getfile.o libhajime.a
	$(CXX) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS)

hajime_getpeerpubkey: hajime_getpeerpubkey.o libhajime.a
	$(CXX) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS)

install: $(LIBS) $(BINS)
	mkdir -p $(PREFIX)/lib
	cp libhajime.a $(PREFIX)/lib
	mkdir -p $(PREFIX)/include
	cp *.h $(PREFIX)/include
	mkdir -p $(PREFIX)/bin
	cp hajime_parse_payload $(PREFIX)/bin
	cp hajime_getfile $(PREFIX)/bin
	cp hajime_getpeerpubkey $(PREFIX)/bin
	cp tailfile_select.pl $(PREFIX)/bin
	

hajime.o: hajime.c hajime.h
hajime_parse_payload.o: hajime_parse_payload.c
hajime_getfile.o: hajime_getfile.c
hajime_getpeerpubkey.o: hajime_getpeerpubkey.c

clean:
	rm -f $(LIBS) $(BINS) $(OBJS)

.PHONY: all clean
