#include <sys/types.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <mad.h>

#include "error.h"
#include "audio.h"

typedef struct audio_s {
  int snd_fd;
  unsigned int channels;
  unsigned int samplerate;
} audio_t;

static audio_t audio;
static int audio_initialized = 0;

static int audio_set_params(audio_t *audio,
                            unsigned int channels,
                            unsigned int samplerate,
                            error_t *error) {
  int ret = 0;
  int fmts;
  unsigned int tchannels;
  unsigned int tsamplerate;

  ret = ioctl(audio->snd_fd, SNDCTL_DSP_RESET, NULL);
  if (ret < 0) {
    error_set_strerror(error, "Could not reset audio");
    goto error;
  }

  /* 16 bits big endian for now */
  fmts = AFMT_S16_NE;
  ret = ioctl(audio->snd_fd, SNDCTL_DSP_SETFMT, &fmts);
  if ((fmts != AFMT_S16_NE) || (ret < 0)) {
    error_set_strerror(error, "Could not set format");
    return 0;
  }

  tchannels = channels;
  ret = ioctl(audio->snd_fd, SNDCTL_DSP_STEREO, &tchannels);
  if (ret < 0) {
    error_printf_strerror(error, "Could not enable %d channels",
			  channels);
    return 0;
  }
  audio.channels = channels;

  tsamplerate = audio->samplerate;
  ret = ioctl(audio->snd_fd, SNDCTL_DSP_SPEED, &tsamplerate);
  if (ret < 0) {
    error_printf_strerror(error, "Could not set samplerate of %d hz",
			  samplerate);
    return 0;
  }
  audio.samplerate = samplerate;
  
  return 1;
}

/* open the soundcard, and set parameters from the mp3 header */
static int audio_init(unsigned int channels,
                       unsigned int samplerate,
                       error_t *error) {
  audio.snd_fd = open("/dev/dsp", O_RDWR);
  if (audio.snd_fd < 0) {
    error_set_strerror(error, "Could not open sound device");
    return 0;
  }

  if (!audio_set_params(channels, samplerate, error))
    goto error;
  
  return 1;
  
 error:
  close(audio.snd_fd);
  audio.snd_fd = -1;
  return 0;
}

int audio_write(struct mad_pcm *pcm, error_t *error) {
  unsigned char audio_buf[1500000];
  unsigned int nchannels, nsamples;
  mad_fixed_t const *left_ch, *right_ch;
  unsigned int len = 0;
  unsigned char *ptr = NULL;
  int ret;

  nchannels = pcm->channels;
  nsamples  = pcm->length;
  left_ch   = pcm->samples[0];
  right_ch  = pcm->samples[1];

  if (!audio_initialized) {
    if (!audio_init(nchannels, nsamples, error))
      return 0;
  }
  
  if ((nchannels != audio->channels) ||
      (pcm->samplerate != audio->samplerate)) {
    if (!audio_set_params(nchannels, samplerate, error)) {
      error_prepend(error, "Could not reset audio");
      return 0;
    }
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

  ret = unix_write(audio.snd_fd, audio_buf, len);
  if (ret < 0) {
    error_set(error, "Error while writing audio data");
    return 0;
  } else if (ret != len) {
    error_set(error, "Could not write all the data to the soundcard");
    return 0;
  } else {
    return 1;
  }
}

void audio_close(void) {
  if (audio.snd_fd != -1)
    close(audio.snd_fd);
  audio.snd_fd = -1;
  audio_initialized = 0;
}

