#include <sys/soundcard.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <mad.h>

#include "maddec.h"

/* error string handling */

char *mp3dec_error(mp3_state_t *state) {
  return state->strerror;
}

static void mp3dec_error_set(mp3_state_t *state, char *str) {
  strncpy(state->strerror, str, sizeof(state->strerror) - 1);
  state->strerror[sizeof(state->strerror) - 1] = '\0';
}

static void mp3dec_error_set_strerror(mp3_state_t *state, char *str) {
  snprintf(state->strerror, sizeof(state->strerror), "%s: %s", str, strerror(errno));
}

static void mp3dec_error_append(mp3_state_t *state, char *str) {
  unsigned char buf[4096];
  strncpy(buf, state->strerror, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  snprintf(state->strerror, sizeof(state->strerror), "%s: %s", buf, str);
}

static void mp3dec_error_prepend(mp3_state_t *state, char *str) {
  unsigned char buf[4096];
  strncpy(buf, state->strerror, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  snprintf(state->strerror, sizeof(state->strerror), "%s: %s", str, buf);
}

/* ahh, the joys of unix */
static int mp3dec_read(int mp3_fd, unsigned char *buf, unsigned int len) {
  int ret;
  unsigned char *ptr = buf;
  unsigned int left = len, total = 0;

  while (left > 0) {
  again:
    ret = read(mp3_fd, ptr, left);
    if (ret < 0) {
      if (ret != EINTR)
	return -1;
      else
	goto again;
    }
    
    if (ret == 0)
      return total;

    assert(ret <= left);
    total += ret;
    left -= ret;
    ptr += ret;
  }

  return total;
}

static int mp3dec_write(int snd_fd, unsigned char *buf, unsigned int len) {
  int ret;
  unsigned char *ptr = buf;
  unsigned int left = len, total = 0;

  while (left > 0) {
  again:
    ret = write(snd_fd, ptr, left);
    if (ret < 0) {
      if (ret != EINTR)
	return -1;
      else
	goto again;
    }
    
    if (ret == 0)
      return total;

    assert(ret <= left);
    total += ret;
    left -= ret;
    ptr += ret;
  }

  return total;
}

/* initializing and stuff */

void mp3dec_reset(mp3_state_t *state) {
  state->channels = 0;
  state->little_endian = 0;
  state->pcmlen = 0;
  state->snd_fd = -1;
  state->mp3_fd = -1;
  state->snd_initialized = 0;

  memset(state->pcm_l, 0, sizeof(state->pcm_l));
  memset(state->pcm_r, 0, sizeof(state->pcm_r));
  memset(state->strerror, 0, sizeof(state->strerror));
}

void mp3dec_close(mp3_state_t *state) {
  mad_decoder_finish(&state->decoder);
  
  if (state->snd_fd >= 0) {
    close(state->snd_fd);
    state->snd_fd = -1;
  }
  if (state->mp3_fd >= 0) {
    close(state->mp3_fd);
    state->mp3_fd = -1;
  }

  mp3dec_reset(state);
}

/* open the soundcard, and set parameters from the mp3 header */
static int mp3dec_audio_init(mp3_state_t *state) {
  int ret = 0;
  int fmts;
  int rate;
  int channels;

  if (state->snd_fd < 0) {
    state->snd_fd = open("/dev/dsp", O_RDWR);
    if (state->snd_fd < 0) {
      mp3dec_error_set_strerror(state, "Could not open sound device");
      return -1;
    }
  }
    
  ret = ioctl(state->snd_fd, SNDCTL_DSP_RESET, NULL);
  if (ret < 0) {
    mp3dec_error_set_strerror(state, "Could not reset audio");
    goto error;
  }

  /* x86 only for now XXX */
  fmts = AFMT_S16_NE;
  ret = ioctl(state->snd_fd, SNDCTL_DSP_SETFMT, &fmts);
  if ((fmts != AFMT_S16_NE) || (ret < 0)) {
    mp3dec_error_set_strerror(state, "Could not set format");
    goto error;
  }
  
  channels = state->channels = state->nchannels;
  ret = ioctl(state->snd_fd, SNDCTL_DSP_STEREO, &channels);
  if (ret < 0) {
    mp3dec_error_set_strerror(state, "Could not set stereo mode");
    goto error;
  }

  rate = state->samplerate;
  ret = ioctl(state->snd_fd, SNDCTL_DSP_SPEED, &rate);
  if (ret < 0) {
    mp3dec_error_set_strerror(state, "Could not set samplerate");
    goto error;
  }

  return 0;
  
 error:
  close(state->snd_fd);
  state->snd_fd = -1;
  return -1;
}

/* write data to the audio card, converting the samples on the way */
static int mp3dec_audio_write(mp3_state_t *state) {
  unsigned char audio_buf[sizeof(state->pcm_l) + sizeof(state->pcm_r)];
  unsigned int len = 0;
  unsigned char *ptr = NULL;

  ptr = audio_buf;
  if (state->little_endian) {
    int i;
    for (i = 0; i < state->pcmlen; i++) {
      /* right channel first ?? XXX */
      *ptr++ = (state->pcm_r[i]& 0xFF);
      *ptr++ = ((state->pcm_r[i] >> 8) & 0xFF);
      if (state->channels == 2) {
	*ptr++ = (state->pcm_l[i]& 0xFF);
	*ptr++ = ((state->pcm_l[i] >> 8) & 0xFF);
      }
    } 
  } else {
    int i;
    for (i = 0; i < state->pcmlen; i++) {
      /* XXX ?? */
      *ptr++ = ((state->pcm_r[i] >> 8) & 0xFF);
      *ptr++ = (state->pcm_r[i]& 0xFF);
      if (state->channels == 2) {
	*ptr++ = ((state->pcm_l[i] >> 8) & 0xFF);
	*ptr++ = (state->pcm_l[i]& 0xFF);
      }
    }
  }

  len = ptr - audio_buf;
  if (mp3dec_write(state->snd_fd, audio_buf, len) != len) {
    mp3dec_error_set_strerror(state, "Could not write audio data to soundcard");
    return -1;
  }

  state->pcmlen = 0;

  return 0;
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
  mp3_state_t *state = data;

  if (state->length == 0)
    return MAD_FLOW_STOP;

  mad_stream_buffer(stream, state->buffer, state->length);
  state->length = 0;

  return MAD_FLOW_CONTINUE;
}

static
enum mad_flow mad_output(void *data, struct mad_header const *header,
			 struct mad_pcm *pcm) {
  mp3_state_t *state = data;
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
      return MAD_FLOW_STOP;
  }

  ptr = audio_buf;
  while (nsamples--) {
    signed int sample;

    sample = mad_scale(*left_ch++);
    *ptr++ = (sample & 0xFF);
    *ptr++ = ((sample >> 8) & 0xFF);
    
    if (nchannels == 2) {
      sample = mad_scale(*right_ch++);
      *ptr++ = (sample & 0xFF);
      *ptr++ = ((sample >> 8) & 0xFF);
    }
  }

  len = ptr - audio_buf;
  printf("write %d bytes\n", len);
  if (mp3dec_write(state->snd_fd, audio_buf, len) != len) {
    mp3dec_error_set_strerror(state, "Could not write audio data to soundcard");
    return MAD_FLOW_STOP;
  } else {
    return MAD_FLOW_CONTINUE;
  }
}

static
enum mad_flow mad_error(void *data, struct mad_stream *stream,
			struct mad_frame *frame) {
  mp3_state_t *state = data;

  fprintf(stderr, "decoder error 0x%05x (%s) at byte offset %u\n",
	  stream->error, mad_stream_errorstr(stream),
	  stream->this_frame);

  return MAD_FLOW_CONTINUE;
}


int mp3dec_decode_file(mp3_state_t *state, char *filename) {
 int retval = 0;
 struct stat stat;

  if (state->snd_fd >= 0)
    close(state->snd_fd);
  
  assert(state->mp3_fd == -1);

  state->mp3_fd = open(filename, O_RDONLY);
  if (state->mp3_fd < 0) {
    mp3dec_error_set_strerror(state, "Could not open the mp3 file");
    return -1;
  }

  if ((fstat(state->mp3_fd, &stat) == -1) ||
      (stat.st_size == 0)) {
    mp3dec_error_set_strerror(state, "Could not stat the mp3 file");
    goto exit;
  }

  state->length = stat.st_size;
  
  state->buffer = mmap(0, state->length,
		       PROT_READ, MAP_SHARED, state->mp3_fd, 0);
  if (state->buffer == MAP_FAILED) {
    mp3dec_error_set_strerror(state, "Could not mmap the mp3 file");
    goto exit;
  }

  mad_decoder_init(&state->decoder, state,
		   mad_input, 0, 0,
		   mad_output,
		   mad_error, 0);

  retval = mad_decoder_run(&state->decoder, MAD_DECODER_MODE_SYNC);

  if (munmap(state->buffer, state->length) == -1) {
    mp3dec_error_set_strerror(state, "Could not unmap the mp3 file");
  }

  mad_decoder_finish(&state->decoder);

 exit:
  close(state->mp3_fd);
  state->mp3_fd = -1;
  return retval;
}

