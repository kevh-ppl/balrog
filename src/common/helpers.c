#define _XOPEN_SOURCE 700  // so the F_TLOCK and lockf are exposed
#include <errno.h>
#include <error.h>   //_exit
#include <stdarg.h>  //va
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  //strlen
#include <sys/file.h>
#include <unistd.h>  //posix std, F_TLOCK

/*
 * This function is used to exit the daemon with an error message.
 * It uses variable arguments to format the error message.
 * It will print the error message to stderr and then exit the daemon.
 * The exit status will be EXIT_FAILURE.
 */
void error_exit(const char* format, ...) {
    va_list ap;

    if (format && *format) {
        va_start(ap, format);
        fprintf(stderr, "%s: ", "balrog");
        vfprintf(stderr, format, ap);
        size_t len = strlen(format);
        if (format[len - 1] != '\n') fprintf(stderr, "\n");
        va_end(ap);
    }

    _exit(EXIT_FAILURE);
}

int create_pid_file(const char* pid_file_name) {
    int fd;
    const int BUF_SIZE = 32;
    char pid_str[BUF_SIZE];

    if (!pid_file_name) {
        fprintf(stderr, "PID file name is NULL\n");
        return -1;
    }

    fd = open(pid_file_name, O_RDWR | O_CREAT, 0644);

    if (fd < 0) {
        errno = EINVAL;
        fprintf(stderr, "Failed to open PID file '%s': %s\n", pid_file_name, strerror(errno));
        return -1;
    }

    // Try to lock the file to ensure only one instance of the daemon can run
    if (lockf(fd, F_TLOCK, 0) == -1) {
        close(fd);
        return -1;
    }

    // Clear the file content before writing the new PID
    if (ftruncate(fd, 0) != 0) {
        close(fd);
        return -1;  // Could not truncate on PID file
    }

    snprintf(pid_str, BUF_SIZE, "%ld\n", (long)getpid());
    if (write(fd, pid_str, strlen(pid_str)) != (int)strlen(pid_str)) {
        close(fd);
        return -1;  // Could not write PID to PID file
    }

    return fd;
}