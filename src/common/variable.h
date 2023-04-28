/**
 * @file variable.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-28
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

namespace contention_prof {

struct SeriesOptions {
    SeriesOptions() : fixed_length(true), test_only(false) {}

    bool fixed_length; // useless now
    bool test_only;
};

}  // namespace contention_prof
