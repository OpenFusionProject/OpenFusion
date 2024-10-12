#if defined(__linux__) && !defined(CONFIG_NOSANDBOX)

#include "core/Core.hpp" // mostly for ARRLEN
#include "settings.hpp"

#include <stdlib.h>
#include <fcntl.h>

#include <filesystem>

#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/mman.h> // for mmap() args
#include <sys/ioctl.h> // for ioctl() args
#include <termios.h> // for ioctl() args

#include <linux/unistd.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>
#include <linux/net.h> // for socketcall() args

#ifndef CONFIG_NOLANDLOCK
#include <linux/landlock.h>
#endif

/*
 * Macros adapted from https://outflux.net/teach-seccomp/
 * Relevant license:
 *     https://source.chromium.org/chromium/chromium/src/+/master:LICENSE
 */
#define syscall_nr (offsetof(struct seccomp_data, nr))
#define arch_nr (offsetof(struct seccomp_data, arch))

#if defined(__i386__)
# define ARCH_NR AUDIT_ARCH_I386
#elif defined(__x86_64__)
# define ARCH_NR AUDIT_ARCH_X86_64
#elif defined(__arm__)
# define ARCH_NR AUDIT_ARCH_ARM
#elif defined(__aarch64__)
# define ARCH_NR AUDIT_ARCH_AARCH64
#else
# error "Seccomp-bpf sandbox unsupported on this architecture"
#endif

#define VALIDATE_ARCHITECTURE \
    BPF_STMT(BPF_LD+BPF_W+BPF_ABS, arch_nr), \
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, ARCH_NR, 1, 0), \
    BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_KILL_PROCESS)

#define EXAMINE_SYSCALL \
    BPF_STMT(BPF_LD+BPF_W+BPF_ABS, syscall_nr)

#define ALLOW_SYSCALL(name) \
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, __NR_##name, 0, 1), \
    BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_ALLOW)

#define DENY_SYSCALL_ERRNO(name, _errno) \
    BPF_JUMP(BPF_JMP+BPF_K+BPF_JEQ, __NR_##name, 0, 1), \
    BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_ERRNO|(_errno))

#define KILL_PROCESS \
    BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_TRAP)

/*
 * Macros adapted from openssh's sandbox-seccomp-filter.c
 * Relevant license:
 *     https://github.com/openssh/openssh-portable/blob/master/LICENCE
 */
#if __BYTE_ORDER == __LITTLE_ENDIAN
# define ARG_LO_OFFSET  0
# define ARG_HI_OFFSET  sizeof(uint32_t)
#elif __BYTE_ORDER == __BIG_ENDIAN
# define ARG_LO_OFFSET  sizeof(uint32_t)
# define ARG_HI_OFFSET  0
#else
#error "Unknown endianness"
#endif

#define ALLOW_SYSCALL_ARG(_nr, _arg_nr, _arg_val) \
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, (__NR_##_nr), 0, 6), \
    /* load and test syscall argument, low word */ \
    BPF_STMT(BPF_LD+BPF_W+BPF_ABS, \
        offsetof(struct seccomp_data, args[(_arg_nr)]) + ARG_LO_OFFSET), \
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, \
        ((_arg_val) & 0xFFFFFFFF), 0, 3), \
    /* load and test syscall argument, high word */ \
    BPF_STMT(BPF_LD+BPF_W+BPF_ABS, \
        offsetof(struct seccomp_data, args[(_arg_nr)]) + ARG_HI_OFFSET), \
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, \
        (((uint32_t)((uint64_t)(_arg_val) >> 32)) & 0xFFFFFFFF), 0, 1), \
    BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_ALLOW), \
    /* reload syscall number; all rules expect it in accumulator */ \
    BPF_STMT(BPF_LD+BPF_W+BPF_ABS, \
        offsetof(struct seccomp_data, nr))

/* Allow if syscall argument contains only values in mask */
#define ALLOW_SYSCALL_ARG_MASK(_nr, _arg_nr, _arg_mask) \
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, (__NR_##_nr), 0, 8), \
    /* load, mask and test syscall argument, low word */ \
    BPF_STMT(BPF_LD+BPF_W+BPF_ABS, \
        offsetof(struct seccomp_data, args[(_arg_nr)]) + ARG_LO_OFFSET), \
    BPF_STMT(BPF_ALU+BPF_AND+BPF_K, ~((_arg_mask) & 0xFFFFFFFF)), \
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0, 0, 4), \
    /* load, mask and test syscall argument, high word */ \
    BPF_STMT(BPF_LD+BPF_W+BPF_ABS, \
        offsetof(struct seccomp_data, args[(_arg_nr)]) + ARG_HI_OFFSET), \
    BPF_STMT(BPF_ALU+BPF_AND+BPF_K, \
        ~(((uint32_t)((uint64_t)(_arg_mask) >> 32)) & 0xFFFFFFFF)), \
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0, 0, 1), \
    BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_ALLOW), \
    /* reload syscall number; all rules expect it in accumulator */ \
    BPF_STMT(BPF_LD+BPF_W+BPF_ABS, \
        offsetof(struct seccomp_data, nr))

/*
 * This is a special case for AArch64 where this syscall apparently only
 * exists in 32-bit compatibility mode, so we can't include the definition
 * even though it gets called somewhere in libc.
 */
#if defined(__aarch64__) && !defined(__NR_fstatat64)
#define __NR_fstatat64 0x4f
#endif

/*
 * The main supported configuration is Linux on x86_64 with either glibc or
 * musl-libc, with secondary support for x86, ARM and ARM64 (AAarch64) Linux.
 *
 * Syscalls marked with "maybe" don't seem to be used in the default
 * configuration, but should probably be whitelisted anyway.
 *
 * Syscalls marked with comments like "musl-libc", "raspi" or "alt DB" were
 * observed to be necessary on that particular configuration, but there are
 * probably other configurations in which they are neccessary as well.
 * ("alt DB" represents libsqlite compiled with different options.)
 *
 * Syscalls marked "vdso" aren't normally caught by seccomp because they are
 * implemented in the vdso(7) in most configurations, but it's still prudent
 * to whitelist them here.
 */
static sock_filter filter[] = {
    VALIDATE_ARCHITECTURE,
    EXAMINE_SYSCALL,

    // memory management
#ifdef __NR_mmap
    ALLOW_SYSCALL_ARG_MASK(mmap, 2, PROT_NONE|PROT_READ|PROT_WRITE),
#endif
    ALLOW_SYSCALL(munmap),
    ALLOW_SYSCALL_ARG_MASK(mprotect, 2, PROT_NONE|PROT_READ|PROT_WRITE),
    ALLOW_SYSCALL(madvise),
    ALLOW_SYSCALL(brk),

    // basic file IO
#ifdef __NR_open
    ALLOW_SYSCALL(open),
#endif
    ALLOW_SYSCALL(openat),
    ALLOW_SYSCALL(read),
    ALLOW_SYSCALL(write),
    ALLOW_SYSCALL(close),
#ifdef __NR_stat
    ALLOW_SYSCALL(stat),
#endif
    ALLOW_SYSCALL(fstat),
#ifdef __NR_newfstatat
    ALLOW_SYSCALL(newfstatat),
#endif
    ALLOW_SYSCALL(fsync), // maybe
#ifdef __NR_creat
    ALLOW_SYSCALL(creat), // maybe; for DB journal
#endif
#ifdef __NR_unlink
    ALLOW_SYSCALL(unlink), // for DB journal
#endif
    ALLOW_SYSCALL(lseek), // musl-libc; alt DB
    ALLOW_SYSCALL(truncate), // for truncate-mode DB
    ALLOW_SYSCALL(ftruncate), // for truncate-mode DB
    ALLOW_SYSCALL(dup), // for perror(), apparently

    // more IO
    ALLOW_SYSCALL(pread64),
    ALLOW_SYSCALL(pwrite64),
    ALLOW_SYSCALL(fdatasync),
    ALLOW_SYSCALL(writev), // musl-libc
    ALLOW_SYSCALL(preadv), // maybe; alt-DB
    ALLOW_SYSCALL(preadv2), // maybe

    // misc syscalls called from libc
    ALLOW_SYSCALL(getcwd),
    ALLOW_SYSCALL(getpid),
    ALLOW_SYSCALL(geteuid),
    ALLOW_SYSCALL(gettid), // maybe
    ALLOW_SYSCALL_ARG(ioctl, 1, TIOCGWINSZ), // musl-libc
    ALLOW_SYSCALL_ARG(fcntl, 1, F_GETFL),
    ALLOW_SYSCALL_ARG(fcntl, 1, F_SETFL),
    ALLOW_SYSCALL_ARG(fcntl, 1, F_GETLK),
    ALLOW_SYSCALL_ARG(fcntl, 1, F_SETLK),
    ALLOW_SYSCALL_ARG(fcntl, 1, F_SETLKW), // maybe
    ALLOW_SYSCALL(exit),
    ALLOW_SYSCALL(exit_group),
    ALLOW_SYSCALL(rt_sigprocmask), // musl-libc
    ALLOW_SYSCALL(clock_nanosleep), // gets called very rarely
#ifdef __NR_rseq
    ALLOW_SYSCALL(rseq),
#endif

    // to crash properly on SIGSEGV
    DENY_SYSCALL_ERRNO(tgkill, EPERM),
    DENY_SYSCALL_ERRNO(tkill, EPERM), // musl-libc
    DENY_SYSCALL_ERRNO(rt_sigaction, EPERM),

    // threading
    ALLOW_SYSCALL(futex),

    // networking
#ifdef __NR_poll
    ALLOW_SYSCALL(poll),
#endif
#ifdef __NR_accept
    ALLOW_SYSCALL(accept),
#endif
    ALLOW_SYSCALL(setsockopt),
    ALLOW_SYSCALL(sendto),
    ALLOW_SYSCALL(recvfrom),
    ALLOW_SYSCALL(shutdown),

    // vdso
    ALLOW_SYSCALL(clock_gettime),
    ALLOW_SYSCALL(gettimeofday),
#ifdef __NR_time
    ALLOW_SYSCALL(time),
#endif
    ALLOW_SYSCALL(rt_sigreturn),

    // i386
#ifdef __NR_socketcall
    ALLOW_SYSCALL_ARG(socketcall, 0, SYS_ACCEPT),
    ALLOW_SYSCALL_ARG(socketcall, 0, SYS_SETSOCKOPT),
    ALLOW_SYSCALL_ARG(socketcall, 0, SYS_SEND),
    ALLOW_SYSCALL_ARG(socketcall, 0, SYS_RECV),
    ALLOW_SYSCALL_ARG(socketcall, 0, SYS_SENDTO), // maybe
    ALLOW_SYSCALL_ARG(socketcall, 0, SYS_RECVFROM), // maybe
    ALLOW_SYSCALL_ARG(socketcall, 0, SYS_SHUTDOWN),
#endif

    // Raspberry Pi (ARM)
#ifdef __NR_set_robust_list
    ALLOW_SYSCALL(set_robust_list),
#endif
#ifdef __NR_clock_gettime64
    ALLOW_SYSCALL(clock_gettime64),
#endif
#ifdef __NR_mmap2
    ALLOW_SYSCALL_ARG_MASK(mmap2, 2, PROT_NONE|PROT_READ|PROT_WRITE),
#endif
#ifdef __NR_fcntl64
    ALLOW_SYSCALL(fcntl64),
#endif
#ifdef __NR_stat64
    ALLOW_SYSCALL(stat64),
#endif
#ifdef __NR_send
    ALLOW_SYSCALL(send),
#endif
#ifdef __NR_recv
    ALLOW_SYSCALL(recv),
#endif
#ifdef __NR_fstat64
    ALLOW_SYSCALL(fstat64),
#endif
#ifdef __NR_geteuid32
    ALLOW_SYSCALL(geteuid32),
#endif
#ifdef __NR_truncate64
    ALLOW_SYSCALL(truncate64),
#endif
#ifdef __NR_ftruncate64
    ALLOW_SYSCALL(ftruncate64),
#endif
#ifdef __NR_sigreturn
    ALLOW_SYSCALL(sigreturn), // vdso
#endif
#ifdef __NR_clock_nanosleep_time64
    ALLOW_SYSCALL(clock_nanosleep_time64), // maybe
#endif

    // AArch64 (ARM64)
#ifdef __NR_unlinkat
    ALLOW_SYSCALL(unlinkat),
#endif
#ifdef __NR_fstatat64
    ALLOW_SYSCALL(fstatat64),
#endif
#ifdef __NR_ppoll
    ALLOW_SYSCALL(ppoll),
#endif

    KILL_PROCESS
};

static sock_fprog prog = {
    ARRLEN(filter), filter
};

// Our own wrapper for the seccomp() syscall.
int seccomp(unsigned int operation, unsigned int flags, void *args) {
    return syscall(__NR_seccomp, operation, flags, args);
}

#ifndef CONFIG_NOLANDLOCK

// Support compilation on systems that only have older Landlock headers.
#ifndef LANDLOCK_ACCESS_FS_REFER
#define LANDLOCK_ACCESS_FS_REFER 0
#endif
#ifndef LANDLOCK_ACCESS_FS_TRUNCATE
#define LANDLOCK_ACCESS_FS_TRUNCATE 0
#endif

struct landlock_ruleset_attr ruleset_attr = {
    .handled_access_fs = LANDLOCK_ACCESS_FS_READ_FILE
        | LANDLOCK_ACCESS_FS_WRITE_FILE
        | LANDLOCK_ACCESS_FS_READ_DIR
        | LANDLOCK_ACCESS_FS_MAKE_REG
        | LANDLOCK_ACCESS_FS_MAKE_DIR
        | LANDLOCK_ACCESS_FS_MAKE_SYM
        | LANDLOCK_ACCESS_FS_MAKE_SOCK
        | LANDLOCK_ACCESS_FS_MAKE_FIFO
        | LANDLOCK_ACCESS_FS_MAKE_BLOCK
        | LANDLOCK_ACCESS_FS_REMOVE_FILE
        | LANDLOCK_ACCESS_FS_REMOVE_DIR
        | LANDLOCK_ACCESS_FS_TRUNCATE
        | LANDLOCK_ACCESS_FS_REFER
};

uint64_t landlock_perms = LANDLOCK_ACCESS_FS_READ_FILE
    | LANDLOCK_ACCESS_FS_WRITE_FILE
    | LANDLOCK_ACCESS_FS_TRUNCATE
    | LANDLOCK_ACCESS_FS_MAKE_REG
    | LANDLOCK_ACCESS_FS_REMOVE_FILE;

int landlock_fd;
bool landlock_supported;

/*
 * Our own wrappers for Landlock syscalls.
 */

int landlock_create_ruleset(const struct landlock_ruleset_attr *attr, size_t size, uint32_t flags) {
    return syscall(__NR_landlock_create_ruleset, attr, size, flags);
}

int landlock_add_rule(int ruleset_fd, enum landlock_rule_type rule_type, const void *rule_attr, uint32_t flags) {
    return syscall(__NR_landlock_add_rule, ruleset_fd, rule_type, rule_attr, flags);
}

int landlock_restrict_self(int ruleset_fd, uint32_t flags) {
    return syscall(__NR_landlock_restrict_self, ruleset_fd, flags);
}

static void landlock_path(std::string path, uint32_t perms) {
    struct landlock_path_beneath_attr path_beneath = {
        .allowed_access = perms
    };

    path_beneath.parent_fd = open(path.c_str(), O_PATH|O_CLOEXEC);
    if (path_beneath.parent_fd < 0) {
        perror(path.c_str());
        exit(1);
    }

    if (landlock_add_rule(landlock_fd, LANDLOCK_RULE_PATH_BENEATH, &path_beneath, 0)) {
        perror("landlock_add_rule");
        exit(1);
    }

    close(path_beneath.parent_fd);
}

static bool landlock_detect() {
    int abi = landlock_create_ruleset(NULL, 0, LANDLOCK_CREATE_RULESET_VERSION);

    if (abi < 0) {
        if (errno == ENOSYS || errno == EOPNOTSUPP) {
            std::cout << "[WARN] No Landlock support on this system" << std::endl;
            return false;
        }
        perror("landlock_create_ruleset");
        exit(1);
    }

    std::cout << "[INFO] Detected Landlock ABI version: " << abi << std::endl;

    switch (abi) {
    case 1:
        ruleset_attr.handled_access_fs &= ~LANDLOCK_ACCESS_FS_REFER;
        landlock_perms &= ~LANDLOCK_ACCESS_FS_REFER;
        // fallthrough
    case 2:
        ruleset_attr.handled_access_fs &= ~LANDLOCK_ACCESS_FS_TRUNCATE;
        landlock_perms &= ~LANDLOCK_ACCESS_FS_TRUNCATE;
    }

    return true;
}

static void landlock_init() {
    std::cout << "[INFO] Setting up Landlock sandbox..." << std::endl;

    landlock_supported = landlock_detect();

    if (!landlock_supported)
        return;

    landlock_fd = landlock_create_ruleset(&ruleset_attr, sizeof(ruleset_attr), 0);
    if (landlock_fd < 0) {
        perror("landlock_create_ruleset");
        exit(1);
    }

    std::string dbdir = std::filesystem::path(settings::DBPATH).parent_path();

    // for the DB files (we can't rely on them being in the working directory)
    landlock_path(dbdir == "" ? "." : dbdir, landlock_perms);
    // for writing the gruntwork file
    landlock_path(settings::TDATADIR, landlock_perms);
    // for passowrd salting during account creation
    landlock_path("/dev/urandom", LANDLOCK_ACCESS_FS_READ_FILE);
    // for core dumps, optionally
    if (settings::SANDBOXEXTRAPATH != "")
        landlock_path(settings::SANDBOXEXTRAPATH, landlock_perms);
}

#endif // !CONFIG_NOLANDLOCK

static void sigsys_handler(int signo, siginfo_t *info, void *context) {
    // report the unhandled syscall
    std::cout << "[FATAL] Unhandled syscall " << info->si_syscall
       << " at " << std::hex << info->si_call_addr << " on arch " << info->si_arch << std::endl;

    std::cout << "If you're unsure why this is happening, please read https://openfusion.dev/docs/development/the-sandbox/" << std::endl
              << "for more information and possibly open an issue at https://github.com/OpenFusionProject/OpenFusion/issues to report"
              << " needed changes in our seccomp filter." << std::endl;

    exit(1);
}

void sandbox_init() {
    if (!settings::SANDBOX) {
        std::cout << "[WARN] Running without a sandbox" << std::endl;
        return;
    }

    // listen to SIGSYS to report unhandled syscalls
    struct sigaction sa = {};

    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = sigsys_handler;

    if (sigaction(SIGSYS, &sa, NULL) < 0) {
        perror("sigaction");
        exit(1);
    }

#ifndef CONFIG_NOLANDLOCK
    landlock_init();
#else
    std::cout << "[WARN] Built without Landlock" << std::endl;
#endif
}

void sandbox_start() {
    if (!settings::SANDBOX)
        return;

    std::cout << "[INFO] Starting seccomp-bpf sandbox..." << std::endl;

    // Sandboxing starts in sandbox_thread_start().
}

void sandbox_thread_start() {
    if (!settings::SANDBOX)
        return;

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
        perror("prctl");
        exit(1);
    }

#ifndef CONFIG_NOLANDLOCK
    if (landlock_supported) {
        if (landlock_restrict_self(landlock_fd, 0)) {
            perror("landlock_restrict_self");
            exit(1);
        }
    }
#endif

    if (seccomp(SECCOMP_SET_MODE_FILTER, 0, &prog) < 0) {
        perror("seccomp");
        exit(1);
    }
}

#endif
