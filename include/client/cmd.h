#ifndef CMD__H
#define CMD__H

#define PIPE_BUF 4096
void write_cmd_to_cmd_pipe(int argc, char* argv[], char* balrog_dir_user_path, char* fifo_user_path,
                           unsigned long int uid);

#endif