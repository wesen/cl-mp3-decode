#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "misc.h"

/* ahh, the joys of unix */
int unix_read(int fd, unsigned char *buf, unsigned int len) {
  int ret;
  unsigned char *ptr = buf;
  unsigned int left = len, total = 0;

  while (left > 0) {
  again:
    ret = read(fd, ptr, left);
    if (ret < 0) {
      if (ret != EINTR)
	return -1;
      else
	goto again;
    }
    
    if (ret == 0)
      return total;

    assert(ret <= left);
    total += ret;
    left -= ret;
    ptr += ret;
  }

  return total;
}

int unix_write(int fd, unsigned char *buf, unsigned int len) {
  int ret;
  unsigned char *ptr = buf;
  unsigned int left = len, total = 0;

  while (left > 0) {
  again:
    ret = write(fd, ptr, left);
    if (ret < 0) {
      if (ret != EINTR)
	return -1;
      else
	goto again;
    }
    
    if (ret == 0)
      return total;

    assert(ret <= left);
    total += ret;
    left -= ret;
    ptr += ret;
  }

  return total;
}

#define BUF_SIZE 256

int mp3dec_write_cmd(int fd, mp3dec_cmd_e cmd, void *data, unsigned int len, mp3dec_error_t *error) {
  unsigned char buf[CMD_BUF_SIZE];
  unsigned char *ptr = buf;
  if (len > BUF_SIZE - 3) {
    
    return -1;
  }
  
  *ptr++ = cmd;
  *ptr++ = (len & 0xFF);
  *ptr++ = ((len >> 8) & 0xFF);
  if (len > 0) {
    memcpy(ptr, data, len);
    ptr += len;
  }

  len = ptr - buf;
  if (unix_write(fd, buf, len) != len) {
    fprintf(stderr, "Could not write command\n");
    return -1;
  }

  return 0;
}

int read_cmd(int fd, cmd_e *cmd, void *data, unsigned int *len, unsigned int max_len) {
  unsigned char buf[BUF_SIZE];
  unsigned char *ptr = buf;
  if (unix_read(fd, ptr, 3) != 3) {
    fprintf(stderr, "Could not read header\n");
    return -1;
  }
  ptr += 3;
  *len = buf[1] | (buf[2] << 8);
  if (*len > 0) {
    if (*len > (BUF_SIZE - 3)) {
      fprintf(stderr, "Data portion too long\n");
      return -1;
    }
    if (unix_read(fd, ptr, *len) != *len) {
      fprintf(stderr, "Could not read data portion\n");
      return -1;
    }
  }

  if (*len > max_len) {
    fprintf(stderr, "Data too long for given buffer\n");
    return -1;
  }
  memcpy(data, ptr, *len);
  *cmd = buf[0];

  return 0;
}

