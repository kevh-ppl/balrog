#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#include "balrog_udev.h"
#include "cmd_opt.h"
#include "daemon.h"

// Forzar reinicialización de streams
__attribute__((constructor)) void refresh_streams();

int main(int argc, char *argv[]) {
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