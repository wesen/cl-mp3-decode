#include <sys/poll.h>
#include <sys/types.h>

#include <assert.h>
#include <string.h>
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

int unix_check_fd_read(int fd) {
  struct pollfd pfd[1];
  int ret;

  //  printf("checking if %d is active\n", fd);
  
  pfd[0].fd = fd;
  pfd[0].events = POLLIN | POLLERR;
  pfd[0].revents = 0;

  ret = poll(pfd, 1, 0);
  if (ret < 0)
    return ret;
  //  printf("revents: %x\n", pfd[0].revents);
  if ((pfd[0].revents & POLLIN) ||
      (pfd[0].revents & POLLERR))
    return 1;
  else
    return 0;
}

int mp3dec_write_cmd(int fd, mp3dec_cmd_e cmd,
		     void *data, unsigned int len,
		     error_t *error) {
  unsigned char buf[CMD_BUF_SIZE];
  unsigned char *ptr = buf;
  unsigned int cmd_len = 0;
  
  if (len > sizeof(buf) - 3) {
    error_set(error, "Data buffer is too big for a command");
    return -1;
  }
  
  *ptr++ = cmd;
  *ptr++ = (len & 0xFF);
  *ptr++ = ((len >> 8) & 0xFF);
  if (len > 0) {
    memcpy(ptr, data, len);
    ptr += len;
  }
  cmd_len = ptr - buf;
  /* XXX timeout?? */
  if (unix_write(fd, buf, cmd_len) != cmd_len) {
    error_set_strerror(error, "Could not write command to pipe");
    return -1;
  }

  return 0;
}

int mp3dec_write_cmd_string(int fd, mp3dec_cmd_e cmd, char *string, error_t *error) {
  return mp3dec_write_cmd(fd, cmd, string, strlen(string) + 1, error);
}

int mp3dec_read_cmd(int fd, mp3dec_cmd_e *cmd,
		    void *data, unsigned int *len, unsigned int max_len,
		    error_t *error) {
  unsigned char buf[CMD_BUF_SIZE];
  unsigned char *ptr = buf;

  assert(CMD_BUF_SIZE >= 3);
  if (unix_read(fd, ptr, 3) != 3) {
    error_set_strerror(error, "Could not read command header from pipe");
    return -1;
  }
  ptr += 3;

  *cmd = buf[0];
  *len = buf[1] | (buf[2] << 8);
    
  if (*len > 0) {
    if (*len > (CMD_BUF_SIZE - 3)) {
      error_set(error, "Data buffer is too big for a command");
      return -1;
    }
    if (unix_read(fd, ptr, *len) != *len) {
      error_set_strerror(error, "Could not read command data buffer from pipe");
      return -1;
    }
  }

  if (data != NULL) {
    if (*len > max_len) {
      error_set(error, "Data buffer is too big for the given buffer");
      return -1;
    }
    memcpy(data, ptr, *len);
  }

  return 0;
}

