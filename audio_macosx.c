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
#include <semaphore.h>

#include <mad.h>

#include "error.h"
#include "audio.h"

#define AUDIO_BUFFER_SIZE 1152 * 2

typdef struct audio_buffer_s {
  floar *ptr;
} audio_buffer_t;

typedef struct audio_s {
  AudioDeviceID device;
  pthread_mutex_t mutex;
  pthread_cond_t  cond;

  float buffer[AUDIO_BUFFER_SIZE];
  unsigned long buffer_count;
  unsigned long buffer_start;
} audio_t;

static audio_t audio;

/* audio_play_proc has to be thread safe */
static OSStatus audio_play_proc(AudioDeviceID inDevice,
                                const AudioTimeStamp *inNow,
                                const AudioBufferList *inInputData,
                                const AudioTimeStamp *inInputTime,
                                AudioBufferList *outOutputData,
                                const AudioTimeStamp *inOutputTime,
                                void *inClientData) {
  pthread_mutex_lock(&audio.mutex);

  float *out_ptr = (float *)outOutputData;
  float *in_ptr  = (float *)audio.buffer + audio.buffer_start;
  unsigned long bytes_left = sizeof(audio.buffer);
  /* not enough data, wait... */
  if (audio.buffer_count < bytes_left) {
    memset(ptr, 0, bytes_left);
    pthread_mutex_unlock(&audio.mutex);
    return 0;
  }

  bytes_left = min(bytes_left, audio.buffer_count);
  unsigned long samples_left = bytes_left / sizeof(*in_ptr);

  while (samples_left--) {
    *out_ptr++ = mad_scale_float(*in_ptr++);
  }

  pthread_cond_signal(&audio.cond);
  pthread_mutex_unlock(&audio.mutex);

  return 0;
}

static int audio_init(error_t *error) {
  int size;
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
  if (device == kAudioDeviceUnknown) {
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

  /* set the buffer size */
  size = sizeof(byte_count);
  ret = AudioDeviceGetProperty(audio.device, 0, false,
                               kAudioDevicePropertyBufferSize,
                               &size, &byte_count);
  if (ret) {
    error_set("Could not get the buffer size");
    return 0;
  }

  ret = pthread_mutex_init(&audio.mutex, NULL);
  if (ret) {
    error_set("Could not initialize mutex");
    return 0;
  }
  ret = pthread_cond_init(&audio.cond, NULL);
  if (ret) {
    error_set("Could not initialize condition variable");
    return 0;
  }

  ret = AudioDeviceAddIOProc(audio.device, audio_play_proc, NULL);
  if (ret) {
    error_set("Could not start the IO proc");
    return 0;
  }
}

int audio_write(struct mad_pcm *pcm, error_t *error) {
}
