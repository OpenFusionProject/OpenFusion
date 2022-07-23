#include "Rand.hpp"
#include "core/Core.hpp"

std::unique_ptr<std::mt19937> Rand::generator;

int32_t Rand::rand(int32_t startInclusive, int32_t endExclusive) {
    std::uniform_int_distribution<int32_t> dist(startInclusive, endExclusive - 1);
    return dist(*Rand::generator);
}

int32_t Rand::rand(int32_t endExclusive) {
    return Rand::rand(0, endExclusive);
}

int32_t Rand::rand() {
    return Rand::rand(0, INT32_MAX);
}

int32_t Rand::randWeighted(const std::vector<int32_t>& weights) {
    std::discrete_distribution<int32_t> dist(weights.begin(), weights.end());
    return dist(*Rand::generator);
}

float Rand::randFloat(float startInclusive, float endExclusive) {
    std::uniform_real_distribution<float> dist(startInclusive, endExclusive);
    return dist(*Rand::generator);
}

float Rand::randFloat(float endExclusive) {
    return Rand::randFloat(0.0f, endExclusive);
}

float Rand::randFloat() {
    return Rand::randFloat(0.0f, 1.0f);
}

#define RANDBYTES 8

/*
 * Cryptographically secure RNG. Borrowed from bcrypt_gensalt().
 */
uint64_t Rand::cryptoRand() {
    uint8_t buf[RANDBYTES];

#ifdef _WIN32
    HCRYPTPROV p;

    // Acquire a crypt context for generating random bytes.
    if (CryptAcquireContext(&p, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) == FALSE) {
        goto fail;
    }

    if (CryptGenRandom(p, RANDBYTES, (BYTE*)buf) == FALSE) {
        goto fail;
    }

    if (CryptReleaseContext(p, 0) == FALSE) {
        goto fail;
    }
#else
    int fd;

    // Get random bytes on Unix/Linux.
    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        perror("open");
        goto fail;
    }

    if (read(fd, buf, RANDBYTES) < RANDBYTES) {
        perror("read");
        close(fd);
        goto fail;
    }

    close(fd);
#endif

    return *(uint64_t*)buf;

fail:
    std::cout << "[FATAL] Failed to generate cryptographic random number" << std::endl;
    terminate(0);

    /* not reached */
    return 0;
}

void Rand::init(uint64_t seed) {
    Rand::generator = std::make_unique<std::mt19937>(std::mt19937(seed));
}
