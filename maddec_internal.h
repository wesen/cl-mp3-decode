#ifndef MADDEC_INTERNAL_H__
#define MADDEC_INTERNAL_H__

#include <mad.h>

#include "error.h"

#define CMD_BUF_SIZE      1024

typedef enum {
  MP3DEC_COMMAND_PLAY = 0,
  MP3DEC_COMMAND_PAUSE,
  MP3DEC_COMMAND_EXIT,
  MP3DEC_COMMAND_LOAD,
  MP3DEC_COMMAND_STATUS,
  MP3DEC_COMMAND_PING,

  MP3DEC_RESPONSE_PONG,
  MP3DEC_RESPONSE_ACK,
  MP3DEC_RESPONSE_ERR,
} mp3dec_cmd_e;

struct mp3dec_state_s {
  pid_t child_pid;
  int cmd_fd;
  int response_fd;
  error_t error;
};

typedef enum {
  CHILD_STOP = 0,
  CHILD_PLAY,
  CHILD_ERROR,
  CHILD_PAUSE,
  CHILD_NONE
} child_state_e;

typedef struct child_state_s {
  int cmd_fd, response_fd;
  child_state_e state;

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

  error_t error;
} child_state_t;

#endif /* MADDEC_INTERNAL_H__ */

