#include "common/init_config.h"

#include <ctype.h>   //isspace
#include <stdarg.h>  //vector args
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  //strlen
#include <unistd.h>

#define MAX_LINE 256

// client only needs access to the monitor socket patb and cmd pipe path

struct daemon_info_t daemon_info = {
    // flag will be set in finale function daemonize()
    .terminated = 0,
    .daemonized = 0,

#ifdef DAEMON_NO_CHDIR
    .no_chdir = DAEMON_NO_CHDIR,
#else
    .no_chdir = 0,
#endif

#ifdef DAEMON_NO_FORK
    .no_fork = DAEMON_NO_FORK,
#else
    .no_fork = 0,
#endif

#ifdef DAEMON_NO_CLOSE_STDIO
    .no_close_stdio = DAEMON_NO_CLOSE_STDIO,
#else
    .no_close_stdio = 0,
#endif

#ifdef DAEMON_PID_FILE_NAME
    .pid_file = DAEMON_PID_FILE_NAME,
#endif

#ifdef DAEMON_LOG_FILE_NAME
    .log_file = DAEMON_LOG_FILE_NAME,
#endif

#ifdef DAEMON_MONITOR_LOG_FILE_NAME
    .monitor_log_file = DAEMON_MONITOR_LOG_FILE_NAME,
#endif

#ifdef DAEMON_MONITOR_PID_FILE_NAME
    .monitor_pid_file = DAEMON_MONITOR_PID_FILE_NAME,
#endif

#ifdef DAEMON_MONITOR_SOCKET_FILE_NAME
    .monitor_socket_file = DAEMON_MONITOR_SOCKET_FILE_NAME,
#endif

#ifdef DAEMON_CMD_PIPE_NAME
    .cmd_pipe = DAEMON_CMD_PIPE_NAME,
#endif
    .fifo_user_path = "",
    .daemon_group = 0,
    .daemon_user = 0,
    .default_log_dir = "/var/log/balrogd/",
    .default_run_dir = "/var/run/balrogd/"};

int main() {
    init_config();
    printf("daemon_info.pid_file => %s\n", daemon_info.pid_file);
    printf("daemon_info.cmd_pipe => %s\n", daemon_info.cmd_pipe);
    printf("daemon_info.monitor_pid_file => %s\n", daemon_info.monitor_pid_file);
    printf("daemon_info.monitor_socket_file => %s\n", daemon_info.monitor_socket_file);
}

/*
 * This function is used to exit the daemon with an error message.
 * It uses variable arguments to format the error message.
 * It will print the error message to stderr and then exit the daemon.
 * The exit status will be EXIT_FAILURE.
 */
void daemon_error_exit(const char* format, ...) {
    va_list ap;

    if (format && *format) {
        va_start(ap, format);
        fprintf(stderr, "%s: ", "balrogd");
        vfprintf(stderr, format, ap);
        va_end(ap);
    }

    _exit(EXIT_FAILURE);
}

/**
 * Trims spaces at start and end of string
 * @param char* str
 */
static void trim(char* str) {
    char* start = str;
    char* end;
    // while char at init is a space it moves 1 ahead the ptr
    while (isspace((unsigned char)*start)) start++;
    if (*start == 0) {  // finished triming, no spaces at end
        *str = 0;
        return;
    }

    // end is actual ptr + string's lenght - 1 (EOF)
    end = start + strlen(start) - 1;

    // now trims spaces from end
    while (end > start && isspace((unsigned char)*end)) end--;
    *(end + 1) = 0;

    if (start != str) {
        memmove(str, start, end - start + 2);
    }
}

/**
 * Assigns values to daemon_info struct from config file values.
 * If there's any misconfiguration, exits.
 */
static void do_assign_values(char* key, char* value) {
    if (strcmp(key, config_opts.pid_file) == 0) {
        // copy string to daemon_info
        strncpy(daemon_info.pid_file, value, sizeof(daemon_info.pid_file) - 1);
        daemon_info.pid_file[sizeof(daemon_info.pid_file) - 1] = '\0';
        return;
    }

    if (strcmp(key, config_opts.cmd_pipe) == 0) {
        // copy string to daemon_info
        strncpy(daemon_info.cmd_pipe, value, sizeof(daemon_info.cmd_pipe) - 1);
        daemon_info.cmd_pipe[sizeof(daemon_info.cmd_pipe) - 1] = '\0';
        return;
    }

    if (strcmp(key, config_opts.monitor_pid_file) == 0) {
        // copy string to daemon_info
        strncpy(daemon_info.monitor_pid_file, value, sizeof(daemon_info.monitor_pid_file) - 1);
        daemon_info.monitor_pid_file[sizeof(daemon_info.monitor_pid_file) - 1] = '\0';
        return;
    }

    if (strcmp(key, config_opts.monitor_socket_file) == 0) {
        // copy string to daemon_info
        strncpy(daemon_info.monitor_socket_file, value,
                sizeof(daemon_info.monitor_socket_file) - 1);
        daemon_info.monitor_socket_file[sizeof(daemon_info.monitor_socket_file) - 1] = '\0';
        return;
    }

    daemon_error_exit("Error in config file\n");
}

static int init_parsing_config_file() {
    // leer archivo
    FILE* config_file = fopen(default_config_file_path, "r");
    if (!config_file) {
        perror("Error opening default config file");
        exit(EXIT_FAILURE);
    }

    int damon_section;
    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, config_file)) {
        trim(line);
        // i need to find header section [balrogd]

        // if line starts with '#', then continue
        if (line[0] == '#' || line[0] == ';' || line[0] == '\0') {
            continue;
        }

        if (line[0] == '[') {
            // may be header section
            if (strcmp(line, daemon_header_config) == 0) {
                damon_section = 1;
                continue;
            }
            if (strcmp(line, client_header_config) == 0) {
                damon_section = 0;
                continue;
            }
            // debe fallar y mostrar log
            daemon_error_exit("Error in config file.\n");
        }

        if (damon_section == 1) {
            // look for =
            char* eq = strchr(line, '=');  // returns a ptr to the char
            if (!eq) continue;

            *eq = 0;           // split string between key and value using = as delimitation
            char* key = line;  // already splited
            char* value = eq + 1;
            trim(key);
            trim(value);

            do_assign_values(key, value);
        }
    }
    fclose(config_file);
    return 0;
}

/**
 * Load initial config files
 */
void init_config() {
    // first i may check if config file exists
    //  if so i must read it and parse its content
    printf("Config file path => %s\n", default_config_file_path);
    if (access(default_config_file_path, F_OK) == 0) {
        init_parsing_config_file();
    } else {
        // init with env vars if exits some
        printf("No config file\n");
        return;
    }
}
