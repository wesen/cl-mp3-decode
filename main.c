#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "maddec.h"

int main(int argc, char *argv[]) {
  mp3dec_state_t *state = mp3dec_new();

  if (state == NULL) {
    printf("not alloc\n");
    return 1;
  }

  if (argc != 2) {
    printf("Please give a mp3 file as argument\n");
    return 1;
  }

  if (mp3dec_start(state) < 0) {
    printf("Could not start: %s\n", mp3dec_error(&state->error));
    return 1;
  }

  if (mp3dec_load(state, argv[1]) < 0) {
    printf("Could not decode %s: %s\n", argv[1], mp3dec_error(&state->error));
    return 1;
  }

  if (mp3dec_play(state) < 0) {
    printf("Could not play %s: %s\n", argv[1], mp3dec_error(&state->error));
    return 1;
  }

  static int count = 0;
  for (;;) {
    if (mp3dec_ping(state) < 0) {
      printf("Coudl not ping player: %s\n", mp3dec_error(&state->error));
      break;
    }
    sleep(1);
    count++;

    printf("time: %d secs\n", count);
    if (count == 1) {
      printf("pausing\n");
      if (mp3dec_pause(state) < 0) {
	printf("Could not pause player: %s\n", mp3dec_error(&state->error));
      }
    }
    if (count == 3) {
      printf("unpausing\n");
      if (mp3dec_pause(state) < 0) {
	printf("Could not unpause player: %s\n", mp3dec_error(&state->error));
      }
    }
    if (count == 7) {
      printf("changing mp3\n");
      if (mp3dec_load(state, "/home/manuel/Lazy_Jones.mp3") < 0) {
	printf("Could not decode %s: %s\n", argv[1], mp3dec_error(&state->error));
	return 1;
      }
    }
    if (count == 15) {
      printf("exiting\n");
      if (mp3dec_exit(state) < 0) {
	printf("Could not exit player: %s\n", mp3dec_error(&state->error));
      }
    }
  }
  
  mp3dec_close(state);
  return 0;
}
