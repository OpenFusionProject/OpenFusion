#if defined(__linux__) && !defined(CONFIG_NOSANDBOX)

#include "core/Core.hpp" // mostly for ARRLEN
#include "settings.hpp"

#include <stdlib.h>

#include <sys/prctl.h>
#include <sys/ptrace.h>

#include <linux/unistd.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>

// our own wrapper for the seccomp() syscall
// TODO: should this be conditional on a feature check or something?
static inline int seccomp(unsigned int operation, unsigned int flags, void *args) {
    return syscall(__NR_seccomp, operation, flags, args);
}

/*
 * Macros borrowed from from https://outflux.net/teach-seccomp/
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
#else
# error "Seccomp-bpf sandbox unsupported on this architecture"
#endif

#define VALIDATE_ARCHITECTURE \
    BPF_STMT(BPF_LD+BPF_W+BPF_ABS, arch_nr), \
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, ARCH_NR, 1, 0), \
    BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_KILL)

#define EXAMINE_SYSCALL \
    BPF_STMT(BPF_LD+BPF_W+BPF_ABS, syscall_nr)

#define ALLOW_SYSCALL(name) \
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, __NR_##name, 0, 1), \
    BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_ALLOW)

#define KILL_PROCESS \
    BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_KILL)

/*
 * The main supported configuration is Linux on x86_64 with either glibc or
 * musl-libc, with secondary support for Linux on the Raspberry Pi (ARM).
 *
 * Syscalls marked with "maybe" don't seem to be used in the default
 * configuration, but should probably be whitelisted anyway.
 *
 * Syscalls marked with "musl-libc", "raspi" or "alt DB" were observed to be
 * necessary on that configuration, but are probably neccessary in other
 * configurations as well. ("alt DB" represents libsqlite compiled with
 * different options.)
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
    ALLOW_SYSCALL(mmap),
#endif
    ALLOW_SYSCALL(munmap),
    ALLOW_SYSCALL(mprotect),
    ALLOW_SYSCALL(madvise),
    ALLOW_SYSCALL(brk),

    // basic file IO
    ALLOW_SYSCALL(open),
    ALLOW_SYSCALL(openat),
    ALLOW_SYSCALL(read),
    ALLOW_SYSCALL(write),
    ALLOW_SYSCALL(close),
    ALLOW_SYSCALL(stat),
    ALLOW_SYSCALL(fstat),
    ALLOW_SYSCALL(fsync), // maybe
    ALLOW_SYSCALL(creat), // maybe; for DB journal
    ALLOW_SYSCALL(unlink), // for DB journal
    ALLOW_SYSCALL(lseek), // musl-libc; alt DB

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
    ALLOW_SYSCALL(ioctl), // musl-libc
    ALLOW_SYSCALL(fcntl),
    ALLOW_SYSCALL(exit),
    ALLOW_SYSCALL(exit_group),
    ALLOW_SYSCALL(rt_sigprocmask), // musl-libc

    // threading
    ALLOW_SYSCALL(futex),

    // networking
    ALLOW_SYSCALL(poll),
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
    ALLOW_SYSCALL(socketcall),
#endif

    // Raspberry Pi (ARM)
#ifdef __NR_set_robust_list
    ALLOW_SYSCALL(set_robust_list),
#endif
#ifdef __NR_clock_gettime64
    ALLOW_SYSCALL(clock_gettime64),
#endif
#ifdef __NR_mmap2
    ALLOW_SYSCALL(mmap2),
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
#ifdef __NR_sigreturn
    ALLOW_SYSCALL(sigreturn), // vdso
#endif

    BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_KILL_PROCESS)
};

static sock_fprog prog = {
    ARRLEN(filter), filter
};

void sandbox_start() {
    if (!settings::SANDBOX) {
        std::cout << "[WARN] Running without a sandbox" << std::endl;
        return;
    }

    std::cout << "[INFO] Starting seccomp-bpf sandbox..." << std::endl;

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
        perror("prctl");
        exit(1);
    }

    if (seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_TSYNC, &prog) < 0) {
        perror("seccomp");
        exit(1);
    }
}

#endif // SANDBOX_SECCOMP
