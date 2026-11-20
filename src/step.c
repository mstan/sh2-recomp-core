#include "sh2_recomp/operations.h"

#include <string.h>

static Sh2StepResult bus_result(Sh2BusResult result)
{
    switch (result) {
    case SH2_BUS_OK: return SH2_STEP_OK;
    case SH2_BUS_WAIT: return SH2_STEP_WAIT;
    default: return SH2_STEP_BUS_FAULT;
    }
}

static Sh2StepResult read_data(Sh2Context *context, uint32_t address,
                               unsigned size, uint32_t *value)
{
    if (address & (size - 1))
        return SH2_STEP_ADDRESS_ERROR;
    if (!context->bus.read)
        return SH2_STEP_BUS_FAULT;
    return bus_result(context->bus.read(context->user, address, size, value));
}

void sh2_init(Sh2Context *context, const Sh2Bus *bus, void *user)
{
    memset(context, 0, sizeof(*context));
    context->bus = *bus;
    context->user = user;
}

Sh2StepResult sh2_power_on(Sh2Context *context)
{
    Sh2State next = {0};
    Sh2StepResult result = read_data(context, 0, 4, &next.pc);
    if (result != SH2_STEP_OK)
        return result;
    result = read_data(context, 4, 4, &next.r[15]);
    if (result != SH2_STEP_OK)
        return result;
    next.sr = 0xF0;
    context->state = next;
    return SH2_STEP_OK;
}

Sh2StepResult sh2_fetch16(Sh2Context *context, uint16_t *word)
{
    const uint32_t pc = context->state.pc;
    if (pc & 1)
        return SH2_STEP_ADDRESS_ERROR;
    if (!context->bus.fetch16)
        return SH2_STEP_BUS_FAULT;
    return bus_result(context->bus.fetch16(context->user, pc, word));
}

Sh2NativeOp sh2_native_lookup(const Sh2Catalog *catalog, uint32_t pc, uint16_t word)
{
    if (!catalog)
        return NULL;
    size_t first = 0;
    size_t last = catalog->count;
    while (first < last) {
        size_t middle = first + (last - first) / 2;
        const Sh2NativeEntry *entry = &catalog->entries[middle];
        if (entry->pc < pc)
            first = middle + 1;
        else if (entry->pc > pc)
            last = middle;
        else
            return entry->word == word ? entry->execute : NULL;
    }
    return NULL;
}

Sh2StepResult sh2_step_native(Sh2Context *context, const Sh2Catalog *catalog)
{
    uint16_t word;
    Sh2StepResult result = sh2_fetch16(context, &word);
    if (result != SH2_STEP_OK)
        return result;
    Sh2NativeOp execute = sh2_native_lookup(catalog, context->state.pc, word);
    return execute ? execute(context) : SH2_STEP_NATIVE_MISS;
}

static uint32_t extend_load(uint32_t value, unsigned size)
{
    if (size < 4) {
        const uint32_t sign = UINT32_C(1) << (8 * size - 1);
        value &= (sign << 1) - 1;
        if (value & sign)
            value |= ~((sign << 1) - 1);
    }
    return value;
}

Sh2StepResult sh2_op_load(Sh2Context *context, unsigned rn, uint32_t address,
                          unsigned size)
{
    uint32_t value;
    Sh2StepResult result = read_data(context, address, size, &value);
    if (result != SH2_STEP_OK)
        return result;
    return sh2_op_register(context, rn, extend_load(value, size));
}

Sh2StepResult sh2_op_mov_l_pc(Sh2Context *context, unsigned rn, uint32_t displacement)
{
    /* PC-relative literal loads in delay slots need the target-PC rules. */
    if (context->state.delay_pending)
        return SH2_STEP_UNSUPPORTED;
    const uint32_t base = (context->state.pc + 4) & ~UINT32_C(3);
    return sh2_op_load(context, rn, base + displacement, 4);
}

Sh2StepResult sh2_op_load_postinc(Sh2Context *context, unsigned rn, unsigned rm, unsigned size)
{
    uint32_t value;
    Sh2StepResult result = read_data(context, context->state.r[rm], size, &value);
    if (result != SH2_STEP_OK)
        return result;
    if (rn != rm)
        context->state.r[rm] += size;
    return sh2_op_register(context, rn, extend_load(value, size));
}

Sh2StepResult sh2_op_mov_l_postinc(Sh2Context *context, unsigned rn, unsigned rm)
{
    return sh2_op_load_postinc(context, rn, rm, 4);
}

Sh2StepResult sh2_op_store(Sh2Context *context, uint32_t address, uint32_t value,
                           unsigned size)
{
    if (address & (size - 1))
        return SH2_STEP_ADDRESS_ERROR;
    if (!context->bus.write)
        return SH2_STEP_BUS_FAULT;
    if (size < 4)
        value &= (UINT32_C(1) << (8 * size)) - 1;
    Sh2StepResult result = bus_result(context->bus.write(context->user, address, size, value));
    return result == SH2_STEP_OK ? sh2_retire(context, 1) : result;
}

Sh2StepResult sh2_op_store_predec(Sh2Context *context, unsigned rn, unsigned rm,
                                  unsigned size)
{
    const uint32_t address = context->state.r[rn] - size;
    Sh2StepResult result = sh2_op_store(context, address, context->state.r[rm], size);
    if (result == SH2_STEP_OK)
        context->state.r[rn] = address;
    return result;
}
