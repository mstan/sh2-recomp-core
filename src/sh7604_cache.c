#include "sh2_recomp/sh7604_cache.h"

#include <string.h>

enum { CACHE_ID = 0x02, CACHE_OD = 0x04, CACHE_TWO_WAY = 0x08, NO_WAY = 4 };

void sh7604_cache_purge(Sh7604Cache *cache)
{
    for (unsigned i = 0; i < 64; ++i) {
        cache->set[i].valid = 0;
        cache->set[i].lru = 0;
    }
}

static unsigned match(const Sh7604CacheSet *set, uint32_t address, bool two_way)
{
    uint32_t tag = address >> 10;
    for (unsigned way = two_way ? 2 : 0; way < 4; ++way)
        if ((set->valid & (1u << way)) && set->tag[way] == tag)
            return way;
    return NO_WAY;
}

static void accessed(Sh7604CacheSet *set, unsigned way)
{
    /* One bit per pair of ways. See the SH7604 pseudo-LRU update table. */
    static const uint8_t masks[] = {0x38, 0x26, 0x15, 0x0b};
    static const uint8_t values[] = {0x00, 0x20, 0x14, 0x0b};
    set->lru = (uint8_t)((set->lru & ~masks[way]) | values[way]);
}

static unsigned replacement(const Sh7604CacheSet *set, bool two_way)
{
    if (two_way)
        return set->lru & 1 ? 2 : 3;
    if ((set->lru & 0x38) == 0x38)
        return 0;
    if ((set->lru & 0x26) == 0x06)
        return 1;
    if ((set->lru & 0x15) == 0x01)
        return 2;
    return 3;
}

static bool fits(unsigned offset, unsigned size)
{
    return (size == 1 || size == 2 || size == 4) && offset + size <= SH7604_CACHE_LINE_SIZE;
}

bool sh7604_cache_read(Sh7604Cache *cache, const Sh7604System *system,
                       uint32_t address, unsigned size, bool instruction,
                       const uint8_t *backing, uint32_t *value)
{
    const unsigned offset = address & 15;
    const unsigned control = system->cache_control;
    if (!(control & SH7604_CACHE_ENABLE) || !fits(offset, size))
        return false;
    Sh7604CacheSet *set = &cache->set[(address >> 4) & 63];
    const bool two_way = (control & CACHE_TWO_WAY) != 0;
    unsigned way = match(set, address, two_way);
    if (way == NO_WAY) {
        if (!backing || (control & (instruction ? CACHE_ID : CACHE_OD)))
            return false;
        way = replacement(set, two_way);
        memcpy(set->line[way], backing, SH7604_CACHE_LINE_SIZE);
        set->tag[way] = address >> 10;
        set->valid |= (uint8_t)(1u << way);
    }
    accessed(set, way);
    uint32_t val = 0;
    for (unsigned i = 0; i < size; ++i)
        val = (val << 8) | set->line[way][offset + i];
    *value = val;
    return true;
}

void sh7604_cache_write(Sh7604Cache *cache, const Sh7604System *system,
                        uint32_t address, unsigned size, uint32_t value)
{
    const unsigned offset = address & 15;
    const unsigned control = system->cache_control;
    if (!(control & SH7604_CACHE_ENABLE) || !fits(offset, size))
        return;
    Sh7604CacheSet *set = &cache->set[(address >> 4) & 63];
    unsigned way = match(set, address, (control & CACHE_TWO_WAY) != 0);
    if (way == NO_WAY)
        return;
    for (unsigned i = size; i != 0; --i) {
        set->line[way][offset + i - 1] = (uint8_t)value;
        value >>= 8;
    }
    accessed(set, way);
}
