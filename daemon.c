/*
 * Based on daemonization template by KoynovStas.
 * Original source: https://github.com/KoynovStas/daemon_templates.git
 * Licensed under the BDS-3 License.
 */

#include "daemon.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include "balrog_udev.h"

volatile struct daemon_info_t daemon_info = {
    // flag will be set in finale function daemonize()
    .terminated = 0,
    .daemonized = 0,

#ifdef DAEMON_NO_CHDIR
    .no_chdir = DAEMON_NO_CHDIR,
#else
    .no_chdir = 0,
#endif

#ifdef DAEMON_NO_FORK
    .no_fork = DAEMON_NO_FORK,
#else
    .no_fork = 0,
#endif

#ifdef DAEMON_NO_CLOSE_STDIO
    .no_close_stdio = DAEMON_NO_CLOSE_STDIO,
#else
    .no_close_stdio = 0,
#endif

#ifdef DAEMON_PID_FILE_NAME
    .pid_file = DAEMON_PID_FILE_NAME,
#else
    .pid_file = NULL,
#endif

#ifdef DAEMON_LOG_FILE_NAME
    .log_file = DAEMON_LOG_FILE_NAME,
#else
    .log_file = NULL,
#endif

#ifdef DAEMON_MONITOR_LOG_FILE_NAME
    .monitor_log_file = DAEMON_MONITOR_LOG_FILE_NAME,
#else
    .monitor_log_file = NULL,
#endif

#ifdef DDAEMON_MONITOR_PID_FILE_NAME
    .monitor_pid_file = DDAEMON_MONITOR_PID_FILE_NAME,
#else
    .monitor_pid_file = NULL,
#endif

#ifdef DAEMON_CMD_PIPE_NAME
    .cmd_pipe = DAEMON_CMD_PIPE_NAME,
#else
    .cmd_pipe = NULL,
#endif

    .default_log_dir = "/var/log/balrog/",
    .default_run_dir = "/var/run/balrog"

};

// Exit if the daemon is not daemonized
// This is used to ensure that the daemon is running in the background
void exit_if_not_daemonized(int exit_status) {
    if (!daemon_info.daemonized) {
        fprintf(stderr, "Daemon is not daemonized. Exiting with status %d\n", exit_status);
        free_udev_enumerator();
        stop_monitoring();
        free_udev_context();
        _exit(exit_status);
    }
}

/*
 * This function is used to exit the daemon with an error message.
 * It uses variable arguments to format the error message.
 * It will print the error message to stderr and then exit the daemon.
 * The exit status will be EXIT_FAILURE.
 */
void daemon_error_exit(const char *format, ...) {
    va_list ap;

    if (format && *format) {
        va_start(ap, format);
        fprintf(stderr, "%s: ", DAEMON_NAME);
        vfprintf(stderr, format, ap);
        va_end(ap);
    }
    free_udev_enumerator();
    stop_monitoring();
    free_udev_context();

    _exit(EXIT_FAILURE);
}

int redirect_stdio_to_devnull(void) {
    int fd;

    fd = open("/dev/null", O_RDWR);
    if (fd == -1) return -1;

    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);

    if (fd > 2) close(fd);

    return 0;
}

int redirect_stdio_to_logfile(const char *log_path) {
    int log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd < 0) {
        fprintf(stderr, "Error opening log file: %s\n", strerror(errno));
        return -1;
    }

    // Redirige stdout y stderr a nivel de descriptor de archivo
    if (dup2(log_fd, STDOUT_FILENO) < 0 || dup2(log_fd, STDERR_FILENO) < 0) {
        fprintf(stderr, "Error dup2: %s\n", strerror(errno));
        close(log_fd);
        return -1;
    }

    if (freopen(log_path, "a", stdout) == NULL || freopen(log_path, "a", stderr) == NULL) {
        fprintf(stderr, "Error freopen: %s\n", strerror(errno));
        close(log_fd);
        return -1;
    }

    setvbuf(stdout, NULL, _IOLBF, 0);  // Línea buffer para stdout
    setvbuf(stderr, NULL, _IONBF, 0);  // Sin buffer para stderr

    fflush(stdout);
    fflush(stderr);

    // write(log_fd, "Daemon started\n", 15);  // Log the start of the daemon

    // Cierra el descriptor original (ya fue duplicado)
    if (log_fd > STDERR_FILENO) close(log_fd);
    return 0;
}

int create_monitor_log_file(const char *log_path) {
    int log_fd = open(log_path, O_CREAT, 0644);

    if (log_fd < 0) {
        fprintf(stderr, "Error creating monitor log file: %s\n", strerror(errno));
        return -1;
    }
    close(log_fd);
    return 0;
}

int create_pid_file(const char *pid_file_name) {
    int fd;
    const int BUF_SIZE = 32;
    char pid_str[BUF_SIZE];

    if (!pid_file_name) {
        fprintf(stderr, "PID file name is NULL\n");
        return -1;
    }

    fd = open(pid_file_name, O_RDWR | O_CREAT, 0644);

    if (fd < 0) {
        errno = EINVAL;
        fprintf(stderr, "Failed to open PID file '%s': %s\n", pid_file_name, strerror(errno));
        return -1;
    }

    // Try to lock the file to ensure only one instance of the daemon can run
    if (lockf(fd, F_TLOCK, 0) == -1) {
        close(fd);
        return -1;
    }

    // Clear the file content before writing the new PID
    if (ftruncate(fd, 0) != 0) {
        close(fd);
        return -1;  // Could not truncate on PID file
    }

    snprintf(pid_str, BUF_SIZE, "%ld\n", (long)getpid());
    if (write(fd, pid_str, strlen(pid_str)) != (int)strlen(pid_str)) {
        close(fd);
        return -1;  // Could not write PID to PID file
    }

    return fd;
}

/*
 * Set a signal handler for a specific signal.
 * This function uses the signal() system call to set the handler.
 * If the handler cannot be set, it will exit with an error message.
 */
void set_sig_handler(int sig, signal_handler_t handler) {
    if (signal(sig, handler) == SIG_ERR)
        daemon_error_exit("Can't set handler for signal: %d %m\n", sig);
    // ojo piojo %m its for gnu libc to print the error message
}

/*
Actual demonization function.
This function will fork the process, but its not changing the directory to "/",
not closing the standard IO files and not changing to a new session (SID).
Just forking the process.
*/
static void do_fork() {
    switch (fork())  // Become background process
    {
        case -1:
            daemon_error_exit("Can't fork: %m\n");
        case 0:
            break;  // child process (go next)
        default:
            _exit(EXIT_SUCCESS);  // We can exit the parent process
    }

    // ---- At this point we are executing as the child process ----
}

/*
This functions handles the demonization options.
*/
void daemonize2(void (*optional_init)(void *), void *data) {
    if (!daemon_info.no_fork) do_fork();

    // Reset the file mode mask
    umask(0);

    struct stat st_balrog_log;
    struct stat st_balrog_run;
    printf("los temerariiios\n");
    if (stat(daemon_info.default_log_dir, &st_balrog_log) < 0) {
        // if it doesn't exists, i must create it
        if (mkdir(daemon_info.default_log_dir, 0750) < 0) {
            printf("Couldn't mkdir dir /var/log/balrog/: %m...\n");
            _exit(EXIT_FAILURE);
        }
    }

    if (stat(daemon_info.default_run_dir, &st_balrog_run) < 0) {
        // if it doesn't exists, i must create it
        if (mkdir(daemon_info.default_run_dir, 0755) < 0) {
            printf("Couldn't mkdir dir /var/run/balrog/: %m...\n");
            _exit(EXIT_FAILURE);
        }
    }
    printf("Dirs created...\n");

    // Create a new process group(session) (SID) for the child process
    // call setsid() only if fork is done
    if (!daemon_info.no_fork && (setsid() == -1)) daemon_error_exit("Can't setsid: %m\n");

    // Change the current working directory to "/"
    // This prevents the current directory from locked
    // The demon must always change the directory to "/"
    if (!daemon_info.no_chdir && (chdir("/") != 0)) daemon_error_exit("Can't chdir: %m\n");

    if (daemon_info.pid_file && (create_pid_file(daemon_info.pid_file) == -1))
        daemon_error_exit("Can't create pid file: %s: %m\n", daemon_info.pid_file);

    printf("PID FILE created...\n");

    if (daemon_info.log_file) {
        if (redirect_stdio_to_logfile(daemon_info.log_file) == -1) {
            daemon_error_exit("Can't redirect stdout/stderr to log file %s: %m\n",
                              daemon_info.log_file);
        }
    }

    printf("Redirecting to log file...\n");

    if (daemon_info.monitor_log_file) {
        if (create_monitor_log_file(daemon_info.monitor_log_file) == -1) {
            daemon_error_exit("Couldn't create monitor log file %s: %m\n",
                              daemon_info.monitor_log_file);
        }
    }

    printf("Monitor log created...\n");

    // call user functions for the optional initialization
    // before closing the standardIO (STDIN, STDOUT, STDERR)
    if (optional_init) optional_init(data);

    if (!daemon_info.no_close_stdio && (redirect_stdio_to_devnull() != 0))
        daemon_error_exit("Can't redirect stdio to /dev/null: %m\n");

    daemon_info.daemonized = 1;  // good job
}
