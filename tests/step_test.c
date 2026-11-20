#include "boot_image.h"
#include "sh2_recomp/interpreter.h"
#include "sh2_recomp/operations.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>

extern const Sh2Catalog sh2_catalog_fixture;

typedef struct {
    uint8_t bytes[sizeof(boot_image)];
    unsigned fetches;
    bool wait_write;
    uint32_t wait_address;
    uint32_t wait_read_address;
    unsigned tracked_reads;
    uint32_t tracked_address;
} Memory;

static Sh2BusResult read_data(void *user, uint32_t address, unsigned size, uint32_t *value)
{
    Memory *memory = user;
    if (address > sizeof(memory->bytes) || size > sizeof(memory->bytes) - address)
        return SH2_BUS_FAULT;
    if (memory->wait_read_address && address == memory->wait_read_address) {
        memory->wait_read_address = 0;
        return SH2_BUS_WAIT;
    }
    if (memory->tracked_address && address == memory->tracked_address)
        ++memory->tracked_reads;
    *value = 0;
    for (unsigned i = 0; i < size; ++i)
        *value = (*value << 8) | memory->bytes[address + i];
    return SH2_BUS_OK;
}

static Sh2BusResult fetch_word(void *user, uint32_t address, uint16_t *word)
{
    Memory *memory = user;
    ++memory->fetches;
    uint32_t value;
    Sh2BusResult result = read_data(user, address, 2, &value);
    if (result == SH2_BUS_OK)
        *word = (uint16_t)value;
    return result;
}

static Sh2BusResult write_data(void *user, uint32_t address, unsigned size, uint32_t value)
{
    Memory *memory = user;
    if (address > sizeof(memory->bytes) || size > sizeof(memory->bytes) - address)
        return SH2_BUS_FAULT;
    if (memory->wait_write || (memory->wait_address && address == memory->wait_address)) {
        memory->wait_write = false;
        memory->wait_address = 0;
        return SH2_BUS_WAIT;
    }
    for (unsigned i = 0; i < size; ++i) {
        memory->bytes[address + size - 1 - i] = (uint8_t)value;
        value >>= 8;
    }
    return SH2_BUS_OK;
}

static void start(Sh2Context *context, Memory *memory)
{
    const Sh2Bus bus = {read_data, write_data, fetch_word};
    memset(memory, 0, sizeof(*memory));
    memcpy(memory->bytes, boot_image, sizeof(boot_image));
    sh2_init(context, &bus, memory);
    assert(sh2_power_on(context) == SH2_STEP_OK);
    assert(context->state.pc == 0x10 && context->state.r[15] == 0x3C);
    assert(context->state.sr == 0xF0);
}

static Sh2StepResult step_fixture(Sh2Context *context, bool fallback)
{
    if (!fallback)
        return sh2_step_native(context, &sh2_catalog_fixture);
    bool interpreted = false;
    const Sh2StepResult result = sh2_step_with_fallback(context, NULL, &interpreted);
    assert(interpreted);
    return result;
}

static void test_word_mac(bool fallback)
{
    Sh2Context context;
    Memory memory;
    start(&context, &memory);
    Sh2State *state = &context.state;
    state->pc = 0x130;
    state->sr = SH2_SR_T;
    state->mach = state->macl = UINT32_MAX;
    assert(step_fixture(&context, fallback) == SH2_STEP_OK);
    assert(state->mach == 0 && state->macl == 0 && state->sr == SH2_SR_T);

    assert(write_data(&memory, 0x138, 2, 0xfffd) == SH2_BUS_OK);
    assert(write_data(&memory, 0x13a, 2, 2) == SH2_BUS_OK);
    assert(write_data(&memory, 0x13c, 2, 0xfffc) == SH2_BUS_OK);
    assert(write_data(&memory, 0x13e, 2, 5) == SH2_BUS_OK);
    state->r[1] = 0x13a;
    state->r[2] = 0x138;
    memory.wait_read_address = 0x13a;
    memory.tracked_address = 0x138;
    assert(step_fixture(&context, fallback) == SH2_STEP_WAIT);
    assert(state->pc == 0x132 && state->cycles == 1);
    assert(state->r[1] == 0x13a && state->r[2] == 0x138 && state->macl == 0);
    assert(!sh2_interrupt_ready(&context, 6) && memory.tracked_reads == 1);
    assert(write_data(&memory, 0x138, 2, 1) == SH2_BUS_OK);
    assert(step_fixture(&context, fallback) == SH2_STEP_OK);
    assert(memory.fetches == 2 && memory.tracked_reads == 1 && sh2_interrupt_ready(&context, 6));
    assert(state->r[1] == 0x13c && state->r[2] == 0x13a && state->cycles == 4);
    assert(state->mach == UINT32_MAX && state->macl == UINT32_C(0xfffffffa));

    assert(step_fixture(&context, fallback) == SH2_STEP_OK);
    assert(state->r[1] == 0x140 && state->mach == UINT32_MAX);
    assert(state->macl == UINT32_C(0xffffffe6));

    state->pc = 0x132;
    state->sr |= SH2_SR_S;
    state->mach = UINT32_C(0x02713568);
    state->macl = UINT32_C(0x7ffffffe);
    state->r[1] = 0x13a;
    state->r[2] = 0x13e;
    assert(step_fixture(&context, fallback) == SH2_STEP_OK);
    assert(state->macl == INT32_MAX && state->mach == UINT32_C(0x02713568));

    state->pc = 0x132;
    state->macl = UINT32_C(0x80000001);
    state->r[1] = 0x13a;
    state->r[2] = 0x13c;
    assert(step_fixture(&context, fallback) == SH2_STEP_OK);
    assert(state->macl == UINT32_C(0x80000000) && state->mach == UINT32_C(0x02713568));

    state->pc = 0x132;
    state->sr &= ~(uint32_t)SH2_SR_S;
    state->mach = UINT32_MAX;
    state->macl = UINT32_C(0xfffffffe);
    state->r[1] = 0x13a;
    state->r[2] = 0x138;
    assert(step_fixture(&context, fallback) == SH2_STEP_OK);
    assert(state->mach == 0 && state->macl == 0);
    assert(state->cycles == 16 && state->sr == SH2_SR_T);
}

int main(void)
{
    Sh2Context native, interpreted;
    Memory native_bus, interpreted_bus;
    start(&native, &native_bus);
    start(&interpreted, &interpreted_bus);
    native_bus.wait_write = true;
    interpreted_bus.wait_write = true;
    unsigned waits = 0;
    for (unsigned step = 0; step < 12; ++step) {
        const uint64_t old_cycles = native.state.cycles;
        Sh2StepResult actual = sh2_step_native(&native, &sh2_catalog_fixture);
        bool used = false;
        assert(actual == sh2_step_with_fallback(&interpreted, NULL, &used));
        assert(used);
        if (actual == SH2_STEP_WAIT) {
            assert(native.state.pc == 0x1A && native.state.cycles == old_cycles);
            assert(native_bus.bytes[0x38] == 0);
            ++waits;
        } else {
            assert(actual == SH2_STEP_OK);
        }
        assert(native.state.pc == interpreted.state.pc);
        assert(native.state.pr == interpreted.state.pr);
        assert(native.state.sr == interpreted.state.sr);
        assert(native.state.gbr == interpreted.state.gbr);
        assert(native.state.cycles == interpreted.state.cycles);
        assert(native.state.delay_pending == interpreted.state.delay_pending);
        assert(native.state.delay_target == interpreted.state.delay_target);
        assert(memcmp(native.state.r, interpreted.state.r, sizeof(native.state.r)) == 0);
        assert(memcmp(native_bus.bytes, interpreted_bus.bytes, sizeof(native_bus.bytes)) == 0);
    }
    assert(waits == 1 && native.state.cycles == 13);
    assert(native.state.pr == 0x1A && native.state.pc == 0x20);
    assert(native.state.r[0] == UINT32_MAX && native.state.r[2] == UINT32_C(0xFFFFFFA2));
    assert(native_bus.bytes[0x38] == 0xFF && native_bus.bytes[0x39] == 0xA2);
    assert(native_bus.fetches == 12 && interpreted_bus.fetches == 12);

    start(&native, &native_bus);
    native_bus.wait_write = true;
    assert(sh2_op_store_predec(&native, 15, 15, 4) == SH2_STEP_WAIT);
    assert(native.state.r[15] == 0x3C && native.state.cycles == 0);
    assert(sh2_op_store_predec(&native, 15, 15, 4) == SH2_STEP_OK);
    assert(native.state.r[15] == 0x38 && native.state.cycles == 1);
    assert(native_bus.bytes[0x3B] == 0x3C);

    start(&native, &native_bus);
    native.state.pc = 0x14;
    native_bus.bytes[0x15] = 1; /* A live writer changed the immediate. */
    assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_NATIVE_MISS);
    assert(native.state.pc == 0x14 && native.state.r[2] == 0 && native.state.cycles == 0);
    bool used = false;
    assert(sh2_step_with_fallback(&native, &sh2_catalog_fixture, &used) == SH2_STEP_OK);
    assert(used && native_bus.fetches == 2 && native.state.r[2] == 1);
    assert(sh2_step_with_fallback(&native, &sh2_catalog_fixture, &used) == SH2_STEP_OK);
    assert(!used && native.state.pc == 0x18 && native.state.delay_pending);

    native.state.pc = 0x16;
    assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_SLOT_ILLEGAL);
    assert(native.state.pc == 0x16 && native.state.pr == 0x1A);
    native.state.pc = 0x17;
    assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_ADDRESS_ERROR);

    native.state.pc = 0x2A;
    assert(sh2_step_with_fallback(&native, &sh2_catalog_fixture, &used) == SH2_STEP_UNSUPPORTED);
    assert(used);

    start(&native, &native_bus);
    start(&interpreted, &interpreted_bus);
    native.state.pc = 0x40;
    native.state.r[2] = 0x60;
    native.state.r[5] = 3;
    native.state.r[7] = 0x68;
    native.state.gbr = 0x60;
    interpreted.state = native.state;
    for (unsigned step = 0; step < 12; ++step) {
        assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_OK);
        assert(sh2_step_with_fallback(&interpreted, NULL, &used) == SH2_STEP_OK && used);
        assert(native.state.pc == interpreted.state.pc && native.state.sr == interpreted.state.sr);
        assert(memcmp(native.state.r, interpreted.state.r, sizeof(native.state.r)) == 0);
        assert(memcmp(native_bus.bytes, interpreted_bus.bytes, sizeof(native_bus.bytes)) == 0);
    }
    assert(native.state.pc == 0x58 && native.state.cycles == 12 && native.state.sr == 0xf1);
    assert(native.state.r[0] == UINT32_C(0xffff03c0));
    assert(native.state.r[1] == UINT32_C(0xffff8003) && native.state.r[3] == native.state.r[1]);
    assert(native.state.r[2] == UINT32_C(0xfffffa01));
    assert(native.state.r[4] == 0x8001 && native.state.r[5] == 0);
    assert(native_bus.bytes[0x63] == 0xc0 && native_bus.bytes[0x68] == 0xff);
    assert(native_bus.bytes[0x69] == 0xff && native_bus.bytes[0x6a] == 3 && native_bus.bytes[0x6b] == 0xc0);
    for (unsigned step = 0; step < 4; ++step) {
        assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_OK);
        assert(sh2_step_with_fallback(&interpreted, NULL, &used) == SH2_STEP_OK && used);
        assert(native.state.r[0] == interpreted.state.r[0]);
        assert(memcmp(native_bus.bytes, interpreted_bus.bytes, sizeof(native_bus.bytes)) == 0);
    }
    assert(native.state.r[0] == UINT32_C(0xffff8003));
    assert(native.state.sr == 0xf1 && native.state.sr == interpreted.state.sr);
    assert(native_bus.bytes[0x6b] == 3 && native_bus.bytes[0x6c] == 3 && native_bus.bytes[0x6d] == 0xc0);

    start(&native, &native_bus);
    start(&interpreted, &interpreted_bus);
    native.state.pc = 0x80;
    native.state.r[1] = 0x12;
    native.state.r[2] = 0x70;
    native.state.r[7] = UINT32_C(0xffffffd4);
    native.state.sr |= SH2_SR_T;
    interpreted.state = native.state;
    for (unsigned step = 0; step < 13; ++step) {
        assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_OK);
        assert(sh2_step_with_fallback(&interpreted, NULL, &used) == SH2_STEP_OK && used);
        assert(native.state.pc == interpreted.state.pc && native.state.pr == interpreted.state.pr);
        assert(memcmp(native.state.r, interpreted.state.r, sizeof(native.state.r)) == 0);
        assert(memcmp(native_bus.bytes, interpreted_bus.bytes, sizeof(native_bus.bytes)) == 0);
        if (step == 1)
            assert(native.state.pc == 0x96 && native.state.r[0] == 0x9c);
    }
    assert(native.state.pc == 0x84 && native.state.pr == 0x84 && native.state.cycles == 15);
    assert(native.state.r[15] == 0x3c && native_bus.bytes[0x3b] == 0x84);
    assert(native.state.r[0] == UINT32_C(0xffffffd1) && native.state.r[3] == UINT32_C(0xffffff81));
    assert(native.state.r[4] == 0x204 && native.state.sr == 0xf0);
    assert(native.state.r[5] == UINT32_C(0x81f00000));

    start(&native, &native_bus);
    native.state.sr = 0x21;
    native.state.pc = 0x1e;
    native.state.vbr = 4;
    assert(write_data(&native_bus, 4 + 17 * 4, 4, 0xb0) == SH2_BUS_OK);
    assert(!sh2_interrupt_ready(&native, 2));
    assert(sh2_interrupt_ready(&native, 3));
    native_bus.wait_address = 0x34;
    assert(sh2_interrupt(&native, 3, 17) == SH2_STEP_WAIT);
    assert(native.state.r[15] == 0x38 && native.state.pc == 0x1e);
    assert(sh2_interrupt_ready(&native, 0));
    assert(sh2_interrupt(&native, 0, 0) == SH2_STEP_OK);
    assert(native.state.r[15] == 0x34 && native.state.pc == 0xb0 && native.state.sr == 0x31);
    assert(native.state.cycles == 8 && native_bus.bytes[0x37] == 0x1e && native_bus.bytes[0x3b] == 0x21);
    interpreted.state = native.state;
    interpreted_bus = native_bus;
    for (unsigned step = 0; step < 2; ++step) {
        assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_OK);
        assert(sh2_step_with_fallback(&interpreted, NULL, &used) == SH2_STEP_OK && used);
        assert(native.state.pc == interpreted.state.pc && native.state.sr == interpreted.state.sr);
        assert(sh2_interrupt_ready(&native, 8) == (step != 0));
    }
    assert(native.state.pc == 0x1e && native.state.r[15] == 0x3c && native.state.r[1] == 1);
    assert(native.state.cycles == 13 && native.state.sr == 0x21);

    native.state.pc = 0x12; /* LDC R1,GBR inhibits acceptance at the next boundary. */
    assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_OK);
    assert(!sh2_interrupt_ready(&native, 8));
    assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_OK);
    assert(sh2_interrupt_ready(&native, 8));

    start(&native, &native_bus);
    start(&interpreted, &interpreted_bus);
    native.state.pc = 0xb4;
    native.state.r[0] = 0x10;
    native.state.r[1] = UINT32_C(0x80000028);
    native.state.r[2] = 0x50;
    interpreted.state = native.state;
    for (unsigned step = 0; step < 4; ++step) {
        assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_OK);
        assert(sh2_step_with_fallback(&interpreted, NULL, &used) == SH2_STEP_OK && used);
        assert(native.state.pc == interpreted.state.pc && native.state.sr == interpreted.state.sr);
        assert(memcmp(native.state.r, interpreted.state.r, sizeof(native.state.r)) == 0);
        assert((native.state.sr & SH2_SR_T) == (step < 3 ? 1u : 0u));
    }
    assert(native.state.pc == 0xbc && native.state.cycles == 4);
    assert(native.state.r[0] == UINT32_C(0xffff8003));
    assert(native.state.r[1] == 0xa0);
    assert(native.state.r[3] == UINT32_C(0xffffff80));

    native.state.sr = 0x51;
    native.state.vbr = UINT32_C(0x89abcdef);
    native.state.gbr = UINT32_C(0xff123400);
    interpreted.state = native.state;
    for (unsigned step = 0; step < 7; ++step) {
        assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_OK);
        assert(sh2_step_with_fallback(&interpreted, NULL, &used) == SH2_STEP_OK && used);
        assert(native.state.pc == interpreted.state.pc && native.state.sr == interpreted.state.sr);
        assert(memcmp(native.state.r, interpreted.state.r, sizeof(native.state.r)) == 0);
        bool ready = step != 0 && step != 3 && step != 5;
        assert(sh2_interrupt_ready(&native, 8) == ready);
        assert(sh2_interrupt_ready(&interpreted, 8) == ready);
    }
    assert(native.state.pc == 0xca && native.state.cycles == 11 && native.state.sr == 0x51);
    assert(native.state.r[0] == 20 && native.state.r[1] == 0xff1234 && native.state.r[2] == 0x89ab);

    native.state.r[0] = 3;
    native.state.r[1] = UINT32_C(0xffffffff);
    native.state.macl = UINT32_C(0x10203040);
    interpreted.state = native.state;
    for (unsigned step = 0; step < 4; ++step) {
        assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_OK);
        assert(sh2_step_with_fallback(&interpreted, NULL, &used) == SH2_STEP_OK && used);
        assert(native.state.pc == interpreted.state.pc && native.state.sr == interpreted.state.sr);
        assert(native.state.macl == interpreted.state.macl);
        assert(memcmp(native.state.r, interpreted.state.r, sizeof(native.state.r)) == 0);
        assert(memcmp(native_bus.bytes, interpreted_bus.bytes, sizeof(native_bus.bytes)) == 0);
        if (step == 2) {
            assert(native.state.r[15] == 0x38 && native_bus.bytes[0x3b] == 0x40);
            native.state.macl = interpreted.state.macl = 0;
        }
    }
    assert(native.state.r[0] == UINT32_C(0x7ffffffe) && native.state.r[15] == 0x3c);
    assert(native.state.macl == UINT32_C(0x10203040) && native.state.sr == 0x51);

    native.state.r[1] = 0x70;
    native.state.r[3] = UINT32_C(0xfffe0003);
    interpreted.state = native.state;
    for (unsigned step = 0; step < 4; ++step) {
        assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_OK);
        assert(sh2_step_with_fallback(&interpreted, NULL, &used) == SH2_STEP_OK && used);
        assert(native.state.pc == interpreted.state.pc && native.state.sr == interpreted.state.sr);
        assert(native.state.macl == interpreted.state.macl);
        assert(memcmp(native.state.r, interpreted.state.r, sizeof(native.state.r)) == 0);
    }
    assert(native.state.r[1] == 0x71 && native.state.r[2] == UINT32_C(0xffffff81));
    assert(native.state.macl == UINT32_C(0xfffffe83) && native.state.r[0] == UINT32_C(0xffffff41));
    assert(native.state.pc == 0xda && native.state.sr == 0x51);

    native.state.r[0] = UINT32_C(0x8000003b);
    native.state.r[1] = UINT32_C(0xfffe001d);
    native.state.r[2] = UINT32_C(0x7fffffe1);
    native.state.r[4] = 71;
    native.state.sr = SH2_SR_Q | SH2_SR_M | 0x51;
    interpreted.state = native.state;
    for (unsigned step = 0; step < 17; ++step) {
        assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_OK);
        assert(sh2_step_with_fallback(&interpreted, NULL, &used) == SH2_STEP_OK && used);
        assert(native.state.pc == interpreted.state.pc && native.state.sr == interpreted.state.sr);
        assert(native.state.macl == interpreted.state.macl);
        assert(memcmp(native.state.r, interpreted.state.r, sizeof(native.state.r)) == 0);
        assert(memcmp(native_bus.bytes, interpreted_bus.bytes, sizeof(native_bus.bytes)) == 0);
        if (step == 3)
            assert(native.state.sr == 0x50);
        if (step == 15)
            assert(native.state.sr & SH2_SR_T);
    }
    assert(native.state.macl == 1711 && native.state.r[3] == 24);
    assert(native.state.r[4] == 71 * 256 && native_bus.bytes[0x1f] == 24);
    assert(native.state.pc == 0xfc && native.state.sr == (0x51 | SH2_SR_Q));

    native.state.r[1] = UINT32_C(0x87654321);
    native.state.r[4] = UINT32_C(0x00ab1234);
    native.state.r[5] = 6;
    native.state.pr = 0x80;
    interpreted.state = native.state;
    for (unsigned step = 0; step < 3; ++step) {
        assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_OK);
        assert(sh2_step_with_fallback(&interpreted, NULL, &used) == SH2_STEP_OK && used);
        assert(native.state.pc == interpreted.state.pc && native.state.pr == interpreted.state.pr);
        assert(memcmp(native.state.r, interpreted.state.r, sizeof(native.state.r)) == 0);
        if (step == 0)
            assert(native.state.pc == 0xfe && native.state.pr == 0x80 && native.state.delay_pending);
        if (step == 1)
            assert(native.state.pc == 0x106 && native.state.irq_inhibit && !native.state.delay_pending);
    }
    assert(native.state.pc == 0x108 && native.state.pr == UINT32_C(0x87654321));
    assert(native.state.r[0] == UINT32_C(0x123400ab));

    native.state.r[0] = UINT32_MAX;
    native.state.r[1] = 2;
    native.state.r[4] = UINT32_C(0xbabeabc7);
    const uint64_t start_cycles = native.state.cycles;
    interpreted.state = native.state;
    for (unsigned step = 0; step < 5; ++step) {
        assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_OK);
        assert(sh2_step_with_fallback(&interpreted, NULL, &used) == SH2_STEP_OK && used);
        assert(native.state.pc == interpreted.state.pc && native.state.sr == interpreted.state.sr);
        assert(native.state.mach == interpreted.state.mach && native.state.macl == interpreted.state.macl);
        assert(memcmp(native.state.r, interpreted.state.r, sizeof(native.state.r)) == 0);
        assert((native.state.sr & SH2_SR_T) == (step != 0));
    }
    assert(native.state.pc == 0x112 && native.state.cycles == start_cycles + 6);
    assert(native.state.r[4] == UINT32_C(0xabc70000) && native.state.mach == 2);
    assert(native.state.macl == UINT32_C(0xfffffffe));

    native.state.r[0] = UINT32_C(0xffff0003);
    native.state.r[1] = 0x10021;
    native.state.r[2] = UINT32_C(0x98765432);
    native.state.r[3] = 0x1004f;
    interpreted.state = native.state;
    for (unsigned step = 0; step < 2; ++step) {
        assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_OK);
        assert(sh2_step_with_fallback(&interpreted, NULL, &used) == SH2_STEP_OK && used);
        assert(native.state.pc == interpreted.state.pc && native.state.sr == interpreted.state.sr);
        assert(memcmp(native.state.r, interpreted.state.r, sizeof(native.state.r)) == 0);
        assert(memcmp(native_bus.bytes, interpreted_bus.bytes, sizeof(native_bus.bytes)) == 0);
    }
    assert(native.state.pc == 0x116 && native.state.cycles == start_cycles + 8);
    assert(native_bus.bytes[0x24] == 0x32);
    assert(native_bus.bytes[0x52] == 0x00 && native_bus.bytes[0x53] == 0x21);

    native.state.r[0] = 0x5e;
    native.state.r[4] = UINT32_C(0x80000000);
    const uint32_t status = native.state.sr;
    interpreted.state = native.state;
    for (unsigned step = 0; step < 2; ++step) {
        assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_OK);
        assert(sh2_step_with_fallback(&interpreted, NULL, &used) == SH2_STEP_OK && used);
        assert(native.state.pc == interpreted.state.pc && native.state.sr == interpreted.state.sr);
        assert(memcmp(native.state.r, interpreted.state.r, sizeof(native.state.r)) == 0);
    }
    assert(native.state.pc == 0x11a && native.state.cycles == start_cycles + 10);
    assert(native.state.r[4] == UINT32_C(0x80000000));
    assert(native.state.r[0] == UINT32_C(0xffff8003) && native.state.sr == status);

    native.state.r[1] = UINT32_C(0x12348072);
    native.state.r[2] = UINT32_C(0xffffff92);
    native.state.r[5] = 0x100;
    interpreted.state = native.state;
    for (unsigned step = 0; step < 4; ++step) {
        assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_OK);
        assert(sh2_step_with_fallback(&interpreted, NULL, &used) == SH2_STEP_OK && used);
        assert(native.state.pc == interpreted.state.pc && native.state.sr == interpreted.state.sr);
        assert(memcmp(native.state.r, interpreted.state.r, sizeof(native.state.r)) == 0);
        assert((native.state.sr & SH2_SR_T) == (step == 0));
    }
    assert(native.state.pc == 0x122 && native.state.cycles == start_cycles + 14);
    assert(native.state.r[3] == UINT32_C(0xffff8072));
    assert(native.state.r[4] == UINT32_C(0xffffff92));

    native.state.mach = UINT32_C(0x80100000);
    native.state.r[3] = 1;
    interpreted.state = native.state;
    for (unsigned step = 0; step < 3; ++step) {
        assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_OK);
        assert(sh2_step_with_fallback(&interpreted, NULL, &used) == SH2_STEP_OK && used);
        assert(native.state.pc == interpreted.state.pc && native.state.sr == interpreted.state.sr);
        assert(memcmp(native.state.r, interpreted.state.r, sizeof(native.state.r)) == 0);
        assert(native.state.irq_inhibit == (step == 0));
        assert(native.state.irq_inhibit == interpreted.state.irq_inhibit);
        assert((native.state.sr & SH2_SR_T) == (step == 2));
    }
    assert(native.state.r[2] == UINT32_C(0x80100000));
    assert(native.state.r[3] == UINT32_C(0x00200003));
    assert(native.state.pc == 0x128 && native.state.cycles == start_cycles + 17);

    native.state.r[0] = 4;
    native.state.r[1] = 0;
    native.state.r[2] = UINT32_MAX;
    interpreted.state = native.state;
    static const uint32_t results[] = {0, 0, 5, 1};
    for (unsigned step = 0; step < sizeof results / sizeof results[0]; ++step) {
        assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_OK);
        assert(sh2_step_with_fallback(&interpreted, NULL, &used) == SH2_STEP_OK && used);
        assert(native.state.pc == interpreted.state.pc && native.state.sr == interpreted.state.sr);
        assert(memcmp(native.state.r, interpreted.state.r, sizeof(native.state.r)) == 0);
        assert(native.state.r[1] == results[step]);
        assert((native.state.sr & SH2_SR_T) == (step < 2));
    }
    assert(native.state.pc == 0x130 && native.state.cycles == start_cycles + 21);
    test_word_mac(false);
    test_word_mac(true);

    native.state.pc = 0x136;
    native.state.r[0] = UINT32_C(0x89abcdef);
    native.state.r[3] = UINT32_C(0x01234567);
    native.state.sr = SH2_SR_Q | SH2_SR_T;
    interpreted.state = native.state;
    const uint64_t extraction_start = native.state.cycles;
    static const uint32_t extracted[] = {0x456789ab, 0x89ab4567};
    for (unsigned step = 0; step < sizeof extracted / sizeof extracted[0]; ++step) {
        assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_OK);
        assert(sh2_step_with_fallback(&interpreted, NULL, &used) == SH2_STEP_OK && used);
        assert(memcmp(native.state.r, interpreted.state.r, sizeof(native.state.r)) == 0);
        assert(native.state.r[0] == extracted[step] && native.state.r[3] == UINT32_C(0x01234567));
        assert(native.state.pc == 0x138 + step * 2 && interpreted.state.pc == native.state.pc);
        assert(native.state.cycles == extraction_start + step + 1);
        assert(interpreted.state.cycles == native.state.cycles);
        assert(native.state.sr == (SH2_SR_Q | SH2_SR_T) && interpreted.state.sr == native.state.sr);
    }

    native.state.pr = 0x40;
    interpreted.state = native.state;
    assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_OK);
    assert(sh2_step_with_fallback(&interpreted, NULL, &used) == SH2_STEP_OK && used);
    assert(native.state.delay_pending && interpreted.state.delay_pending);
    assert(native.state.pc == 0x13c && interpreted.state.pc == 0x13c);
    assert(native.state.sr == (SH2_SR_Q | SH2_SR_T));
    const uint64_t delay_slot_start = native.state.cycles;
    assert(sh2_step_native(&native, &sh2_catalog_fixture) == SH2_STEP_OK);
    assert(sh2_step_with_fallback(&interpreted, NULL, &used) == SH2_STEP_OK && used);
    assert(!native.state.delay_pending && !interpreted.state.delay_pending);
    assert(native.state.pc == 0x40 && interpreted.state.pc == 0x40);
    assert(native.state.sr == SH2_SR_Q && interpreted.state.sr == SH2_SR_Q);
    assert(native.state.cycles == delay_slot_start + 1 && interpreted.state.cycles == native.state.cycles);
    return 0;
}
