#ifndef __COMMON_H_
#define __COMMON_H_

#include <sys/types.h>
#include <iostream>

static inline bool
is_power_of_2(u_int32_t num) {
    return (num & (num - 1)) == 0;
}

/* convert to a number to power of 2 which is greater than it
 * For example, convert 3 to power of 2, the result is 4
 */
static inline u_int32_t
convert_to_power_of_2(u_int32_t num) {
    u_int32_t bits = 0;
    while (num) {
        ++bits;
        num >>= 1;
    }

    // bits should not exceed 31
    if (bits >= sizeof(num) * 8)
        bits = sizeof(num) * 8 - 1;

#ifdef DEBUG
    std::cout << "bits is " << bits << std::endl;
    std::cout << "sizeof(num) * 8 = " << sizeof(num) * 8 << std::endl;
#endif
    return (bits == 0) ? 0 : 1 << bits;
}

#endif
