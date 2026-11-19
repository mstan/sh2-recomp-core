#include "sh2_recomp/decode.h"

#include <stdio.h>

typedef struct {
    uint16_t word;
    Sh2Instruction expected;
} DecodeCase;

int main(void)
{
    static const DecodeCase cases[] = {
        {0x0009, {SH2_OP_NOP,        0,  0,    0,     0, false}},
        {0x000B, {SH2_OP_RTS,        0,  0,    0,     0, true}},
        {0xEF80, {SH2_OP_MOV_IMM,   15,  0, -128,     0, false}},
        {0xE27F, {SH2_OP_MOV_IMM,    2,  0,  127,     0, false}},
        {0x73FF, {SH2_OP_ADD_IMM,    3,  0,   -1,     0, false}},
        {0x6A53, {SH2_OP_MOV_REG,   10,  5,    0,     0, false}},
        {0x31EC, {SH2_OP_ADD_REG,    1, 14,    0,     0, false}},
        {0x3D40, {SH2_OP_CMP_EQ_REG,13,  4,    0,     0, false}},
        {0xA7FF, {SH2_OP_BRA,        0,  0,    0,  4094, true}},
        {0xA800, {SH2_OP_BRA,        0,  0,    0, -4096, true}},
        {0xBFFF, {SH2_OP_BSR,        0,  0,    0,    -2, true}},
        {0x897F, {SH2_OP_BT,         0,  0,    0,   254, false}},
        {0x8B80, {SH2_OP_BF,         0,  0,    0,  -256, false}},
        {0x8D00, {SH2_OP_BT_S,       0,  0,    0,     0, true}},
        {0x8FFF, {SH2_OP_BF_S,       0,  0,    0,    -2, true}},
        {0x4E2B, {SH2_OP_JMP,        0, 14,    0,     0, true}},
        {0x470B, {SH2_OP_JSR,        0,  7,    0,     0, true}},
        {0x002B, {SH2_OP_UNSUPPORTED,0,  0,    0,     0, false}},
        {0x0109, {SH2_OP_UNSUPPORTED,0,  0,    0,     0, false}},
        {0x3D41, {SH2_OP_UNSUPPORTED,0,  0,    0,     0, false}},
        {0x4E2A, {SH2_OP_UNSUPPORTED,0,  0,    0,     0, false}},
        {0xFFFF, {SH2_OP_UNSUPPORTED,0,  0,    0,     0, false}}
    };

    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        Sh2Instruction actual = sh2_decode(cases[i].word);
        Sh2Instruction expected = cases[i].expected;
        if (actual.opcode != expected.opcode || actual.rn != expected.rn ||
            actual.rm != expected.rm || actual.immediate != expected.immediate ||
            actual.displacement != expected.displacement ||
            actual.delayed != expected.delayed) {
            fprintf(stderr, "Incorrect decode of %04X\n", (unsigned)cases[i].word);
            return 1;
        }
    }
    return 0;
}
