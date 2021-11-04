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

#define DENY_SYSCALL(name) \
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, __NR_##name, 0, 1), \
    BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_KILL_PROCESS)

static sock_filter filter[] = {
    VALIDATE_ARCHITECTURE,
    EXAMINE_SYSCALL,

    // examples of undesirable syscalls
    DENY_SYSCALL(execve),
    DENY_SYSCALL(fork),
    DENY_SYSCALL(vfork),
    DENY_SYSCALL(clone),
    DENY_SYSCALL(connect),
    DENY_SYSCALL(listen),
    DENY_SYSCALL(bind),
    DENY_SYSCALL(kill),
    DENY_SYSCALL(settimeofday),
    // etc

    // default-permit mode
    BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_ALLOW)
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
