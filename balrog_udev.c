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

        if (offset + 1 >= buffer_size) {
            buffer_size *= 2;
            char *new_buffer = realloc(info_dev, buffer_size);
            if (!new_buffer) {
                perror("realloc");
                free(info_dev);
                return -1;
            }
            info_dev = new_buffer;
        }

        offset += snprintf(info_dev + offset, buffer_size - offset, "I: DEVPATH = %s\n",
                           udev_device_get_devpath(device_to_enumerate));

        if (offset + 1 >= buffer_size) {
            buffer_size *= 2;
            char *new_buffer = realloc(info_dev, buffer_size);
            if (!new_buffer) {
                perror("realloc");
                free(info_dev);
                return -1;
            }
            info_dev = new_buffer;
        }

        offset += snprintf(info_dev + offset, buffer_size - offset, "I: KERNEL = %s\n",
                           udev_device_get_sysname(device_to_enumerate));

        if (offset + 1 >= buffer_size) {
            buffer_size *= 2;
            char *new_buffer = realloc(info_dev, buffer_size);
            if (!new_buffer) {
                perror("realloc");
                free(info_dev);
                return -1;
            }
            info_dev = new_buffer;
        }

        offset += snprintf(info_dev + offset, buffer_size - offset, "I: DEVNODE = %s\n",
                           udev_device_get_devnode(device_to_enumerate));

        if (offset + 1 >= buffer_size) {
            buffer_size *= 2;
            char *new_buffer = realloc(info_dev, buffer_size);
            if (!new_buffer) {
                perror("realloc");
                free(info_dev);
                return -1;
            }
            info_dev = new_buffer;
        }

        offset += snprintf(info_dev + offset, buffer_size - offset, "I: DEVTYPE = %s\n",
                           udev_device_get_devtype(device_to_enumerate));

        if (offset + 1 >= buffer_size) {
            buffer_size *= 2;
            char *new_buffer = realloc(info_dev, buffer_size);
            if (!new_buffer) {
                perror("realloc");
                free(info_dev);
                return -1;
            }
            info_dev = new_buffer;
        }

        offset += snprintf(info_dev + offset, buffer_size - offset, "I: SUBSYSTEM = %s\n",
                           udev_device_get_subsystem(device_to_enumerate));

        if (offset + 1 >= buffer_size) {
            buffer_size *= 2;
            char *new_buffer = realloc(info_dev, buffer_size);
            if (!new_buffer) {
                perror("realloc");
                free(info_dev);
                return -1;
            }
            info_dev = new_buffer;
        }

        offset += snprintf(info_dev + offset, buffer_size - offset, "I: DEVNUM = %llu\n",
                           (unsigned long long)udev_device_get_devnum(device_to_enumerate));

        if (offset + 1 >= buffer_size) {
            buffer_size *= 2;
            char *new_buffer = realloc(info_dev, buffer_size);
            if (!new_buffer) {
                perror("realloc");
                free(info_dev);
                return -1;
            }
            info_dev = new_buffer;
        }

        offset += snprintf(info_dev + offset, buffer_size - offset, "I: DRIVER = %s\n",
                           udev_device_get_driver(device_to_enumerate));

        if (offset + 1 >= buffer_size) {
            buffer_size *= 2;
            char *new_buffer = realloc(info_dev, buffer_size);
            if (!new_buffer) {
                perror("realloc");
                free(info_dev);
                return -1;
            }
            info_dev = new_buffer;
        }

        offset += snprintf(info_dev + offset, buffer_size - offset, "I: ACTION = %s\n",
                           udev_device_get_action(device_to_enumerate));

        if (offset + 1 >= buffer_size) {
            buffer_size *= 2;
            char *new_buffer = realloc(info_dev, buffer_size);
            if (!new_buffer) {
                perror("realloc");
                free(info_dev);
                return -1;
            }
            info_dev = new_buffer;
        }

        offset += snprintf(info_dev + offset, buffer_size - offset, "I: SEQNUM = %llu\n",
                           udev_device_get_seqnum(device_to_enumerate));

        if (offset + 1 >= buffer_size) {
            buffer_size *= 2;
            char *new_buffer = realloc(info_dev, buffer_size);
            if (!new_buffer) {
                perror("realloc");
                free(info_dev);
                return -1;
            }
            info_dev = new_buffer;
        }

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

            offset +=
                snprintf(info_dev + offset, buffer_size - offset, "I: DEVSIZE = %llu\n",
                         (unsigned long long)(disk_size * block_size) / (1024LL * 1024 * 1024));

            if (offset + 1 >= buffer_size) {
                buffer_size *= 2;
                char *new_buffer = realloc(info_dev, buffer_size);
                if (!new_buffer) {
                    perror("realloc");
                    free(info_dev);
                    return -1;
                }
                info_dev = new_buffer;
            }
        } else {
            offset += snprintf(info_dev + offset, buffer_size - offset, "I: DEVSIZE = %s\n", "n");
            if (offset + 1 >= buffer_size) {
                buffer_size *= 2;
                char *new_buffer = realloc(info_dev, buffer_size);
                if (!new_buffer) {
                    perror("realloc");
                    free(info_dev);
                    return -1;
                }
                info_dev = new_buffer;
            }
        }

        offset +=
            snprintf(info_dev + offset, buffer_size - offset, "I: BLOCK_SIZE=%hu\n\n", block_size);

        if (offset + 1 >= buffer_size) {
            buffer_size *= 2;
            char *new_buffer = realloc(info_dev, buffer_size);
            if (!new_buffer) {
                perror("realloc");
                free(info_dev);
                return -1;
            }
            info_dev = new_buffer;
        }

        // free dev to enumerate
        udev_device_unref(device_to_enumerate);
    }
    info_dev[offset] = '\0';  // EOF
    if (fd_fifo_user > 0) {
        if (write(fd_fifo_user, info_dev, strlen(info_dev)) < 0) {
            fprintf(stderr, "Error writing to FIFO: %m\n");
            close(fd_fifo_user);
            return -1;
        }

    } else {
        printf("Not fifo_user_path provided, printing info to stdout:\n");
        printf("%s\n", info_dev);
    }
    printf("Info written to FIFO: %s\n", info_dev);
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