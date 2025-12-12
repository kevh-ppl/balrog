#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>

#include "common/init_config.h"
#include "daemon/cmd_opt.h"
#include "daemon/daemon.h"

int main(int argc, char* argv[]) {
    init_config();

    if (!notify_init("balrog")) {
        perror("Init notify");
        return -1;
    }
    int fd_cmd_pipe = open(daemon_info.cmd_pipe, O_WRONLY);

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
    return EXIT_SUCCESS;
}