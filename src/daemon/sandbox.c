#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

static int pivot_root(char* new_root, char* old_root) {
    return syscall(SYS_pivot_root, new_root, old_root);
}

static void die(char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void ensure_dir(char* dir, mode_t mode) {
    if (mkdir(dir, mode) == -1 && errno != EEXIST) die(dir);
}

int sandbox(const char* dev, const char* fstype) {
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
    char* host_dev = "/tmp/sbx-newroot";
    ensure_dir(host_dev, 0755);
    if (mount("tmpfs", host_dev, "tmpfs", MS_NOSUID | MS_NODEV, "size=256m") == -1)
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
    snprintf(mnt_dev, sizeof(mnt_dev), "%s/mnt/", host_dev);
    char usb_dir[256];
    snprintf(usb_dir, sizeof(usb_dir), "%s/usb", mnt_dev);
    char bin_dir[256];
    snprintf(bin_dir, sizeof(bin_dir), "%s/bin", host_dev);

    ensure_dir(old_root, 0755);
    ensure_dir(dev_dir, 0755);
    ensure_dir(proc_dir, 0755);
    ensure_dir(mnt_dev, 0755);
    ensure_dir(usb_dir, 0755);
    ensure_dir(bin_dir, 0755);

    // bind-mount del device node específico hacia el newroot
    char dev_in_root[256];
    snprintf(dev_in_root, sizeof(dev_in_root), "%s/dev/%s", host_dev, strrchr(dev, '/') + 1);
    printf("dev_in_root para hacer bind => %s\n", dev_in_root);
    // crear archivo destino si no existe
    int fd = open(dev_in_root, O_CREAT | O_RDONLY, 0600);
    if (fd == -1) die("create dev node placeholder");
    close(fd);
    if (mount(dev, dev_in_root, NULL, MS_BIND, NULL) == -1) die("bind-mount device node");

    // INICIO CUIDADO AUÍ
    // Es mejor solo montar las dependencias exactas
    char lib_dir[256];
    snprintf(lib_dir, sizeof(lib_dir), "%s/lib", host_dev);
    char lib64_dir[256];
    snprintf(lib64_dir, sizeof(lib64_dir), "%s/lib64", host_dev);

    ensure_dir(lib_dir, 0755);
    ensure_dir(lib64_dir, 0755);

    if (mount("/lib", lib_dir, NULL, MS_BIND | MS_REC, NULL)) die("bind-mount /lib");
    if (mount("/lib64", lib64_dir, NULL, MS_BIND | MS_REC, NULL)) die("bind-mount /lib64");

    if (mount("/bin", bin_dir, NULL, MS_BIND | MS_REC, NULL)) die("bind-mount /bin/sh");
    // FIN CUIDADO AUÍ

    // ahora si hacemos el pivote del root
    if (pivot_root(host_dev, old_root) == -1) die("pivot_root");
    if (chdir("/") == -1) die("chdir /");

    // mantar proc en la nueva raiz
    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL) == -1)
        die("mount /proc");

    // montar deispositivo
    unsigned long usb_flags = MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC;
    if (mount(dev, "/mnt/usb", fstype, usb_flags, NULL) == -1) die("mount device");

    // desmontar la anterior raiz
    if (umount2("/.old_root", MNT_DETACH) == -1) die("unmount oldroot");
    rmdir("/.oldroot");

    // aquí se puede bajar capacidades y limitar syscalls
    // ====================================================

    printf("Sandbox listo. Dispositivo montado en /mnt/usb (RO, nosuid,nodev,noexec).\n");

    pid_t pid = fork();
    if (pid == 0) {
        char* argv_shell[] = {"/bin/sh", NULL};
        execve("/bin/sh", argv_shell, NULL);
        _exit(1);
    }

    int status;
    while (1) {
        pid_t w = waitpid(-1, &status, 0);
        if (w == -1) {
            if (errno == EINTR) continue;
            if (errno == ECHILD) continue;
            perror("waitpid");
            break;
        }
        printf("Proceso %d terminó con status %d\n", w, status);
    }

    return 0;
}