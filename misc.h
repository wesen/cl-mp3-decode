#ifndef MISC_H__
#define MISC_H__

#include "maddec.h"
#include "maddec_internal.h"

int unix_read(int fd, unsigned char *buf, unsigned int len);
int unix_write(int fd, unsigned char *buf, unsigned int len);
int unix_check_fd_read(int fd);

int mp3dec_write_cmd(int fd, mp3dec_cmd_e cmd,
		     void *data, unsigned int len,
		     error_t *error);
int mp3dec_write_cmd_string(int fd, mp3dec_cmd_e cmd, char *string, error_t *error);
int mp3dec_read_cmd(int fd, mp3dec_cmd_e *cmd,
		    void *data, unsigned int *len, unsigned int max_len,
		    error_t *error);

#endif /* MISC_H__ */
