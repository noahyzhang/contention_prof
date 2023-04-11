#include <time.h>
#include "common.h"

namespace contention_prof {

uint64_t Util::get_monotonic_time_ns() {
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec *  static_cast<uint64_t>(1E9) + now.tv_nsec;
}

}  // namespace contention_prof
