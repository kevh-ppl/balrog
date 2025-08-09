#ifndef DAEMON_H
#define DEAMON_H

/*
 * Based on daemonization template by KoynovStas.
 * Original source: https://github.com/KoynovStas/daemon_templates.git
 * Licensed under the BDS-3 License.
 */

#include <stddef.h>  //for null
#include <sys/types.h>

#define F_TLOCK 2  // file lock type, used in lockf()

#define DAEMON_NAME "balrog"

#define DAEMON_DEF_TO_STR_(text) #text
#define DAEMON_DEF_TO_STR(arg) DAEMON_DEF_TO_STR_(arg)

#define DAEMON_MAJOR_VERSION_STR DAEMON_DEF_TO_STR(DAEMON_MAJOR_VERSION)
#define DAEMON_MINOR_VERSION_STR DAEMON_DEF_TO_STR(DAEMON_MINOR_VERSION)
#define DAEMON_PATCH_VERSION_STR DAEMON_DEF_TO_STR(DAEMON_PATCH_VERSION)

#define DAEMON_VERSION_STR \
    DAEMON_MAJOR_VERSION_STR "." DAEMON_MINOR_VERSION_STR "." DAEMON_PATCH_VERSION_STR

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

    const char *pid_file;
    const char *log_file;
    const char *cmd_pipe;
    const char *monitor_log_file;
    const char *monitor_pid_file;
    const char *default_run_dir;
    const char *default_log_dir;
};

extern volatile struct daemon_info_t daemon_info;

void exit_if_not_daemonized(int exit_status);
void daemon_error_exit(const char *format,
                       ...);  // this one use variable args with the lib stdarg.h
int create_pid_file(const char *pid_file_name);

typedef void (*signal_handler_t)(int);
void set_sig_handler(int sig, signal_handler_t handler);

void daemonize2(void (*optional_init)(void *), void *data);
static inline void daemonize() { daemonize2(NULL, NULL); }

int demonized();

#endif