#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
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
    std::string res;
    int fd = open("/proc/self/maps", O_RDONLY);
    if (fd < 0) {
        return 0;
    }
    char buf[10] = {0};
    for (;;) {
        buf[0] = '\0';
        ssize_t nr = read(fd, buf, sizeof(buf)-1);
        if (nr < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        } else if (nr > 0) {
            res.append(buf);
        } else {  // nr == 0
            break;
        }
    }
    close(fd);
    return res;
}

}  // namespace contention_prof
