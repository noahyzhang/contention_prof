#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <fstream>
#include <sstream>
#include "common.h"

namespace contention_prof {

uint64_t Util::get_monotonic_time_ns() {
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec *  static_cast<uint64_t>(1E9) + now.tv_nsec;
}

uint64_t Util::get_monotonic_time_us() {
    return get_monotonic_time_ns() / 1000L;
}

int64_t Util::gettimeofday_us() {
    timeval now;
    gettimeofday(&now, nullptr);
    return now.tv_sec * 1000000L + now.tv_usec;
}

uint64_t Util::fmix64(uint64_t k) {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}

std::string Util::get_self_maps() {
    std::ifstream maps("/proc/self/maps");
    std::stringstream oss;
    std::string line;

    if (maps.is_open()) {
        for (; std::getline(maps, line); ) {
            oss << line << '\n';
        }
        maps.close();
    }
    return oss.str();
}

}  // namespace contention_prof
