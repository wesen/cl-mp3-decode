/*
 * (Thread-safe) ring-buffer skeleton
 *
 * (c) 2005 bl0rg.net
 */

#ifndef RB_H__
#define RB_H__

#define USE_PTHREAD

#ifdef USE_PTHREAD
#include <pthread.h>
#endif

#ifndef countof
#define countof(i) (sizeof(i) / (sizeof((i)[0])))
#endif

typedef float rb_elt_t;

#define RB_COUNT 16
#define RB_SIZE 1152 * 2 * RB_COUNT

typedef struct rb_s {
  rb_elt_t buf[RB_SIZE];
  unsigned long start;
  unsigned long count;

#ifdef USE_PTHREAD
  pthread_mutex_t mutex;
  pthread_cond_t  cond;
#endif
} rb_t;

void rb_init(rb_t *rb);
int  rb_enqueue(rb_t *rb, rb_elt_t *data, unsigned long count);
int  rb_dequeue(rb_t *rb, rb_elt_t *dest, unsigned long count);
void rb_destroy(rb_t *rb);

#ifdef USE_PTHREAD
void rb_wait(rb_t *rb, unsigned long count);
#endif

#endif /* RB_H__ */
