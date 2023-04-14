#include <unistd.h>
#include "common/reducer.h"
#include "common/log.h"
#include "common/common.h"
#include "common/sampler.h"

namespace contention_prof {

const int WARN_NOSLEEP_THRESHOLD = 2;

struct CombineSampler {
    void operator()(Sampler*& s1, Sampler* s2) const {
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

class SamplerCollector : public Reducer<Sampler*, CombineSampler> {
public:
    static SamplerCollector* get_instance() {
        static SamplerCollector instance;
        return &instance;
    }

    SamplerCollector()
        : created_(false)
        , stop_(false)
        , cumulated_time_us_(0) {
        int res = pthread_create(&tid_, nullptr, sampling_thread, this);
        if (res != 0) {
            LOG(ERROR) << "create sampling_thread failed";
        } else {
            created_ = true;
        }
    }
    ~SamplerCollector() {
        if (created_) {
            stop_ = true;
            pthread_join(tid_, nullptr);
            created_ = false;
        }
    }

    static double get_cumulated_time(void* arg) {
        return ((SamplerCollector*)arg)->cumulated_time_us_ / 1E6;
    }

private:
    void run();
    static void* sampling_thread(void* arg) {
        ((SamplerCollector*)arg)->run();
        return nullptr;
    }

private:
    bool created_;
    bool stop_;
    int64_t cumulated_time_us_;
    pthread_t tid_;
};

void SamplerCollector::run() {
    LinkNode<Sampler> root;
    int consecutive_nosleep = 0;
    for (; !stop_;) {
        int64_t abstime = Util::gettimeofday_us();
        Sampler* s = this->reset();
        if (s) {
            s->insert_before_as_list(&root);
        }
        int nremoved = 0;
        int nsampled = 0;
        for (LinkNode<Sampler>* p = root.next(); p != &root;) {
            LinkNode<Sampler>* saved_next = p->next();
            Sampler* s = p->value();
            s->mtx_.lock();
            if (!s->used_) {
                s->mtx_.unlock();
                p->remove_from_list();
                delete s;
                ++nremoved;
            } else {
                s->take_sample();
                s->mtx_.unlock();
                ++nsampled;
            }
            p = saved_next;
        }
        bool slept = false;
        int64_t now = Util::gettimeofday_us();
        cumulated_time_us_ += now - abstime;
        abstime += 1000000L;
        for (; abstime > now;) {
            usleep(abstime - now);
            slept = true;
            now = Util::gettimeofday_us();
        }
        if (slept) {
            consecutive_nosleep = 0;
        } else {
            if (++consecutive_nosleep >= WARN_NOSLEEP_THRESHOLD) {
                consecutive_nosleep = 0;
                LOG(WARN) << "var is busy at sampling for" << WARN_NOSLEEP_THRESHOLD << " seconds!";
            }
        }
    }
}

Sampler::Sampler() : used_(true) {}

Sampler::~Sampler() {}

void Sampler::schedule() {
    *SamplerCollector::get_instance() << this;
}

void Sampler::destroy() {
    mtx_.lock();
    used_ = false;
    mtx_.unlock();
}

}  // namespace contention_prof
