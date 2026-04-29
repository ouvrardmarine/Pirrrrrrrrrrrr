#ifndef PANIC_HPP_
#define PANIC_HPP_

#ifdef __cplusplus
extern "C" {
#endif

#include <cstdint>
#include "messages.h"

/**
 * Enumeration class defining possible panic sources
 */
enum class PanicType : uint8_t {
    ChargerError = 1,   // error related to battery charging
    AdcError,           // error related to battery voltage conversion
    MallocError         // memory allocation failed
};

/**
 * @brief Handle an unrecoverable error and display corresponding error on leds
 * This function never returns.
 */
[[noreturn]] void Panic_Raise(PanicType panicId);

#ifdef __cplusplus
}
#endif

#endif /* PANIC_HPP_ */
