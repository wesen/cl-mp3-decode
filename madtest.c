/*
 * simple mad decoder to test the audio output.
 * ripped from minimad.c in the libmad distribution
 *
 * (c) 2005 bl0rg.net
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include <mad.h>

#include "error.h"
#include "audio.h"

typedef struct buffer_s {
  unsigned char *buf;
  unsigned long len;
} buffer_t;

static enum mad_flow mad_input(void *data,
                               struct mad_stream *stream) {
  buffer_t *buffer = data;
  if (!buffer->len)
    return MAD_FLOW_STOP;
  mad_stream_buffer(stream, buffer->buf, buffer->len);
  buffer->len = 0;
  return MAD_FLOW_CONTINUE;
}

static enum mad_flow mad_output(void *data,
                                struct mad_header const *header,
                                struct mad_pcm *pcm) {
  buffer_t *buffer = data;
  error_t error;
  if (!audio_write(pcm, &error)) {
    printf("Could not write pcm data: %s\n", error_get(&error));
    return MAD_FLOW_BREAK;
  }
  return MAD_FLOW_CONTINUE;
}

static enum mad_flow mad_error(void *data,
                               struct mad_stream *stream,
                               struct mad_frame *frame) {
  buffer_t *buffer = data;

  fprintf(stderr, "decoding error 0x%04x (%s) at byte offset %u\n",
	  stream->error, mad_stream_errorstr(stream),
	  stream->this_frame - buffer->buf);

  /* return MAD_FLOW_BREAK here to stop decoding (and propagate an error) */

  return MAD_FLOW_CONTINUE;
  
}

static void mad_decode(unsigned char *data, unsigned long size) {
  struct mad_decoder decoder;
  buffer_t buffer;

  buffer.buf = data;
  buffer.len = size;

  mad_decoder_init(&decoder, &buffer,
                   mad_input,
                   NULL, /* header */
                   NULL, /* filter */
                   mad_output,
                   mad_error,
                   NULL /* message */
                   );

  int ret = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);

  mad_decoder_finish(&decoder);

  error_t error;
  if (!audio_close(&error)) {
    printf("Could not close audio: %s\n", error_get(&error));
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: ./madtest mp3file\n");
    return 1;
  }

  char *filename = argv[1];
  int ret;

  struct stat s;
  ret = stat(filename, &s);
  if (ret < 0) {
    fprintf(stderr, "Could not stat %s\n", filename);
    return 1;
  }

  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    perror("open");
    return 1;
  }
  
  unsigned char *data;
  data = mmap(NULL, s.st_size, PROT_READ, MAP_FILE, fd, 0);
  if ((int)data == -1) {
    perror("mmap");
    close(fd);
    return 1;
  }

  mad_decode(data, s.st_size);

  munmap(data, s.st_size);
  close(fd);
  
  return 0;
}
