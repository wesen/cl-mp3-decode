#include <stdio.h>
#include <string.h>

#include <mad.h>

#include "error.h"
#include "audio.h"

int main(void) {
  error_t error;

  struct mad_pcm pcm;
  memset(pcm.samples, 0, sizeof(mad_fixed_t) * 2 * 1152);

  int i;
  for (i = 0; i < 10; i++) {
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
