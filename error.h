#ifndef ERROR_H__
#define ERROR_H__

#define ERROR_STRING_SIZE 256

typedef struct error_s {
  char strerror[ERROR_STRING_SIZE];
} error_t;

void error_reset(error_t *error);
char *error_get(error_t *error);
void error_set(error_t *error, char *str);
void error_set_strerror(error_t *error, char *str);
void error_append(error_t *error, char *str);
void error_prepend(error_t *error, char *str);

#endif /* ERROR_H__ */
