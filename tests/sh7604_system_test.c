#include "sh2_recomp/sh7604_system.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

int main(void)
{
    Sh7604System system;
    uint32_t value;
    sh7604_system_power_on(&system);
    assert(sh7604_system_read(&system, SH7604_STANDBY_CONTROL, 1, &value) && value == 0);
    assert(sh7604_system_read(&system, SH7604_CACHE_CONTROL, 1, &value) && value == 0);
    assert(!sh7604_system_read(&system, SH7604_CACHE_CONTROL, 2, &value));
    assert(sh7604_system_write(&system, SH7604_CACHE_CONTROL, 4, 0x15) == SH7604_SYSTEM_UNMAPPED);
    assert(sh7604_system_write(&system, SH7604_CACHE_CONTROL, 1, 0xdf) == SH7604_SYSTEM_CACHE_PURGE);
    assert(sh7604_system_read(&system, SH7604_CACHE_CONTROL, 1, &value) && value == 0xcf);
    assert(sh7604_system_write(&system, SH7604_STANDBY_CONTROL, 1, 0x41) == SH7604_SYSTEM_WRITTEN);
    assert(sh7604_system_read(&system, SH7604_STANDBY_CONTROL, 1, &value) && value == 0x41);
    assert(sh7604_system_read(&system, SH7604_CACHE_CONTROL, 1, &value) && value == 0xcf);
    assert(sh7604_system_write(&system, SH7604_INTERRUPT_PRIORITY_A, 2, 0xfa9b) == SH7604_SYSTEM_WRITTEN);
    assert(sh7604_system_read(&system, SH7604_INTERRUPT_PRIORITY_A, 2, &value) && value == 0xfa90);
    assert(!sh7604_system_read(&system, SH7604_INTERRUPT_PRIORITY_A, 1, &value));
    sh7604_system_power_on(&system);
    assert(sh7604_system_read(&system, SH7604_INTERRUPT_PRIORITY_A, 2, &value) && value == 0);
    assert(sh7604_system_read(&system, SH7604_CACHE_CONTROL, 1, &value) && value == 0);
    assert(sh7604_system_read(&system, SH7604_STANDBY_CONTROL, 1, &value) && value == 0);
    return 0;
}
