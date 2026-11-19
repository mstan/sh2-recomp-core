#ifndef SH2_RECOMP_SH7604_SYSTEM_H
#define SH2_RECOMP_SH7604_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>

#define SH7604_STANDBY_CONTROL UINT32_C(0xFFFFFE91)
#define SH7604_CACHE_CONTROL UINT32_C(0xFFFFFE92)

enum { SH7604_CACHE_ENABLE = 0x01 };

typedef struct {
    uint8_t standby_control, cache_control;
} Sh7604System;

typedef enum {
    SH7604_SYSTEM_UNMAPPED,
    SH7604_SYSTEM_WRITTEN,
    SH7604_SYSTEM_CACHE_PURGE
} Sh7604SystemWrite;

#ifdef __cplusplus
extern "C" {
#endif

void sh7604_system_power_on(Sh7604System *system);
bool sh7604_system_read(const Sh7604System *system, uint32_t address,
                        unsigned size, uint32_t *value);

/* Apply operating-mode changes and clear cache validity and replacement
 * state when the result requests a purge. CP itself reads as zero. */
Sh7604SystemWrite sh7604_system_write(Sh7604System *system, uint32_t address,
                                     unsigned size, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif
