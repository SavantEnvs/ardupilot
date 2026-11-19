/*
 * SITL links every program with -Wl,--wrap,malloc and AP_Common/c++.cpp defines the real
 * __wrap_malloc. The AFL++ runtime (afl-compiler-rt.o) calls malloc too, so waf's configure-time
 * header/function checks (tiny test programs without AP_Common) fail to link and silently turn
 * off HAVE_ENDIAN_H etc. This WEAK fallback only satisfies those test links; in every real
 * ArduPilot program the strong definition in AP_Common/c++.cpp wins.
 */
#include <stddef.h>
void *__libc_malloc(size_t size);
/* __libc_malloc, not __real_malloc: the earliest checks link without --wrap,malloc. */
__attribute__((weak)) void *__wrap_malloc(size_t size) { return __libc_malloc(size); }
