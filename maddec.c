#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>

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

int mp3dec_parent_main(mp3dec_state_t *state, int cmd_fd, int response_fd) {
  if (mp3dec_write_cmd(cmd_fd, MP3DEC_COMMAND_EXIT, NULL, 0, &state->error) < 0) {
    mp3dec_error_prepend(&state->error, "Could not send EXIT to child");
    return -1;
  }

  return 0;
}

int mp3dec_parent_cmd_ack(mp3dec_state_t *state, mp3dec_cmd_e cmd, void *data, unsigned int len) {
  mp3dec_cmd_e resp;
  unsigned char buf[CMD_BUF_SIZE];
  unsigned int buflen;
  
  if (state->child_pid == -1) {
    mp3dec_error_set(&state->error, "No child started");
    return -1;
  }

  assert(state->cmd_fd != -1);
  assert(state->response_fd != -1);
  assert(state->child_pid != -1);

  if (mp3dec_write_cmd(state->cmd_fd, cmd, data, len, &state->error) < 0) {
    mp3dec_error_prepend(&state->error, "Could not write command to child");
    return -1;
  }
  if (mp3dec_read_cmd(state->response_fd, &resp, buf, &buflen, sizeof(buf), &state->error) < 0) {
    mp3dec_error_prepend(&state->error, "Could not read response from child");
    return -1;
  }

  if (resp == MP3DEC_RESPONSE_ACK) {
    return 0;
  } else if (resp == MP3DEC_RESPONSE_ERR) {
    mp3dec_error_set(&state->error, "Error from child");
    mp3dec_error_append(&state->error, buf); /* ensure that buf is a string XXX */
    return -1;
  } else {
    mp3dec_error_set(&state->error, "Unknown response from child");
    return -1;
  }
}

int mp3dec_parent_null_cmd_ack(mp3dec_state_t *state, mp3dec_cmd_e cmd) {
  return mp3dec_parent_cmd_ack(state, cmd, NULL, 0);
}

int mp3dec_play(mp3dec_state_t *state) {
  return mp3dec_parent_null_cmd_ack(state, MP3DEC_COMMAND_PLAY);
}

int mp3dec_pause(mp3dec_state_t *state) {
  return mp3dec_parent_null_cmd_ack(state, MP3DEC_COMMAND_PAUSE);
}

int mp3dec_exit(mp3dec_state_t *state) {
  return mp3dec_parent_null_cmd_ack(state, MP3DEC_COMMAND_EXIT);
}

int mp3dec_load(mp3dec_state_t *state, char *filename) {
  return mp3dec_parent_cmd_ack(state, MP3DEC_COMMAND_LOAD, filename, strlen(filename) + 1);
}

int mp3dec_ping(mp3dec_state_t *state) {
 mp3dec_cmd_e resp;
  unsigned char buf[CMD_BUF_SIZE];
  unsigned int buflen;
  
  if (state->child_pid == -1) {
    mp3dec_error_set(&state->error, "No child started");
    return -1;
  }

  assert(state->cmd_fd != -1);
  assert(state->response_fd != -1);
  assert(state->child_pid != -1);

  if (mp3dec_write_cmd(state->cmd_fd, MP3DEC_COMMAND_PING, NULL, 0, &state->error) < 0) {
    mp3dec_error_prepend(&state->error, "Could not write PING to child");
    return -1;
  }
  if (mp3dec_read_cmd(state->response_fd, &resp, buf, &buflen, sizeof(buf), &state->error) < 0) {
    mp3dec_error_prepend(&state->error, "Could not read response from child");
    return -1;
  }

  if (resp == MP3DEC_RESPONSE_PONG) {
    return 0;
  } else if (resp == MP3DEC_RESPONSE_ERR) {
    mp3dec_error_set(&state->error, "Error from child");
    mp3dec_error_append(&state->error, buf); /* ensure that buf is a string XXX */
    return -1;
  } else {
    printf("got response %x\n", resp);
    mp3dec_error_set(&state->error, "Unknown response from child");
    return -1;
  }
}

int mp3dec_start(mp3dec_state_t *state) {
  int ret;
  int retval = 0;
  int cmd_fd[2]      = { -1, -1 };
  int response_fd[2] = { -1, -1 };

  ret = pipe(cmd_fd);
  if (ret < 0) {
    mp3dec_error_set_strerror(&state->error, "Could not open command pipe");
    retval = -1;
    goto error;
  }
  ret = pipe(response_fd);
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
    /* child */
    close(response_fd[0]);
    response_fd[0] = -1;
    close(cmd_fd[1]);
    cmd_fd[1] = -1;
    mp3dec_child_main(cmd_fd[0], response_fd[1]);
    close(response_fd[1]);
    close(cmd_fd[0]);
    exit(0);

  } else {
    /* parent */

    state->child_pid = ret;

    close(response_fd[1]);
    response_fd[1] = -1;
    close(cmd_fd[0]);
    cmd_fd[0] = -1;

    state->cmd_fd = cmd_fd[1];
    state->response_fd = response_fd[0];

    if (mp3dec_ping(state) != 0) {
      mp3dec_error_prepend(&state->error, "Could not PING child");
      goto error;
    }

    return 0;
  }

 error:
  if (state->child_pid != -1) {
    kill(state->child_pid, SIGTERM);
    waitpid(state->child_pid, NULL, 0);
    state->child_pid = -1;
    state->cmd_fd = -1;
    state->response_fd = -1;
  }
  
  if (cmd_fd[0] != -1) {
    close(cmd_fd[0]);
    cmd_fd[0] = -1;
  }
  if (cmd_fd[1] != -1) {
    close(cmd_fd[1]);
    cmd_fd[1] = -1;
  }
  if (response_fd[0] != -1) {
    close(response_fd[0]);
    response_fd[0] = -1;
  }
  if (response_fd[1] != -1) {
    close(response_fd[1]);
    response_fd[1] = -1;
  }

  return retval;
}
