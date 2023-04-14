#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <map>
#include <mutex>
#include <list>
#include <gflags/gflags.h>
#include "common/time.h"
#include "common/log.h"
#include "collector.h"

namespace contention_prof {

DEFINE_int32(collector_max_pending_samples, 1000, "Destroy unprocessed samples when they're too many");

static CollectorSpeedLimit g_null_speed_limit;

class Collector : public Reducer<Collected*, CombineCollected> {
public:
    Collector();
    ~Collector();

    int64_t last_active_cpuwide_us() const {
        return last_active_cpuwide_us_;
    }

    void wakeup_grab_thread();

public:
    static Collector* get_instance() {
        static Collector instance;
        return &instance;
    }

private:
    void grab_thread();
    void dump_thread();

    void update_speed_limit(
        CollectorSpeedLimit* speed_limit,
        size_t* last_grab_count,
        size_t cur_grab_count,
        int64_t interval_us);

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
    int64_t grab_count_;
    int64_t drop_count_;
    int64_t dump_count_;
    pthread_mutex_t dump_thread_mutex_;
    pthread_cond_t dump_thread_cond_;
    LinkNode<Collected> dump_root_;
    pthread_mutex_t sleep_mutex_;
    pthread_cond_t sleep_cond_;
};

Collector::Collector()
    : last_active_cpuwide_us_(Util::get_monotonic_time_us())
    , created_(false)
    , stop_(false)
    , grab_thread_(0)
    , dump_thread_(0)
    , grab_count_(0)
    , drop_count_(0)
    , dump_count_(0) {
    pthread_mutex_init(&dump_thread_mutex_, nullptr);
    pthread_cond_init(&dump_thread_cond_, nullptr);
    pthread_mutex_init(&sleep_mutex_, nullptr);
    pthread_cond_init(&sleep_cond_, nullptr);
    int res = pthread_create(&grab_thread_, nullptr, run_grab_thread, this);
    if (res != 0) {
        LOG(ERROR) << "Fail to create Collector, " << strerror(errno);
    } else {
        created_ = true;
    }
}

Collector::~Collector() {
    if (created_) {
        stop_ = true;
        pthread_join(grab_thread_, nullptr);
        created_ = false;
    }
    pthread_mutex_destroy(&dump_thread_mutex_);
    pthread_cond_destroy(&dump_thread_cond_);
    pthread_mutex_destroy(&sleep_mutex_);
    pthread_cond_destroy(&sleep_cond_);
}

void Collector::grab_thread() {
    last_active_cpuwide_us_ = Util::get_monotonic_time_us();
    int64_t last_before_update_sl = last_active_cpuwide_us_;

    int res = pthread_create(&dump_thread_, nullptr, run_dump_thread, this);
    if (res < 0) {
        LOG(ERROR) << "pthread_create failed, err: " << strerror(errno);
        return;
    }

    double busy_second = 0;

    using GrabMap = std::map<CollectorSpeedLimit*, size_t>;
    GrabMap last_grab_count_map;
    GrabMap grab_count_map;
    using PreprocessorMap = std::map<CollectorPreprocessor*, std::vector<Collected*>>;
    PreprocessorMap prep_map;

    for (; !stop_;) {
        const int64_t abstime = last_active_cpuwide_us_ + COLLECTOR_GRAB_INTERVAL_US;
        for (auto it = prep_map.begin(); it != prep_map.end(); ++it) {
            it->second.clear();
        }
        LinkNode<Collected>* head = this->reset();
        if (head) {
            LinkNode<Collected> tmp_root;
            head->insert_before_as_list(&tmp_root);
            head = nullptr;

            for (LinkNode<Collected>* p = tmp_root.next(); p != &tmp_root;) {
                LinkNode<Collected>* saved_next = p->next();
                p->remove_from_list();
                CollectorPreprocessor* prep = p->value()->preprocessor();
                prep_map[prep].push_back(p->value());
                p = saved_next;
            }
            LinkNode<Collected> root;
            for (PreprocessorMap::iterator it = prep_map.begin(); it != prep_map.end(); ++it) {
                std::vector<Collected*>& list = it->second;
                if (it->second.empty()) {
                    continue;
                }
                if (it->first != nullptr) {
                    it->first->process(list);
                }
                for (size_t i = 0; i < list.size(); ++i) {
                    Collected* p = list[i];
                    CollectorSpeedLimit* speed_limit = p->speed_limit();
                    if (speed_limit == nullptr) {
                        ++grab_count_map[&g_null_speed_limit];
                    } else {
                        ++grab_count_map[speed_limit];
                    }
                    ++grab_count_;
                    if (grab_count_ >= drop_count_ + dump_count_ + FLAGS_collector_max_pending_samples) {
                        ++drop_count_;
                        p->destroy();
                    } else {
                        p->insert_before(&root);
                    }
                }
            }
            if (root.next() != &root) {
                LinkNode<Collected>* head2 = root.next();
                root.remove_from_list();
                pthread_mutex_lock(&dump_thread_mutex_);
                head2->insert_before_as_list(&dump_root_);
                pthread_cond_signal(&dump_thread_cond_);
                pthread_mutex_unlock(&dump_thread_mutex_);
            }
        }
        int64_t now = Util::get_monotonic_time_us();
        int64_t interval = now - last_before_update_sl;
        last_before_update_sl = now;
        for (GrabMap::iterator it = grab_count_map.begin(); it != grab_count_map.end(); ++it) {
            update_speed_limit(it->first, &last_grab_count_map[it->first], it->second, interval);
        }

        now = Util::get_monotonic_time_us();
        busy_second += (now - last_active_cpuwide_us_) / 1E6;
        last_active_cpuwide_us_ = now;
        if (!stop_ && abstime > now) {
            timespec abstimespec = microseconds_from_now(abstime - now);
            pthread_mutex_lock(&sleep_mutex_);
            pthread_cond_timedwait(&sleep_cond_, &sleep_mutex_, &abstimespec);
            pthread_mutex_unlock(&sleep_mutex_);
        }
        last_active_cpuwide_us_ = Util::get_monotonic_time_us();
    }
    pthread_mutex_lock(&dump_thread_mutex_);
    stop_ = true;
    pthread_cond_signal(&dump_thread_cond_);
    pthread_mutex_unlock(&dump_thread_mutex_);
    pthread_join(dump_thread_, nullptr);
}

void Collector::wakeup_grab_thread() {
    pthread_mutex_lock(&sleep_mutex_);
    pthread_cond_signal(&sleep_cond_);
    pthread_mutex_unlock(&sleep_mutex_);
}

void Collector::update_speed_limit(
    CollectorSpeedLimit* sl, size_t* last_grab_count,
    size_t cur_grab_count, int64_t interval_us) {
    const size_t round_grab_count = cur_grab_count - *last_grab_count;
    if (round_grab_count == 0) {
        return;
    }
    *last_grab_count = cur_grab_count;
    if (interval_us < 0) {
        interval_us = 0;
    }
    size_t new_sampling_range = 0;
    const size_t old_sampling_range = sl->sampling_range;
    if (!sl->ever_grabbed) {
        if (sl->first_sample_real_us) {
            interval_us = Util::gettimeofday_us() - sl->first_sample_real_us;
            if (interval_us < 0) {
                interval_us = 0;
            }
        } else {

        }
        new_sampling_range = FLAGS_collector_expected_per_second
            * interval_us * COLLECTOR_SAMPLING_BASE / (1000000L * round_grab_count);
    } else {
        new_sampling_range = FLAGS_collector_expected_per_second
            * interval_us * old_sampling_range / (1000000L * round_grab_count);
        if (interval_us < 1000000L) {
            new_sampling_range = (new_sampling_range * interval_us
                + old_sampling_range * (1000000L - interval_us)) / 1000000L;
        }
    }
    if (new_sampling_range == 0) {
        new_sampling_range = 1;
    } else if (new_sampling_range > COLLECTOR_SAMPLING_BASE) {
        new_sampling_range = COLLECTOR_SAMPLING_BASE;
    }
    if (new_sampling_range != old_sampling_range) {
        sl->sampling_range = new_sampling_range;
    }
    if (!sl->ever_grabbed) {
        sl->ever_grabbed = true;
    }
}

size_t is_collectable_before_first_time_grabbed(CollectorSpeedLimit* sl) {
    if (!sl->ever_grabbed) {
        int before_add = sl->count_before_grabbed.fetch_add(1, std::memory_order_relaxed);
        if (before_add == 0) {
            sl->first_sample_real_us = Util::get_monotonic_time_us();
        } else if (before_add >= FLAGS_collector_expected_per_second) {
            Collector::get_instance()->wakeup_grab_thread();
        }
    }
    return sl->sampling_range;
}

void Collector::dump_thread() {
    int64_t last_ns = Util::get_monotonic_time_ns();
    double busy_seconds = 0;

    LinkNode<Collected> root;
    size_t round = 0;
    for (; !stop_;) {
        ++round;
        LinkNode<Collected>* new_head = nullptr;
        {
            pthread_mutex_lock(&dump_thread_mutex_);
            for (; !stop_ && dump_root_.next() == &dump_root_;) {
                const uint64_t now_ns = Util::get_monotonic_time_ns();
                busy_seconds += (now_ns - last_ns) / 1E9;
                pthread_cond_wait(&dump_thread_cond_, &dump_thread_mutex_);
                last_ns = Util::get_monotonic_time_ns();
            }
            if (stop_) {
                break;
            }
            new_head = dump_root_.next();
            dump_root_.remove_from_list();
            pthread_mutex_unlock(&dump_thread_mutex_);
        }
        new_head->insert_before_as_list(&root);
        for (LinkNode<Collected>* p = root.next(); !stop_ && p != &root;) {
            LinkNode<Collected>* saved_next = p->next();
            p->remove_from_list();
            Collected* s = p->value();
            s->dump_and_destroy(round);
            ++dump_count_;
            p = saved_next;
        }
    }
}

void Collected::submit(uint64_t cpu_time_us) {
    Collector* d = Collector::get_instance();
    if (cpu_time_us < d->last_active_cpuwide_us() + COLLECTOR_GRAB_INTERVAL_US * 2) {
        *d << this;
    } else {
        destroy();
    }
}

}  // namespace contention_prof
