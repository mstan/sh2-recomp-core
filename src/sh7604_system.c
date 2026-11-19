#include "sh2_recomp/sh7604_system.h"

void sh7604_system_power_on(Sh7604System *system)
{
    *system = (Sh7604System){0};
}

bool sh7604_system_read(const Sh7604System *system, uint32_t address,
                        unsigned size, uint32_t *value)
{
    if (size != 1)
        return false;
    switch (address) {
    case SH7604_STANDBY_CONTROL: *value = system->standby_control; return true;
    case SH7604_CACHE_CONTROL: *value = system->cache_control; return true;
    default: return false;
    }
}

Sh7604SystemWrite sh7604_system_write(Sh7604System *system, uint32_t address,
                                     unsigned size, uint32_t value)
{
    if (size != 1)
        return SH7604_SYSTEM_UNMAPPED;
    switch (address) {
    case SH7604_STANDBY_CONTROL:
        system->standby_control = (uint8_t)(value & 0xdf);
        return SH7604_SYSTEM_WRITTEN;
    case SH7604_CACHE_CONTROL:
        system->cache_control = (uint8_t)(value & 0xcf);
        return value & 0x10 ? SH7604_SYSTEM_CACHE_PURGE : SH7604_SYSTEM_WRITTEN;
    default:
        return SH7604_SYSTEM_UNMAPPED;
    }
}
