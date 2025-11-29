/*
 * Based on daemonization template by KoynovStas.
 * Original source: https://github.com/KoynovStas/daemon_templates.git
 * Licensed under the BDS-3 License.
 */

#include "daemon/cmd_opt.h"

#include <getopt.h>
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

#include "common/init_config.h"
#include "daemon/balrog_udev.h"
#include "daemon/daemon.h"

pthread_t pthread_cmd_pipe;
pthread_t pthread_user_end_monitoring;

// Define the help string for balrog-usb-utility
const char* help_str = DAEMON_NAME
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
    "  -s   --shell <devpath>           Run sandbox for devpath and get an interactive shell\n"
    "  -n   --nodes                     Print devnodes attached"
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
    cmd_shell_dev = 's',
    cmd_print_nodes = 'n',

    // daemon options (start from a value outside ASCII range)
    cmd_opt_no_chdir = 1000,
    cmd_opt_no_fork,
    cmd_opt_no_close,
    cmd_opt_pid_file,
    cmd_opt_log_file,
    cmd_opt_monitor_log_file,
    cmd_opt_cmd_pipe
};

static const char* short_opts = "hvemwpsn";
static const struct option long_opts[] = {
    {"version", no_argument, NULL, cmd_opt_version},
    {"help", no_argument, NULL, cmd_opt_help},
    {"enumerate", no_argument, NULL, cmd_enumerate},
    {"start-monitor", no_argument, NULL, cmd_start_monitor},
    {"stop-monitor", no_argument, NULL, cmd_stop_monitor},
    {"print-udev-vars", no_argument, NULL, cmd_print_udev_vars},
    {"shell", no_argument, NULL, cmd_shell_dev},
    {"nodes", no_argument, NULL, cmd_print_nodes},

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
    // here i must delete balrog.cmd

    unlink(daemon_info.pid_file);
    // it's not deleting the cmd_pipe because userside it's still using it
    //
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
DEAMON SIDE
Deamon always executes this function.
It process the command written to the cnd_pipe ak fifo_deamon that does the work for
the Inter Process Communication.
@param int argc
@param char** argv
 */
void processing_cmd(int argc, char* argv[]) {
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

    if (argc > 1) {
        char* fifo_user_path = argv[argc - 2];  // it must be -2 for fifo_path
    }

    while ((opt = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        fprintf(stderr, "opt => %d\n", opt);
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
                char* version = DAEMON_NAME " version " DAEMON_VERSION_STR "\n";
                if (fd_fifo_user > 0) {
                    write(fd_fifo_user, version, strlen(version));
                } else {
                    // If no arguments are provided, print to stdout
                    printf("No fifo user: \n%s\n", version);
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
                printf("Udev context: %p\n", (void*)udev);
                printf("Udev monitor: %p\n", (void*)monitor);
                break;

            case cmd_start_monitor:
                if (fd_fifo_user > 0) {
                    // char pid_monitor_file[50];
                    // snprintf(pid_monitor_file, 50, "%s/monitor_%s.pid", argv[argc - 3],
                    //          argv[argc - 1]);
                    if (access(daemon_info.monitor_pid_file, F_OK) >= 0) {
                        unlink(daemon_info.monitor_pid_file);
                    }
                    create_pid_file(daemon_info.monitor_pid_file);

                    if (pipe(exit_pipe) < 0) {
                        perror(
                            "creating one way communication of type pipe for "
                            "pthread_monitoring");
                        exit(EXIT_FAILURE);
                    }
                    if (pthread_create(&pthread_monitoring, NULL, start_monitoring,
                                       (void*)(intptr_t)fd_fifo_user) != 0)
                        error_exit("Can't create thread_cmd_pipe: %m\n");
                } else {
                    printf("Couldn't open fd_fifo_user: %m\n");
                }

                exit_if_not_daemonized(EXIT_SUCCESS);
                break;

            case cmd_stop_monitor:
                puts("Stopping to monitor USB devices...");
                // char pid_monitor_file[50];
                // snprintf(pid_monitor_file, 50, "%s/monitor_%s.pid", argv[argc - 3], argv[argc
                // - 1]);
                if (access(daemon_info.monitor_pid_file, F_OK) < 0) {
                    char* msg_not_monitor_yet =
                        "It doesn't exists a monitor yet...\nYou can create one doing balrog "
                        "-m\n";
                    if (write(fd_fifo_user, msg_not_monitor_yet, strlen(msg_not_monitor_yet)) < 0) {
                        perror("Error writting ...");
                    }
                    break;
                }
                unlink(daemon_info.monitor_pid_file);
                stop_monitoring();
                printf("Monitor udev pointer value: %p\n", (void*)monitor);
                exit_if_not_daemonized(EXIT_SUCCESS);
                break;

            case cmd_print_nodes:
                fprintf(stderr, "devs_paths = %p\n", (void*)devs_paths);
                fprintf(stderr, "devs_paths_index => %d", devs_paths_index);
                char output[4096];
                output[0] = '\0';
                int offset = 0;

                for (int i = 0; i < devs_paths_index; i++) {
                    int written =
                        snprintf(output + offset, sizeof(output) - offset, "%s\n", devs_paths[i]);

                    if (written < 0 || written >= sizeof(output) - offset) {
                        // Se llenó el buffer
                        fprintf(stderr, "balrog --shell: Se llenó el buffer");
                        break;
                    }

                    offset += written;
                }

                if (write(fd_fifo_user, output, offset) < 0) {
                    fprintf(stderr, "balrog --shell Error writting...");
                }
                fprintf(stderr, "---- dumping devs_paths ----\n");
                for (int i = 0; i < devs_paths_index; i++) {
                    fprintf(stderr, "[%d] ptr=%p\n", i, (void*)devs_paths[i]);
                    if (devs_paths[i]) fprintf(stderr, "val='%s'\n", devs_paths[i]);
                }
                fprintf(stderr, "---------------------------\n");
                break;

            case cmd_shell_dev:
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
                strncpy(daemon_info.pid_file, optarg, sizeof(daemon_info.pid_file) - 1);
                break;

            case cmd_opt_log_file:
                strncpy(daemon_info.log_file, optarg, sizeof(daemon_info.log_file) - 1);
                break;

            case cmd_opt_monitor_log_file:
                strncpy(daemon_info.monitor_log_file, optarg,
                        sizeof(daemon_info.monitor_log_file) - 1);
                break;

            case cmd_opt_cmd_pipe:
                strncpy(daemon_info.cmd_pipe, optarg, sizeof(daemon_info.cmd_pipe) - 1);
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
static void* cmd_pipe_thread(void* thread_arg) {
    int fd;
    int argc;
    char* arg;
    char** argv;
    char* cmd_pipe_buf;

    unlink(daemon_info.cmd_pipe);

    argv = (char**)malloc(PIPE_BUF * sizeof(char*));
    if (!argv) error_exit("balrogd", "Can't get mem for argv (CMD_PIPE)");

    cmd_pipe_buf = (char*)malloc(PIPE_BUF);
    if (!cmd_pipe_buf) error_exit("balrogd", "Can't get mem for cmd_pipe_buf");

    if (mkfifo(daemon_info.cmd_pipe, 0622) != 0) error_exit("balrogd", "Can't create CMD_PIPE");

    fd = open(daemon_info.cmd_pipe,
              O_RDONLY | O_NONBLOCK);  // ok, O_NONBLOCK means that its gonna fail instead of trying
                                       // again the operation and the failure needs to be handled
    if (fd == -1) error_exit("balrogd", "Can't open CMD_PIPE");

    while (1) {
        memset(cmd_pipe_buf, 0, PIPE_BUF);

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);
        select(fd + 1, &read_fds, NULL, NULL, NULL);

        ssize_t bytes_read = read(fd, cmd_pipe_buf, PIPE_BUF);

        if (bytes_read == -1) error_exit("balrogd", "read CMD_PIPE return -1");

        if (bytes_read == 0) {
            // Fin de archivo: todos los writers cerraron
            close(fd);
            printf("cmd_pipe_thread() => daemon_info.cmd_pipe => %s\n", daemon_info.cmd_pipe);
            fd = open(daemon_info.cmd_pipe, O_RDONLY);
            if (fd == -1) error_exit("balrogd", "Can't reopen CMD_PIPE");
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

void init_cmd_line(void* data) {
    init_signals();

    if (pthread_create(&pthread_cmd_pipe, NULL, cmd_pipe_thread, NULL) != 0)
        error_exit("balrogd", "Can't create thread_cmd_pipe");

    // Here is your code to initialize
    // start_monitoring();
}