#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <map>
#include <list>
#include "collector.h"

namespace contention_prof {

static const int64_t COLLECTOR_GRAB_INTERVAL_US = 100000L;  // 100ms

struct CombineCollected {
    void operator()(Collected*& s1, Collected* s2) const {
        if (s2 == nullptr) {
            return;
        }
        if (s1 == nullptr) {
            s1 = s2;
            return;
        }
        s1->insert_before_as_list(s2);
    }
};

class Collector : public Reducer<Collected*, CombineCollected> {
public:
    Collector();
    ~Collector();

public:
    static Collector* get_instance() {
        static Collector instance;
        return &instance;
    }

private:
    void grab_thread();
    void dump_thread();

    static void* run_grab_thread(void* arg) {
        static_cast<Collector*>(arg)->grab_thread();
        return nullptr;
    }
    static void* run_dump_thread(void* arg) {
        static_cast<Collector*>(arg)->dump_thread();
        return nullptr;
    }

private:
    int64_t last_active_cpuwide_us_;
    bool created_;
    bool stop_;
    pthread_t grab_thread_;
    pthread_t dump_thread_;
    int64_t ngrab_;
    int64_t ndrop_;
    int64_t ndump_;
    pthread_mutex_t dump_thread_mutex_;
    pthread_cond_t dump_thread_cond_;
    std::list<Collected> dump_root_;
    pthread_mutex_t sleep_mutex_;
    pthread_cond_t sleep_cond_;
};

void Collector::grab_thread() {
    last_active_cpuwide_us_ = Util::get_monotonic_time_us();
    int64_t last_before_update_sl = last_active_cpuwide_us_;

    int res = pthread_create(&dump_thread_, nullptr, run_dump_thread, this);
    if (res < 0) {
        LOG(ERROR) << "pthread_create failed, err: " << strerror(errno);
        return;
    }

    typedef std::map<CollectorPreprocessor*, std::vector<Collected*>> PreprocessorMap;
    PreprocessorMap prep_map;

    for (; !stop_;) {
        const int64_t abstime = last_active_cpuwide_us_ + COLLECTOR_GRAB_INTERVAL_US;
        for (auto it = prep_map.begin(); it != prep_map.end(); ++it) {
            it->second.clear();
        }
        std::list<Collected> head;
        if (!head.empty()) {
            for (auto it = head.begin(); it != head.end(); ++it) {
                CollectorPreprocessor* prep = it->preprocessor();
                prep_map[prep].emplace_back(&(*it));
            }
            std::list<Collected> root;
            for (auto it = prep_map.begin(); it != prep_map.end(); ++it) {
                if (it->second.empty()) {
                    continue;
                }
                auto& list = it->second;
                if (it->first != nullptr) {
                    it->first->process(list);
                }
                for (size_t i = 0; i < list.size(); ++i) {
                    Collected* p = list[i];
                    CollectorSpeedLimit* speed_limit = p->speed_limit();
                    if (speed_limit == nullptr) {
                        ++ngrab_map[&g_null_speed_limit];
                    } else {
                        ++ngrab_map[speed_limit];
                    }
                    ++ngrab_;
                    // ???
                }
            }
            if (!root.empty()) {
                dump_root_.swap(root);
                pthread_cond_signal(&dump_thread_cond_);
            }
        }
        
    }
}

void Collector::dump_thread() {
    int64_t last_ns = Util::get_monotonic_time_ns();
    double busy_seconds = 0;

    size_t round = 0;
    for (; !stop_;) {
        ++round;
        for (; !stop_ && dump_root_.empty();) {
            const uint64_t now_ns = Util::get_monotonic_time_ns();
            busy_seconds += (now_ns - last_ns) / 1E9;
            pthread_cond_wait(&dump_thread_cond_, &dump_thread_mutex_);
            last_ns = Util::get_monotonic_time_ns();
        }
        if (stop_) {
            break;
        }
    }
    for (; !stop_ && !dump_root_.empty();) {
        Collected& p = dump_root_.front();
        p.dump_and_destroy(round);
        ++ndump_;
    }
}

void Collected::submit(uint64_t cpu_us) {
    if (cpu_us < Collector::get_instance()->last_active_cpuwide_us() + COLLECTOR_GRAB_INTERVAL_US * 2) {
        *Collector::get_instance() << this;
    } else {
        destroy();
    }
}

}  // namespace contention_prof
