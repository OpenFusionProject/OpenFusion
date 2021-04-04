#include "Rand.hpp"

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

void Rand::init(uint64_t seed) {
    Rand::generator = std::make_unique<std::mt19937>(std::mt19937(seed));
}
