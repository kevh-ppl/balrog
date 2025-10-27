#ifndef INIT_CONFIG_H
#define INIT_CONFIG_H

#include <sys/types.h>

/*
 * This is the daemon info structure, it contains all the information
 * that the daemon needs to run, like the pid file, log file, cmd pipe,
 * and some flags to control the daemon behavior.
 * The flags are:
 * - terminated: if the daemon is terminated
 * - daemonized: if the daemon is daemonized
 * - no_chdir: if the daemon should not change the directory to '/'
 * - no_fork: if the daemon should not fork
 * - no_close_stdio: if the daemon should not close the standard IO files
 * - pid_file: the pid file name
 * - log_file: the log file name
 * - cmd_pipe: the command pipe name
 */
struct daemon_info_t {
    // flags
    unsigned int terminated : 1;
    unsigned int daemonized : 1;
    unsigned int no_chdir : 1;
    unsigned int no_fork : 1;
    unsigned int no_close_stdio : 1;

    gid_t daemon_group;
    uid_t daemon_user;

    char pid_file[256];
    char log_file[256];
    char cmd_pipe[256];
    char monitor_log_file[256];
    char monitor_pid_file[256];
    char monitor_socket_file[256];
    char default_run_dir[256];
    char default_log_dir[256];
    char fifo_user_path[256];
};

extern struct daemon_info_t daemon_info;

void init_config();

#endif