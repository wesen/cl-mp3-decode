CFLAGS += -I/usr/local/include
LDFLAGS += -L/usr/local/lib

CFLAGS += -Wall -g

all: libmaddec.a maddec

MADDEC_OBJS := misc.o error.o maddec.o child.o

libmaddec.so: maddec.c
	$(CC) $(CFLAGS) -fPIC -c -o libmaddec.o maddec.c
	ld $(LDFLAGS) -lm -lmp3lame -shared libmaddec.o -o libmaddec.so

libmaddec.a: $(MADDEC_OBJS)
	ar r $@ $(MADDEC_OBJS)

maddec: main.o libmaddec.a
	$(CC) -g -o $@ main.o $(LDFLAGS) -L. -lmaddec -lmad -lm

clean:
	- rm -rf *.o mp3-decode mp3dec maddec *.so *.a