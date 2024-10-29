#pragma once

#include <array>
#include <optional>

#include <assert.h>

template<class T, size_t N>
class Bucket {
    std::array<T, N> buf;
    size_t sz;
public:
    Bucket() {
        sz = 0;
    }

    void add(const T& item) {
        assert(sz < N);
        buf[sz++] = item;
    }

    std::optional<T> get(size_t idx) const {
        if (idx < sz) {
            return buf[idx];
        }
        return std::nullopt;
    }

    size_t size() const {
        return sz;
    }

    bool isFull() const {
        return sz == N;
    }

    void clear() {
        sz = 0;
    }
};
