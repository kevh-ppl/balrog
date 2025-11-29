#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>

uid_t get_uid() {
    uid_t uid = getuid();
    if (!uid) {
        printf("UID null, so we're 'monky'\n");
        // here i may log this with syslog
        return (uid_t)-1;
    }
    return uid;
}

struct passwd* get_pw_user() {
    uid_t uid = get_uid();
    struct passwd* pw_user = getpwuid(uid);  // get user info for the current user
    if (!pw_user) {
        fprintf(stderr, "Couldn't get user info for UID %d: %m\n", uid);
        _exit(EXIT_FAILURE);
    }
}