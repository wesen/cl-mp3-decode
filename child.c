#include <sys/types.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <stdlib.h>

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <mad.h>

#include "maddec.h"
#include "maddec_internal.h"
#include "error.h"
#include "misc.h"

#include "audio.h"

static enum mad_flow mad_input(void *data, struct mad_stream *stream);
static enum mad_flow mad_output(void *data, struct mad_header const *header, struct mad_pcm *pcm);
static enum mad_flow mad_error(void *data, struct mad_stream *stream, struct mad_frame *frame);
static int mp3dec_child_read_cmd(child_state_t *state);

static const char *mp3dec_child_state_str(child_state_t *state) {
  switch (state->state) {
  case CHILD_NONE:
    return "CHILD_NONE";
  case CHILD_PLAY:
    return "CHILD_PLAY";
  case CHILD_ERROR:
    return "CHILD_ERROR";
  case CHILD_PAUSE:
    return "CHILD_PAUSE";
  case CHILD_STOP:
    return "CHILD_STOP";
  default:
    return "UNKNOWN";
  }
}

/* initializing and stuff */
static void mp3dec_child_reset(child_state_t *state,
			       int cmd_fd, int response_fd) {
  state->cmd_fd = cmd_fd;
  state->response_fd = response_fd;
  
  error_reset(&state->error);
  memset(state->filename, 0, sizeof(state->filename));
  

  state->state = CHILD_NONE;
  state->nchannels = 0;
  state->samplerate = 0;
  state->audio = NULL;

  state->mp3_fd = -1;
  state->mp3len = 0;
  state->mp3eof = 0;
  memset(state->mp3data, 0, sizeof(state->mp3data));

  mad_decoder_init(&state->decoder, state,
		   mad_input, 0, 0,
		   mad_output,
		   mad_error, 0);
  state->mad_initialized = 1;
}

static void mp3dec_child_close(child_state_t *state) {
  if (state->mad_initialized) {
    mad_decoder_finish(&state->decoder);
    state->mad_initialized = 0;
  }

  if (state->audio)
    audio_close(state->audio);

  if (state->mp3_fd != -1) {
    close(state->mp3_fd);
    state->mp3_fd = -1;
  }

  if (state->cmd_fd != -1) {
    close(state->cmd_fd);
    state->cmd_fd = -1;
  }

  if (state->response_fd != -1) {
    close(state->response_fd);
    state->response_fd = -1;
  }
}

/*
 * The following utility routine performs simple rounding, clipping, and
 * scaling of MAD's high-resolution samples down to 16 bits. It does not
 * perform any dithering or noise shaping, which would be recommended to
 * obtain any exceptional audio quality. It is therefore not recommended to
 * use this routine if high-quality output is desired.
 */
static inline
signed int mad_scale(mad_fixed_t sample)
{
  /* round */
  sample += (1L << (MAD_F_FRACBITS - 16));

  /* clip */
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  /* quantize */
  return sample >> (MAD_F_FRACBITS + 1 - 16);
}

static
enum mad_flow mad_input(void *data, struct mad_stream *stream) {
  child_state_t *state = data;
  int ret;

  if ((state->mp3eof) || (state->state != CHILD_PLAY)) {
    return MAD_FLOW_STOP;
  }

  if (unix_check_fd_read(state->cmd_fd)) {
    if (mp3dec_child_read_cmd(state) < 0) {
      state->state = CHILD_ERROR;
      return MAD_FLOW_BREAK;
    }
  }

  if (stream->next_frame) {
    memmove(state->mp3data, stream->next_frame,
	    (state->mp3len = &state->mp3data[state->mp3len] - stream->next_frame));
  } else {
    state->mp3len = 0;
  }

  assert(state->mp3_fd >= 0);

  ret = unix_read(state->mp3_fd, state->mp3data + state->mp3len,
		  sizeof(state->mp3data) - state->mp3len);
  
  if (ret < 0) {
    error_printf_strerror(&state->error, "Could not read from \"%s\"",
			  mp3dec_child_state_str(state));
    state->state = CHILD_ERROR;
    return MAD_FLOW_BREAK;
    
  } else if (ret == 0) {
    assert(sizeof(state->mp3data) - state->mp3len >= MAD_BUFFER_GUARD);

    while (ret < MAD_BUFFER_GUARD)
      state->mp3data[ret++] = 0;

    state->mp3eof = 1;
  }
  
  state->mp3len += ret;

  assert(state->mp3len > MAD_BUFFER_GUARD);
  
  mad_stream_buffer(stream, state->mp3data, state->mp3len);
  return MAD_FLOW_CONTINUE;
}

static
enum mad_flow mad_output(void *data, struct mad_header const *header,
			 struct mad_pcm *pcm) {
  child_state_t *state = data;

  if (state->state != CHILD_PLAY) {
    return MAD_FLOW_STOP;
  }

  if (unix_check_fd_read(state->cmd_fd)) {
    if (mp3dec_child_read_cmd(state) < 0) {
      state->state = CHILD_ERROR;
      return MAD_FLOW_BREAK;
    }
  }

  if (!audio_write(pcm, &state->error)) {
    error_prepend(&state->error, "Could not write pcm data to audio");
    state->state = CHILD_ERROR:
    return MAD_FLOW_BREAK;
  }

  return MAD_FLOW_CONTINUE;
}

static
enum mad_flow mad_error(void *data, struct mad_stream *stream,
			struct mad_frame *frame) {
  child_state_t *state = data;

  fprintf(stderr, "decoder error 0x%05x (%s) at byte offset %u\n",
	  stream->error, mad_stream_errorstr(stream),
	  stream->this_frame - state->mp3data);

  /* XXX check maximum number of resyncs */
  if (unix_check_fd_read(state->cmd_fd)) {
    if (mp3dec_child_read_cmd(state) < 0) {
      state->state = CHILD_ERROR;
      return MAD_FLOW_BREAK;
    }
  }
  
  return MAD_FLOW_CONTINUE;
}

static int mp3dec_child_read_cmd(child_state_t *state) {
  mp3dec_cmd_e cmd;
  unsigned char buf[CMD_BUF_SIZE];
  unsigned int buflen;
  int ret;

  printf("reading cmd\n");
  ret = mp3dec_read_cmd(state->cmd_fd, &cmd,
			buf, &buflen, sizeof(buf),
			&state->error);
  printf("read: %d, %d\n", ret, cmd);
  if (ret < 0)
    return ret;

  switch (cmd) {
  case MP3DEC_COMMAND_PLAY: {
    switch (state->state) {
    case CHILD_ERROR:
      goto error;
    case CHILD_NONE:
      error_set(&state->error, "Cannot play: no track loaded");
      goto error;
    case CHILD_PAUSE:
    case CHILD_STOP:
      state->state = CHILD_PLAY;
      mp3dec_write_cmd(state->response_fd, MP3DEC_RESPONSE_ACK, NULL, 0, &state->error);
      mad_decoder_run(&state->decoder, MAD_DECODER_MODE_SYNC);
      return 0;
    default:
      error_printf(&state->error, "Cannot play: unknown state (%d)",
		   state->state);
      goto error;
    }
    break;
  }

  case MP3DEC_COMMAND_PAUSE: {
    if (state->state == CHILD_ERROR)
      goto error;
    
    if (state->state == CHILD_PLAY) {
      state->state = CHILD_PAUSE;
      mp3dec_write_cmd(state->response_fd, MP3DEC_RESPONSE_ACK, NULL, 0, &state->error);
      while (state->state == CHILD_PAUSE) {
	mp3dec_child_read_cmd(state);
      }
      return 0;
    } else if (state->state == CHILD_PAUSE) {
      state->state = CHILD_PLAY;
      goto ack;
    } else {
      error_printf(&state->error, "Cannot pause when in state %s",
		   mp3dec_child_state_str(state));
      goto error;
    }
  }

  case MP3DEC_COMMAND_EXIT: {
    mp3dec_write_cmd(state->response_fd, MP3DEC_RESPONSE_ACK, NULL, 0, &state->error);
    mp3dec_child_close(state);
    exit(0);
    break;
  }

  case MP3DEC_COMMAND_LOAD: {
    if (state->mp3_fd != -1) {
      close(state->mp3_fd);
      state->mp3_fd = -1;
    }
    
    assert(state->mp3_fd == -1);
    state->mp3_fd = open(buf, O_RDONLY); /* XXX receive string correctly */
    if (state->mp3_fd < 0) {
      error_printf_strerror(&state->error, "Could not open \"%s\"", buf);
      state->state = CHILD_ERROR;
      goto error;
    } else {
      if (state->state == CHILD_NONE)
	state->state = CHILD_STOP;
      strncpy(state->filename, buf, sizeof(state->filename));
      state->filename[sizeof(state->filename) - 1] = '\0';
      goto ack;
    }
    break;
  }

  case MP3DEC_COMMAND_STATUS: {
    error_set(&state->error, "STATUS not supported");
    goto error;
    break;
  }

  case MP3DEC_COMMAND_PING: {
    printf("pong\n");
    ret = mp3dec_write_cmd(state->response_fd, MP3DEC_RESPONSE_PONG,
			   buf, buflen, &state->error);
    if (ret < 0) {
      error_prepend(&state->error, "Could not send PONG");
      return -1;
    } else {
      return 0;
    }
    break;
  }

  default:
    error_set(&state->error, "Unknown command received");
    goto error;
  }

 ack:
  ret = mp3dec_write_cmd(state->response_fd, MP3DEC_RESPONSE_ACK, NULL, 0, &state->error);
  if (ret < 0) {
    error_prepend(&state->error, "Could not send ACK");
    return -1;
  } else {
    return 0;
  }
  
 error:
  ret = mp3dec_write_cmd_string(state->response_fd, MP3DEC_RESPONSE_ERR,
				error_get(&state->error),
				&state->error);

  if (ret < 0) {
    error_prepend(&state->error, "Could not send ERROR");
    return -1;
  } else {
    return 0;
  }
}

int mp3dec_child_main(int cmd_fd, int response_fd) {
  child_state_t state;
  mp3dec_child_reset(&state, cmd_fd, response_fd);

  for (;;) {
    if (unix_check_fd_read(cmd_fd)) {
      if (mp3dec_child_read_cmd(&state) < 0) {
	fprintf(stderr, "error reading cmd\n");
	return -1;
      }
    }
    printf("again\n");
    usleep(50 * 1000);
  }
}
