/*
 * ripped from audio_macosx.c from mpg123, originally by
 * guillaume.outters@free.fr
 */

#include <CoreAudio/AudioHardware.h>
#include <stdlib.h>
#include <stdio.h>

#include <mad.h>

#include "error.h"
#include "audio.h"

typedef struct audio_s {
} audio_t;

int audio_write(struct mad_pcm *pcm, error_t *error) {
}
