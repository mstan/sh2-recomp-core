#ifndef SH2_RECOMP_OPERATIONS_H
#define SH2_RECOMP_OPERATIONS_H

#include "sh2_recomp/step.h"

/* One-instruction primitives shared by generated C and the interpreter.
 * The decoder supplies in-range register indices and aligned widths. */
enum { SH2_SR_T = 1, SH2_SR_Q = 0x100, SH2_SR_M = 0x200, SH2_SR_BITS = 0x3F3 };

static inline Sh2StepResult sh2_retire(Sh2Context *context, unsigned cycles)
{
    Sh2State *state = &context->state;
    state->irq_inhibit = false;
    state->cycles += cycles;
    if (state->delay_pending) {
        state->pc = state->delay_target;
        state->delay_target = 0;
        state->delay_pending = false;
    } else {
        state->pc += 2;
    }
    return SH2_STEP_OK;
}

static inline Sh2StepResult sh2_op_register(Sh2Context *context, unsigned rn,
                                            uint32_t value)
{
    context->state.r[rn] = value;
    return sh2_retire(context, 1);
}

static inline Sh2StepResult sh2_op_t(Sh2Context *context, bool value)
{
    context->state.sr = (context->state.sr & ~(uint32_t)SH2_SR_T) | (uint32_t)value;
    return sh2_retire(context, 1);
}

static inline Sh2StepResult sh2_op_addc(Sh2Context *context, unsigned rn, unsigned rm)
{
    Sh2State *state = &context->state;
    const uint64_t sum = (uint64_t)state->r[rn] + state->r[rm] + (state->sr & SH2_SR_T);
    state->r[rn] = (uint32_t)sum;
    return sh2_op_t(context, (sum >> 32) != 0);
}

static inline Sh2StepResult sh2_op_subc(Sh2Context *context, unsigned rn, unsigned rm)
{
    Sh2State *state = &context->state;
    const uint64_t minuend = state->r[rn];
    const uint64_t subtrahend = (uint64_t)state->r[rm] + (state->sr & SH2_SR_T);
    state->r[rn] = (uint32_t)(minuend - subtrahend);
    return sh2_op_t(context, minuend < subtrahend);
}

static inline Sh2StepResult sh2_op_control(Sh2Context *context, uint32_t *destination,
                                          uint32_t value)
{
    *destination = value;
    sh2_retire(context, 1);
    context->state.irq_inhibit = true;
    return SH2_STEP_OK;
}

static inline Sh2StepResult sh2_op_shlr(Sh2Context *context, unsigned rn)
{
    uint32_t value = context->state.r[rn];
    context->state.r[rn] = value >> 1;
    return sh2_op_t(context, (value & 1) != 0);
}

static inline Sh2StepResult sh2_op_shll(Sh2Context *context, unsigned rn)
{
    uint32_t value = context->state.r[rn];
    context->state.r[rn] = value << 1;
    return sh2_op_t(context, (value >> 31) != 0);
}

static inline Sh2StepResult sh2_op_rotl(Sh2Context *context, unsigned rn)
{
    Sh2State *state = &context->state;
    const uint32_t before = state->r[rn];
    const uint32_t high = before >> 31;
    state->r[rn] = (before << 1) | high;
    return sh2_op_t(context, high != 0);
}

static inline Sh2StepResult sh2_op_rotr(Sh2Context *context, unsigned rn)
{
    uint32_t value = context->state.r[rn];
    context->state.r[rn] = (value >> 1) | (value << 31);
    return sh2_op_t(context, (value & 1) != 0);
}

static inline Sh2StepResult sh2_op_rotcl(Sh2Context *context, unsigned rn)
{
    Sh2State *state = &context->state;
    const uint32_t before = state->r[rn];
    state->r[rn] = (before << 1) | (state->sr & SH2_SR_T);
    return sh2_op_t(context, (before >> 31) != 0);
}

static inline Sh2StepResult sh2_op_shar(Sh2Context *context, unsigned rn)
{
    uint32_t value = context->state.r[rn];
    context->state.r[rn] = (value >> 1) | (value & UINT32_C(0x80000000));
    return sh2_op_t(context, (value & 1) != 0);
}

static inline Sh2StepResult sh2_op_muls_w(Sh2Context *context, unsigned rn, unsigned rm)
{
    Sh2State *state = &context->state;
    const int32_t left = (int32_t)((state->r[rn] & 0xffffu) ^ 0x8000u) - 0x8000;
    const int32_t right = (int32_t)((state->r[rm] & 0xffffu) ^ 0x8000u) - 0x8000;
    state->macl = (uint32_t)(left * right);
    return sh2_retire(context, 1);
}

static inline Sh2StepResult sh2_op_mulu_w(Sh2Context *context, unsigned rn, unsigned rm)
{
    Sh2State *state = &context->state;
    state->macl = (state->r[rn] & 0xffffu) * (state->r[rm] & 0xffffu);
    return sh2_retire(context, 1);
}

static inline uint32_t sh2_signed_order(uint32_t value)
{
    return value ^ UINT32_C(0x80000000);
}

static inline uint32_t sh2_extend_signed(uint32_t value, unsigned size)
{
    if (size < 4) {
        const uint32_t sign = UINT32_C(1) << (8 * size - 1);
        const uint32_t mask = (sign << 1) - 1;
        return ((value & mask) ^ sign) - sign;
    }
    return value;
}

static inline Sh2StepResult sh2_op_mul_l(Sh2Context *context, unsigned rn, unsigned rm)
{
    Sh2State *state = &context->state;
    state->macl = state->r[rn] * state->r[rm];
    return sh2_retire(context, 2);
}

static inline Sh2StepResult sh2_op_div0(Sh2Context *context, bool q, bool m)
{
    Sh2State *state = &context->state;
    state->sr = (state->sr & ~(uint32_t)(SH2_SR_Q | SH2_SR_M)) |
                (q ? SH2_SR_Q : 0u) | (m ? SH2_SR_M : 0u);
    return sh2_op_t(context, q != m);
}

static inline Sh2StepResult sh2_op_div1(Sh2Context *context, unsigned rn, unsigned rm)
{
    Sh2State *state = &context->state;
    const bool m = (state->sr & SH2_SR_M) != 0;
    const bool add_divisor = ((state->sr & SH2_SR_Q) != 0) != m;
    const bool high_bit = (state->r[rn] >> 31) != 0;
    const uint32_t shifted = (state->r[rn] << 1) | (state->sr & SH2_SR_T);
    state->r[rn] = shifted;

    /* The SH-2 shifts Rn before the add/subtract, including when Rm is Rn. */
    const uint32_t divisor = state->r[rm];
    const uint32_t partial = add_divisor ? shifted + divisor : shifted - divisor;
    const bool carry_or_borrow = add_divisor ? partial < shifted : partial > shifted;
    const bool q = (high_bit != carry_or_borrow) != m;
    state->r[rn] = partial;
    state->sr = (state->sr & ~(uint32_t)SH2_SR_Q) | (q ? SH2_SR_Q : 0u);
    return sh2_op_t(context, q == m);
}

static inline Sh2StepResult sh2_op_dt(Sh2Context *context, unsigned rn)
{
    return sh2_op_t(context, --context->state.r[rn] == 0);
}

static inline uint32_t sh2_literal_pc(const Sh2State *state)
{
    return state->delay_pending ? state->delay_target + 2 : state->pc + 4;
}

static inline Sh2StepResult sh2_op_branch(Sh2Context *context, uint32_t target,
                                         bool taken, bool delayed, bool link)
{
    Sh2State *state = &context->state;
    if (state->delay_pending)
        return SH2_STEP_SLOT_ILLEGAL;
    if (!taken && !delayed)
        return sh2_retire(context, 1);
    state->irq_inhibit = false;
    if (link)
        state->pr = state->pc + 4;
    if (delayed) {
        state->delay_target = taken ? target : state->pc + 4;
        state->delay_pending = true;
        state->pc += 2;
        state->cycles += taken ? 2 : 1;
    } else {
        state->pc = target;
        state->cycles += 3;
    }
    return SH2_STEP_OK;
}

#ifdef __cplusplus
extern "C" {
#endif

Sh2StepResult sh2_op_mov_l_pc(Sh2Context *context, unsigned rn, uint32_t displacement);
Sh2StepResult sh2_op_mov_w_pc(Sh2Context *context, unsigned rn, uint32_t displacement);
Sh2StepResult sh2_op_mov_l_postinc(Sh2Context *context, unsigned rn, unsigned rm);
Sh2StepResult sh2_op_sts_l_pr(Sh2Context *context, unsigned rn);
Sh2StepResult sh2_op_lds_l_pr(Sh2Context *context, unsigned rm);
Sh2StepResult sh2_op_sts_l(Sh2Context *context, unsigned rn, uint32_t value);
Sh2StepResult sh2_op_lds_l(Sh2Context *context, unsigned rm, uint32_t *destination);
Sh2StepResult sh2_op_rte(Sh2Context *context);
Sh2StepResult sh2_op_load_postinc(Sh2Context *context, unsigned rn, unsigned rm, unsigned size);
Sh2StepResult sh2_op_load(Sh2Context *context, unsigned rn, uint32_t address, unsigned size);
Sh2StepResult sh2_op_store(Sh2Context *context, uint32_t address, uint32_t value,
                           unsigned size);
Sh2StepResult sh2_op_store_predec(Sh2Context *context, unsigned rn, unsigned rm,
                                  unsigned size);

#ifdef __cplusplus
}
#endif
#endif
