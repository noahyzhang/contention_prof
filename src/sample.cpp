#include <math.h>
#include <mutex>
#include <sstream>
#include "common/object_pool.h"
#include "profiler.h"
#include "sample.h"

namespace contention_prof {

CollectorSpeedLimit g_cp_sl;

// ----------- SampledContention ------------------

void SampledContention::dump_and_destroy(size_t) {
    if (g_cp) {
        pthread_mutex_lock(&g_cp_mutex);
        if (g_cp) {
            g_cp->dump_and_destroy(this);
            pthread_mutex_unlock(&g_cp_mutex);
            return;
        }
        pthread_mutex_unlock(&g_cp_mutex);
    }
    destroy();
}

void SampledContention::destroy() {
    return_object(this);
}



}  // namespace contention_prof
