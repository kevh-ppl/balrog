#ifndef CMD_OPT_H
#define CMD_OPT_H

#include <pthread.h>

#define PIPE_BUF 4096

// pthread_t pthread_cmd_pipe;

void processing_cmd(int argc, char *argv[]);
void init_cmd_line(void *data);

#endif
