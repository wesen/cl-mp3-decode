#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "error.h"

/* error string handling */

void error_reset(error_t *error) {
  memset(error->strerror, 0, sizeof(error->strerror));
}

char *error_get(error_t *error) {
  return error->strerror;
}

void error_set(error_t *error, char *str) {
  strncpy(error->strerror, str, sizeof(error->strerror) - 1);
  error->strerror[sizeof(error->strerror) - 1] = '\0';
}

void error_set_strerror(error_t *error, char *str) {
  snprintf(error->strerror, sizeof(error->strerror), "%s: %s", str, strerror(errno));
}

void error_append(error_t *error, char *str) {
  unsigned char buf[4096];
  strncpy(buf, error->strerror, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  snprintf(error->strerror, sizeof(error->strerror), "%s: %s", buf, str);
}

void error_prepend(error_t *error, char *str) {
  unsigned char buf[4096];
  strncpy(buf, error->strerror, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  snprintf(error->strerror, sizeof(error->strerror), "%s: %s", str, buf);
}
