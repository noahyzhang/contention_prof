/**
 * @file series.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-14
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

namespace contention_prof {

template <typename T, typename Op>
class SeriesBase {
public:
    explicit SeriesBase(const Op& op) {}
};

// template <typename T, typename Op>
// void SeriesBase<T, Op>::append_day(const T& value) {
//     day_.day(nday_) = value;
//     ++nday_;
//     if (nday_ >= 30) {
//         nday_ = 0;
//     }
// }

template <typename T, typename Op>
class Series : public SeriesBase {

};

}  // namespace contention_prof
