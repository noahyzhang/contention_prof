/**
 * @file type_traits.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-27
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

namespace contention_prof {

template <typename T, T v>
struct integral_constant {
  static const T value = v;
  typedef T value_type;
  typedef integral_constant<T, v> type;
};

template <typename T, T v> const T integral_constant<T, v>::value;

typedef integral_constant<bool, true> true_type;
typedef integral_constant<bool, false> false_type;

template <typename T, typename U> struct is_same : public false_type {};
template <typename T> struct is_same<T, T> : true_type {};


}  // namespace contention_prof
