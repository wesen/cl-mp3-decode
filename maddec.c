#include <sys/soundcard.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <stdlib.h>

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <mad.h>

#include "maddec.h"
#include "error.h"
#include "misc.h"

/* commands:
   - load file
   - play
   - stop
   - status
   - pause
*/

/* initializing and stuff */

int mp3dec_child_main(int cmd_fd, int response_fd);

mp3dec_state_t *mp3dec_new(void) {
  mp3dec_state_t *state = malloc(sizeof(mp3dec_state_t));
  if (state == NULL)
    return NULL;

  mp3dec_error_reset(&state->error);
  state->child_pid = -1;
  state->cmd_fd = -1;
  state->response_fd = -1;
  
  return state;
}

void mp3dec_close(mp3dec_state_t *state) {
  free(state);
}

void mp3dec_parent_main(mp3dec_state_t *state, int cmd_fd, int response_fd) {
  mp3dec_cmd_e cmd;
  unsigned char buf[CMD_BUF_SIZE];
  unsigned int buflen;
  if (mp3dec_write_cmd_string(cmd_fd, MP3DEC_COMMAND_PING, "hello", &state->error) < 0) {
    mp3dec_error_prepend(&state->error, "Could not send PING to child");
    return;
  }
  if (mp3dec_read_cmd(response_fd, &cmd, buf, &buflen, sizeof(buf), &state->error) < 0) {
    mp3dec_error_prepend(&state->error, "Could not read PONG from child");
    return;
  }
  if (strncmp(buf, "hello", buflen) != 0) {
    mp3dec_error_set(&state->error, "Got incorrect PONG answer from child");
    return;
  }

  printf("Got PONG!\n");
}

int mp3dec_start(mp3dec_state_t *state) {
  int ret;
  int retval = 0;
  int cmd[2]      = { -1, -1 };
  int response[2] = { -1, -1 };

  ret = pipe(cmd);
  if (ret < 0) {
    mp3dec_error_set_strerror(&state->error, "Could not open command pipe");
    retval = -1;
    goto error;
  }
  ret = pipe(response);
  if (ret < 0) {
    mp3dec_error_set_strerror(&state->error, "Could not open response pipe");
    retval = -1;
    goto error;
  }

  ret = fork();
  if (ret < 0) {
    mp3dec_error_set_strerror(&state->error, "Could not fork");
    retval = -1;
    goto error;
  }

  if (ret == 0) {
    close(response[0]);
    close(cmd[1]);
    mp3dec_child_main(cmd[0], response[1]);
  } else {
    state->child_pid = ret;
    close(response[1]);
    close(cmd[0]);
    mp3dec_parent_main(state, cmd[1], response[0]);
    waitpid(state->child_pid, NULL, 0);
  }

 error:
  if (cmd[0] != -1) {
    close(cmd[0]);
    cmd[0] = -1;
  }
  if (cmd[1] != -1) {
    close(cmd[1]);
    cmd[1] = -1;
  }
  if (response[0] != -1) {
    close(response[0]);
    response[0] = -1;
  }
  if (response[1] != -1) {
    close(response[1]);
    response[1] = -1;
  }

  return retval;
}
