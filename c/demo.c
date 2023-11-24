#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sched.h>
#include <fcntl.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>

/* Print formatters */
#define pr_with_prefix_and_source(prefix, format, arg...) \
    printf("["prefix"] %s:%d: "format, __FUNCTION__, __LINE__, ##arg)
#define pr_with_prefix(prefix, format, arg...) \
    printf("["prefix"] "format, ##arg)

#ifdef DEBUGGING
#define pr_debug(format, arg...) \
    pr_with_prefix_and_source("DEBUG", format, ##arg)
#else /* no-op debugging print */
#define pr_debug(format, arg...)
#endif
#define pr_info(format, arg...) pr_with_prefix("INFO", format, ##arg)
#define pr_warn(format, arg...) pr_with_prefix("WARN", format, ##arg)
#define pr_error(format, arg...)  \
    pr_with_prefix_and_source("ERROR", format, ##arg)
#define pr_error_with_errno(format, arg...) \
    pr_error(format", errno: %d, error: %s\n", ##arg, errno, strerror(errno))
#define pr_error_with_errno_or_bad_result(r, call, format, arg...) \
    if (r == -1) pr_error_with_errno(format, ##arg); \
    else pr_error("Unexpected return %d from "#call"()\n", r)

#define pr_error_ret_fail(func, cond, suffix, arg...) \
    if ((r = func(arg)) cond) { \
        pr_error_with_errno_or_bad_result(r, func, "Failed to "#func"()" suffix); \
        return -1; \
    }

#define unshare_checked(flag, name) \
    if ((r = unshare(flag))) { \
        pr_error_with_errno_or_bad_result(r, unshare, "Failed to unshare "#name); \
        return -1; \
    }

#define write_idmap_declare(child, parent, range, type) { \
    int r; \
    pr_error_ret_fail(open, < 0, " "#type"_map", "/proc/self/"#type"_map", O_WRONLY); \
    int fd = r; \
    pr_error_ret_fail(dprintf, < 5, " to "#type"_map", fd, "%d %d %d", child, parent, range); \
    pr_error_ret_fail(close, , " "#type"_map", fd); \
    return 0; \
}

static inline
int write_uidmap(uid_t child, uid_t parent, uid_t range) 
    write_idmap_declare(child, parent, range, uid)

static inline
int write_gidmap(gid_t child, gid_t parent, gid_t range) 
    write_idmap_declare(child, parent, range, gid)

#define idmaps_declare(type) \
    struct type##map { type##_t child, parent, range; }

idmaps_declare(uid);
idmaps_declare(gid);

#define write_idmaps_at_declare(type, atfd, maps, count) { \
    int r; \
    pr_error_ret_fail(openat, < 0, " "#type"_map", atfd, #type"_map", O_WRONLY); \
    int fd = r; \
    for (unsigned short i = 0; i < count; ++i) { \
        struct type##map *map = maps + i; \
        pr_info("Map: %d %d %d\n", map->child, map->parent, map->range); \
        pr_error_ret_fail(dprintf, < 5, " to "#type"_map", fd, "%d %d %d\n", map->child, map->parent, map->range); \
    } \
    pr_error_ret_fail(close, , " "#type"_map", fd); \
    return 0; \
}

static inline
int write_uidmaps_at(int atfd, struct uidmap *uidmaps, unsigned short count)
    // write_idmaps_at_declare(uid, atfd, uidmaps, count)
{

    int r;
    pr_error_ret_fail(openat, < 0, " uid_map", atfd, "uid_map", O_WRONLY);
    int fd = r;
    pr_error_ret_fail(dprintf, < 5, " to uid_map", fd, "0 1000 1\n1 100000 65535"); // 1\t100000\t65535
    pr_error_ret_fail(close, , " uid_map", fd);
    return 0;
}

static inline
int write_gidmaps_at(int atfd, struct gidmap *gidmaps, unsigned short count)
    write_idmaps_at_declare(gid, atfd, gidmaps, count)

// {
//     int r;
//     pr_error_ret_fail(open, < 0, " gid_map", "/proc/self/gid_map", O_WRONLY);
//     int fd = r;
//     pr_error_ret_fail(dprintf, < 5, " to gid_map", fd, "0 998 1\n1 100000 65535"); //
//     pr_error_ret_fail(close, , " gid_map", fd);
//     return 0;
// }

static inline
int deny_setgroups_at(int atfd) {
    int r;
    pr_error_ret_fail(openat, < 0, " to write to setgroups", atfd, "setgroups", O_WRONLY);
    int fd = r;
    pr_error_ret_fail(write, != 4, " deny to setgroups", fd, "deny", 4);
    pr_error_ret_fail(close, , " setgroups to finish write", fd);
    return 0;
}

static inline
int write_idmaps_at(int atfd, uid_t uid, gid_t gid) {
    struct uidmap uid_maps[2] = {
        {0, uid, 1},
        {1, 100000, 65536}
    };
    struct gidmap gid_maps[2] = {
        {0, gid, 1},
        {1, 100000, 65536}
    };
    // return deny_setgroups() || write_gidmap(gid, gid, 1) || write_uidmap(uid, uid, 1);
    return deny_setgroups_at(atfd) || write_uidmaps_at(atfd, uid_maps, 2);
    // write_gidmaps_at(atfd, gid_maps, 2)  || 
    return 0;
}

static inline
int getresugid_checked(
    uid_t *ruid, uid_t *euid, uid_t *suid,
    gid_t *rgid, gid_t *egid, gid_t *sgid
) {
    int r;
    pr_error_ret_fail(getresuid, , " to get real & effective & saved UIDs", ruid, euid, suid);
    pr_error_ret_fail(getresgid, ," to get real & effective & saved GIDs", rgid, egid, sgid);
    if (*ruid && *euid && *suid && *rgid && *egid && *sgid) return 0;
    else {
        pr_error("Running with root permission\n");
        return -1;
    }
}

static inline
int waitpid_checked(pid_t pid) {
    int status, r;
    pr_error_ret_fail(waitpid, <= 0, " for forked child", pid, &status, 0);
    if (!WIFEXITED(status)) {
        pr_error("Child %d did not exit cleanly\n", pid);
        return -1;
    }
    if ((status = WEXITSTATUS(status))) {
        pr_error("Child %d bad return %d\n", pid, status);
        return -1;
    }
    return 0;
}

static inline
int wait_reaper(pid_t child) {
    int r = prctl(PR_SET_CHILD_SUBREAPER, 1);
    if (r) {
        pr_error_with_errno_or_bad_result(r, prctl, 
            "Failed to set self as reaper for all children");
        waitpid_checked(child);
        return -1;
    }
    r = 0;
    int status;
    for (;;) {
        pid_t waited_pid = wait(&status);
        if (waited_pid > 0) {
            if (waited_pid == child) {
                if (WIFEXITED(status)) {
                    if ((status = WEXITSTATUS(status))) {
                        pr_error("Child %d bad return %d\n", child, status);
                        r = -1;
                    }
                } else {
                    pr_error("Child %d did not exit normally\n", child);
                    r = -1;
                }
            } // We don't really care other child
        } else if (waited_pid == -1) {
            switch (errno) {
            case EINTR:
                break;
            case ECHILD:
                goto end_wait;
            default:
                pr_error_with_errno("Failed to wait()");
                return -1;
            }
        } else { // 0 unexpecetd, or other negative
            pr_error("Unexpected return %d from wait()\n", waited_pid);
            return -1;
        }
    }
end_wait:
    return r;
}

static inline
int mount_bind(char const *source, char const *target) {
    int r = mount(source, target, NULL, MS_BIND, NULL);
    if (r) {
        pr_error_with_errno_or_bad_result(r, mount, 
            "Failed to bind '%s' to '%s'", source, target);
        return -1;
    }
    return 0;
}

#define bind_self_ret_fail(dir) \
    if (mount_bind("/"dir, dir)) return -1;

static inline
int pivot_root_here() {
    int r;
    pr_error_ret_fail(syscall, , " to pivot_root here", SYS_pivot_root, ".", ".");
    return 0;
}


#define pr_error_ret_fail_symlink(from, to) \
    pr_error_ret_fail(symlink, , " "to" -> "from, from, to)

static inline
int create_file(char const *const restrict path, mode_t mode) {
    int r;
    pr_error_ret_fail(creat, < 0, " file", path, mode);
    int fd = r;
    pr_error_ret_fail(close, , " file", fd);
    return 0;
}

static inline
int mount_bind_create_file(char const *source, char const *target, mode_t mode) {
    if (create_file(target, mode) || mount_bind(source, target)) return -1;
    return 0;
}

static inline
int bind_console() {
    // Attempt to bind the corresponding pseudo terminal
    int r;
    char buffer[0x100];
    pr_error_ret_fail(readlink, < 0, " stdout", "/proc/self/fd/1", buffer, 0x100);
    if (r >= 0x100) {
        pr_error("Link target too long\n");
        return -1;
    }
    buffer[r] = '\0';
    if (mount_bind_create_file(buffer, "dev/console", 0600)) return -1;
    return 0;
}

#define mount_bind_create_dev_ret_fail(name) \
    if (mount_bind_create_file("/dev/"name, "dev/"name, 0600)) return -1;



static inline
int create_dev() {
    int r;
    pr_error_ret_fail(mount, , " proc", "proc", "proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL);
    pr_error_ret_fail(mkdir, , " dev subdir", "dev", 0755);
    pr_error_ret_fail(mkdir, , " dev/pts subdir", "dev/pts", 0755);
    pr_error_ret_fail(mount, , " dev/pts", "devpts", "dev/pts", "devpts", MS_NOSUID | MS_NOEXEC, "newinstance,ptmxmode=0666,mode=620");
    pr_error_ret_fail(mkdir, , " dev/shm subdir", "dev/shm", 0755);
    if (bind_console()) return -1;
    mount_bind_create_dev_ret_fail("full");
    mount_bind_create_dev_ret_fail("null");
    mount_bind_create_dev_ret_fail("random");
    mount_bind_create_dev_ret_fail("tty");
    mount_bind_create_dev_ret_fail("urandom");
    mount_bind_create_dev_ret_fail("zero");
    pr_error_ret_fail_symlink("/proc/kcore", "dev/core");
    pr_error_ret_fail_symlink("/proc/self/fd", "dev/fd");
    pr_error_ret_fail_symlink("pts/ptmx", "dev/ptmx");
    pr_error_ret_fail_symlink("/proc/self/fd/0", "dev/stdin");
    pr_error_ret_fail_symlink("/proc/self/fd/1", "dev/stdout");
    pr_error_ret_fail_symlink("/proc/self/fd/2", "dev/stderr");
    return 0;
}

static inline
int create_root() {
    int r;
    char root[] = "/tmp/buildos.root.XXXXXX";
    if (!mkdtemp(root)) {
        pr_error("Failed to create temporary root\n");
        return -1;
    }
    if (mount_bind(root, root)) return -1;
    pr_error_ret_fail(chdir, , "", root);
    pr_error_ret_fail(mkdir, , " usr subdir", "usr", 0755);
    // pr_error_ret_fail(mkdir, , " usr/bin subdir", "usr/bin", 0755);
    // pr_error_ret_fail(mkdir, , " usr/lib subdir", "usr/lib", 0755);
    pr_error_ret_fail(mkdir, , " proc subdir", "proc", 0755);
    pr_error_ret_fail(mount, , " proc", "proc", "proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL);
    pr_error_ret_fail(mkdir, , " etc subdir", "etc", 0755);
    bind_self_ret_fail("usr");
    // bind_self_ret_fail("usr/bin");
    // bind_self_ret_fail("usr/lib");
    pr_error_ret_fail(symlink, , " lib64 -> usr/lib", "usr/lib", "lib64");
    if (create_dev()) return -1;
    pr_error_ret_fail(syscall, , " to pivot_root here", SYS_pivot_root, ".", ".");
    pr_error_ret_fail(umount2, , " the old root", ".", MNT_DETACH);
    return 0;
}


int main(int argc, char *argv[]) {
    if (argc <= 1) {
        pr_error("Too few arguments\n");
        return -1;
    }
    uid_t ruid, euid, suid;
    gid_t rgid, egid, sgid;
    if (getresugid_checked(&ruid, &euid, &suid, &rgid, &egid, &sgid)) return -1;
    int r;
    pr_error_ret_fail(prctl, , " to set no new privilege", PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
    unshare_checked(CLONE_NEWUSER, "user");
    unshare_checked(CLONE_NEWPID, "pid");
    unshare_checked(CLONE_NEWNS, "mount");
    pr_error_ret_fail(fork, < 0, "");
    if (r > 0) {
        pid_t child = r;
        char pid_string[64];
        pr_error_ret_fail(snprintf, < 0, " to create pid string", pid_string, 64, "/proc/%d", child);
        if (r >= 64) {
            pr_warn("PID string too long, truncated\n");
            r = 63;
        }
        pid_string[r] = '\0';
        pr_info("PID string: %s\n", pid_string);
        pr_error_ret_fail(open, < 0 , " proc/child", pid_string, O_RDONLY | O_DIRECTORY);
        int proc_child_fd = r;
        if (write_idmaps_at(proc_child_fd, ruid, rgid)) {
            pr_warn("Failed to write id maps");
        }
        // if (write_idmaps_self(ruid, rgid)) {
        //     pr_error("Failed to write uid/gid maps to map self\n");
        //     return -1;
        // }
        return waitpid_checked(child); // Parent, main daemon
    }
    sleep(3);
    // Child
    pr_error_ret_fail(fork, < 0, "");
    if (r > 0) return wait_reaper(r); // Child/Parent
    // Grand child
    if (create_root()) return -1;
    if (execvp(argv[1], argv + 1)) {
        pr_error_with_errno_or_bad_result(r, execvp, "Failed to exec child");
        return -1;
    }
    pr_error("Child should not be here!\n");
    return -1;
}