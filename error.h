#ifndef ERROR_H__
#define ERROR_H__

#define ERROR_STRING_SIZE 256

typedef struct mp3dec_error_s {
  char strerror[ERROR_STRING_SIZE];
} mp3dec_error_t;

void mp3dec_error_reset(mp3dec_error_t *error);
char *mp3dec_error(mp3dec_error_t *error);
void mp3dec_error_set(mp3dec_error_t *error, char *str);
void mp3dec_error_set_strerror(mp3dec_error_t *error, char *str);
void mp3dec_error_append(mp3dec_error_t *error, char *str);
void mp3dec_error_prepend(mp3dec_error_t *error, char *str);

#endif /* ERROR_H__ */
