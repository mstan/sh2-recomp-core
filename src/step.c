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
    context->interrupt_stage = 0;
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
    const uint32_t base = sh2_literal_pc(&context->state) & ~UINT32_C(3);
    return sh2_op_load(context, rn, base + displacement, 4);
}

Sh2StepResult sh2_op_mov_w_pc(Sh2Context *context, unsigned rn, uint32_t displacement)
{
    return sh2_op_load(context, rn, sh2_literal_pc(&context->state) + displacement, 2);
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

Sh2StepResult sh2_op_lds_l_pr(Sh2Context *context, unsigned rm)
{
    uint32_t value;
    Sh2StepResult result = read_data(context, context->state.r[rm], 4, &value);
    if (result != SH2_STEP_OK)
        return result;
    context->state.r[rm] += 4;
    return sh2_op_control(context, &context->state.pr, value);
}

static Sh2StepResult write_data(Sh2Context *context, uint32_t address, unsigned size,
                                uint32_t value)
{
    if (address & (size - 1))
        return SH2_STEP_ADDRESS_ERROR;
    if (!context->bus.write)
        return SH2_STEP_BUS_FAULT;
    if (size < 4)
        value &= (UINT32_C(1) << (8 * size)) - 1;
    return bus_result(context->bus.write(context->user, address, size, value));
}

Sh2StepResult sh2_op_store(Sh2Context *context, uint32_t address, uint32_t value,
                           unsigned size)
{
    Sh2StepResult result = write_data(context, address, size, value);
    return result == SH2_STEP_OK ? sh2_retire(context, 1) : result;
}

static Sh2StepResult store_predec(Sh2Context *context, unsigned rn, uint32_t value, unsigned size)
{
    const uint32_t address = context->state.r[rn] - size;
    Sh2StepResult result = sh2_op_store(context, address, value, size);
    if (result == SH2_STEP_OK)
        context->state.r[rn] = address;
    return result;
}

Sh2StepResult sh2_op_store_predec(Sh2Context *context, unsigned rn, unsigned rm, unsigned size)
{
    return store_predec(context, rn, context->state.r[rm], size);
}

Sh2StepResult sh2_op_sts_l_pr(Sh2Context *context, unsigned rn)
{
    Sh2StepResult result = store_predec(context, rn, context->state.pr, 4);
    if (result == SH2_STEP_OK)
        context->state.irq_inhibit = true;
    return result;
}

bool sh2_interrupt_ready(const Sh2Context *context, unsigned level)
{
    const Sh2State *state = &context->state;
    return context->interrupt_stage ||
           (level > ((state->sr >> 4) & 15) && level <= 15 &&
            !state->delay_pending && !state->irq_inhibit);
}

Sh2StepResult sh2_interrupt(Sh2Context *context, unsigned level, uint8_t vector)
{
    Sh2State *state = &context->state;
    if (!context->interrupt_stage) {
        if (!sh2_interrupt_ready(context, level))
            return SH2_STEP_UNSUPPORTED;
        context->interrupt_stage = 1;
        context->interrupt_level = (uint8_t)level;
        context->interrupt_vector = vector;
    }

    while (context->interrupt_stage <= 2) {
        uint32_t stack = state->r[15] - 4;
        uint32_t value = context->interrupt_stage == 1 ? state->sr : state->pc;
        Sh2StepResult result = write_data(context, stack, 4, value);
        if (result != SH2_STEP_OK)
            return result;
        state->r[15] = stack;
        ++context->interrupt_stage;
    }

    uint32_t target;
    Sh2StepResult result = read_data(context, state->vbr + 4u * context->interrupt_vector,
                                    4, &target);
    if (result != SH2_STEP_OK)
        return result;
    state->sr = (state->sr & ~UINT32_C(0xf0)) | ((uint32_t)context->interrupt_level << 4);
    state->pc = target;
    /* Eight nominal states from acceptance through vector fetch, before bus waits. */
    state->cycles += 8;
    context->interrupt_stage = 0;
    return SH2_STEP_OK;
}

Sh2StepResult sh2_op_rte(Sh2Context *context)
{
    Sh2State *state = &context->state;
    if (state->delay_pending)
        return SH2_STEP_SLOT_ILLEGAL;

    uint32_t target, sr;
    Sh2StepResult result = read_data(context, state->r[15], 4, &target);
    if (result != SH2_STEP_OK)
        return result;
    result = read_data(context, state->r[15] + 4, 4, &sr);
    if (result != SH2_STEP_OK)
        return result;
    state->r[15] += 8;
    state->sr = sr & SH2_SR_BITS;
    sh2_op_branch(context, target, true, true, false);
    state->cycles += 2;
    return SH2_STEP_OK;
}
