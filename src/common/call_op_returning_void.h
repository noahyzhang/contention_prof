/**
 * @file call_op_returning_void.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */

namespace contention_prof {

template <typename Op, typename T1, typename T2>
inline void call_op_returning_void(const Op& op, T1& v1, const T2& v2) {
    return op(v1, v2);
}

}  // namespace contention_prof
