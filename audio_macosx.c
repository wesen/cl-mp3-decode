/*
 * macosx audio output
 *
 * (c) 2005 bl0rg.net
 *
 * ripped from audio_macosx.c from mpg123, originally by
 * guillaume.outters@free.fr, and from libao macosx plugin
 * by Timothy Wood
 */

#include <CoreAudio/AudioHardware.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <mad.h>

#include "audio_macosx_rb.h"
#include "error.h"
#include "audio.h"

#define AUDIO_BUFFER_SIZE 1152 * 2

typedef struct audio_s {
  AudioDeviceID device;

  unsigned long channels;
  unsigned long samplerate;
  
  rb_t rb;
} audio_t;

static audio_t audio;
static int audio_initialized = 0;
static int audio_started = 0;

/* audio_play_proc has to be thread safe */
static OSStatus audio_play_proc(AudioDeviceID inDevice,
                                const AudioTimeStamp *inNow,
                                const AudioBufferList *inInputData,
                                const AudioTimeStamp *inInputTime,
                                AudioBufferList *outOutputData,
                                const AudioTimeStamp *inOutputTime,
                                void *inClientData) {
  int i;
  for(i = 0; i < outOutputData->mNumberBuffers; i++) {
    AudioBuffer *buffer = outOutputData->mBuffers + i;

    /*
      printf("handling data %d, size %d, channels %d\n",
      i, buffer->mDataByteSize / sizeof(float),
      buffer->mNumberChannels);
    */

    if ((buffer->mDataByteSize / sizeof(float)) != (1152 * 2)) {
      printf("Incorrect buffer size: %d\n", buffer->mDataByteSize / sizeof(float));
      continue;
    }

    int ret;
    ret = rb_dequeue(&audio.rb, buffer->mData, 1152 * 2);
    if (ret == 0) {
      printf("Could not dequeue\n");
      memset(buffer->mData, 0, 1152 * 2 * sizeof(float));
    }
  }

  return 0;
}

static int audio_init(error_t *error) {
  UInt32 size;
  int ret;
  AudioStreamBasicDescription format;
  UInt32 byte_count;

  /* get device */
  size = sizeof(audio.device);
  ret = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
                                 &size, &audio.device);
  if (ret != 0) {
    error_set(error, "Could not get default audio device");
    return 0;
  }
  if (audio.device == kAudioDeviceUnknown) {
    error_set(error, "Unknown audio device");
    return 0;
  }

  /* check that the format is pcm */
  size = sizeof(format);
  ret = AudioDeviceGetProperty(audio.device, 0, false,
                               kAudioDevicePropertyStreamFormat,
                               &size, &format);
  if (ret != 0) {
    error_set(error, "Could not get the stream format");
    return 0;
  }
  if (format.mFormatID != kAudioFormatLinearPCM) {
    error_set(error, "The output device is not using PCM format");
    return 0;
  }

  /* set the buffer size, channels, samplerate */
  /* XXX channels, samplerate */
  size = sizeof(byte_count);
  ret = AudioDeviceGetProperty(audio.device, 0, false,
                               kAudioDevicePropertyBufferSize,
                               &size, &byte_count);
  if (ret) {
    error_set(error, "Could not get the buffer size");
    return 0;
  }
  byte_count = 1152 * 2 * sizeof(float);
  ret = AudioDeviceSetProperty(audio.device, NULL, 0, false,
                               kAudioDevicePropertyBufferSize,
                               size, &byte_count);
  if (ret) {
    error_set(error, "Could not set the buffer size");
    return 0;
  }

  /* initialize the ring buffer */
  rb_init(&audio.rb);
  
  ret = AudioDeviceAddIOProc(audio.device, audio_play_proc, NULL);
  if (ret) {
    error_set(error, "Could not start the IO proc");
    return 0;
  }

  audio_initialized = 1;

  return 1;
}

static inline
float mad_scale_float(mad_fixed_t sample) {
  return (float)(sample/(float)(1L << MAD_F_FRACBITS));
}

int audio_write(struct mad_pcm *pcm, error_t *error) {
  if (!audio_initialized) {
    audio.channels = pcm->channels;
    audio.samplerate = pcm->samplerate;
    if (!audio_init(error)) {
      error_prepend(error, "Could not initialize audio");
      return 0;
    }
  }

  if ((audio.channels != pcm->channels) ||
      (audio.samplerate != pcm->samplerate)) {
    /* XXX */
    error_set(error, "Changing the audio parameters is not supported");
    return 0;
  }

  if (pcm->length != 1152) {
    error_printf(error, "Unknown number of samples in the mad buffer: %d",
                 pcm->length);
    return 0;
  }

  if (pcm->channels != 2) {
    error_set(error, "Only stereo PCM data supported");
    return 0;
  }

  int ret;
  float buf[1152 * pcm->channels];
  float *ptr = buf;
  int i;
  mad_fixed_t const *left_ch, *right_ch;
  left_ch  = pcm->samples[0];
  right_ch = pcm->samples[1];
  for (i = 0; i < pcm->length; i++) {
    *ptr++ = mad_scale_float(*left_ch++);
    *ptr++ = mad_scale_float(*right_ch++);
  }
  ret = rb_enqueue(&audio.rb, buf, 1152 * pcm->channels);

  if (ret == 0) {
    error_set(error, "Could not enqueue the PCM samples");
    return 0;
  }

  if (!audio_started) {
    printf("start audio\n");
    ret = AudioDeviceStart(audio.device, audio_play_proc);
    if (ret) {
      error_set(error, "Could not start the audio playback");
      return 0;
    }
    audio_started = 1;
  }

  return 1;
}

int audio_close(error_t *error) {
  int ret;
  if (audio_started) {
    ret = AudioDeviceStop(audio.device, audio_play_proc);
    if (ret) {
      error_set(error, "Could not stop audio playback");
      return 0;
    }
    audio_started = 0;
  }
  if (audio_initialized) {
    ret = AudioDeviceRemoveIOProc(audio.device, audio_play_proc);
    if (ret) {
      error_set(error, "Could not remove the IOProc");
      return 0;
    }
    audio_initialized = 0;
  }
  rb_destroy(&audio.rb);

  return 1;
}
