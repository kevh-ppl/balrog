#include <stdio.h>
#include <libudev.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>

#define BLOCK_SIZE 512

int main()
{

    struct udev_device *device_to_enumerate; // to enumerate in the for each
    struct udev_enumerate *enumerator;
    struct udev_list_entry *devices, *dev_list_entry;

    struct udev *udev = udev_new();
    if (!udev)
    {
        fprintf(stderr, "Failed to create udev context object\n");
        udev_unref(udev);
        return -1;
    }

    //================================
    // ENUMERATOR

    // i need to create a enumerator to get the devices
    enumerator = udev_enumerate_new(udev);
    if (!enumerator)
    {
        fprintf(stderr, "Failed to create udev enumerator\n");
        udev_enumerate_unref(enumerator);
        udev_unref(udev);
        return -1;
    }

    // filter to only get block devices for the enumerator
    udev_enumerate_add_match_subsystem(enumerator, "block");
    udev_enumerate_scan_devices(enumerator);

    // fillup device list
    devices = udev_enumerate_get_list_entry(enumerator);
    if (!devices)
    {
        fprintf(stderr, "Failed to get devices list\n");
        udev_enumerate_unref(enumerator);
        udev_unref(udev);
        return -1;
    }

    udev_list_entry_foreach(dev_list_entry, devices)
    {
        const char *path, *tmp;
        unsigned long long disk_size = 0;
        unsigned short int block_size = BLOCK_SIZE;

        path = udev_list_entry_get_name(dev_list_entry);
        device_to_enumerate = udev_device_new_from_syspath(udev, path);

        // since its already store the info of the udev_device to enumerate, its posibble to print such info
        printf("I: DEVNODE=%s\n", udev_device_get_devnode(device_to_enumerate));
        printf("I: KERNEL=%s\n", udev_device_get_sysname(device_to_enumerate));
        printf("I: DEVPATH=%s\n", udev_device_get_devpath(device_to_enumerate));
        printf("I: DEVTYPE=%s\n", udev_device_get_devtype(device_to_enumerate));
        printf("I: SUBSYSTEM=%s\n", udev_device_get_subsystem(device_to_enumerate));
        printf("I: DEVNUM=%llu\n", (unsigned long long)udev_device_get_devnum(device_to_enumerate));
        printf("I: DRIVER=%s\n", udev_device_get_driver(device_to_enumerate));
        printf("I: ACTION=%s\n", udev_device_get_action(device_to_enumerate));
        printf("I: SEQNUM=%llu\n", udev_device_get_seqnum(device_to_enumerate));
        printf("\n");

        // looks for the size attribute in the sysfs
        tmp = udev_device_get_sysattr_value(device_to_enumerate, "size");
        if (tmp)
        {
            // convert string to unsigned long long
            // tmp is string, second param is the **endptr and 10 is the numeric base
            disk_size = strtoull(tmp, NULL, 10);
        }

        // looks for the device's logical block size
        tmp = udev_device_get_sysattr_value(device_to_enumerate, "queue/logical_block_size");

        if (tmp)
            block_size = atoi(tmp);

        printf("I: DEVSIZE=");
        // check for an optic device (CD/DVD)
        // Also it could be done retreaving the ID_CDROM property
        if (strncmp(udev_device_get_sysname(device_to_enumerate), "sr", 2) != 0)
        {
            // here using GiB and not GB decimal, also forcing to use 64 bit arithmetic
            printf("%lld GiB\n", (disk_size * block_size) / (1024LL * 1024 * 1024));
        }
        else
            printf("n/a\n");

        // free dev to enumerate
        udev_device_unref(device_to_enumerate);
    }

    // free enumerator
    udev_enumerate_unref(enumerator);

    // ENUMERATOR
    // ================================

    // ================================
    // MONITOR

    printf("\n\nNow monitoring...\n\n");

    struct udev_monitor *monitor = udev_monitor_new_from_netlink(udev, "udev");
    if (!monitor)
    {
        fprintf(stderr, "Failded to create udev monitor\n");
        udev_monitor_unref(monitor);
        udev_unref(udev);
        return 1;
    }
    printf("Udev monitor created succesfully\n");

    // filter to only get block devices for the monitor
    udev_monitor_filter_add_match_subsystem_devtype(monitor, "usb", NULL);

    int enabled_receiving = udev_monitor_enable_receiving(monitor);
    if(enabled_receiving < 0)
    {
        fprintf(stderr, "Failed to enable receiving on udev monitor\n");
        udev_monitor_unref(monitor);
        udev_unref(udev);
        return 1;
    }

    int fd = udev_monitor_get_fd(monitor);
    if (fd < 0)
    {
        fprintf(stderr, "Failed to get monitor file descriptor\n");
        udev_monitor_unref(monitor);
        udev_unref(udev);
        return 1;
    }
    fd_set fds;

    while (1)
    {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        select(fd + 1, &fds, NULL, NULL, NULL);

        if (FD_ISSET(fd, &fds))
        {
            struct udev_device *dev = udev_monitor_receive_device(monitor);
            if (dev)
            {
                const char *action = udev_device_get_action(dev);
                const char *node = udev_device_get_devnode(dev);
                const char *subsystem = udev_device_get_subsystem(dev);

                printf("[%s] %s (%s)\n", action, node, subsystem);
                udev_device_unref(dev);
            }
        }
    }

    udev_monitor_unref(monitor);
    // MONITOR
    // ================================

    // free udev context
    udev_unref(udev);
}