#include <sys/soundcard.h>
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
#include "error.h"
#include "misc.h"

/* commands:
   - load file
   - play
   - stop
   - status
   - pause
*/

static enum mad_flow mad_input(void *data, struct mad_stream *stream);
static enum mad_flow mad_output(void *data, struct mad_header const *header, struct mad_pcm *pcm);
static enum mad_flow mad_error(void *data, struct mad_stream *stream, struct mad_frame *frame);

/* initializing and stuff */
void mp3dec_child_reset(child_state_t *state, int cmd_fd, int response_fd) {
  state->cmd_fd = cmd_fd;
  state->response_fd = response_fd;
  
  mp3dec_error_reset(&state->error);
  
  state->nchannels = 0;
  state->samplerate = 0;
  state->snd_fd = -1;
  state->snd_initialized = 0;

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

void mp3dec_child_close(child_state_t *state) {
  if (state->mad_initialized) {
    mad_decoder_finish(&state->decoder);
    state->mad_initialized = 0;
  }
  
  if (state->snd_fd != -1) {
    close(state->snd_fd);
    state->snd_fd = -1;
    state->snd_initialized = 0;
  }

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

/* open the soundcard, and set parameters from the mp3 header */
static int mp3dec_audio_init(child_state_t *state) {
  int ret = 0;
  int fmts;
  int rate;
  int channels;

  if (state->snd_fd < 0) {
    state->snd_fd = open("/dev/dsp", O_RDWR);
    if (state->snd_fd < 0) {
      mp3dec_error_set_strerror(&state->error, "Could not open sound device");
      return -1;
    }
  }
    
  ret = ioctl(state->snd_fd, SNDCTL_DSP_RESET, NULL);
  if (ret < 0) {
    mp3dec_error_set_strerror(&state->error, "Could not reset audio");
    goto error;
  }

  /* 16 bits big endian for now */
  fmts = AFMT_S16_NE;
  ret = ioctl(state->snd_fd, SNDCTL_DSP_SETFMT, &fmts);
  if ((fmts != AFMT_S16_NE) || (ret < 0)) {
    mp3dec_error_set_strerror(&state->error, "Could not set format");
    goto error;
  }
  
  channels = state->nchannels;
  ret = ioctl(state->snd_fd, SNDCTL_DSP_STEREO, &channels);
  if (ret < 0) {
    mp3dec_error_set_strerror(&state->error, "Could not set stereo mode");
    goto error;
  }

  rate = state->samplerate;
  ret = ioctl(state->snd_fd, SNDCTL_DSP_SPEED, &rate);
  if (ret < 0) {
    mp3dec_error_set_strerror(&state->error, "Could not set samplerate");
    goto error;
  }

  state->snd_initialized = 1;

  return 0;
  
 error:
  close(state->snd_fd);
  state->snd_fd = -1;
  return -1;
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

  if (state->mp3eof) {
    return MAD_FLOW_STOP;
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
    mp3dec_error_set_strerror(&state->error, "Could not read from the mp3 file");
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
  unsigned char audio_buf[1500000];
  unsigned int nchannels, nsamples;
  mad_fixed_t const *left_ch, *right_ch;
  unsigned int len = 0;
  unsigned char *ptr = NULL;

  nchannels = pcm->channels;
  nsamples  = pcm->length;
  left_ch   = pcm->samples[0];
  right_ch  = pcm->samples[1];

  if (!state->snd_initialized ||
      (nchannels != state->nchannels) ||
      (pcm->samplerate != state->samplerate)) {
    state->nchannels = nchannels;
    state->samplerate = pcm->samplerate;
    if (mp3dec_audio_init(state) < 0)
      return MAD_FLOW_BREAK;
    state->snd_initialized = 1;
  }

  ptr = audio_buf;
  while (nsamples--) {
    signed int sample;

    sample = mad_scale(*left_ch++);
    *ptr++ = (sample& 0xFF);
    *ptr++ = ((sample >> 8) & 0xFF);
    
    if (nchannels == 2) {
      sample = mad_scale(*right_ch++);
      *ptr++ = (sample& 0xFF);
      *ptr++ = ((sample >> 8) & 0xFF);
    }
  }

  len = ptr - audio_buf;
  if (unix_write(state->snd_fd, audio_buf, len) != len) {
    mp3dec_error_set_strerror(&state->error, "Could not write audio data to soundcard");
    return MAD_FLOW_BREAK;
  } else {
    return MAD_FLOW_CONTINUE;
  }
}

static
enum mad_flow mad_error(void *data, struct mad_stream *stream,
			struct mad_frame *frame) {
  child_state_t *state = data;

  fprintf(stderr, "decoder error 0x%05x (%s) at byte offset %u\n",
	  stream->error, mad_stream_errorstr(stream),
	  stream->this_frame - state->mp3data);

  return MAD_FLOW_CONTINUE;
}

int mp3dec_child_read_cmd(child_state_t *state) {
  mp3dec_cmd_e cmd;
  unsigned char buf[CMD_BUF_SIZE];
  unsigned int buflen;
  int ret;

  ret = mp3dec_read_cmd(state->cmd_fd, &cmd,
			buf, &buflen, sizeof(buf),
			&state->error);
  if (ret < 0)
    return ret;

  switch (cmd) {
  case MP3DEC_COMMAND_PLAY:
    break;

  case MP3DEC_COMMAND_PAUSE:
    break;

  case MP3DEC_COMMAND_GET_ERROR:
    break;

  case MP3DEC_COMMAND_LOAD:
    break;

  case MP3DEC_COMMAND_STATUS:
    break;

  case MP3DEC_COMMAND_PING: {
    ret = mp3dec_write_cmd(state->response_fd, MP3DEC_COMMAND_PONG,
			   buf, buflen, &state->error);
    if (ret < 0) {
      mp3dec_error_prepend(&state->error, "Could not send PONG");
      return -1;
    }
  }
    break;

  case MP3DEC_COMMAND_PONG:
  default:
    mp3dec_error_set(&state->error, "Unknown command received");
    return -1;
  }

  return 0;
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
    usleep(50 * 1000);
  }
}

#if  0 
int mp3dec_decode_file(mp3dec_state_t *state, char *filename) {
  int retval = 0;
  
  if (state->snd_fd >= 0) {
    close(state->snd_fd);
    state->snd_fd = -1;
  }

  assert(state->mp3_fd == -1);

  state->mp3_fd = open(filename, O_RDONLY);
  if (state->mp3_fd < 0) {
    mp3dec_error_set_strerror(state, "Could not open the mp3 file");
    return -1;
  }

  retval = mad_decoder_run(&state->decoder, MAD_DECODER_MODE_SYNC);

  close(state->mp3_fd);
  state->mp3_fd = -1;

  return retval;
}

#endif
