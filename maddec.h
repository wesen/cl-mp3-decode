#ifndef MP3_DECODE_H__
#define MP3_DECODE_H__

#include <mad.h>

#define BUF_SIZE          40960
#define ERROR_STRING_SIZE 256

typedef struct mp3_state_s {
  int channels;
  int little_endian;

  short pcm_l[BUF_SIZE * 10], pcm_r[BUF_SIZE * 10];
  unsigned int pcmlen;

  char strerror[ERROR_STRING_SIZE];

  int snd_fd;
  int mp3_fd;

  int snd_initialized;
  int nchannels;
  int samplerate;

  void *buffer;
  unsigned int length;

  struct mad_decoder decoder;
} mp3_state_t;

char *mp3dec_error(mp3_state_t *state);
void mp3dec_reset(mp3_state_t *state);
void mp3dec_close(mp3_state_t *state);
int mp3dec_decode_file(mp3_state_t *state, char *filename);

#endif /* MP3_DECODE_H__ */
