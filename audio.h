#ifndef AUDIO_H__
#define AUDIO_H__

int  audio_write(struct mad_pcm *pcm, error_t *error);
int audio_close(error_t *error);

#endif /* AUDIO_H__ */
