#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <sched.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PATH_MAX 4096

static void die_errno(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, ": errno=%d (%s)\n", errno, strerror(errno));
    exit(1);
}

static void die(const char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "USAGE: %s <device> <fstype>\n", argv[0]);
        return 2;
    }

    const char* dev = argv[1];
    const char* fstype = argv[2];

    uid_t ruid = getuid();
    gid_t rgid = getgid();

    if (geteuid() != 0) {
        die("Run as root");
    }

    // Make mounts private on this namespace
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) {
        die_errno("mount(/, MS_PRIVATE) failed");
    }

    // Create new mount namespace
    if (unshare(CLONE_NEWNS) == -1) {
        die_errno("unshare(CLONE_NEWNS) failed");
    }

    // Create a secure temporary newroot
    char template[] = "/tmp/sbx-newroot-XXXXXX";
    char* newroot = mkdtemp(template);
    if (!newroot) die_errno("mkdtemp failed");

    // mount a private tmpfs for the new root
    if (mount("tmpfs", newroot, "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC, "size=256m") == -1) {
        die_errno("mount tmpfs on newroot failed");
    }

    // create dirs inside newroot
    char old_root[PATH_MAX];
    char dev_dir[PATH_MAX];
    char proc_dir[PATH_MAX];
    char mnt_dir[PATH_MAX];
    snprintf(old_root, sizeof(old_root), "%s/.old_root", newroot);
    snprintf(dev_dir, sizeof(dev_dir), "%s/dev", newroot);
    snprintf(proc_dir, sizeof(proc_dir), "%s/proc", newroot);
    snprintf(mnt_dir, sizeof(mnt_dir), "%s/mnt", newroot);

    if (mkdir(old_root, 0755) && errno != EEXIST) die_errno("mkdir old_root");
    if (mkdir(dev_dir, 0755) && errno != EEXIST) die_errno("mkdir dev_dir");
    if (mkdir(proc_dir, 0755) && errno != EEXIST) die_errno("mkdir proc_dir");
    if (mkdir(mnt_dir, 0755) && errno != EEXIST) die_errno("mkdir mnt_dir");

    // Bind minimal set /bin, /lib, /lib64 to allow /bin/sh to run
    char target_bin[PATH_MAX];
    char target_lib[PATH_MAX];
    char target_lib64[PATH_MAX];
    snprintf(target_bin, sizeof(target_bin), "%s/bin", newroot);
    snprintf(target_lib, sizeof(target_lib), "%s/lib", newroot);
    snprintf(target_lib64, sizeof(target_lib64), "%s/lib64", newroot);

    mkdir(target_bin, 0755);
    mkdir(target_lib, 0755);
    mkdir(target_lib64, 0755);

    if (mount("/bin", target_bin, NULL, MS_BIND | MS_REC, NULL) == -1) die_errno("bind /bin");
    if (mount("/lib", target_lib, NULL, MS_BIND | MS_REC, NULL) == -1) {
        if (errno != ENOENT) die_errno("bind /lib");
    }
    if (mount("/lib64", target_lib64, NULL, MS_BIND | MS_REC, NULL) == -1) {
        if (errno != ENOENT) die_errno("bind /lib64");
    }

    // create placeholder for device inside newroot and bind-mount the device node
    char dev_in_root[PATH_MAX];
    snprintf(dev_in_root, sizeof(dev_in_root), "%s/dev/%s", newroot,
             strrchr(dev, '/') ? (strrchr(dev, '/') + 1) : dev);
    int fd = open(dev_in_root, O_CREAT | O_RDONLY, 0600);
    if (fd == -1) die_errno("create dev placeholder");
    close(fd);

    if (mount(dev, dev_in_root, NULL, MS_BIND | MS_RDONLY, NULL) == -1)
        die_errno("bind device node");

    // sand_setup
    char target_sand_setup[PATH_MAX];
    snprintf(target_sand_setup, sizeof(target_sand_setup), "%s/sand_setup", newroot);

    //  create an empty file as mount target
    int fdt = open(target_sand_setup, O_CREAT | O_RDONLY, 0755);
    if (fdt == -1) die_errno("create target sand_setup placeholder");
    close(fdt);

    if (mount("/usr/bin/sand_setup", target_sand_setup, NULL, MS_BIND, NULL) == -1)
        die_errno("bind sand_setup (file->file)");

    // PIVOT
    // pivot_root into newroot
    if (pivot_root(newroot, old_root) == -1) die_errno("pivot_root failed");

    if (chdir("/") == -1) die_errno("chdir / failed");

    // mount proc inside new root
    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL) == -1)
        die_errno("mount /proc");

    // create mount point for the device and mount the block device read-only
    if (mkdir("/mnt/usb", 0755) == -1 && errno != EEXIST) die_errno("mkdir /mnt/usb");
    unsigned long usb_flags = MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC;

    char* base = strrchr(dev, '/');
    if (!base)
        base = (char*)dev;
    else
        base++;

    char dev_after_pivot[PATH_MAX];
    snprintf(dev_after_pivot, sizeof(dev_after_pivot), "/dev/%s", base);

    if (access(dev_after_pivot, F_OK) != 0) die_errno("device missing after pivot_root");

    if (mount(dev_after_pivot, "/mnt/usb", fstype, usb_flags, NULL) == -1)
        die_errno("mount device");

    struct stat st;
    if (stat(old_root, &st) == 0) {
        if (umount2(old_root, MNT_DETACH) == -1) {
            die_errno("umount2 old_root");
        }
        rmdir(old_root);
    }

    // exec the setup binary with remaining args, pass the mountpoint of usb and any
    // command construct argv for exec
    int extra = 1;  // setup_path
    int i;
    for (i = 4; i < argc; ++i) extra++;  // append user commands

    char** nargv = calloc(extra + 2, sizeof(char*));
    if (!nargv) die("calloc failed");
    nargv[0] = "/sand_setup";  // ruta dentro del sandbox
    nargv[1] = "/mnt/usb";
    int j = 2;
    for (i = 4; i < argc; ++i) nargv[j++] = argv[i];
    nargv[j] = NULL;

    // dropping privileges
    const char* sudo_uid = getenv("SUDO_UID");
    const char* sudo_gid = getenv("SUDO_GID");

    if (!sudo_uid || !sudo_gid) {
        fprintf(stderr, "No SUDO_UID/SUDO_GID; no puedo dejar de ser root.\n");
        exit(1);
    }

    uid_t uid = atoi(sudo_uid);
    gid_t gid = atoi(sudo_gid);

    if (setgid(gid) != 0) {
        perror("setgid");
        exit(1);
    }

    if (setuid(uid) != 0) {
        perror("setuid");
        exit(1);
    }

    execv("/sand_setup", nargv);
    die_errno("execv setup_path failed");
    return 0;
}