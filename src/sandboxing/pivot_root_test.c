#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

static int pivot_root(char *new_root, char *old_root) {
    return syscall(SYS_pivot_root, new_root, old_root);
}

static void die(char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void ensure_dir(char *dir, mode_t mode) {
    if (mkdir(dir, mode) == -1 && errno != EEXIST) die(dir);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "USO: %s /dev/sd<x> <fstype> vfat|ext4|ext3|btrf>\n", argv[0]);
        return 1;
    }

    const char *dev = argv[1];
    const char *fstype = argv[2];

    printf("dev => %s\n", dev);
    printf("fstype => %s\n", fstype);

    // nuevo namespace de montaje
    if (unshare(CLONE_NEWNS) == -1) die("unshare(CLONE_NEWNS)");

    // montaje ya en el nuevo namespace de montaje (valga la rebusnancia)
    // MS_REC es para aplicar cambios recursivamente y MS_PRIVATE hace privado este montaje en el
    // nuevo namespace
    // básicamente esto es para lograr el aislamiento con el fs host, creando una nueva tabla de
    // montaje
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) die("mount MS_PRIVATE");

    // ojo que hasta aquí no se tiene un fs totalmente separado
    // es el mismo fs visto desde dos namespaces distintos, cada uno manteniendo su propia tabla de
    // montaje logrando que lo que monte/desmonte en un namespace no se refleje en el otro

    // habiendo hecho esta disociación entre namespaces, ahora sí se puede crear un fs completamente
    // nuevo aprovechando el nuevo namespace, pero primero
    // se debe montar el tempfs para el dispositivo
    const char *host_dev = "/tmp/sbx-newroot";
    ensure_dir(host_dev, 0755);
    if (mount("tmpfs", host_dev, "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC, "size=256m") == -1)
        die("mount tmpfs");

    // aquí se ha usado MS_NOSUID porque no se necesita la info de GUID ni UID,
    // MS_NODEV para no permitir el uso de arvichos de dispositivos (/dev) que permitan acceder a
    // info del hardware, MS_NOEXEC para no permitir la ejecución de binarios dentro de este tmpfs

    // ya que se creo el nuevo fs, se necita montar lo necesario para un funcionamiento básico
    char old_root[256];
    snprintf(old_root, sizeof(old_root), "%s/.old_root", host_dev);
    char dev_dir[256];
    snprintf(dev_dir, sizeof(dev_dir), "%s/dev", host_dev);
    char proc_dir[256];
    snprintf(proc_dir, sizeof(proc_dir), "%s/proc", host_dev);
    char mnt_dev[256];
    snprintf(mnt_dev, sizeof(mnt_dev), "%s/mnt/dev", host_dev);

    ensure_dir(old_root, 0755);
    ensure_dir(dev_dir, 0755);
    ensure_dir(proc_dir, 0755);
    ensure_dir(mnt_dev, 0755);

    // bind-mount del device node específico hacia el newroot
    char dev_in_root[256];
    snprintf(dev_in_root, sizeof(dev_in_root), "%s/dev/%s", host_dev, strrchr(dev, '/') + 1);
    // crear archivo destino si no existe
    int fd = open(dev_in_root, O_CREAT | O_RDONLY, 0600);
    if (fd == -1) die("create dev node placeholder");
    close(fd);
    if (mount(dev, dev_in_root, NULL, MS_BIND, NULL) == -1) die("bind-mount device node");

    // ahora si hacemos el pivote del root
    if (pivot_root(host_dev, old_root) == -1) die("pivot_root");
    if (chdir("/") == -1) die("chdir /");

    // mantar proc en la nueva raiz
    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL) == -1)
        die("mount /proc");

    // montar deispositivo
    unsigned long usb_flags = MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC;
    if (mount("/dev/sdb1", "/mnt/usb", fstype, usb_flags, NULL) == -1) die("mount device");

    // desmontar la anterior raiz
    if (umount2("/.oldroot", MNT_DETACH) == -1) die("unmount oldroot");
    rmdir("/.oldroot");

    // aquí se puede bajar capacidades y limitar syscalls
    // ====================================================

    printf("Sandbox listo. Dispositivo montado en /mnt/usb (RO, nosuid,nodev,noexec).\n");

    pause();

    return 0;
}