CFLAGS += -I/usr/local/include
LDFLAGS += -L/usr/local/lib

CFLAGS += -Wall -g

all: libmp3dec.a mp3dec

libmp3dec.a: mp3dec.o
	ar r $@ mp3dec.o

mp3dec: main.o
	$(CC) -o $@ main.o $(LDFLAGS) -L. -lmp3dec -lmp3lame -lm

clean:
	- rm -rf *.o mp3-decode