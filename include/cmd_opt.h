#ifndef CMD_OPT_H
#define CMD_OPT_H

#include <pthread.h>
#include <unistd.h>

#define PIPE_BUF 4096

extern pthread_t pthread_cmd_pipe;

void write_cmd_to_cmd_pipe(int argc, char *argv[], char *balrog_dir_user_path, char *fifo_user_path,
                           unsigned long int uid);
void processing_cmd(int argc, char *argv[]);
void init_cmd_line(void *data);

#endif
