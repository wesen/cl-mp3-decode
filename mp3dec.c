#include <sys/soundcard.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/ioctl.h>

#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <lame/lame.h>

#include "mp3dec.h"

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

mp3_state_t *mp3dec_new(void) {
  mp3_state_t *state = malloc(sizeof(*state));
  if (state == NULL)
    return NULL;
  
  state->channels = 0;
  state->little_endian = 0;
  state->pcmlen = 0;
  state->snd_fd = -1;
  state->mp3_fd = -1;
  state->initialized = 0;
  state->lame_initialized = 0;

  memset(state->pcm_l, 0, sizeof(state->pcm_l));
  memset(state->pcm_r, 0, sizeof(state->pcm_r));
  memset(state->strerror, 0, sizeof(state->strerror));

  return state;
}

void mp3dec_close(mp3_state_t *state) {
  if (state->lame_initialized) {
    lame_decode_exit();
  }

  if (state->snd_fd >= 0) {
    close(state->snd_fd);
    state->snd_fd = -1;
  }
  if (state->mp3_fd >= 0) {
    close(state->mp3_fd);
    state->mp3_fd = -1;
  }

  free(state);
}

/* open the soundcard, and set parameters from the mp3 header */
static int mp3dec_audio_init(mp3_state_t *state, mp3data_struct *mp3data) {
  int ret = 0;
  int fmts;
  int rate;
  int channels;

  assert(state->snd_fd == -1);
  
  state->snd_fd = open("/dev/dsp", O_RDWR);
  if (state->snd_fd < 0) {
    mp3dec_error_set_strerror(state, "Could not open sound device");
    return -1;
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
  
  channels = state->channels = mp3data->stereo;
  ret = ioctl(state->snd_fd, SNDCTL_DSP_STEREO, &channels);
  if (ret < 0) {
    mp3dec_error_set_strerror(state, "Could not set stereo mode");
    goto error;
  }

  rate = mp3data->samplerate;
  ret = ioctl(state->snd_fd, SNDCTL_DSP_SPEED, &rate);
  if (ret < 0) {
    mp3dec_error_set_strerror(state, "Could not set samplerate");
    goto error;
  }

  state->initialized = 1;
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
  {
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
  }

  len = ptr - audio_buf;
  if (mp3dec_write(state->snd_fd, audio_buf, len) != len) {
    mp3dec_error_set_strerror(state, "Could not write audio data to soundcard");
    return -1;
  }

  state->pcmlen = 0;

  return 0;
}

/* decode the audio data in the buffer */
int mp3dec_decode_data(mp3_state_t *state, unsigned char *buf, unsigned int len) {
  if (!state->lame_initialized) {
    unsigned short bla = 0x1234;
    unsigned char *ptr = (unsigned char *)&bla;
    int ret;

    ret = lame_decode_init();
    if (ret < 0) {
      mp3dec_error_set(state, "Could not initialize LAME library");
      return -1;
    }
    if (ptr[0] == 0x12) {
      state->little_endian = 0;
    } else {
      state->little_endian = 1;
    }
    state->lame_initialized = 1;
  }

  if (!state->initialized) {
    mp3data_struct mp3data;
    int nout;

    nout = lame_decode1_headers(buf, len, state->pcm_l, state->pcm_r, &mp3data);
    if (nout < 0) {
      mp3dec_error_set(state, "Decoding error");
      return -1;
    } else if (nout == 0) {
      return 0;
    } else {
      int ret = 0;
      
      state->pcmlen = nout;
      ret = mp3dec_audio_init(state, &mp3data);
      if (ret < 0) {
	mp3dec_error_prepend(state, "Could not initialize audio");
	return -1;
      }
      ret = mp3dec_audio_write(state);
      if (ret < 0) {
	mp3dec_error_prepend(state,
			     "Could not write decoded audio data to the soundcard");
	return -1;
      }
    }
  } else {
    int nout;

    assert(state->pcmlen == 0);
    nout = lame_decode(buf, len, state->pcm_l, state->pcm_r);
    if (nout < 0) {
      mp3dec_error_set(state, "Decoding error");
      return -1;
    } else if (nout == 0) {
      return 0;
    } else {
      int ret = 0;
      state->pcmlen = nout;
      ret = mp3dec_audio_write(state);
      if (ret < 0) {
	mp3dec_error_prepend(state,
			     "Could not write decoded audio data to the soundcard");
	return -1;
      }
    }
  }

  return 0;
}

int mp3dec_decode_file(mp3_state_t *state, char *filename) {
  unsigned char mp3data[BUF_SIZE];
  int ret = 0, retval = 0;

  if (state->initialized)
    mp3dec_close(state);
  
  assert(state->mp3_fd == -1);

  state->mp3_fd = open(filename, O_RDONLY);
  if (state->mp3_fd < 0) {
    mp3dec_error_set_strerror(state, "Could not open the mp3 file");
    return -1;
  }

  for (;;) {
    ret = mp3dec_read(state->mp3_fd, mp3data, sizeof(mp3data));
    if (ret < 0) {
      mp3dec_error_set_strerror(state, "Could not read from the mp3 file");
      retval = -1;
      goto exit;
    } else if (ret == 0) {
      retval = 0;
      goto exit;
    } else {
      int mp3len = ret;
      ret = mp3dec_decode_data(state, mp3data, mp3len);
      if (ret < 0) {
	mp3dec_error_prepend(state, "Could not decode mp3 data");
	retval = -1;
	goto exit;
      }
    }
  }

 exit:
  close(state->mp3_fd);
  state->mp3_fd = -1;
  return retval;
}

