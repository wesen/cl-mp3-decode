#ifndef AUDIO_H__
#define AUDIO_H__

int  audio_write(struct mad_pcm *pcm, error_t *error);
void audio_close(void);

#endif /* AUDIO_H__ */
