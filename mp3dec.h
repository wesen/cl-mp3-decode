#ifndef MP3_DECODE_H__
#define MP3_DECODE_H__

#define BUF_SIZE          4096
#define ERROR_STRING_SIZE 256

typedef struct mp3_state_s {
  int channels;
  int little_endian;

  short pcm_l[BUF_SIZE * 100], pcm_r[BUF_SIZE * 100];
  unsigned int pcmlen;

  char strerror[ERROR_STRING_SIZE];

  int snd_fd;
  int mp3_fd;

  int initialized;
  int lame_initialized;
} mp3_state_t;

char *mp3dec_error(mp3_state_t *state);
mp3_state_t *mp3dec_reset(void);
void mp3dec_close(mp3_state_t *state);
int mp3dec_decode_data(mp3_state_t *state, unsigned char *buf, unsigned int len);
int mp3dec_decode_file(mp3_state_t *state, char *filename);

#endif /* MP3_DECODE_H__ */
