/*
 * Based on daemonization template by KoynovStas.
 * Original source: https://github.com/KoynovStas/daemon_templates.git
 * Licensed under the BDS-3 License.
 */

#include "daemon/daemon.h"

#include <common/helpers.h>
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

#include "common/init_config.h"

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

int demonized() {
    FILE* pid_file = fopen(daemon_info.pid_file, "r");
    if (!pid_file) {
        fprintf(stderr, "Daemon not running (PID file missing)\n");  // on restart with systemd it
                                                                     // shows this log
        return -1;
    }

    pid_t pid;
    if (fscanf(pid_file, "%d", &pid) != 1) {
        fprintf(stderr, "Could not read PID from file\n");
        fclose(pid_file);
        return -1;
    }
    fclose(pid_file);

    // Comprobar si el proceso existe
    if (kill(pid, 0) == 0) {
        // Proceso existe y tengo permiso
        return 1;
    } else {
        if (errno == EPERM) {
            // Existe pero pertenece a otro usuario
            daemon_info.daemonized = 1;
            return 1;
        } else if (errno == ESRCH) {
            // No existe
            return -1;
        } else {
            // Otro errooorrrrrrr ñia inesperado
            perror("kill");
            return -1;
        }
    }
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

int redirect_stdio_to_logfile(const char* log_path) {
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

int create_monitor_log_file(const char* log_path) {
    int log_fd = open(log_path, O_CREAT, 0644);

    if (log_fd < 0) {
        fprintf(stderr, "Error creating monitor log file: %s\n", strerror(errno));
        return -1;
    }
    close(log_fd);
    return 0;
}

/*
 * Set a signal handler for a specific signal.
 * This function uses the signal() system call to set the handler.
 * If the handler cannot be set, it will exit with an error message.
 */
void set_sig_handler(int sig, signal_handler_t handler) {
    if (signal(sig, handler) == SIG_ERR)
        error_exit("balrogd", "Can't set handler for signal: %d", sig);
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
            error_exit("balrogd", "Can't fork");
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
void daemonize2(void (*optional_init)(void*), void* data) {
    printf("Init daemonizing");
    if (!daemon_info.no_fork) do_fork();

    // Reset the file mode mask
    umask(0);

    uid_t uid = getuid();
    if (!uid) printf("UID null, so we're 'monky'\n");
    gid_t gid = getgid();
    if (!gid) printf("GID null, so we're from 'monky'\n");
    daemon_info.daemon_user = uid;
    daemon_info.daemon_group = gid;

    struct stat st_balrog_log;
    struct stat st_balrog_run;
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
    if (!daemon_info.no_fork && (setsid() == -1)) error_exit("balrogd", "Can't setsid");

    // Change the current working directory to "/"
    // This prevents the current directory from locked
    // The demon must always change the directory to "/"
    if (!daemon_info.no_chdir && (chdir("/") != 0)) error_exit("balrogd", "Can't chdir");

    if (daemon_info.pid_file && (create_pid_file(daemon_info.pid_file) == -1))
        error_exit("balrogd", "Can't create pid file: %s", daemon_info.pid_file);

    printf("PID FILE created...\n");

    // if (daemon_info.log_file) {
    //     if (redirect_stdio_to_logfile(daemon_info.log_file) == -1) {
    //         error_exit("Can't redirect stdout/stderr to log file %s: %m\n",
    //                           daemon_info.log_file);
    //     }
    // }

    // printf("Redirecting to log file...\n");

    // if (daemon_info.monitor_log_file) {
    //     if (create_monitor_log_file(daemon_info.monitor_log_file) == -1) {
    //         error_exit("Couldn't create monitor log file %s: %m\n",
    //                           daemon_info.monitor_log_file);
    //     }
    // }

    // printf("Monitor log created...\n");

    // call user functions for the optional initialization
    // before closing the standardIO (STDIN, STDOUT, STDERR)
    if (optional_init) optional_init(data);

    if (!daemon_info.no_close_stdio && (redirect_stdio_to_devnull() != 0))
        error_exit("balrogd", "Can't redirect stdio to /dev/null");

    daemon_info.daemonized = 1;  // good job
}
