#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "error.h"

/* error string handling */

void mp3dec_error_reset(mp3dec_error_t *error) {
  memset(error->strerror, 0, sizeof(error->strerror));
}

char *mp3dec_error(mp3dec_error_t *error) {
  return error->strerror;
}

void mp3dec_error_set(mp3dec_error_t *error, char *str) {
  strncpy(error->strerror, str, sizeof(error->strerror) - 1);
  error->strerror[sizeof(error->strerror) - 1] = '\0';
}

void mp3dec_error_set_strerror(mp3dec_error_t *error, char *str) {
  snprintf(error->strerror, sizeof(error->strerror), "%s: %s", str, strerror(errno));
}

void mp3dec_error_append(mp3dec_error_t *error, char *str) {
  unsigned char buf[4096];
  strncpy(buf, error->strerror, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  snprintf(error->strerror, sizeof(error->strerror), "%s: %s", buf, str);
}

void mp3dec_error_prepend(mp3dec_error_t *error, char *str) {
  unsigned char buf[4096];
  strncpy(buf, error->strerror, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  snprintf(error->strerror, sizeof(error->strerror), "%s: %s", str, buf);
}
