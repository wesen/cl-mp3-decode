#include <sys/poll.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

typedef enum {
  CMD_START = 0,
  CMD_STOP,
  CMD_LOAD,
} cmd_e;

static int unix_read(int fd, unsigned char *buf, unsigned int len) {
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

static int unix_write(int fd, unsigned char *buf, unsigned int len) {
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

int write_cmd(int fd, cmd_e cmd, void *data, unsigned int len) {
  unsigned char buf[BUF_SIZE];
  unsigned char *ptr = buf;
  if (len > BUF_SIZE - 3) {
    fprintf(stderr, "Too long command\n");
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

void parent_main(pid_t child_pid, int cmd_fd, int response_fd) {
  if (write_cmd(cmd_fd, CMD_START, NULL, 0) != 0) {
    fprintf(stderr, "Could not write command\n");
    return;
  }
  if (write_cmd(cmd_fd, CMD_LOAD, "foobar", strlen("foobar") + 1) != 0) {
    fprintf(stderr, "Could not write load command\n");
    return;
  } 
  if (write_cmd(cmd_fd, CMD_STOP, NULL, 0) != 0) {
    fprintf(stderr, "Could not write stop command\n");
    return;
  } 
}

int check_fd_active(int fd) {
  struct pollfd pfd[1];
  int ret;
  
  pfd[0].fd = fd;
  pfd[0].events = POLLIN | POLLERR;
  pfd[0].revents = 0;

  ret = poll(pfd, 1, 0);
  if (ret < 0)
    return ret;
  if ((pfd[0].revents & POLLIN) ||
      (pfd[0].revents & POLLERR))
    return 1;
  else
    return 0;
}

void child_main(int cmd_fd, int response_fd) {
  cmd_e cmd;
  unsigned char buf[BUF_SIZE];
  unsigned int len;

  while (read_cmd(cmd_fd, &cmd, buf, &len, sizeof(buf)) == 0) {
    printf("received cmd: %d\n", cmd);
    if (cmd == CMD_START) {
      printf("Received start command!\n");
    } else if (cmd == CMD_LOAD) {
      printf("Received load command: %s\n", buf);
    } else if (cmd == CMD_STOP) {
      printf("Received stop command!\n");
      return;
    }
  }
}

int main(void) {
  int cmd[2], response[2];
  int ret;

  ret = pipe(cmd);
  if (ret < 0) {
    perror("pipe");
    return 1;
  }
  ret = pipe(response);
  if (ret < 0) {
    perror("pipe");
    return 1;
  }

  ret = fork();
  if (ret < 0) {
    perror("fork");
    return 1;
  }

  if (ret == 0) {
    close(response[0]);
    close(cmd[1]);
    child_main(cmd[0], response[1]);
    close(cmd[0]);
    close(response[1]);
  } else {
    close(response[1]);
    close(cmd[0]);
    parent_main(ret, cmd[1], response[0]);
    close(cmd[1]);
    close(response[0]);
    waitpid(ret, NULL, 0);
  }

  return 0;
}
