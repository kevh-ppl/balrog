#ifndef BALROG_UDEV_H
#define BALROG_UDEV_H

#include <pthread.h>
#include <signal.h>

#define BLOCK_SIZE 512

extern pthread_t pthread_monitoring;

extern struct udev *udev;
extern struct udev_device *device_to_enumerate;  // to enumerate in the for each
extern struct udev_enumerate *enumerator;
extern struct udev_list_entry *devices, *dev_list_entry;

extern struct udev_monitor *monitor;

int init_udev_context();

void do_enumerate(int fd_fifo_user);
void free_udev_enumerator();

void *start_monitoring();
void stop_monitoring();

void free_udev_context();

#endif  // BALROG_UDEV_H