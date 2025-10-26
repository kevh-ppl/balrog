#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

#include "client/user_end_monitor.h"
#include "common/helpers.h"
#include "common/init_config.h"

#define PIPE_BUF 4096

/*
USER SIDE
Waits for the deamon response to the cmd
@param char* fifo_user_path
*/
static void wait_and_print_daemon_response(char* fifo_user_path) {
    char* response_buffer = malloc(PIPE_BUF);
    if (!response_buffer) {
        error_exit("malloc");
    }
    size_t buffer_size = PIPE_BUF;
    int fd_fifo_user = open(fifo_user_path, O_RDONLY);

    if (fd_fifo_user < 0) {
        free(response_buffer);
        error_exit("open fifo_user_path");
    }

    size_t total_read = 0;
    ssize_t bytes_read;
    while ((bytes_read = read(fd_fifo_user, response_buffer + total_read,
                              buffer_size - total_read - 1)) > 0) {
        total_read += bytes_read;
        if (total_read + 1 >= buffer_size) {
            buffer_size *= 2;
            char* new_buffer = realloc(response_buffer, buffer_size);
            if (!new_buffer) {
                free(response_buffer);
                close(fd_fifo_user);
                error_exit("realloc");
            }
            response_buffer = new_buffer;
        }
    }

    if (bytes_read < 0) {
        error_exit("read");
    }

    response_buffer[total_read] = '\0';
    // si la respuesta es del monitor, usuario ejecutor lanza notificación

    printf("%s", response_buffer);
    close(fd_fifo_user);
    free(response_buffer);
    return;
}

/*
USER SIDE
Writes the user's input command to the fifo_deamon and the triggers the wait for the response.
It uses the args from the program and adds at the end the fifo_user_path the deamon should write the
response.
@param int argc
@param char** argv
@param char* fifo_user_path
*/
void write_cmd_to_cmd_pipe(int argc, char* argv[], char* balrog_dir_user_path, char* fifo_user_path,
                           unsigned long int uid) {
    int fd_cmd_pipe = open(daemon_info.cmd_pipe, O_WRONLY);
    if (fd_cmd_pipe == -1) {
        error_exit("Error opening cmd pipe");
    }

    char cmd_line[PIPE_BUF] = {0};
    int offset = 0;

    // si opcion -m
    int start_monitor = -1;
    char* monitor_pid_name = "monitor.pid";
    char monitor_pid_path[strlen(balrog_dir_user_path) + strlen(monitor_pid_name) + 2];
    snprintf(monitor_pid_path, sizeof(monitor_pid_path), "%s/%s", balrog_dir_user_path,
             monitor_pid_name);
    printf("ahahah => %s\n", monitor_pid_path);
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--start-monitor") == 0) {
            // checar si existe $HOME/.balrog/monitor.pid
            // si existe se debe lanzar mensaje de que ya se está monitoreando,
            // si no hay que proceder a crear $HOME/.balrog/monitor.pid y
            // hacer doble fork()

            if (access(monitor_pid_path, F_OK) >= 0) {
                printf("Already exists a monitor\n");
                continue;
            }
            start_monitor = 0;
        }
        if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--stop-monitor") == 0) {
            // si existe $HOME/.balrog/monitor.pid
            // proceder escribiendo en cmd_pipe
            // si no hay que lanzar mensaje de que no hay proceso de monitoreo que detener
            if (access(monitor_pid_path, F_OK) < 0) {
                printf(
                    "It doesn't exists a monitor\nYou can create one by doing balrog -m or balrog "
                    "--start-monitor\n");
                continue;
            }
            int fd_monitor_pid_user_end = open(monitor_pid_path, O_RDONLY);
            if (fd_monitor_pid_user_end < 0) {
                perror("Error opening monitor.pid");
                return;
            }

            char buf[32] = {0};
            ssize_t n = read(fd_monitor_pid_user_end, buf, sizeof(buf) - 1);
            close(fd_monitor_pid_user_end);

            if (n <= 0) {
                perror("Error reading monitor.pid");
                return;
            }

            pid_t pid_monitor_user_end = (pid_t)strtol(buf, NULL, 10);
            printf("pid buf => %s | pid pid => %d\n", buf, pid_monitor_user_end);

            if (kill(pid_monitor_user_end, SIGTERM) < 0) {
                perror("Error sending SIGTERM to monitor");
                return;
            }
            unlink(monitor_pid_path);
        }

        if (start_monitor == 0) {
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            if (pid == 0) {
                if (setsid() < 0) error_exit("Can't setsid: %m\n");
                pid = fork();
                if (pid < 0) error_exit("Segundo fork falló: %m\n");
                if (pid > 0) exit(0);
                if (chdir("/") != 0) error_exit("Can't chdir: %m\n");
                freopen("/dev/null", "r", stdin);
                freopen("/dev/null", "w", stdout);
                freopen("/dev/null", "w", stderr);
                if (create_pid_file(monitor_pid_path) < 0) {
                    perror("Error creando monitor.pid");
                    return;
                }
                start_user_end_monitoring(NULL);
                exit(0);
            }
        }

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
}