#include <errno.h>
#include <libudev.h>
#include <linux/limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "acl_balrog.h"
#include "balrog_udev.h"
#include "cmd_opt.h"
#include "daemon.h"

int main(int argc, char *argv[]) {
    if (!notify_init("balrog")) {
        perror("Init notify");
        return -1;
    }
    int fd_cmd_pipe = open(daemon_info.cmd_pipe, O_WRONLY);
    if (demonized() == 1) {
        puts("Ya demonizado...");
        uid_t uid = getuid();
        if (!uid) printf("UID null, so we're 'monky'\n");

        struct passwd *pw_user = getpwuid(uid);  // get user info for the current user
        if (!pw_user) {
            fprintf(stderr, "Couldn't get user info for UID %d: %m\n", uid);
            _exit(EXIT_FAILURE);
        }
        // printf("Current directory: %s\n", pw_user->pw_dir);

        char fifo_user_name[18];
        snprintf(fifo_user_name, sizeof(fifo_user_name), "fifo_%lu", (unsigned long int)uid);

        // Creation of balrog dir
        char users_balrog_dir[PATH_MAX];
        snprintf(users_balrog_dir, sizeof(users_balrog_dir), "%s/.balrog", pw_user->pw_dir);

        struct stat st_balrog;
        if (stat(users_balrog_dir, &st_balrog) < 0) {
            // if it doesn't exists, i must create it
            if (mkdir(users_balrog_dir, 0740) < 0) {
                printf("Couldn't mkdir dir balrog: %m...\n");
                _exit(EXIT_FAILURE);
            }
        }
        // Creation of balrog dir
        char fifo_user_path[strlen(users_balrog_dir) + strlen(fifo_user_name) + 2];
        snprintf(fifo_user_path, sizeof(fifo_user_path), "%s/%s", users_balrog_dir, fifo_user_name);

        printf("fifo_user_path => %s\n", fifo_user_path);
        daemon_info.fifo_user_path = fifo_user_path;

        // Crear FIFO si no existe
        if (access(fifo_user_path, F_OK) == -1) {
            if (mkfifo(fifo_user_path, 0420) == -1) {
                fprintf(stderr, "Couldn't create FIFO %s: %s\n", fifo_user_path, strerror(errno));
                _exit(EXIT_FAILURE);
            }
            // if (set_acl_user_rw(fifo_user_path, "balrogd") == -1) {
            //     fprintf(stderr, "Failed to set ACL for balrogd on %s\n", fifo_user_path);
            //     _exit(EXIT_FAILURE);
            // }

            char cmd_home_dir[PATH_MAX];
            snprintf(cmd_home_dir, sizeof(cmd_home_dir), "setfacl -m u:balrogd:rx %s",
                     pw_user->pw_dir);
            int ret_home_dir = system(cmd_home_dir);
            if (ret_home_dir != 0) {
                fprintf(stderr, "Error al ejecutar comando setfacl para %s\n", users_balrog_dir);
            }

            char cmd_balrog_dir[PATH_MAX + 30];
            snprintf(cmd_balrog_dir, sizeof(cmd_balrog_dir), "setfacl -m u:balrogd:rx %s",
                     users_balrog_dir);

            int ret_balrog = system(cmd_balrog_dir);
            if (ret_balrog != 0) {
                fprintf(stderr, "Error al ejecutar comando setfacl para %s\n", users_balrog_dir);
            }
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "setfacl -m u:balrogd:w- %s", fifo_user_path);
            int ret = system(cmd);
            if (ret != 0) {
                fprintf(stderr, "Error al ejecutar comando setfacl para %s\n", users_balrog_dir);
            }
        }

        // que wey, ya le estoy pasando el path, auque podria pasarle el fd de una vez
        //  int fd_fifo_user = open(fifo_user_path, O_RDWR, 0644);  // client opens the FIFO
        //  if (fd_fifo_user < 0) {
        //      printf("Couldn't open fifo_user: %m...\n");
        //      _exit(EXIT_FAILURE);
        //  }

        // si ya esta demonizado, debo de mandar la salida del daemon al FIFO del user
        // por lo que:
        // primero creo el FIFO para el usuario especifico y cada que este ejecute un comando
        // debo verificar que ya este creado el FIFO_USER, sino se crea
        // Ejecutar comando y salida a FIFO_USER
        // Leer FIFO y print a consola

        close(fd_cmd_pipe);
        write_cmd_to_cmd_pipe(argc, argv, users_balrog_dir, fifo_user_path, (unsigned long int)uid);
        return 0;
    }
    processing_cmd(argc, argv);  // first use for initialize the daemon_info structure
    // init_cmd_line initializes the thread for the command pipe
    // and sets the signal handlers.
    daemonize2(init_cmd_line, NULL);

    printf("%s: daemon is running\n", DAEMON_NAME);
    pthread_join(pthread_cmd_pipe, NULL);
    // if (init_udev_context()) daemon_error_exit("Failed to initialize udev context\n");

    // printf("udev => %p\n", (void *)udev);

    // int monitor_fd = set_monitor();
    // if (monitor_fd < 0) daemon_error_exit("Failed to set udev monitor\n");
    // while (!daemon_info.terminated) {
    //     // if (start_monitoring(monitor_fd) < 0) {
    //     //     daemon_error_exit("Failed to start monitoring USB IO events\n");
    //     // }
    //     sleep(1);
    // }
    notify_uninit();
    return EXIT_SUCCESS;  // good job (we interrupted (finished) main loop)
}