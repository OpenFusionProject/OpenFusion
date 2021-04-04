#pragma once

#include <random>
#include <memory>

namespace Rand {
    extern std::unique_ptr<std::mt19937> generator;

    void init(uint64_t seed);

    int32_t rand(int32_t startInclusive, int32_t endExclusive);
    int32_t rand(int32_t endExclusive);
    int32_t rand();

    int32_t randWeighted(const std::vector<int32_t>& weights);

    float randFloat(float startInclusive, float endExclusive);
    float randFloat(float endExclusive);
    float randFloat();
};
