#include <glib-2.0/glib.h>
#include <libnotify/notify-features.h>
#include <libnotify/notify.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "daemon.h"

void *start_user_end_monitoring(void *args) {
    // pthread_detach(pthread_self());
    printf("User end monitoring activated...\n");

    setenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/run/user/1000/bus", 1);
    if (!notify_init("Balrog")) {
        perror("notify_init");
    }

    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return NULL;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/var/run/balrog/balrogd.sock");

    while (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        if (errno == ENOENT) {
            usleep(100000);  // esperar 100ms a que el socket exista
            continue;
        } else {
            perror("connect");
            close(sock_fd);
            return NULL;
        }
    }

    NotifyNotification *new_noti;
    GError *errors = NULL;
    char buffer[512];

    printf("Getting into loop...\n");
    while (1) {
        ssize_t n = read(sock_fd, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            printf("Evento recibido: %s", buffer);

            new_noti = notify_notification_new("Balrog", buffer, "mono_autorizo_54px");
            if (!notify_notification_show(new_noti, &errors)) {
                if (errors) {
                    fprintf(stderr, "notify error: %s\n", errors->message);
                    g_error_free(errors);
                    errors = NULL;
                }
            }
            g_object_unref(G_OBJECT(new_noti));
        } else if (n == 0) {
            // desconexión del daemon
            break;
        } else if (errno != EINTR) {
            perror("read");
            break;
        }
    }

    notify_uninit();
    close(sock_fd);
    return NULL;
}
