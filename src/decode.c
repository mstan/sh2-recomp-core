#include "sh2_recomp/decode.h"

typedef enum {
    NO_OPERANDS,
    IMMEDIATE_RN,
    RM_RN,
    TARGET_RM,
    DISP8,
    DISP12,
    LITERAL_LONG,
    RM_RN_DISP_LONG,
    DISP_GBR_BYTE,
    DISP_GBR_WORD,
    DISP_GBR_LONG,
    IMM8_UNSIGNED,
    IMM8_SIGNED
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
    {0xF0FF, 0x400B, SH2_OP_JSR,        TARGET_RM, true},
    {0xF000, 0xD000, SH2_OP_MOV_L_PC,   LITERAL_LONG, false},
    {0xF00F, 0x6006, SH2_OP_MOV_L_POSTINC, RM_RN, false},
    {0xF000, 0x1000, SH2_OP_MOV_L_STORE_DISP, RM_RN_DISP_LONG, false},
    {0xF00F, 0x2000, SH2_OP_MOV_B_STORE, RM_RN, false},
    {0xF00F, 0x2001, SH2_OP_MOV_W_STORE, RM_RN, false},
    {0xF00F, 0x2004, SH2_OP_MOV_B_PREDEC, RM_RN, false},
    {0xF00F, 0x2005, SH2_OP_MOV_W_PREDEC, RM_RN, false},
    {0xF00F, 0x2006, SH2_OP_MOV_L_PREDEC, RM_RN, false},
    {0xF00F, 0x6002, SH2_OP_MOV_L_LOAD, RM_RN, false},
    {0xF000, 0x5000, SH2_OP_MOV_L_LOAD_DISP, RM_RN_DISP_LONG, false},
    {0xFF00, 0xC400, SH2_OP_MOV_B_LOAD_GBR, DISP_GBR_BYTE, false},
    {0xFF00, 0xC600, SH2_OP_MOV_L_LOAD_GBR, DISP_GBR_LONG, false},
    {0xFF00, 0xC100, SH2_OP_MOV_W_STORE_GBR, DISP_GBR_WORD, false},
    {0xFF00, 0xC200, SH2_OP_MOV_L_STORE_GBR, DISP_GBR_LONG, false},
    {0xF0FF, 0x400E, SH2_OP_LDC_SR,     TARGET_RM, false},
    {0xF0FF, 0x401E, SH2_OP_LDC_GBR,    TARGET_RM, false},
    {0xF0FF, 0x402E, SH2_OP_LDC_VBR,    TARGET_RM, false},
    {0xF00F, 0x3008, SH2_OP_SUB_REG,    RM_RN, false},
    {0xFF00, 0xC800, SH2_OP_TST_IMM,    IMM8_UNSIGNED, false},
    {0xFF00, 0x8800, SH2_OP_CMP_EQ_IMM, IMM8_SIGNED, false}
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
        case LITERAL_LONG:
            decoded.rn = (uint8_t)((word >> 8) & 15u);
            decoded.displacement = (int16_t)(4 * (word & 0xFFu));
            break;
        case RM_RN_DISP_LONG:
            decoded.rn = (uint8_t)((word >> 8) & 15u);
            decoded.rm = (uint8_t)((word >> 4) & 15u);
            decoded.displacement = (int16_t)(4 * (word & 15u));
            break;
        case DISP_GBR_BYTE:
            decoded.displacement = (int16_t)(word & 0xFFu);
            break;
        case DISP_GBR_WORD:
            decoded.displacement = (int16_t)(2 * (word & 0xFFu));
            break;
        case DISP_GBR_LONG:
            decoded.displacement = (int16_t)(4 * (word & 0xFFu));
            break;
        case IMM8_UNSIGNED:
            decoded.immediate = (int16_t)(word & 0xFFu);
            break;
        case IMM8_SIGNED:
            decoded.immediate = sign_extend(word & 0xFFu, 0x100);
            break;
        }
        break;
    }
    return decoded;
}
