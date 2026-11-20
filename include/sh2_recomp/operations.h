#ifndef SH2_RECOMP_OPERATIONS_H
#define SH2_RECOMP_OPERATIONS_H

#include "sh2_recomp/step.h"

/* One-instruction primitives shared by generated C and the interpreter.
 * The decoder supplies in-range register indices and aligned widths. */
enum { SH2_SR_T = 1, SH2_SR_BITS = 0x3F3 };

static inline Sh2StepResult sh2_retire(Sh2Context *context, unsigned cycles)
{
    Sh2State *state = &context->state;
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

static inline Sh2StepResult sh2_op_control(Sh2Context *context, uint32_t *control,
                                          uint32_t value)
{
    *control = value;
    return sh2_retire(context, 1);
}

static inline Sh2StepResult sh2_op_branch(Sh2Context *context, uint32_t target,
                                         bool taken, bool delayed, bool link)
{
    Sh2State *state = &context->state;
    if (state->delay_pending)
        return SH2_STEP_SLOT_ILLEGAL;
    if (!taken)
        return sh2_retire(context, 1);
    if (link)
        state->pr = state->pc + 4;
    if (delayed) {
        state->delay_target = target;
        state->delay_pending = true;
        state->pc += 2;
        state->cycles += 2;
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
Sh2StepResult sh2_op_mov_l_postinc(Sh2Context *context, unsigned rn, unsigned rm);
Sh2StepResult sh2_op_load(Sh2Context *context, unsigned rn, uint32_t address, unsigned size);
Sh2StepResult sh2_op_store(Sh2Context *context, uint32_t address, uint32_t value,
                           unsigned size);
Sh2StepResult sh2_op_store_predec(Sh2Context *context, unsigned rn, unsigned rm,
                                  unsigned size);

#ifdef __cplusplus
}
#endif
#endif
