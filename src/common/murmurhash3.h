/**
 * @file murmurhash3.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-14
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdlib.h>
#include <stdint.h>

namespace contention_prof {

void MurmurHash3_x86_32(const void* key, int len, uint32_t seed, void* out);

}  // namespace contention_prof
