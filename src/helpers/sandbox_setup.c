#define _GNU_SOURCE
#include <errno.h>
#include <pty.h>  //pseudoterminal
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <unistd.h>

static void die_errno(const char* msg) {
    fprintf(stderr, "%s: errno=%d (%s)\n", msg, errno, strerror(errno));
    exit(1);
}

int main(int argc, char** argv) {
    // argv[1] is the mounted device path ("/mnt/usb") created by helper
    if (argc < 2) {
        return -1;
    }

    const char* usb_mount = argv[1];

    if (geteuid() == 0) {
        fprintf(stderr, "Warning: still running as root\n");
    }

    // drop capabilities
    prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);

    // If a command is provided, exec it, otherwise spawn a shell
    // if (argc >= 3) {
    //     execv(argv[2], &argv[2]);
    //     die_errno("execv command failed");
    // }

    char* shell = "/bin/sh";
    char* sargv[] = {shell, NULL};
    execv(shell, sargv);

    die_errno("exec /bin/sh failed");
    return 0;
}