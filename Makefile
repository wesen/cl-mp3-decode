CFLAGS += -I/usr/local/include
LDFLAGS += -L/usr/local/lib

CFLAGS += -Wall -g

all: libmaddec.a maddec

libmaddec.so: maddec.c
	$(CC) $(CFLAGS) -fPIC -c -o libmaddec.o maddec.c
	ld $(LDFLAGS) -lm -lmp3lame -shared libmaddec.o -o libmaddec.so

libmaddec.a: maddec.o
	ar r $@ maddec.o

maddec: main.o libmaddec.a
	$(CC) -g -o $@ main.o $(LDFLAGS) -L. -lmaddec -lmad -lm

clean:
	- rm -rf *.o mp3-decode mp3dec maddec *.so *.a