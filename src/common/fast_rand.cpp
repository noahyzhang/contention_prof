#include <limits>
#include "common/common.h"
#include "fast_rand.h"

namespace contention_prof {

using SplitMix64Seed = uint64_t;

inline uint64_t splitmix64_next(SplitMix64Seed* seed) {
    uint64_t z = (*seed += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

inline uint64_t xorshift128_next(FastRandSeed* seed) {
    uint64_t s1 = seed->s[0];
    const uint64_t s0 = seed->s[1];
    seed->s[0] = s0;
    s1 ^= s1 << 23;
    seed->s[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);
    return seed->s[1] + s0;
}

void init_fast_rand_seed(FastRandSeed* seed) {
    SplitMix64Seed seed4seed = Util::gettimeofday_us();
    seed->s[0] = splitmix64_next(&seed4seed);
    seed->s[1] = splitmix64_next(&seed4seed);
}

inline uint64_t fast_rand_impl(uint64_t range, FastRandSeed* seed) {
    const uint64_t div = std::numeric_limits<uint64_t>::max() / range;
    uint64_t result;
    do {
        result = xorshift128_next(seed) / div;
    } while (result >= range);
    return result;
}

static __thread FastRandSeed tls_seed_ = {{0, 0}};

inline bool need_init(const FastRandSeed& seed) {
    return seed.s[0] == 0 && seed.s[1] == 0;
}

uint64_t fast_rand() {
    if (need_init(tls_seed_)) {
        init_fast_rand_seed(&tls_seed_);
    }
    return xorshift128_next(&tls_seed_);
}

uint64_t fast_rand(FastRandSeed* seed) {
    return xorshift128_next(seed);
}

uint64_t fast_rand_less_than(uint64_t range) {
    if (range == 0) {
        return 0;
    }
    if (need_init(tls_seed_)) {
        init_fast_rand_seed(&tls_seed_);
    }
    return fast_rand_impl(range, &tls_seed_);
}

int64_t fast_rand_in_64(int64_t min, int64_t max) {
    if (need_init(tls_seed_)) {
        init_fast_rand_seed(&tls_seed_);
    }
    if (min >= max) {
        if (min == max) {
            return min;
        }
        const int64_t tmp = min;
        min = max;
        max = tmp;
    }
    int64_t range = max - min + 1;
    if (range == 0) {
        return (int64_t)xorshift128_next(&tls_seed_);
    }
    return min + (int64_t)fast_rand_impl(max - min + 1, &tls_seed_);
}

uint64_t fast_rand_in_u64(uint64_t min, uint64_t max) {
    if (need_init(tls_seed_)) {
        init_fast_rand_seed(&tls_seed_);
    }
    if (min >= max) {
        if (min == max) {
            return min;
        }
        const uint64_t tmp = min;
        min = max;
        max = tmp;
    }
    uint64_t range = max - min + 1;
    if (range == 0) {
        return xorshift128_next(&tls_seed_);
    }
    return min + fast_rand_impl(range, &tls_seed_);
}

}  // namespace contention_prof
