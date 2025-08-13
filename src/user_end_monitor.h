#ifndef USER_END_MONITOR_H
#define USER_END_MONITOR_H

#include <pthread.h>

extern pthread_t pthread_user_end_monitoring;
void *start_user_end_monitoring(void *args);

#endif