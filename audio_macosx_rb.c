/*
 * (Thread-safe) ring-buffer skeleton
 *
 * (c) 2005 bl0rg.net
 */

#include <assert.h>
#include <string.h>

#include "audio_macosx_rb.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

void rb_init(rb_t *rb) {
  rb->start = 0;
  rb->count = 0;
  memset(rb->buf, 0, sizeof(rb->buf));

#ifdef USE_PTHREAD
  int ret;
  ret = pthread_mutex_init(&rb->mutex, NULL);
  assert(ret == 0);
  ret = pthread_cond_init(&rb->cond, NULL);
  assert(ret == 0);
#endif /* USE_PTHREAD */
}

void rb_destroy(rb_t *rb) {
#ifdef USE_PTHREAD
  int ret;
  ret = pthread_cond_destroy(&rb->cond);
  assert(ret == 0);
  ret = pthread_mutex_destroy(&rb->mutex);
  assert(ret == 0);
#endif /* USE_PTHREAD */

  memset(rb->buf, 0, sizeof(rb->buf));
}

#ifdef USE_PTHREAD
/* call with rb->mutex held */
void rb_wait(rb_t *rb, unsigned long count) {
  int ret;
  unsigned long left;
  left = countof(rb->buf) - rb->count;
  while (left < count) {
    ret = pthread_cond_wait(&rb->cond, &rb->mutex);
    assert(ret == 0);
    left = countof(rb->buf) - rb->count;
  }
}
#endif

/* enqueue all the data pointed to by data. returns 1 on success, 0
   on failure.
*/
int rb_enqueue(rb_t *rb, rb_elt_t *data, unsigned long count) {
  int retval = 1;
  
#ifdef USE_PTHREAD
  int ret;
  ret = pthread_mutex_lock(&rb->mutex);
  assert(ret == 0);
#endif

  unsigned long left;
  left = countof(rb->buf) - rb->count;
  
#ifdef USE_PTHREAD
  if (count > countof(rb->buf)) {
    retval = 0;
    goto exit;
  }
  rb_wait(rb, count);
  left = countof(rb->buf) - rb->count;
  assert(count <= left);
#else /* USE_PTHREAD */
  if (count > left) {
    retval = 0;
    goto exit;
  }
#endif /* !USE_PTHREAD */

  unsigned long end = (rb->start + rb->count) % countof(rb->buf);

  if ((end + count) > countof(rb->buf)) {
    unsigned long block_length;
    block_length = countof(rb->buf) - end;
    memcpy(rb->buf + end, data, block_length * sizeof(rb_elt_t));
    count -= block_length;
    rb->count += block_length;
    end = (rb->start + rb->count) % countof(rb->buf);
    assert(end == 0);
    data += block_length;
  }

  memcpy(rb->buf + end, data, count * sizeof(rb_elt_t));
  rb->count += count;
  count -= count;

  assert(count == 0);
  assert(rb->count <= countof(rb->buf));

 exit:
#ifdef USE_PTHREAD
  ret = pthread_mutex_unlock(&rb->mutex);
  assert(ret == 0);
#endif

  return retval;
}

int rb_dequeue(rb_t *rb, rb_elt_t *dest, unsigned long count) {
  int retval = 1;
  
#ifdef USE_PTHREAD
  int ret;
  ret = pthread_mutex_lock(&rb->mutex);
  assert(ret == 0);
#endif

  if (rb->count < count) {
    retval = 0;
    goto exit;
  }

  unsigned long block_length;
  block_length = min(count, countof(rb->buf) - rb->start);
  memcpy(dest, rb->buf + rb->start, block_length * sizeof(rb_elt_t));
  count -= block_length;
  rb->count -= block_length;
  rb->start = (rb->start + block_length) % countof(rb->buf);

  if (count > 0) {
    assert(rb->start == 0);
    memcpy(dest + block_length, rb->buf, count * sizeof(rb_elt_t));
    rb->start += count;
    rb->count -= count;
  }
  assert(rb->start < countof(rb->buf));
  assert(rb->count >= 0);

 exit:
#ifdef USE_PTHREAD
  ret = pthread_cond_signal(&rb->cond);
  assert(ret == 0);
  ret = pthread_mutex_unlock(&rb->mutex);
  assert(ret == 0);
#endif

  return retval;
}
