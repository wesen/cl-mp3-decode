CFLAGS += -I/usr/local/include -fPIC
LDFLAGS += -L/usr/local/lib

CFLAGS += -Wall -g

all: libmaddec.so maddec

MADDEC_OBJS := misc.o error.o maddec.o child.o

libmaddec.so: $(MADDEC_OBJS)
	ld $(LDFLAGS) -lm -lmad -shared $(MADDEC_OBJS) -o libmaddec.so

maddec: main.o libmaddec.so
	$(CC) -g -o $@ main.o $(LDFLAGS) -L. -lmaddec -lmad -lm

clean:
	- rm -rf *.o mp3-decode mp3dec maddec *.so *.a