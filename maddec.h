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

  struct mad_decoder decoder;

  unsigned char mp3data[MAD_BUFFER_MDLEN];
  unsigned int mp3len;
  unsigned char mp3eof;
} mp3_state_t;

char *mp3dec_error(mp3_state_t *state);
mp3_state_t *mp3dec_new(void);
void mp3dec_close(mp3_state_t *state);
int mp3dec_decode_file(mp3_state_t *state, char *filename);

#endif /* MP3_DECODE_H__ */
