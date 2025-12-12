#ifndef DAEMON_H
#define DEAMON_H

/*
 * Based on daemonization template by KoynovStas.
 * Original source: https://github.com/KoynovStas/daemon_templates.git
 * Licensed under the BDS-3 License.
 */

#include <stddef.h>  //for null
#include <sys/types.h>

#define F_TLOCK 2  // file lock type, used in lockf()

#define DAEMON_NAME "balrog"

#define DAEMON_DEF_TO_STR_(text) #text
#define DAEMON_DEF_TO_STR(arg) DAEMON_DEF_TO_STR_(arg)

#define DAEMON_MAJOR_VERSION_STR DAEMON_DEF_TO_STR(DAEMON_MAJOR_VERSION)
#define DAEMON_MINOR_VERSION_STR DAEMON_DEF_TO_STR(DAEMON_MINOR_VERSION)
#define DAEMON_PATCH_VERSION_STR DAEMON_DEF_TO_STR(DAEMON_PATCH_VERSION)

#define DAEMON_VERSION_STR \
    DAEMON_MAJOR_VERSION_STR "." DAEMON_MINOR_VERSION_STR "." DAEMON_PATCH_VERSION_STR

void exit_if_not_daemonized(int exit_status);

typedef void (*signal_handler_t)(int);
void set_sig_handler(int sig, signal_handler_t handler);

void daemonize2(void (*optional_init)(void*), void* data);
static inline void daemonize() { daemonize2(NULL, NULL); }

int demonized();

#endif