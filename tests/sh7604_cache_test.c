#include "sh2_recomp/sh7604_cache.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stddef.h>

int main(void)
{
    Sh7604Cache cache = {0};
    Sh7604System control = {0};
    uint8_t line[SH7604_CACHE_LINE_SIZE] = {0x12, 0x34, 0x56, 0x78};
    uint32_t value;
    assert(!sh7604_cache_read(&cache, &control, 0, 2, true, line, &value));

    control.cache_control = 0x03;
    assert(!sh7604_cache_read(&cache, &control, 0, 2, true, line, &value));
    assert(sh7604_cache_read(&cache, &control, 0, 4, false, line, &value));
    assert(value == UINT32_C(0x12345678));
    line[0] = 0xab;
    assert(sh7604_cache_read(&cache, &control, 0, 2, true, line, &value));
    assert(value == 0x1234);
    sh7604_cache_write(&cache, &control, 0, 2, 0x5678);
    assert(sh7604_cache_read(&cache, &control, 0, 4, true, NULL, &value));
    assert(value == UINT32_C(0x56785678));

    sh7604_cache_purge(&cache);
    assert(!sh7604_cache_read(&cache, &control, 0, 2, true, NULL, &value));
    control.cache_control = 1;
    for (unsigned i = 0; i < 4; ++i) {
        line[0] = (uint8_t)i;
        assert(sh7604_cache_read(&cache, &control, i * 0x400, 1, false, line, &value));
        assert(value == i);
        assert(cache.set[0].tag[3 - i] == i);
    }
    /* Updating the oldest entry makes the next-oldest one the victim. */
    sh7604_cache_write(&cache, &control, 0, 1, 0x55);
    assert(sh7604_cache_read(&cache, &control, 0x1000, 1, false, line, &value));
    assert(!sh7604_cache_read(&cache, &control, 0x400, 1, false, NULL, &value));
    assert(sh7604_cache_read(&cache, &control, 0, 1, true, NULL, &value) && value == 0x55);

    sh7604_cache_purge(&cache);
    control.cache_control = 0x09;
    for (unsigned i = 0; i < 3; ++i)
        assert(sh7604_cache_read(&cache, &control, i * 0x400, 1, false, line, &value));
    assert(cache.set[0].valid == 0x0c);
    assert(!sh7604_cache_read(&cache, &control, 0, 1, false, NULL, &value));
    return 0;
}
