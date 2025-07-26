#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#include "balrog_udev.h"
#include "cmd_opt.h"
#include "daemon.h"

int main(int argc, char *argv[]) {
    processing_cmd(argc, argv);
    daemonize2(init_cmd_line, NULL);

    while (!daemon_info.terminated) {
        // Here а routine of daemon

        printf("%s: daemon is run\n", DAEMON_NAME);
        sleep(10);
    }

    return EXIT_SUCCESS;  // good job (we interrupted (finished) main loop)
}