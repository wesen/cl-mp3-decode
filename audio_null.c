#include <stdio.h>

#include <mad.h>

#include "error.h"
#include "audio.h"

int audio_write(struct mad_pcm *pcm, error_t *error) {
  printf("Asked to write %d samples on %d channels\n",
         pcm->length, pcm->channels);
  return 1;
}

void audio_close(void) {
  printf("Closing audio\n");
}
