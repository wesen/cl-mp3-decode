CFLAGS += -Wall -g

# Create dependencies
%.d: %.c
	$(CC) -MM $(CFLAGS) $< > $@
	$(CC) -MM $(CFLAGS) $< | sed s/\\.o/.d/ >> $@

all: $(LIB_MADDEC) maddec madtest

LIB_MADDEC_OBJS := misc.o error.o maddec.o child.o $(AUDIO_OBJS)
MADDEC_OBJS := main.o

$(OBJS) := $(LIB_MADDEC_OBJS) $(MADDEC_OBJS)

DEPS := $(patsubst %.o,%.d,$(OBJS))
include $(DEPS)

$(LIB_MADDEC): $(LIB_MADDEC_OBJS)
	ld $(LDFLAGS) -lm -lmad -shared $(MADDEC_OBJS) -o libmaddec.so

maddec: $(MADDEC_OBJS) $(LIB_MADDEC)
	$(CC) -g -o $@ $(MADDEC_OBJS) $(LDFLAGS) -L. -lmaddec -lmad -lm

clean:
	- rm -rf *.o maddec $(LIB_MADDEC) *.a