/*
 * Based on daemonization template by KoynovStas.
 * Original source: https://github.com/KoynovStas/daemon_templates.git
 * Licensed under the BDS-3 License.
 */

#include "cmd_opt.h"

#include <getopt.h>
#include <libnotify/notify-features.h>
#include <libnotify/notify.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "balrog_udev.h"
#include "daemon.h"

pthread_t pthread_cmd_pipe;

// Define the help string for balrog-usb-utility
const char *help_str = DAEMON_NAME
    "\n"
    " Version: " DAEMON_VERSION_STR
    "\n\n"
#ifdef DEBUG
    " Build mode: debug\n"
#else
    " Build mode: release\n"
#endif
    " Build date: " __DATE__
    "\n"
    " Build time: " __TIME__
    "\n"
    " Options:                          description:\n\n"
    "       --no-chdir                  Don't change the directory to '/'\n"
    "       --no-fork                   Don't do fork\n"
    "       --no-close                  Don't close standart IO files\n"
    "       --pid-file [value]          Set pid file name\n"
    "       --log-file-path [value]     Set log file name\n"
    "       --cm-pipe [value]           Set CMD Pipe name\n"
    "  -p,  --print-udev-vars           Print udev vars for debugging\n"
    "  -e,  --enumerate                 Enumerates all block (storage) devices\n"
    "  -m,  --start-monitor             Starts monitoring USB devices related IO events\n"
    "  -w,  --stop-monitor              Stops monitoring USB devices related IO events\n"
    "  -s,  --monitor-log-file          Set your monitor log file instead of default path\n"
    "  -v,  --version                   Display version\n"
    "  -h,  --help                      Display this help\n\n\0";

enum {
    cmd_opt_help = 'h',
    cmd_opt_version = 'v',
    cmd_enumerate = 'e',
    cmd_start_monitor = 'm',
    cmd_stop_monitor = 'w',
    cmd_print_udev_vars = 'p',

    // daemon options (start from a value outside ASCII range)
    cmd_opt_no_chdir = 1000,
    cmd_opt_no_fork,
    cmd_opt_no_close,
    cmd_opt_pid_file,
    cmd_opt_log_file,
    cmd_opt_monitor_log_file,
    cmd_opt_cmd_pipe
};

static const char *short_opts = "hvemwp";
static const struct option long_opts[] = {
    {"version", no_argument, NULL, cmd_opt_version},
    {"help", no_argument, NULL, cmd_opt_help},
    {"enumerate", no_argument, NULL, cmd_enumerate},
    {"start-monitor", no_argument, NULL, cmd_start_monitor},
    {"stop-monitor", no_argument, NULL, cmd_stop_monitor},
    {"print-udev-vars", no_argument, NULL, cmd_print_udev_vars},

    // daemon options
    {"no-chdir", no_argument, NULL, cmd_opt_no_chdir},
    {"no-fork", no_argument, NULL, cmd_opt_no_fork},
    {"no-close", no_argument, NULL, cmd_opt_no_close},
    {"pid-file", required_argument, NULL, cmd_opt_pid_file},
    {"log-file", required_argument, NULL, cmd_opt_log_file},
    {"monitor-log-file", required_argument, NULL, cmd_opt_monitor_log_file},
    {"cmd-pipe", required_argument, NULL, cmd_opt_cmd_pipe},

    {NULL, no_argument, NULL, 0}};

static void daemon_exit_handler(int sig) {
    // Here we release resources

    unlink(daemon_info.pid_file);
    unlink(daemon_info.cmd_pipe);

    _exit(EXIT_FAILURE);
}

static void init_signals(void) {
    set_sig_handler(SIGINT,
                    daemon_exit_handler);  // for Ctrl-C in terminal for debug (in debug mode)
    set_sig_handler(SIGTERM, daemon_exit_handler);

    set_sig_handler(SIGCHLD, SIG_IGN);  // ignore child
    set_sig_handler(SIGTSTP, SIG_IGN);  // ignore tty signals
    set_sig_handler(SIGTTOU, SIG_IGN);
    set_sig_handler(SIGTTIN, SIG_IGN);
    set_sig_handler(SIGHUP, SIG_IGN);
}

/*
Waits for the deamon response to the cmd
@param char* fifo_user_path
*/
static void wait_and_print_daemon_response(char *fifo_user_path) {
    char *response_buffer = malloc(PIPE_BUF);
    if (!response_buffer) {
        perror("malloc");
        return;
    }
    size_t buffer_size = PIPE_BUF;
    int fd_fifo_user = open(fifo_user_path, O_RDONLY);

    if (fd_fifo_user < 0) {
        perror("open fifo_user_path");
        free(response_buffer);
        return;
    }

    size_t total_read = 0;
    ssize_t bytes_read;
    while ((bytes_read = read(fd_fifo_user, response_buffer + total_read,
                              buffer_size - total_read - 1)) > 0) {
        total_read += bytes_read;
        if (total_read + 1 >= buffer_size) {
            buffer_size *= 2;
            char *new_buffer = realloc(response_buffer, buffer_size);
            if (!new_buffer) {
                perror("realloc");
                free(response_buffer);
                close(fd_fifo_user);
                exit(EXIT_FAILURE);
            }
            response_buffer = new_buffer;
        }
    }

    if (bytes_read < 0) {
        perror("read");
    }

    response_buffer[total_read] = '\0';
    // si la respuesta es del monitor, usuario ejecutor lanza notificación

    printf("%s", response_buffer);
    close(fd_fifo_user);
    free(response_buffer);
}

/*
Writes the user's input command to the fifo_deamon and the triggers the wait for the response.
It uses the args from the program and adds at the end the fifo_user_path the deamon should write the
response.
@param int argc
@param char** argv
@param char* fifo_user_path
*/
void write_cmd_to_cmd_pipe(int argc, char *argv[], char *balrog_dir_user_path, char *fifo_user_path,
                           unsigned long int uid) {
    int fd_cmd_pipe = open(daemon_info.cmd_pipe, O_WRONLY);
    if (fd_cmd_pipe == -1) {
        perror("Error opening cmd pipe");
        return;
    }

    char cmd_line[PIPE_BUF] = {0};
    int offset = 0;

    // Añadir argumentos
    for (int i = 1; i < argc; i++) {
        int len = snprintf(cmd_line + offset, PIPE_BUF - offset, "%s ", argv[i]);
        if (len < 0 || len >= PIPE_BUF - offset) {
            fprintf(stderr, "Command line too long (argv)\n");
            close(fd_cmd_pipe);
            return;
        }
        offset += len;
    }

    // Añadir balrog_dir_user_path
    int len = snprintf(cmd_line + offset, PIPE_BUF - offset, "%s ", balrog_dir_user_path);
    if (len < 0 || len >= PIPE_BUF - offset) {
        fprintf(stderr, "Command line too long (balrog_dir)\n");
        close(fd_cmd_pipe);
        return;
    }
    offset += len;

    // Añadir fifo_user_path
    len = snprintf(cmd_line + offset, PIPE_BUF - offset, "%s ", fifo_user_path);
    if (len < 0 || len >= PIPE_BUF - offset) {
        fprintf(stderr, "Command line too long (fifo_path)\n");
        close(fd_cmd_pipe);
        return;
    }
    offset += len;

    // Añadir uid
    len = snprintf(cmd_line + offset, PIPE_BUF - offset, "%lu\n", uid);
    if (len < 0 || len >= PIPE_BUF - offset) {
        fprintf(stderr, "Command line too long (uid)\n");
        close(fd_cmd_pipe);
        return;
    }
    offset += len;

    printf("cmd_line => %s\n", cmd_line);

    int bytes_written = write(fd_cmd_pipe, cmd_line, strlen(cmd_line));
    printf("bytes_written => %d\n", bytes_written);
    if (bytes_written == -1) perror("Couldn't write command into cmd pipe");

    close(fd_cmd_pipe);
    wait_and_print_daemon_response(fifo_user_path);
}

/*
Deamon always executes this function.
It process the command written to the cnd_pipe ak fifo_deamon that does the work for
the Inter Process Communication.
@param int argc
@param char** argv
 */
void processing_cmd(int argc, char *argv[]) {
    int opt;

    // We use the processing_cmd function for processing the command line and
    // for commands from the DAEMON_CMD_PIPE_NAME
    // For this we use the getopt_long function several times
    // to work properly, we must reset the optind
    optind = 0;

    int fd_fifo_user = -1;
    if (argc > 1) {
        fd_fifo_user = open(argv[argc - 2], O_WRONLY);
    }
    printf("fd_fifo_user => %d\n", fd_fifo_user);

    if (argc > 1) {
        char *fifo_user_path = argv[argc - 2];
        printf("char *fifo_user_path = argv[argc - 2]; => %s\n", fifo_user_path);
    }

    while ((opt = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        printf("opt => %d\n", opt);
        switch (opt) {
            case cmd_opt_help:
                // the last arg is always the fifo_user_path to write the output
                if (fd_fifo_user > 0) {
                    if (write(fd_fifo_user, help_str, strlen(help_str)) == -1) {
                        printf("Error printing help: %m\n");
                    }
                } else {
                    // If no arguments are provided, print to stdout
                    printf("No arguments provided, printing help to stdout:\n");
                    puts(help_str);
                }
                exit_if_not_daemonized(EXIT_SUCCESS);
                break;

            case cmd_opt_version:
                char *version = DAEMON_NAME " version " DAEMON_VERSION_STR "\n";
                if (fd_fifo_user > 0) {
                    write(fd_fifo_user, version, strlen(version));
                } else {
                    // If no arguments are provided, print to stdout
                    puts(version);
                }

                exit_if_not_daemonized(EXIT_SUCCESS);
                break;

            case cmd_enumerate:
                puts("Enumerating devices...");
                do_enumerate(fd_fifo_user);
                exit_if_not_daemonized(EXIT_SUCCESS);
                break;

            case cmd_print_udev_vars:
                puts("Printing udev variables for debugging...");
                printf("Udev context: %p\n", (void *)udev);
                printf("Udev monitor: %p\n", (void *)monitor);
                break;

            case cmd_start_monitor:
                if (fd_fifo_user > 0) {
                    char pid_monitor_file[50];
                    snprintf(pid_monitor_file, 50, "%s/monitor_%s.pid", argv[argc - 3],
                             argv[argc - 1]);
                    if (access(pid_monitor_file, F_OK) >= 0) {
                        char msg_monitor[60] =
                            "Monitoreo de dispositivos USB ya se encuentra activo...\n";
                        if (write(fd_fifo_user, msg_monitor, strlen(msg_monitor)) < 0) {
                            printf("Couldn't write on fd_fifo_user: %m\n");
                        }
                        break;
                    }
                    create_pid_file(daemon_info.monitor_pid_file);

                    if (pipe(exit_pipe) < 0) {
                        perror(
                            "creating one way communication of type pipe for pthread_monitoring");
                        exit(EXIT_FAILURE);
                    }
                    if (pthread_create(&pthread_monitoring, NULL, start_monitoring,
                                       (void *)(intptr_t)fd_fifo_user) != 0)
                        daemon_error_exit("Can't create thread_cmd_pipe: %m\n");
                } else {
                    printf("Couldn't open fd_fifo_user: %m\n");
                }

                exit_if_not_daemonized(EXIT_SUCCESS);
                break;

            case cmd_stop_monitor:
                puts("Stopping to monitor USB devices...");
                char pid_monitor_file[50];
                snprintf(pid_monitor_file, 50, "%s/monitor_%s.pid", argv[argc - 3], argv[argc - 1]);
                if (access(pid_monitor_file, F_OK) >= 0) {
                    unlink(pid_monitor_file);
                }
                stop_monitoring();
                printf("Monitor udev pointer value: %p", (void *)monitor);
                exit_if_not_daemonized(EXIT_SUCCESS);
                break;

            // daemon options
            case cmd_opt_no_chdir:
                daemon_info.no_chdir = 1;
                break;

            case cmd_opt_no_fork:
                daemon_info.no_fork = 1;
                break;

            case cmd_opt_no_close:
                daemon_info.no_close_stdio = 1;
                break;

            case cmd_opt_pid_file:
                daemon_info.pid_file = optarg;
                break;

            case cmd_opt_log_file:
                daemon_info.log_file = optarg;
                break;

            case cmd_opt_monitor_log_file:
                daemon_info.monitor_log_file = optarg;
                break;

            case cmd_opt_cmd_pipe:
                daemon_info.cmd_pipe = optarg;
                break;

            default:
                puts("for more detail see help\n\n");
                exit_if_not_daemonized(EXIT_FAILURE);
                break;
        }
    }
    close(fd_fifo_user);
    return;
}

/*
Loop that the pthread is gonna be running to get commands written in the cmd_pipe (fifo)
*/
static void *cmd_pipe_thread(void *thread_arg) {
    int fd;
    int argc;
    char *arg;
    char **argv;
    char *cmd_pipe_buf;

    pthread_detach(pthread_self());
    unlink(daemon_info.cmd_pipe);

    argv = (char **)malloc(PIPE_BUF * sizeof(char *));
    if (!argv) daemon_error_exit("Can't get mem for argv (CMD_PIPE): %m\n");

    cmd_pipe_buf = (char *)malloc(PIPE_BUF);
    if (!cmd_pipe_buf) daemon_error_exit("Can't get mem for cmd_pipe_buf: %m\n");

    if (mkfifo(daemon_info.cmd_pipe, 0622) != 0) daemon_error_exit("Can't create CMD_PIPE: %m\n");

    fd = open(daemon_info.cmd_pipe,
              O_RDONLY | O_NONBLOCK);  // ok, O_NONBLOCK means that its gonna fail instead of trying
                                       // again the operation and the failure needs to be handled
    if (fd == -1) daemon_error_exit("Can't open CMD_PIPE: %m\n");

    while (1) {
        memset(cmd_pipe_buf, 0, PIPE_BUF);

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);
        select(fd + 1, &read_fds, NULL, NULL, NULL);

        ssize_t bytes_read = read(fd, cmd_pipe_buf, PIPE_BUF);

        if (bytes_read == -1) daemon_error_exit("read CMD_PIPE return -1: %m\n");

        if (bytes_read == 0) {
            // Fin de archivo: todos los writers cerraron
            close(fd);
            printf("cmd_pipe_thread() => daemon_info.cmd_pipe => %s\n", daemon_info.cmd_pipe);
            fd = open(daemon_info.cmd_pipe, O_RDONLY);
            if (fd == -1) daemon_error_exit("Can't reopen CMD_PIPE: %m\n");
            continue;
        }

        argc = 1;  // see getopt_long function
        arg = strtok(cmd_pipe_buf, " \t\n");

        while ((arg != NULL) && (argc < PIPE_BUF)) {
            printf("arg => %s\n", arg);
            argv[argc++] = arg;
            arg = strtok(NULL, " \t\n");
        }

        if (argc >= 1) processing_cmd(argc, argv);
    }

    return NULL;
}

void init_cmd_line(void *data) {
    init_signals();

    if (pthread_create(&pthread_cmd_pipe, NULL, cmd_pipe_thread, NULL) != 0)
        daemon_error_exit("Can't create thread_cmd_pipe: %m\n");

    // Here is your code to initialize
    // start_monitoring();
}