#include <stdio.h>
#include <string.h>

#include <mad.h>

#include "error.h"
#include "audio.h"

int main(void) {
  error_t error;

  struct mad_pcm pcm;
  int i;
  for (i = 0; i < 1152; i++)
    pcm.samples[0][i] = mad_f_fromint(i * 14);
  for (i = 0; i < 1152; i++)
    pcm.samples[1][i] = mad_f_fromint(i * 14);
  memset(pcm.samples, 0, sizeof(mad_fixed_t) * 2 * 1152);
  pcm.channels = 2;
  pcm.samplerate = 44100;
  pcm.length = 1152;

  for (i = 0; i < 100; i++) {
    printf("playing frame %d\n", i);
    int ret;
    ret = audio_write(&pcm, &error);
    if (!ret) {
      printf("Could not play frame %d: %s\n", i, error_get(&error));
      return 1;
    }
  }

  int ret = audio_close(&error);
  if (!ret) {
    printf("Could not close audio: %s\n", error_get(&error));
    return 1;
  }

  return 0;
}
