#if defined(__OpenBSD__) && !defined(CONFIG_NOSANDBOX)

#include "core/Core.hpp"
#include "settings.hpp"

#include <stdio.h>
#include <unistd.h>

#include <err.h>

static void eunveil(const char *path, const char *permissions) {
    if (unveil(path, permissions) < 0)
        err(1, "unveil");
}

void sandbox_init() {}
void sandbox_thread_start() {}

void sandbox_start() {
    /*
     * There shouldn't ever be a reason to disable this one, but might as well
     * be consistent with the Linux sandbox.
     */
    if (!settings::SANDBOX) {
        std::cout << "[WARN] Running without a sandbox" << std::endl;
        return;
    }

    std::cout << "[INFO] Starting pledge+unveil sandbox..." << std::endl;

    if (pledge("stdio rpath wpath cpath inet flock unveil", NULL) < 0)
        err(1, "pledge");

    // database stuff
    eunveil(settings::DBPATH.c_str(), "rwc");
    eunveil((settings::DBPATH + "-journal").c_str(), "rwc");
    eunveil((settings::DBPATH + "-wal").c_str(), "rwc");

    // tabledata stuff
    eunveil((settings::TDATADIR + "/" + settings::GRUNTWORKJSON).c_str(), "wc");

    // for bcrypt_gensalt()
    eunveil("/dev/urandom", "r");

    eunveil(NULL, NULL);
}

#endif
