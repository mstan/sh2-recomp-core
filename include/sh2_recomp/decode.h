#ifndef SH2_RECOMP_DECODE_H
#define SH2_RECOMP_DECODE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SH2_OP_UNSUPPORTED,
    SH2_OP_NOP,
    SH2_OP_MOV_IMM,
    SH2_OP_MOV_REG,
    SH2_OP_ADD_IMM,
    SH2_OP_ADD_REG,
    SH2_OP_CMP_EQ_REG,
    SH2_OP_BRA,
    SH2_OP_BSR,
    SH2_OP_BT,
    SH2_OP_BF,
    SH2_OP_BT_S,
    SH2_OP_BF_S,
    SH2_OP_JMP,
    SH2_OP_JSR,
    SH2_OP_RTS
} Sh2Opcode;

typedef struct {
    Sh2Opcode opcode;
    uint8_t rn;
    uint8_t rm;
    int16_t immediate;
    int16_t displacement;
    bool delayed;
} Sh2Instruction;

/* Decode a host-order instruction word. Relative branches use a signed byte
 * displacement from the instruction address plus four. Unused fields are
 * zero. UNSUPPORTED covers both unimplemented and unrecognized encodings. */
Sh2Instruction sh2_decode(uint16_t word);

#endif
