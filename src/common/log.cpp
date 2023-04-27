#include "common/log.h"

namespace contention_prof {

std::ofstream Logger::file_;

std::string get_log_level_str(LogLevel log_level) {
    switch (log_level) {
    case DEBUG:
        return "DEBUG";
    case INFO:
        return "INFO";
    case WARN:
        return "WARN";
    case ERROR:
        return "ERROR";
    }
    return "INVALID_LOG_LEVEL";
}

void init_logger(const std::string& filename) {
    Logger::file_.open(filename, std::ofstream::app);
}

}  // namespace contention_prof
