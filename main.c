#include <stdio.h>
#include <fcntl.h>
#include "mp3dec.h"

int main(int argc, char *argv[]) {
  mp3_state_t state;
  mp3dec_reset(&state);

  if (argc != 2) {
    printf("Please give a mp3 file as argument\n");
    return 1;
  }

  if (mp3dec_decode_file(&state, argv[1]) < 0) {
    printf("Could not decode %s: %s\n", argv[1], mp3dec_error(&state));
    return 1;
  }
  
  mp3dec_close(&state);
  return 0;
}
