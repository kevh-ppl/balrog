#include <errno.h>
#include <libudev.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "balrog_udev.h"
#include "cmd_opt.h"
#include "daemon.h"

// Forzar reinicialización de streams

int main(int argc, char *argv[]) {
    int fd_cmd_pipe = open(daemon_info.cmd_pipe, O_RDWR);
    if (fd_cmd_pipe != -1) {
        // puts("Ya demonizado...");
        uid_t uid = getuid();
        if (!uid) printf("UID null, so we're 'monky'\n");

        struct passwd *pw_user = getpwuid(uid);  // get user info for the current user
        if (!pw_user) {
            fprintf(stderr, "Couldn't get user info for UID %d: %m\n", uid);
            _exit(EXIT_FAILURE);
        }
        // printf("Current directory: %s\n", pw_user->pw_dir);

        char fifo_user_path[1024];
        snprintf(fifo_user_path, sizeof(fifo_user_path), "%s/.balrog/fifo_%d", pw_user->pw_dir,
                 pw_user->pw_uid);

        // Creation of balrog dir
        char *balrog_dir = pw_user->pw_dir;
        strcat(pw_user->pw_dir, "/.balrog");
        struct stat st_balrog;
        if (stat(balrog_dir, &st_balrog) < 0) {
            // if it doesn't exists, i must create it
            if (mkdir(balrog_dir, 0740) < 0) {
                printf("Couldn't mkdir dir balrog: %m...\n");
                _exit(EXIT_FAILURE);
            }
        }
        // Creation of balrog dir

        // printf("fifo_user_path => %s\n", fifo_user_path);

        // Crear FIFO si no existe
        if (access(fifo_user_path, F_OK) == -1) {
            if (mkfifo(fifo_user_path, 0600) == -1) {
                fprintf(stderr, "Couldn't create FIFO %s: %s\n", fifo_user_path, strerror(errno));
                _exit(EXIT_FAILURE);
            }
        }

        int fd_fifo_user = open(fifo_user_path, O_RDWR, 0644);
        if (fd_fifo_user < 0) {
            printf("Couldn't open fifo_user: %m...\n");
            _exit(EXIT_FAILURE);
        }

        // si ya esta demonizado, debo de mandar la salida del daemon al FIFO del user
        // por lo que:
        // primero creo el FIFO para el usuario especifico y cada que este ejecute un comando
        // debo verificar que ya este creado el FIFO_USER, sino se crea
        // Ejecutar comando y salida a FIFO_USER
        // Leer FIFO y print a consola

        close(fd_cmd_pipe);
        write_cmd_to_cmd_pipe(argc, argv, fifo_user_path);
        return 0;
    }
    processing_cmd(argc, argv);  // first use for initialize the daemon_info structure
    // init_cmd_line initializes the thread for the command pipe
    // and sets the signal handlers.
    daemonize2(init_cmd_line, NULL);

    printf("%s: daemon is running\n", DAEMON_NAME);

    if (init_udev_context()) daemon_error_exit("Failed to initialize udev context\n");

    printf("udev => %p\n", (void *)udev);

    int monitor_fd = set_monitor();
    if (monitor_fd < 0) daemon_error_exit("Failed to set udev monitor\n");
    while (!daemon_info.terminated) {
        if (start_monitoring(monitor_fd) < 0) {
            daemon_error_exit("Failed to start monitoring USB IO events\n");
        }
    }
    return EXIT_SUCCESS;  // good job (we interrupted (finished) main loop)
}