#ifndef __SOFTBUS_INTERNAL_H__
#define __SOFTBUS_INTERNAL_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "softbus/softbus.h"

/**
 * @brief An  architecture and ABI dependent bit-mask whose settings indicate
 *   detailed processor capabilities.
 * @note getauxval(AT_HWCAP)
 * @note checkout kernel source file `arch/xxx/include/asm/cpufeature.h` for
 *   details.
 */
extern size_t bus__hwcap;

#ifdef __cplusplus
}
#endif
#endif
