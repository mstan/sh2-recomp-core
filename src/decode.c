#include "sh2_recomp/decode.h"

typedef enum {
    NO_OPERANDS,
    IMMEDIATE_RN,
    RM_RN,
    TARGET_RM,
    DISP8,
    DISP12
} OperandFormat;

typedef struct {
    uint16_t mask;
    uint16_t value;
    Sh2Opcode opcode;
    OperandFormat format;
    bool delayed;
} Encoding;

static const Encoding encodings[] = {
    {0xFFFF, 0x0009, SH2_OP_NOP,        NO_OPERANDS, false},
    {0xFFFF, 0x000B, SH2_OP_RTS,        NO_OPERANDS, true},
    {0xF000, 0xE000, SH2_OP_MOV_IMM,    IMMEDIATE_RN, false},
    {0xF000, 0x7000, SH2_OP_ADD_IMM,    IMMEDIATE_RN, false},
    {0xF00F, 0x6003, SH2_OP_MOV_REG,    RM_RN, false},
    {0xF00F, 0x300C, SH2_OP_ADD_REG,    RM_RN, false},
    {0xF00F, 0x3000, SH2_OP_CMP_EQ_REG, RM_RN, false},
    {0xF000, 0xA000, SH2_OP_BRA,        DISP12, true},
    {0xF000, 0xB000, SH2_OP_BSR,        DISP12, true},
    {0xFF00, 0x8900, SH2_OP_BT,         DISP8, false},
    {0xFF00, 0x8B00, SH2_OP_BF,         DISP8, false},
    {0xFF00, 0x8D00, SH2_OP_BT_S,       DISP8, true},
    {0xFF00, 0x8F00, SH2_OP_BF_S,       DISP8, true},
    {0xF0FF, 0x402B, SH2_OP_JMP,        TARGET_RM, true},
    {0xF0FF, 0x400B, SH2_OP_JSR,        TARGET_RM, true}
};

static int16_t sign_extend(uint16_t field, int32_t modulus)
{
    return (int16_t)(field < modulus / 2 ? field : (int32_t)field - modulus);
}

Sh2Instruction sh2_decode(uint16_t word)
{
    Sh2Instruction decoded = {0};
    for (unsigned i = 0; i < sizeof(encodings) / sizeof(encodings[0]); ++i) {
        const Encoding *encoding = &encodings[i];
        if ((word & encoding->mask) != encoding->value)
            continue;

        decoded.opcode = encoding->opcode;
        decoded.delayed = encoding->delayed;
        switch (encoding->format) {
        case NO_OPERANDS:
            break;
        case IMMEDIATE_RN:
            decoded.rn = (uint8_t)((word >> 8) & 15u);
            decoded.immediate = sign_extend(word & 0xFFu, 0x100);
            break;
        case RM_RN:
            decoded.rn = (uint8_t)((word >> 8) & 15u);
            decoded.rm = (uint8_t)((word >> 4) & 15u);
            break;
        case TARGET_RM:
            decoded.rm = (uint8_t)((word >> 8) & 15u);
            break;
        case DISP8:
            decoded.displacement = (int16_t)(2 * sign_extend(word & 0xFFu, 0x100));
            break;
        case DISP12:
            decoded.displacement = (int16_t)(2 * sign_extend(word & 0xFFFu, 0x1000));
            break;
        }
        break;
    }
    return decoded;
}
