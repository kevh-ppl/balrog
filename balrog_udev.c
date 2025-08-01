#include "balrog_udev.h"

#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/select.h>
#include <unistd.h>

#include "cmd_opt.h"
#include "daemon.h"

struct udev *udev = NULL;
struct udev_enumerate *enumerator = NULL;
struct udev_list_entry *devices = NULL, *dev_list_entry = NULL;
struct udev_device *device_to_enumerate = NULL;
struct udev_monitor *monitor = NULL;

int init_udev_context() {
    udev = udev_new();  // declared in enumerate.h

    if (!udev) {
        fprintf(stderr, "Failed to create udev context object\n");
        udev_unref(udev);
        return -1;
    }
    return 0;
}

// ================================
// ENUMERATOR

static int create_enumerator() {
    // i need to create a enumerator to get the devices
    enumerator = udev_enumerate_new(udev);
    if (!enumerator) {
        fprintf(stderr, "Failed to create udev enumerator\n");
        udev_enumerate_unref(enumerator);
        return -1;
    }

    // filter to only get block devices for the enumerator
    udev_enumerate_add_match_subsystem(enumerator, "block");
    udev_enumerate_scan_devices(enumerator);
    return 0;
}

static void increase_buffer_size(size_t offset, size_t *buffer_size, char **info_dev) {
    printf("Increasing buffer... offset %zu | buffer_size %zu\n", offset, *buffer_size);
    *buffer_size *= 2;
    printf("New buffer size => %zu\n", *buffer_size);
    char *new_buffer = realloc((void *)*info_dev, *buffer_size);
    printf("new_buffer %p\n", new_buffer);
    if (!new_buffer) {
        printf("Reallocated?\n");
        perror("Realloc");
        free(info_dev);
        return;
    }
    *info_dev = new_buffer;
    printf("info_dev = new_buffer ? %p\n", *info_dev);
    return;
}

static void safe_append(char **buf, size_t *offset, size_t *buf_size, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    while (1) {
        int written = vsnprintf(*buf + *offset, *buf_size - *offset, fmt, args);
        va_end(args);
        printf("written %d | offset %zu | buffer_size %zu\n", written, *offset, *buf_size);
        if (written < 0) return;
        if ((size_t)written + 1 < *buf_size - *offset) {
            *offset += written;
            printf("todu ben\n");
            break;
        }
        printf("k\n");
        increase_buffer_size(*offset, buf_size, buf);
        va_start(args, fmt);
    }
}

static int fillup_device_list(int fd_fifo_user) {
    printf("Filling up device list... fd_fifo_user: %d\n", fd_fifo_user);
    // fillup device list
    devices = udev_enumerate_get_list_entry(enumerator);
    if (!devices) {
        fprintf(stderr, "Failed to get devices list\n");
        udev_enumerate_unref(enumerator);
        return -1;
    }

    printf("Creating buffer...\n");
    char *info_dev = malloc(PIPE_BUF);
    if (!info_dev) {
        perror("malloc");
        return -1;
    }
    info_dev[0] = '\0';
    size_t offset = 0;
    size_t buffer_size = PIPE_BUF;

    udev_list_entry_foreach(dev_list_entry, devices) {
        const char *path, *tmp;
        unsigned long long disk_size = 0;
        unsigned short int block_size = BLOCK_SIZE;

        path = udev_list_entry_get_name(dev_list_entry);
        device_to_enumerate = udev_device_new_from_syspath(udev, path);

        // since its already store the info of the udev_device to enumerate, its posibble to print
        // such info

        printf("AHHHHHHHHHHHHHHHHHH\n");
        safe_append(&info_dev, &offset, &buffer_size, "I: DEVPATH = %s\n",
                    udev_device_get_devpath(device_to_enumerate));

        safe_append(&info_dev, &offset, &buffer_size, "I: KERNEL = %s\n",
                    udev_device_get_sysname(device_to_enumerate));

        safe_append(&info_dev, &offset, &buffer_size, "I: DEVNODE = %s\n",
                    udev_device_get_devnode(device_to_enumerate));

        safe_append(&info_dev, &offset, &buffer_size, "I: DEVTYPE = %s\n",
                    udev_device_get_devtype(device_to_enumerate));

        safe_append(&info_dev, &offset, &buffer_size, "I: SUBSYSTEM = %s\n",
                    udev_device_get_subsystem(device_to_enumerate));

        safe_append(&info_dev, &offset, &buffer_size, "I: DEVNUM = %llu\n",
                    (unsigned long long)udev_device_get_devnum(device_to_enumerate));

        safe_append(&info_dev, &offset, &buffer_size, "I: DRIVER = %s\n",
                    udev_device_get_driver(device_to_enumerate));

        safe_append(&info_dev, &offset, &buffer_size, "I: ACTION = %s\n",
                    udev_device_get_action(device_to_enumerate));

        safe_append(&info_dev, &offset, &buffer_size, "I: SEQNUM = %llu\n",
                    udev_device_get_seqnum(device_to_enumerate));

        // looks for the size attribute in the sysfs
        tmp = udev_device_get_sysattr_value(device_to_enumerate, "size");
        if (tmp) {
            // convert string to unsigned long long
            // tmp is string, second param is the **endptr and 10 is the numeric base
            disk_size = strtoull(tmp, NULL, 10);
        }

        // looks for the device's logical block size
        tmp = udev_device_get_sysattr_value(device_to_enumerate, "queue/logical_block_size");

        if (tmp) block_size = atoi(tmp);

        // check for an optic device (CD/DVD)
        // Also it could be done retreaving the ID_CDROM property
        if (strncmp(udev_device_get_sysname(device_to_enumerate), "sr", 2) != 0) {
            // here using GiB and not GB decimal, also forcing to use 64 bit arithmetic

            safe_append(&info_dev, &offset, &buffer_size, "I: DEVSIZE = %llu\n",
                        (unsigned long long)(disk_size * block_size) / (1024LL * 1024 * 1024));

        } else {
            safe_append(&info_dev, &offset, &buffer_size, "I: DEVSIZE = %llu\n", "n");
        }

        safe_append(&info_dev, &offset, &buffer_size, "I: BLOCK_SIZE=%hu\n\n", block_size);

        // free dev to enumerate
        udev_device_unref(device_to_enumerate);
    }  // fin foreach

    printf("FIN DE FOR EACH => offset %zu | buffer_size %zu | length info_dev %zu\n", offset,
           buffer_size, strlen(info_dev));

    info_dev[offset] = '\0';  // EOF
    if (fd_fifo_user > 0) {
        int bytes_written = write(fd_fifo_user, info_dev, strlen(info_dev));
        printf("Bytes written to FIFO: %d\n", bytes_written);
        printf("String length: %zu\n", strlen(info_dev));
        if (bytes_written < 0) {
            fprintf(stderr, "Error writing to FIFO: %m\n");
            close(fd_fifo_user);
            return -1;
        }

    } else {
        printf("Not fifo_user_path provided, printing info to stdout:\n");
        printf("%s\n", info_dev);
    }
    printf("Info written to FIFO: \n%s", info_dev);
    close(fd_fifo_user);
    return 0;
}

/*
Initiate block devices enumeration
*/
void do_enumerate(int fd_fifo_user) {
    if (!udev) {
        fprintf(stderr, "Udev context is not initialized\n");
        init_udev_context();
    }
    if (!enumerator) {
        fprintf(stderr, "Udev enumerator is not created\n");
        create_enumerator();
    }

    fillup_device_list(fd_fifo_user);
}

/*
Free memory used for the enumerator
*/
void free_udev_enumerator() {
    // free enumerator
    udev_enumerate_unref(enumerator);
}

// ENUMERATOR
// ================================

// ================================
// MONITOR

static int create_udev_usb_monitor() {
    if (!udev) {
        fprintf(stderr, "Udev context is not initialized\n");
        init_udev_context();
    }
    printf("FROM start_monitoring() -> create_udev_usb_monitor() con &udev: %p\n", (void *)udev);
    monitor = udev_monitor_new_from_netlink(udev, "udev");
    if (!monitor) {
        fprintf(stderr, "Failded to create udev monitor\n");
        udev_monitor_unref(monitor);
        return -1;
    }
    printf("Udev monitor created succesfully at: %p\n", (void *)monitor);
    // filter to only get block devices for the monitor
    if (!udev_monitor_filter_add_match_subsystem_devtype(monitor, "usb", NULL)) {
        // daemon_error_exit("Failed to add filter for udev monitor\n");
        fprintf(stderr, "Failed to add filter for udev monitor\n");
        printf("Continuing without filter...\n");
    }

    return 0;
}

/*
Set the udev monitor to listen for events
Returns the file descriptor for the monitor
*/
int set_monitor() {
    if (create_udev_usb_monitor() < 0) {
        return -1;
    }
    printf("udev => %p\n", (void *)udev);
    int enabled_receiving = udev_monitor_enable_receiving(monitor);
    if (enabled_receiving < 0) {
        fprintf(stderr, "Failed to enable receiving on udev monitor\n");
        udev_monitor_unref(monitor);
        return -1;
    }
    printf("Enabled receiving...\n");

    int fd = udev_monitor_get_fd(monitor);
    if (fd < 0) {
        fprintf(stderr, "Failed to get monitor file descriptor\n");
        udev_monitor_unref(monitor);
        return -1;
    }
    return fd;  // return the file descriptor for the monitor
}

/*
Starts monitoring
This is the task that will be run in the main loop
*/
int start_monitoring(int fd) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    select(fd + 1, &fds, NULL, NULL, NULL);

    if (FD_ISSET(fd, &fds)) {
        struct udev_device *dev = udev_monitor_receive_device(monitor);
        if (dev) {
            const char *action = udev_device_get_action(dev);
            const char *node = udev_device_get_devnode(dev);
            const char *subsystem = udev_device_get_subsystem(dev);

            printf("[%s] %s (%s)\n", action, node, subsystem);
            udev_device_unref(dev);
        }
    }

    return 0;  // This line is unreachable, but added to satisfy the function signature
}

/*
Stops monitoring juasjuas
*/
void stop_monitoring() { udev_monitor_unref(monitor); }

// MONITOR
// ================================

// i might call this when the service/daemon stops to run
void free_udev_context() {
    // free udev context
    udev_unref(udev);
}