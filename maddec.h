#ifndef MP3_DECODE_H__
#define MP3_DECODE_H__

#include <mad.h>

#include "error.h"

typedef enum {
  MP3DEC_COMMAND_PLAY = 0,
  MP3DEC_COMMAND_PAUSE,
  MP3DEC_COMMAND_STOP,
  MP3DEC_COMMAND_GET_ERROR,
  MP3DEC_COMMAND_LOAD,
  MP3DEC_COMMAND_STATUS,
  MP3DEC_COMMAND_PING,
  MP3DEC_COMMAND_PONG,
} mp3dec_cmd_e;

#define CMD_BUF_SIZE      1024

typedef struct mp3dec_state_s {
  pid_t child_pid;
  int cmd_fd, response_fd;
  mp3dec_error_t error;
} mp3dec_state_t;

typedef struct child_state_s {
  int cmd_fd, response_fd;

  int snd_fd;
  int snd_initialized;
  int nchannels;
  int samplerate;

  struct mad_decoder decoder;
  int mad_initialized;

  int mp3_fd;
  unsigned char mp3data[MAD_BUFFER_MDLEN];
  unsigned int  mp3len;
  unsigned char mp3eof;

  mp3dec_error_t error;
} child_state_t;

mp3dec_state_t *mp3dec_new(void);
void mp3dec_close(mp3dec_state_t *state);
int mp3dec_decode_file(mp3dec_state_t *state, char *filename);

#endif /* MP3_DECODE_H__ */
