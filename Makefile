CFLAGS += -I/usr/local/include
LDFLAGS += -L/usr/local/lib

CFLAGS += -Wall -g

all: libmp3dec.so libmp3dec.a libmaddec.a mp3dec maddec

libmp3dec.so: mp3dec.c
	$(CC) $(CFLAGS) -fPIC -c -o libmp3dec.o mp3dec.c
	ld $(LDFLAGS) -lm -lmp3lame -shared libmp3dec.o -o libmp3dec.so

libmp3dec.a: mp3dec.o
	ar r $@ mp3dec.o

libmaddec.a: maddec.o
	ar r $@ maddec.o

mp3dec: main.o libmp3dec.a
	$(CC) -o $@ main.o $(LDFLAGS) -L. -lmp3dec -lmp3lame -lm

maddec: main.o libmaddec.a
	$(CC) -o $@ main.o $(LDFLAGS) -L. -lmaddec -lmad -lm

clean:
	- rm -rf *.o mp3-decode