#ifndef MADDEC_H__
#define MADDEC_H__

struct mp3dec_state_s;
typedef struct mp3dec_state_s mp3dec_state_t;

mp3dec_state_t *mp3dec_new(void);
void mp3dec_delete(mp3dec_state_t *state);

int mp3dec_play(mp3dec_state_t *state);
int mp3dec_pause(mp3dec_state_t *state);
int mp3dec_load(mp3dec_state_t *state, char *filename);
int mp3dec_ping(mp3dec_state_t *state);

char *mp3dec_error(mp3dec_state_t *state);

#endif /* MP3_DECODE_H__ */
