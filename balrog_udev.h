#ifndef BALROG_UDEV_H
#define BALROG_UDEV_H

#define BLOCK_SIZE 512

extern struct udev *udev;
extern struct udev_device *device_to_enumerate;  // to enumerate in the for each
extern struct udev_enumerate *enumerator;
extern struct udev_list_entry *devices, *dev_list_entry;

extern struct udev_monitor *monitor;

void do_enumerate();
void free_udev_enumerator();

int start_monitoring();
void stop_monitoring();

void free_udev_context();

#endif  // BALROG_UDEV_H