#include <assert.h>
#include <stdint.h>
#include <inttypes.h>

#include "asm.h"
#include "ext_function.h"
#include "function.h"
#include "generator.h"
#include "ir.h"
#include "type_punning.h"
#include "unicode.h"


/* This module generates assembly code from Bude word-oriented IR bytecode (WIR), which can then be assembled using FASM.
 *
 * Notes about the codegen:
 *  - Target: x86-64 (Windows)
 *  - Bude calling convention: arguments passed and returned on stack, no alignment requirements (but the stack is always aligned to 8 bytes)
 *  - Registers with special purposes:
 *     = rsp: stack pointer
 *     = rbx: auxiliary stack base pointer
 *     = rsi: auxiliary stack pointer
 *     = rdi: loop counter
 *     = rax, rdx: top two stack slots
 *     = rcx: temporary value storage
 */


struct generator {
    struct asm_block *assembly;
    struct module *module;
    int loop_level;
};


static void generate_header(struct generator *generator) {
    struct asm_block *assembly = generator->assembly;
    asm_write(assembly, "format PE64 console\n");
    asm_write(assembly, "include 'win64ax.inc'\n");
    asm_write(assembly, "\n");
}

static void generate_popn(struct generator *generator, int n) {
    assert(n > 0);
    struct asm_block *assembly = generator->assembly;
    if (n == 1) {
        asm_write_inst2(assembly, "mov", "rdx", "rax");
        asm_write_inst1(assembly, "pop", "rax");
        return;
    }
    if (n >= 3) {
        asm_write_inst2f(assembly, "add", "rsp", "%d", 8 * (n - 2));
    }
    asm_write_inst1(assembly, "pop", "rdx");
    asm_write_inst1(assembly, "pop", "rax");
}

static void generate_dupen(struct generator *generator, int n) {
    assert(n > 0);
    struct asm_block *assembly = generator->assembly;
    if (n == 1) {
        asm_write_inst1(assembly, "push", "rax");
        asm_write_inst2(assembly, "mov", "rax", "rdx");
        return;
    }
    // (42 5 -7)
    // 42 rax:5 rdx:-7
    // 42 5 -7 42 rax:5 rdx:-7
    asm_write_inst1(assembly, "push", "rax");
    asm_write_inst1(assembly, "push", "rdx");
    for (int i = 0; i < n - 2; ++i) {
        asm_write_inst1f(assembly, "push", "qword [rsp+%d]", 8 * (n - 1));
    }
}

static void generate_pack_instruction(struct generator *generator, int n, uint8_t sizes[n]) {
    assert(n > 0);
    struct asm_block *assembly = generator->assembly;
    for (int i = n - 2; i >= 0; --i) {
        asm_write_inst2f(assembly, "shl", "rdx", "%d", 8 * sizes[i]);
        asm_write_inst2(assembly, "xor", "rdx", "rax");
        asm_write_inst1(assembly, "pop", "rax");
    }
}

static void generate_unpack_instruction(struct generator *generator, int n, uint8_t sizes[n]) {
    assert(n > 0);
    struct asm_block *assembly = generator->assembly;
    for (int i = 0; i < n - 1; ++i) {
        asm_write_inst1(assembly, "push", "rax");
        int size = sizes[i];
        switch (size) {
        case 8: assert(0 && "Unreachable"); break;
        case 4: asm_write_inst2(assembly, "mov", "eax", "edx"); break;
        case 2: asm_write_inst2(assembly, "movzx", "eax", "dx"); break;
        case 1: asm_write_inst2(assembly, "movzx", "eax", "dl"); break;
        default:
            assert(0 && "Bad register size");
        }
        asm_write_inst2f(assembly, "shr", "rdx", "%d", 8 * size);
    }
}

static void generate_pack_field_get(struct generator *generator, int offset, int size) {
    struct asm_block *assembly = generator->assembly;
    asm_write_inst1(assembly, "push", "rax");
    asm_write_inst2(assembly, "mov", "rax", "rdx");
    assert(offset >= 0);
    if (offset > 0) {
        asm_write_inst2f(assembly, "shr", "rdx", "%d", 8 * offset);
    }
    switch (size) {
    case 8: break;  // Full-size field.
    case 4: asm_write_inst2(assembly, "mov", "edx", "edx"); break;
    case 2: asm_write_inst2(assembly, "movzx", "edx", "dx"); break;
    case 1: asm_write_inst2(assembly, "movzx", "edx", "dl"); break;
    default:
        assert(0 && "Bad register size");
    }
}

static void generate_pack_field_set(struct generator *generator, int offset, int size) {
    struct asm_block *assembly = generator->assembly;
    // offset = 0, size = 4.
    // 00000000ffffffff
    uint64_t mask = ~(((1ULL << 8*size) - 1) << 8*offset);  // Mask off old value of field.
    if (offset > 0) {
        asm_write_inst2f(assembly, "shl", "rdx", "%d", 8 * offset);
    }
    if (mask <= INT32_MAX) {
        asm_write_inst2f(assembly, "and", "rax", "%d", mask);
    }
    else {
        asm_write_inst2f(assembly, "mov", "rcx", "%"PRId64, mask);
        asm_write_inst2(assembly, "and", "rax", "rcx");
    }
    asm_write_inst2(assembly, "xor", "rdx", "rax");
    asm_write_inst1(assembly, "pop", "rax");
}

static void generate_comp_field_get(struct generator *generator, int offset) {
    assert(offset > 0);
    struct asm_block *assembly = generator->assembly;
    // (600f32 400f32<>> 0 0.2_f32), offset=2
    // [... 600f32 400f32 0<] rax:0.2_f32 rdx:0
    asm_write_inst1(assembly, "push", "rax");
    asm_write_inst2(assembly, "mov", "rax", "rdx");
    if (offset > 1) {
        asm_write_inst2f(assembly, "mov", "rdx", "[rsp+%d]", 8 * (offset - 2));
    }
    // If offset == 1, we leave the result in rdx.
}

static void generate_comp_field_set(struct generator *generator, int offset) {
    // (600f32 400f32 0 0.2_f32), offset=2
    // [... 600f32 400f32] rax:1 rdx:0.2_f32
    assert(offset > 0);
    struct asm_block *assembly = generator->assembly;
    if (offset > 2) {
        asm_write_inst2f(assembly, "mov", "[rsp+%d]", "rdx", 8 * (offset - 2));
        asm_write_inst2(assembly, "mov", "rdx", "rax");
        asm_write_inst1(assembly, "pop", "rax");
    }
    else if (offset == 2) {
        // If offset == 2, the value in rdx will end up in rax, and the value in [rsp]
        // is discarded (since it would end up in rax but is replaced by the new value).
        asm_write_inst2(assembly, "xchg", "rax", "rdx");
        asm_write_inst1(assembly, "pop", "rcx");  // Dummy pop.
    }
    else {
        // If offset == 1, we are writing to the final word, which we leave in rdx.
        asm_write_inst1(assembly, "pop", "rax");
    }
}

static void generate_subcomp_get(struct generator *generator, int offset, int size) {
    struct asm_block *assembly = generator->assembly;
    assert(size > 0);
    assert(offset > 0);
    assert(offset >= size);
    if (size == 1) {
        generate_comp_field_get(generator, offset);
        return;
    }
    asm_write_inst1(assembly, "push", "rax");
    asm_write_inst1(assembly, "push", "rdx");
    for (int i = 0; i < size - 2; ++i) {
        asm_write_inst1f(assembly, "push", "qword [rsp+%d]", 8 * (offset - 1));
    }
    if (offset >= size + 2) {
        asm_write_inst2f(assembly, "mov", "rax", "[rsp+%d]", 8 * (offset - 1));
        asm_write_inst2f(assembly, "mov", "rdx", "[rsp+%d]", 8 * (offset - 2));
    }
    else if (offset == size + 1) {
        asm_write_inst2f(assembly, "mov", "rax", "[rsp+%d]", 8 * (offset - 1));
        // rdx remains with same contents.
    }
    // Else, offset == size, so rax and rdx already have the correct values.
}

static void generate_subcomp_set(struct generator *generator, int offset, int size) {
    struct asm_block *assembly = generator->assembly;
    assert(size > 0);
    assert(offset > 0);
    assert(offset >= size);
    if (size == 1) {
        generate_comp_field_set(generator, offset);
        return;
    }
    // [ .. .. (x y z a b c d< (e f))] (E F)
    // offset = 2
    for (int i = 0; i < size - 2; ++i) {
        asm_write_inst1f(assembly, "pop", "qword [rsp+%d]", 8 * (offset - 4));
    }
    if (offset >= size + 1) {
        asm_write_inst2f(assembly, "mov", "[rsp+%d]", "rdx", 8 * (offset - (size - 2) - 2));
        asm_write_inst1(assembly, "pop", "rdx");
    }
    else {
        // rdx has the right contents already.
        asm_write_inst1(assembly, "pop", "rcx");
    }
    if (offset >= size + 2) {
        asm_write_inst2f(assembly, "mov", "[rsp+%d]", "rax", 8 * (offset - (size - 2) - 1));
        asm_write_inst1(assembly, "pop", "rax");
    }
    else {
        // rax has the right contents already.
        asm_write_inst1(assembly, "pop", "rcx");
    }
}

static void shift_block_down(struct asm_block *assembly, int size, int count) {
    for (int i = size - 3; i >= 0; --i) {
        int read_offset = 8 * i;
        int write_offset = read_offset + 8 * count;
        asm_write_inst2f(assembly, "mov", "rcx", "[rsp+%d]", read_offset);
        asm_write_inst2f(assembly, "mov", "[rsp+%d]", "rcx", write_offset);
    }
    if (size >= 2) {
        asm_write_inst2f(assembly, "mov", "[rsp+%d]", "rax", 8 * (count - 1));
    }
    if (count >= 2) {
        asm_write_inst2f(assembly, "mov", "[rsp+%d]", "rdx",  8 * (count - 2));
    }
    else {
        asm_write_inst2(assembly, "mov", "rax", "rdx");
    }
}

static void shift_block_up(struct asm_block *assembly, int size, int count) {
    if (count >= 2) {
        asm_write_inst2f(assembly, "mov", "rdx", "[rsp+%d]", 8 * (count - 2));
    }
    else {
        asm_write_inst2(assembly, "mov", "rdx", "rax");
    }
    if (size >= 2) {
        asm_write_inst2f(assembly, "mov", "rax", "[rsp+%d]", 8 * (count - 1));
    }
    for (int i = 0; i < size - 2; ++i) {
        int write_offset = 8 * i;
        int read_offset = write_offset + 8 * count;
        asm_write_inst2f(assembly, "mov", "rcx", "[rsp+%d]", read_offset);
        asm_write_inst2f(assembly, "mov", "[rsp+%d]", "rcx", write_offset);
    }
}

static void save_block(struct asm_block *assembly, int start_offset, int size) {
    int i = 0;
    if (start_offset == 0) {
        asm_write_inst2f(assembly, "mov", "[rsp-%d]", "rdx", 8 * size);
        ++i;
    }
    if (start_offset <= 1) {
        asm_write_inst2f(assembly, "mov", "[rsp-%d]", "rax", 8 * (size - 1 + start_offset));
        ++i;
    }
    for (; i < size; ++i) {
        int read_offset = 8 * (start_offset + i - 2);
        int write_offset = 8 * (size - i);
        asm_write_inst2f(assembly, "mov", "rcx", "[rsp+%d]", read_offset);
        asm_write_inst2f(assembly, "mov", "[rsp-%d]", "rcx", write_offset);
    }
}

static void restore_block(struct asm_block *assembly, int start_offset, int size) {
    int i = 0;
    if (start_offset == 0) {
        asm_write_inst2f(assembly, "mov", "rdx", "[rsp-%d]", 8 * size);
        ++i;
    }
    if (start_offset <= 1) {
        asm_write_inst2f(assembly, "mov", "rax", "[rsp-%d]", 8 * (size - 1 + start_offset));
        ++i;
    }
    for (; i < size; ++ i) {
        int read_offset = 8 * (size - i);
        int write_offset = 8 * (start_offset + i - 2);
        asm_write_inst2f(assembly, "mov", "rcx", "[rsp-%d]", read_offset);
        asm_write_inst2f(assembly, "mov", "[rsp+%d]", "rcx", write_offset);
    }
}

static void generate_swap_comps(struct generator *generator, int lhs_size, int rhs_size) {
    struct asm_block *assembly = generator->assembly;
    assert(lhs_size > 0 && rhs_size > 0);
    if (lhs_size == 1 && rhs_size == 1) {
        // Special case: word-size swap.
        asm_write_inst2(assembly, "mov", "rcx", "rax");
        asm_write_inst2(assembly, "mov", "rax", "rdx");
        asm_write_inst2(assembly, "mov", "rdx", "rcx");
    }
    else if (lhs_size == 1) {
        // Special case: lhs fits in a register.
        asm_write_inst2f(assembly, "mov", "r8", "[rsp+%d]", 8 * (rhs_size - 2));
        shift_block_down(assembly, rhs_size, 1);
        asm_write_inst2(assembly, "mov", "rdx", "r8");
    }
    else if (rhs_size == 1) {
        // Special case: rhs fits in a register.
        asm_write_inst2(assembly, "mov", "r8", "rdx");
        shift_block_up(assembly, lhs_size, 1);
        asm_write_inst2f(assembly, "mov", "[rsp+%d]", "r8", 8 * (lhs_size - 2));
    }
    else if (lhs_size == rhs_size) {
        // Special case: lhs = rhs (i.e. non-overlapping).
        assert(rhs_size >= 2);
        asm_write_inst2(assembly, "mov", "rcx", "rdx");
        asm_write_inst2f(assembly, "mov", "rdx", "[rsp+%d]", 8 * (rhs_size - 2));
        asm_write_inst2f(assembly, "mov", "[rsp+%d]", "rcx", 8 * (rhs_size - 2));
        asm_write_inst2(assembly, "mov", "rcx", "rax");
        asm_write_inst2f(assembly, "mov", "rax", "[rsp+%d]", 8 * (rhs_size - 1));
        asm_write_inst2f(assembly, "mov", "[rsp+%d]", "rcx", 8 * (rhs_size - 1));
        for (int i = 2; i < rhs_size; ++i) {
            int lhs_offset = 8 * (i + rhs_size - 2);
            int rhs_offset = 8 * (i - 2);
            asm_write_inst2f(assembly, "mov", "rcx", "[rsp+%d]", rhs_offset);
            asm_write_inst2f(assembly, "mov", "r8", "[rsp+%d]", lhs_offset);
            asm_write_inst2f(assembly, "mov", "[rsp+%d]", "r8", rhs_offset);
            asm_write_inst2f(assembly, "mov", "[rsp+%d]", "rcx", lhs_offset);
        }
    }
    else if (lhs_size < rhs_size) {
        // General case, lhs < rhs.
        save_block(assembly, rhs_size, lhs_size);
        shift_block_down(assembly, rhs_size, lhs_size);
        restore_block(assembly, 0, lhs_size);
    }
    else {
        // General case, lhs > rhs.
        save_block(assembly, 0, rhs_size);
        shift_block_up(assembly, lhs_size, rhs_size);
        restore_block(assembly, lhs_size, rhs_size);
    }
}

static bool is_power_of_two(unsigned n) {
    return (n & (n-1)) == 0 && n != 0;
}

static int get_power_of_two(unsigned n) {
    assert(is_power_of_two(n));
    int power = 0;
    for (n -= 1; n != 0; ++power) {
        n &= n - 1;
    }
    return power;
}

static void generate_array_get(struct generator *generator, int element_count, int word_count) {
    struct asm_block *assembly = generator->assembly;
    // TODO: Add optional bounds checking.
    if (word_count > 1) {
        if (is_power_of_two(word_count)) {
            // Use left bitshift for powers of two.
            asm_write_inst2f(assembly, "shl", "rdx", "%d", get_power_of_two(word_count));
        }
        else {
            asm_write_inst2f(assembly, "mul", "rdx", "%d", word_count);
        }
    }
    asm_write_inst1(assembly, "neg", "rdx");  // Index.
    // [(5 42 2 -17) (-7 31 69 0) (77 19 33 -9)] 2
    // [_2 _1 _0 5 42 2 -17 -7 31 69 0 77 19 33 -8 77 19< _0 _1 _2 _3 _4 _5 _6 _7<] rax:33 rdx:-9
    // rcx:------------------------------------^
    asm_write_inst2f(assembly, "lea", "rcx", "[rsp+rdx*8+%d]", (element_count - 1) * word_count * 8);
    asm_write_inst2(assembly, "cmp", "rcx", "rsp");
    asm_write_inst2(assembly, "cmove", "rdx", "rax");
    asm_write_inst2(assembly, "cmovne", "rdx", "[rcx-8]");
    if (word_count <= 1) return;
    asm_write_inst1(assembly, "push", "rax");
    asm_write_inst2(assembly, "mov", "rax", "[rcx]");
    for (int i = word_count - 2; i >= 1; --i) {
        asm_write_inst1f(assembly, "push", "qword [rcx+%d]", 8 * i);
    }
}

static void generate_array_set(struct generator *generator, int element_count, int word_count) {
    struct asm_block *assembly = generator->assembly;
    if (word_count > 1) {
        if (is_power_of_two(word_count)) {
            // Use left bitshift for powers of two.
            asm_write_inst2f(assembly, "shl", "rdx", "%d", get_power_of_two(word_count));
        }
        else {
            asm_write_inst2f(assembly, "mul", "rdx", "%d", word_count);
        }
    }
    // [(5 42 -7) (1 2 3)] (11 54 9) 1
    // [5 42 -7 11< 54 9 11 54 _0 _1 _2] rax:54 rdx:9
    // rcx:-------------^
    // mov [rcx], rax
    // pop rax
    asm_write_inst1(assembly, "neg", "rdx");  // Index.
    asm_write_inst2f(assembly, "lea", "rcx", "[rsp+rdx*8+%d]", (element_count * word_count - 1) * 8);
    asm_write_inst2(assembly, "mov", "[rcx]", "rax");
    for (int i = 1; i < word_count; ++i) {
        asm_write_inst1f(assembly, "pop", "qword [rcx+%d]", 8 * i);
    }
    asm_write_inst1(assembly, "pop", "rdx");
    asm_write_inst1(assembly, "pop", "rax");
}

static void generate_external_call_bude(struct generator *generator,
                                        struct ext_function *external) {
    asm_write_inst1f(generator->assembly, "call", "[%"PRI_SV"]", SV_FMT(external->name));
}

// NOTE: In this function, the stack is referenced throguh `rbp`, including the top two elements.
static void move_comp_to_aux(struct generator *generator, int word_count, int end_offset) {
    for (int i = 0; i < word_count; ++i) {
        // NOTE: this function reverses the stack words of the comp, putting it in its canonical form on aux.
        int field_offset = end_offset + word_count - i + 1;
        asm_write_inst2f(generator->assembly, "mov", "rcx", "[rbp+%d]", 8 * field_offset);
        asm_write_inst2f(generator->assembly, "mov", "[rsi+%d]", "rcx", 8 * i);
    }
    asm_write_inst2f(generator->assembly, "add", "rsi", "%d", 8 * word_count);
}

static void generate_external_call_ms_x64(struct generator *generator,
                                          struct ext_function *external) {
    int param_count = external->sig.param_count;
    type_index *params = external->sig.params;
    struct type_table *types = &generator->module->types;
    struct asm_block *assembly = generator->assembly;
    type_index ret_type = (external->sig.ret_count > 0) ? external->sig.rets[0] : TYPE_ERROR;
    assert(param_count >= 0);
    asm_write_inst1(assembly, "push", "rax");
    asm_write_inst1(assembly, "push", "rdx");
    asm_write_inst2(assembly, "lea", "rbp", "[rsp]");
    asm_write_inst2(assembly, "and", "spl", "0F0h");
    if (param_count > 4 && param_count % 2 == 1) {
        // Odd number of parameters; extra push to (mis)align stack.
        asm_write_inst1(assembly, "push", "rax");
    }
    // We need to push all the higher arguments in reverse order.
    int offset = 0;
    int aux_alloc_count = 0;  // Number of words temporarily allocated on aux.
    bool overlong_ret = type_word_count(types, ret_type) > 1;
    for (; offset < param_count - 4 + overlong_ret; ++offset) {
        type_index type = params[offset + 4];
        int word_count = type_word_count(types, type);
        assert(word_count > 0);
        if (word_count == 1) {
            asm_write_inst1f(assembly, "push", "qword [rbp+%d]", 8 * offset);
        }
        else {
            move_comp_to_aux(generator, type, offset);
            aux_alloc_count += word_count;
            asm_write_inst2f(assembly, "lea", "rax", "[rsi-%d]", 8 * word_count);
            asm_write_inst1(assembly, "push", "rax");  // Pointer to start of comp.
        }
    }
#define WRITE_REGISTER_PARAM(type, intreg, floatreg)                    \
    do {                                                                \
        int word_count = type_word_count(types, type);                  \
        assert(word_count > 0);                                         \
        if (word_count == 1 && !is_float(type)) {                       \
            asm_write_inst2f(assembly, "mov", intreg, "[rbp+%d]", 8 * offset); \
        }                                                               \
        else if (type == TYPE_F32) {                                    \
            asm_write_inst2f(assembly, "movd", floatreg, "dword [rbp+%d]", 8 * offset); \
        }                                                               \
        else if (type == TYPE_F64) {                                    \
            asm_write_inst2f(assembly, "movq", floatreg, "qword [rbp+%d]", 8 * offset); \
        }                                                               \
        else {                                                          \
            move_comp_to_aux(generator, type, offset);                  \
            aux_alloc_count += word_count;                              \
            asm_write_inst2f(assembly, "lea", intreg, "[rsi-%d]", 8 * word_count); \
        }                                                               \
    } while (0)
    /* It turns out switch fallthrough is useful in some rare cases. */
    switch (param_count - offset + overlong_ret) {
    case 4: {
        type_index type = params[3 - overlong_ret];
        WRITE_REGISTER_PARAM(type, "r9", "xmm3");
        ++offset;
    }
        /* Fallthrough */
    case 3: {
        type_index type = params[2 - overlong_ret];
        WRITE_REGISTER_PARAM(type, "r8", "xmm2");
        ++offset;
    }
        /* Fallthrough */
    case 2: {
        type_index type = params[1 - overlong_ret];
        WRITE_REGISTER_PARAM(type, "rdx", "xmm1");
        ++offset;
    }
        /* Fallthrough */
    case 1:
        if (!overlong_ret) {
            type_index type = params[0];
            WRITE_REGISTER_PARAM(type, "rcx", "xmm0");
        }
        else {
            int word_count = type_word_count(types, ret_type);
            aux_alloc_count += word_count;
            asm_write_inst2(assembly, "lea", "rcx", "[rsi]");  // Reserve space on aux.
            asm_write_inst2f(assembly, "add", "rsi", "%d", 8 * word_count);
        }
        break;
    case 0:
        break;
    default:
        assert(0 && "Unreachable");
    }
#undef WRITE_REGISTER_PARAM
    asm_write_inst2(assembly, "sub", "rsp", "32");  // Shadow space.
    asm_write_inst1f(assembly, "call", "[%"PRI_SV"]", SV_FMT(external->name));
    asm_write_inst2f(assembly, "lea", "rsp", "[rbp+%d]", 8 * param_count);
    if (overlong_ret) {
        int word_count = type_word_count(types, ret_type);
        assert(word_count >= 2);
        for (int i = 0; i < word_count - 2; ++i) {
            asm_write_inst1f(assembly, "push", "qword [rax+%d]", 8 * i);
        }
        asm_write_inst2f(assembly, "mov", "rdx", "[rax+%d]", 8 * (word_count - 1));
        asm_write_inst2f(assembly, "mov", "rax", "[rax+%d]", 8 * (word_count - 2));
    }
    else if (external->sig.ret_count > 0) {
        // External functions have either 0 or 1 return value(s), no more.
        assert(external->sig.ret_count == 1);
        int ret_size = type_size(types, ret_type);
        if (ret_type == TYPE_F64) {
            asm_write_inst2(assembly, "movq", "rdx", "xmm0");
        }
        else if (ret_type == TYPE_F32) {
            asm_write_inst2(assembly, "movd", "edx", "xmm0");
        }
        else if (ret_size == 1) {
            asm_write_inst2(assembly, "movzx", "edx", "al");
        }
        else if (ret_size == 2) {
            asm_write_inst2(assembly, "movzx", "edx", "ax");
        }
        else if (ret_size == 4) {
            asm_write_inst2(assembly, "mov", "edx", "eax");
        }
        else {
            assert(ret_size == 8 && "Unaccounted-for return value size");
        }
        asm_write_inst1(assembly, "pop", "rax");
    }
    else {
        asm_write_inst1(assembly, "pop", "rdx");
        asm_write_inst1(assembly, "pop", "rax");
    }
    if (aux_alloc_count > 0) {
        asm_write_inst2f(assembly, "sub", "rsi", "%d", 8 * aux_alloc_count);
    }
}

static void generate_external_call_sysv_amd64(struct generator *generator,
                                              struct ext_function *external) {
    // TODO: implement this.
    (void)generator, (void)external;
    assert(0 && "Not implemented");
}

static void generate_external_call(struct generator *generator, struct ext_function *external) {
    typedef void (*extcall_generator)(struct generator *generator, struct ext_function *external);
    static extcall_generator dispatch_table[] = {
        [CC_BUDE] = generate_external_call_bude,
#if defined(_WIN32)
        [CC_NATIVE] = generate_external_call_ms_x64,
#elif defined(__linux__)
        [CC_NATIVE] = generate_external_call_sysv_amd64,
#else
        [CC_NATIVE] = NULL,
#endif
        [CC_MS_X64] = generate_external_call_ms_x64,
        [CC_SYSV_AMD64] = generate_external_call_sysv_amd64,
    };
    dispatch_table[external->call_conv](generator, external);
}

static void generate_function_call(struct generator *generator, int func_index) {
    asm_write_inst1f(generator->assembly, "call", "func_%d", func_index);
}

static void generate_function_return(struct generator *generator) {
    struct asm_block *assembly = generator->assembly;
    if (generator->loop_level > 0) {
        // Restore old loop counter. Only needed when returning in a loop.
        asm_write_inst2(assembly, "mov", "rdi", "[rbx+8]");
    }
    asm_write_inst2(assembly, "lea", "rsi", "[rbx]");
    asm_write_inst2(assembly, "mov", "rbx", "[rbx]");
    asm_write_inst2(assembly, "sub", "rsi", "8");
    asm_write_inst1(assembly, "push", "qword [rsi]");
    asm_write_inst0(assembly, "ret");
}

static void generate_function(struct generator *generator, int func_index) {
    generator->loop_level = 0;
    struct asm_block *assembly = generator->assembly;
    struct function *function = get_function(&generator->module->functions, func_index);
    asm_label(assembly, "func_%d", func_index);
    // Layout of aux frame: [ret][base][... Loops ...][... Locals ...][... aux ...]
    //                            ^rbx                                 ^rsi
    asm_write_inst1(assembly, "pop", "qword [rsi]");
    asm_write_inst2(assembly, "mov", "[rsi+8]", "rbx");
    asm_write_inst2(assembly, "lea", "rbx", "[rsi+8]");
    asm_write_inst2f(assembly, "add", "rsi", "%d",
                     8 * (2 + function->max_for_loop_level + function->locals_size));
    struct ir_block *block = &function->w_code;
    // Instructions.
    for (int ip = 0; ip < block->count; ++ip) {
        if (is_jump_dest(block, ip)) {
            // We need a label.
            asm_label(assembly, ".addr_%d", ip);
        }
        enum w_opcode instruction = block->code[ip];
        if (instruction == W_OP_NOP) continue;
        asm_write(assembly, "  ;;\t=== %s ===\n", get_opcode_name(instruction));
        switch (instruction) {
        case W_OP_NOP:
            // Do nothing.
            break;
        case W_OP_PUSH8: {
            uint8_t value = read_u8(block, ip + 1);
            ip += 1;
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst2(assembly, "mov", "rax", "rdx");
            asm_write_inst2f(assembly, "mov", "edx", "%"PRIu8, value);
            break;
        }
        case W_OP_PUSH16: {
            uint16_t value = read_u16(block, ip + 1);
            ip += 2;
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst2(assembly, "mov", "rax", "rdx");
            asm_write_inst2f(assembly, "mov", "edx", "%"PRIu16, value);
            break;
        }
        case W_OP_PUSH32: {
            uint32_t value = read_u32(block, ip + 1);
            ip += 4;
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst2(assembly, "mov", "rax", "rdx");
            asm_write_inst2f(assembly, "mov", "edx", "%"PRIu32, value);
            break;
        }
        case W_OP_PUSH64: {
            uint64_t value = read_u64(block, ip + 1);
            ip += 8;
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst2(assembly, "mov", "rax", "rdx");
            asm_write_inst2f(assembly, "mov", "rdx", "%"PRIu64, value);
            break;
        }
        case W_OP_PUSH_INT8: {
            int8_t value = read_s8(block, ip + 1);
            ip += 1;
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst2(assembly, "mov", "rax", "rdx");
            asm_write_inst2f(assembly, "mov", "edx", "%"PRId8, value);
            break;
        }
        case W_OP_PUSH_INT16: {
            int16_t value = read_s16(block, ip + 1);
            ip += 2;
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst2(assembly, "mov", "rax", "rdx");
            asm_write_inst2f(assembly, "mov", "edx", "%"PRId16, value);
            break;
        }
        case W_OP_PUSH_INT32: {
            int32_t value = read_s32(block, ip + 1);
            ip += 4;
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst2(assembly, "mov", "rax", "rdx");
            asm_write_inst2f(assembly, "mov", "edx", "%"PRId32, value);
            break;
        }
        case W_OP_PUSH_INT64: {
            int64_t value = read_s64(block, ip + 1);
            ip += 8;
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst2(assembly, "mov", "rax", "rdx");
            asm_write_inst2f(assembly, "mov", "rdx", "%"PRId64, value);
            break;
        }
        case W_OP_PUSH_FLOAT32: {
            uint32_t value = read_u32(block, ip + 1);
            ip += 4;
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst2(assembly, "mov", "rax", "rdx");
            asm_write_inst2f(assembly, "mov", "edx", "%"PRIu32, value);
            break;
        }
        case W_OP_PUSH_FLOAT64: {
            uint64_t value = read_u64(block, ip + 1);
            ip += 8;
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst2(assembly, "mov", "rax", "rdx");
            asm_write_inst2f(assembly, "mov", "rdx", "%"PRIu64, value);
            break;
        }
        case W_OP_PUSH_CHAR8: {
            uint8_t codepoint = read_u8(block, ip + 1);
            ip += 1;
            uint32_t bytes = encode_utf8_u32(codepoint);
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst2(assembly, "mov", "rax", "rdx");
            asm_write_inst2f(assembly, "mov", "edx", "%"PRIu32, bytes);
            break;
        }
        case W_OP_PUSH_CHAR16: {
            uint16_t codepoint = read_u16(block, ip + 1);
            ip += 2;
            uint32_t bytes = encode_utf8_u32(codepoint);
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst2(assembly, "mov", "rax", "rdx");
            asm_write_inst2f(assembly, "mov", "eax", "%"PRIu32, bytes);
            break;
        }
        case W_OP_PUSH_CHAR32: {
            uint32_t codepoint = read_u32(block, ip + 1);
            ip += 4;
            uint32_t bytes = encode_utf8_u32(codepoint);
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst2(assembly, "mov", "rax", "rdx");
            asm_write_inst2f(assembly, "mov", "eax", "%"PRIu32, bytes);
            break;
        }
        case W_OP_LOAD_STRING8: {
            ++ip;
            uint8_t index = read_u8(block, ip);
            struct string_view *string = read_string(generator->module, index);
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst1(assembly, "push", "rdx");
            asm_write_inst2f(assembly, "lea", "rax", "[str%"PRIu8"]", index);
            asm_write_inst2f(assembly, "mov", "edx", "%d", string->length);
            break;
        }
        case W_OP_LOAD_STRING16: {
            ip += 2;
            uint16_t index = read_u16(block, ip - 1);
            struct string_view *string = read_string(generator->module, index);
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst1(assembly, "push", "rdx");
            asm_write_inst2f(assembly, "lea", "rax", "[str%"PRIu16"]", index);
            asm_write_inst2f(assembly, "mov", "edx", "%d", string->length);
            break;
        }
        case W_OP_LOAD_STRING32: {
            ip += 4;
            uint32_t index = read_u32(block, ip - 3);
            struct string_view *string = read_string(generator->module, index);
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst1(assembly, "push", "rdx");
            asm_write_inst2f(assembly, "lea", "rax", "[str%"PRIu32"]", index);
            asm_write_inst2f(assembly, "mov", "edx", "%d", string->length);
            break;
        }
        case W_OP_POP:
            generate_popn(generator, 1);
            break;
        case W_OP_POPN8: {
            int8_t n = read_s8(block, ip + 1);
            ip += 1;
            generate_popn(generator, n);
            break;
        }
        case W_OP_POPN16: {
            int16_t n = read_s16(block, ip + 1);
            ip += 2;
            generate_popn(generator, n);
            break;
        }
        case W_OP_POPN32: {
            int32_t n = read_s32(block, ip + 1);
            ip += 4;
            generate_popn(generator, n);
            break;
        }
        case W_OP_ADD:
            // Addition is commutative.
            asm_write_inst2(assembly, "add", "rdx", "rax");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_ADDF32:
            asm_write_inst2(assembly, "movd", "xmm0", "eax");
            asm_write_inst2(assembly, "movd", "xmm1", "edx");
            asm_write_inst2(assembly, "addss", "xmm0", "xmm1");
            asm_write_inst2(assembly, "movd", "edx", "xmm0");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_ADDF64:
            asm_write_inst2(assembly, "movq", "xmm0", "rax");
            asm_write_inst2(assembly, "movq", "xmm1", "rdx");
            asm_write_inst2(assembly, "addsd", "xmm0", "xmm1");
            asm_write_inst2(assembly, "movq", "rdx", "xmm0");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_AND:
            asm_write_inst2(assembly, "test", "rax", "rax");
            asm_write_inst2(assembly, "cmovz", "rdx", "rax");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_DEREF:
            asm_write_inst2(assembly, "movzx", "edx", "byte [rdx]");
            break;
        case W_OP_DIVF32:
            asm_write_inst2(assembly, "movd", "xmm0", "eax");
            asm_write_inst2(assembly, "movd", "xmm1", "edx");
            asm_write_inst2(assembly, "divss", "xmm0", "xmm1");
            asm_write_inst2(assembly, "movd", "edx", "xmm0");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_DIVF64:
            asm_write_inst2(assembly, "movq", "xmm0", "rax");
            asm_write_inst2(assembly, "movq", "xmm1", "rdx");
            asm_write_inst2(assembly, "divsd", "xmm0", "xmm1");
            asm_write_inst2(assembly, "movq", "rdx", "xmm0");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_DIVMOD:
            asm_write_inst2(assembly, "mov", "rcx", "rdx");
            asm_write_inst2(assembly, "xor", "rdx", "rdx");
            asm_write_inst1(assembly, "div", "rcx");
            break;
        case W_OP_IDIVMOD:
            asm_write_inst2(assembly, "mov", "rcx", "rdx");
            asm_write_inst0(assembly, "cqo");
            asm_write_inst1(assembly, "idiv", "rcx");
            break;
        case W_OP_EDIVMOD:
            // Algorithm:
            // if a > 0, then edivmod(a, b) = idivmod(a, b).
            // otherwise, edivmod(a, b) = q, r, where
            //   q', r' = idivmod(a, b)
            //   q = q' - sign(b)
            //   r = r' + abs(b)
            asm_write_inst2(assembly, "mov", "rcx", "rdx");
            asm_write_inst2(assembly, "mov", "r8", "rcx");
            asm_write_inst1(assembly, "neg", "r8");
            asm_write_inst2(assembly, "cmovs", "r8", "rcx");  // r8 = abs(b).
            asm_write_inst1(assembly, "setg", "r9b");
            asm_write_inst2(assembly, "movzx", "r9", "r9b");
            asm_write_inst1(assembly, "setl", "r10b");
            asm_write_inst2(assembly, "movzx", "r10", "r10b");
            asm_write_inst2(assembly, "sub", "r10", "r9");  // r10 = -sign(b).
            asm_write_inst0(assembly, "cqo");
            asm_write_inst1(assembly, "idiv", "rcx");
            asm_write_inst2(assembly, "add", "r10", "rax");  // r10 = q' - sign(b)
            asm_write_inst2(assembly, "add", "r8", "rdx");   // r8 = r' + abs(b)
            asm_write_inst2(assembly, "test", "rdx", "rdx");   // This will set/clear the sign flag based on the sign bit of rdx.
            asm_write_inst2(assembly, "cmovs", "rax", "r10");  // Only replace q and r if r < 0.
            asm_write_inst2(assembly, "cmovs", "rdx", "r8");
            break;
        case W_OP_DUPE:
            generate_dupen(generator, 1);
            break;
        case W_OP_DUPEN8: {
            int8_t n = read_s8(block, ip + 1);
            ip += 1;
            generate_dupen(generator, n);
            break;
        }
        case W_OP_DUPEN16: {
            int16_t n = read_s16(block, ip + 1);
            ip += 2;
            generate_dupen(generator, n);
            break;
        }
        case W_OP_DUPEN32: {
            int32_t n = read_s32(block, ip + 1);
            ip += 4;
            generate_dupen(generator, n);
            break;
        }
        case W_OP_EQUALS:
            asm_write_inst2(assembly, "cmp", "rax", "rdx");
            asm_write_inst1(assembly, "sete", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_EQUALS_F32:
            asm_write_inst2(assembly, "movd", "xmm1", "edx");
            asm_write_inst2(assembly, "movd", "xmm0", "eax");
            asm_write_inst2(assembly, "ucomiss", "xmm0", "xmm1");
            asm_write_inst1(assembly, "sete", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_EQUALS_F64:
            asm_write_inst2(assembly, "movq", "xmm1", "rdx");
            asm_write_inst2(assembly, "movq", "xmm0", "rax");
            asm_write_inst2(assembly, "ucomisd", "xmm0", "xmm1");
            asm_write_inst1(assembly, "sete", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_EXIT:
            asm_write_inst2(assembly, "mov", "rcx", "rdx");
            asm_write_inst1(assembly, "call", "[ExitProcess]");
            break;
        case W_OP_FOR_DEC_START: {
            ip += 2;
            int16_t skip_jump = read_s16(block, ip - 1);
            int skip_jump_addr = ip - 1 + skip_jump;
            int old_offset = 8 * ++generator->loop_level;  // +1 for previous aux base pointer.
            asm_write_inst2(assembly, "mov", "rcx", "rdx");  // rcx = loop_counter.
            asm_write_inst2(assembly, "mov", "rdx", "rax");
            asm_write_inst1(assembly, "pop", "rax");
            asm_write_inst2(assembly, "cmp", "rcx", "0");
            asm_write_inst1f(assembly, "jle", ".addr_%d", skip_jump_addr);
            asm_write_inst2f(assembly, "mov", "[rbx+%d]", "rdi", old_offset);
            asm_write_inst2c(assembly, "mov", "rdi", "rcx", "Load loop counter.");
            break;
        }
        case W_OP_FOR_DEC: {
            ip += 2;
            int16_t loop_jump = read_s16(block, ip - 1);
            int loop_jump_addr = ip - 1 + loop_jump;
            int old_offset = 8 * generator->loop_level--;  // +1 for previous aux base pointer.
            asm_write_inst1(assembly, "dec", "rdi");
            asm_write_inst2(assembly, "test", "rdi", "rdi");
            asm_write_inst1f(assembly, "jnz", ".addr_%d", loop_jump_addr);
            // Restore previous loop counter.
            asm_write_inst2f(assembly, "mov", "rdi", "[rbx%+d]", old_offset);
            break;
        }
        case W_OP_FOR_INC_START: {
            ip += 2;
            int16_t skip_jump = read_s16(block, ip - 1);
            int skip_jump_addr = ip - 1 + skip_jump;
            int old_offset = (generator->loop_level + 1)*8;  // +1 for previous aux base pointer.
            generator->loop_level += 2;  // +2 to allow space for loop target.
            int target_offset = 8 * generator->loop_level;  // -1 for counter, +1 for base ptr.
            asm_write_inst2(assembly, "mov", "rcx", "rdx");  // rcx = loop_target.
            asm_write_inst2(assembly, "mov", "rdx", "rax");
            asm_write_inst1(assembly, "pop", "rax");
            asm_write_inst2(assembly, "cmp", "rcx", "0");
            asm_write_inst1f(assembly, "jle", ".addr_%d", skip_jump_addr);
            asm_write_inst2f(assembly, "mov", "[rbx+%d]", "rcx", target_offset);
            asm_write_inst2f(assembly, "mov", "[rbx+%d]", "rdi", old_offset);
            asm_write_inst2(assembly, "xor", "rdi", "rdi");  // Zero out loop counter.
            break;
        }
        case W_OP_FOR_INC: {
            ip += 2;
            int16_t loop_jump = read_s16(block, ip - 1);
            int loop_jump_addr = ip - 1 + loop_jump;
            int target_offset = 8 * generator->loop_level;  // -1 for counter, +1 for base ptr.
            generator->loop_level -= 2;  // -2 to remove target.
            int old_offset = 8*(generator->loop_level + 1);  // +1 for previous aux base pointer.
            asm_write_inst1(assembly, "inc", "rdi");
            asm_write_inst2f(assembly, "cmp", "rdi", "[rbx+%d]", target_offset);
            asm_write_inst1f(assembly, "jl", ".addr_%d", loop_jump_addr);
            // Restore previous loop counter.
            asm_write_inst2f(assembly, "mov", "rdi", "[rbx%+d]", old_offset);
            break;
        }
        case W_OP_GET_LOOP_VAR: {
            ip += 2;
            assert(generator->loop_level > 0);
            uint16_t offset = read_u16(block, ip - 1);  // Offset from top of loop stack.
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst2(assembly, "mov", "rax", "rdx");
            if (offset == 0) {
                // Current loop.
                asm_write_inst2(assembly, "mov", "rdx", "rdi");
            }
            else {
                // Outer loop.
                int base_offset = generator->loop_level - offset + 1;  // +1 for base ptr.
                asm_write_inst2f(assembly, "mov", "rdx", "[rbx+%d]", 8 * base_offset);
            }
            break;
        }
        case W_OP_GREATER_EQUALS:
            asm_write_inst2(assembly, "cmp", "rax", "rdx");
            asm_write_inst1(assembly, "setge", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_GREATER_EQUALS_F32:
            asm_write_inst2(assembly, "movd", "xmm1", "edx");
            asm_write_inst2(assembly, "movd", "xmm0", "eax");
            asm_write_inst2(assembly, "ucomiss", "xmm0", "xmm1");
            asm_write_inst1(assembly, "setae", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_GREATER_EQUALS_F64:
            asm_write_inst2(assembly, "movq", "xmm1", "rdx");
            asm_write_inst2(assembly, "movq", "xmm0", "rax");
            asm_write_inst2(assembly, "ucomisd", "xmm0", "xmm1");
            asm_write_inst1(assembly, "setae", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_GREATER_THAN:
            asm_write_inst2(assembly, "cmp", "rax", "rdx");
            asm_write_inst1(assembly, "setg", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_GREATER_THAN_F32:
            asm_write_inst2(assembly, "movd", "xmm1", "edx");
            asm_write_inst2(assembly, "movd", "xmm0", "eax");
            asm_write_inst2(assembly, "ucomiss", "xmm0", "xmm1");
            asm_write_inst1(assembly, "seta", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_GREATER_THAN_F64:
            asm_write_inst2(assembly, "movq", "xmm1", "rdx");
            asm_write_inst2(assembly, "movq", "xmm0", "rax");
            asm_write_inst2(assembly, "ucomisd", "xmm0", "xmm1");
            asm_write_inst1(assembly, "seta", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_HIGHER_SAME:
            asm_write_inst2(assembly, "cmp", "rax", "rdx");
            asm_write_inst1(assembly, "setae", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_HIGHER_THAN:
            asm_write_inst2(assembly, "cmp", "rax", "rdx");
            asm_write_inst1(assembly, "seta", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_JUMP: {
            ip += 2;
            int16_t jump = read_s16(block, ip - 1);
            int jump_addr = ip - 1 + jump;  // -1 since jumps are calculated from the opcode.
            asm_write_inst1f(assembly, "jmp", ".addr_%d", jump_addr);
            break;
        }
        case W_OP_JUMP_COND: {
            ip += 2;
            int16_t jump = read_s16(block, ip - 1);
            int jump_addr = ip - 1 + jump;
            asm_write_inst2(assembly, "test", "rdx", "rdx");
            asm_write_inst2(assembly, "mov", "rdx", "rax");
            asm_write_inst1(assembly, "pop", "rax");
            asm_write_inst1f(assembly, "jnz", ".addr_%d", jump_addr);
            break;
        }
        case W_OP_JUMP_NCOND: {
            ip += 2;
            int16_t jump = read_s16(block, ip - 1);
            int jump_addr = ip -1 + jump;
            asm_write_inst2(assembly, "test", "rdx", "rdx");
            asm_write_inst2(assembly, "mov", "rdx", "rax");
            asm_write_inst1(assembly, "pop", "rax");
            asm_write_inst1f(assembly, "jz", ".addr_%d", jump_addr);
            break;
        }
        case W_OP_LESS_EQUALS:
            asm_write_inst2(assembly, "cmp", "rax", "rdx");
            asm_write_inst1(assembly, "setle", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_LESS_EQUALS_F32:
            asm_write_inst2(assembly, "movd", "xmm1", "edx");
            asm_write_inst2(assembly, "movd", "xmm0", "eax");
            asm_write_inst2(assembly, "ucomiss", "xmm0", "xmm1");
            asm_write_inst1(assembly, "setbe", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_LESS_EQUALS_F64:
            asm_write_inst2(assembly, "movq", "xmm1", "rdx");
            asm_write_inst2(assembly, "movq", "xmm0", "rax");
            asm_write_inst2(assembly, "ucomisd", "xmm0", "xmm1");
            asm_write_inst1(assembly, "setbe", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_LESS_THAN:
            asm_write_inst2(assembly, "cmp", "rax", "rdx");
            asm_write_inst1(assembly, "setl", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_LESS_THAN_F32:
            asm_write_inst2(assembly, "movd", "xmm1", "edx");
            asm_write_inst2(assembly, "movd", "xmm0", "eax");
            asm_write_inst2(assembly, "ucomiss", "xmm0", "xmm1");
            asm_write_inst1(assembly, "setb", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_LESS_THAN_F64:
            asm_write_inst2(assembly, "movq", "xmm1", "rdx");
            asm_write_inst2(assembly, "movq", "xmm0", "rax");
            asm_write_inst2(assembly, "ucomisd", "xmm0", "xmm1");
            asm_write_inst1(assembly, "setb", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_LOCAL_GET: {
            ip += 2;
            int index = read_u16(block, ip - 1);
            struct local *local = &function->locals.items[index];
            int word_count = local->size;
            assert(word_count > 0);
            asm_write_inst1(assembly, "push", "rax");
            if (word_count == 1) {
                asm_write_inst2(assembly, "mov", "rax", "rdx");
            }
            else {
                asm_write_inst1(assembly, "push", "rdx");
            }
            int offset = 1 + function->max_for_loop_level + local->offset;
            for (int i = 0; i < word_count - 2; ++i, ++offset) {
                asm_write_inst1f(assembly, "push", "qword [rbx+%d]", 8 * offset);
            }
            if (word_count >= 2) {
                asm_write_inst2f(assembly, "mov", "rax", "[rbx+%d]", 8 * offset++);
            }
            asm_write_inst2f(assembly, "mov", "rdx", "[rbx+%d]", 8 * offset);
            break;
        }
        case W_OP_LOCAL_SET: {
            ip += 2;
            int index = read_u16(block, ip - 1);
            struct local *local = &function->locals.items[index];
            int word_count = local->size;
            assert(word_count > 0);
            int offset = 1 + function->max_for_loop_level + local->offset + word_count - 1;
            asm_write_inst2f(assembly, "mov", "[rbx+%d]", "rdx", 8 * offset--);
            if (word_count >= 2) {
                asm_write_inst2f(assembly, "mov", "[rbx+%d]", "rax", 8 * offset--);
            }
            for (int i = 0; i < word_count - 2; ++i, --offset) {
                asm_write_inst1f(assembly, "pop", "qword [rbx+%d]", 8 * offset);
            }
            if (word_count == 1) {
                asm_write_inst2(assembly, "mov", "rdx", "rax");
            }
            else {
                asm_write_inst1(assembly, "pop", "rdx");
            }
            asm_write_inst1(assembly, "pop", "rax");
            break;
        }
        case W_OP_LOWER_SAME:
            asm_write_inst2(assembly, "cmp", "rax", "rdx");
            asm_write_inst1(assembly, "setbe", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_LOWER_THAN:
            asm_write_inst2(assembly, "cmp", "rax", "rdx");
            asm_write_inst1(assembly, "setb", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_MULT:
            asm_write_inst2(assembly, "imul", "rdx", "rax");  // Multiplication is commutative.
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_MULTF32:
            asm_write_inst2(assembly, "movd", "xmm0", "eax");
            asm_write_inst2(assembly, "movd", "xmm1", "edx");
            asm_write_inst2(assembly, "mulss", "xmm0", "xmm1");
            asm_write_inst2(assembly, "movd", "edx", "xmm0");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_MULTF64:
            asm_write_inst2(assembly, "movq", "xmm0", "rax");
            asm_write_inst2(assembly, "movq", "xmm1", "rdx");
            asm_write_inst2(assembly, "mulsd", "xmm0", "xmm1");
            asm_write_inst2(assembly, "movq", "rdx", "xmm0");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_NEG:
            asm_write_inst1(assembly, "neg", "rdx");
            break;
        case W_OP_NEGF32:
            asm_write_inst2(assembly, "mov", "rcx", "8000'0000h");
            asm_write_inst2(assembly, "xor", "rdx", "rcx");
            break;
        case W_OP_NEGF64:
            asm_write_inst2(assembly, "mov", "rcx", "8000'0000'0000'0000h");
            asm_write_inst2(assembly, "xor", "rdx", "rcx");
            break;
        case W_OP_NOT:
            asm_write_inst2(assembly, "test", "rdx", "rdx");
            asm_write_inst1(assembly, "setz", "dl");
            asm_write_inst2(assembly, "movzx", "edx", "dl");
            break;
        case W_OP_NOT_EQUALS:
            asm_write_inst2(assembly, "cmp", "rax", "rdx");
            asm_write_inst1(assembly, "setne", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_NOT_EQUALS_F32:
            asm_write_inst2(assembly, "movd", "xmm1", "edx");
            asm_write_inst2(assembly, "movd", "xmm0", "eax");
            asm_write_inst2(assembly, "ucomiss", "xmm0", "xmm1");
            asm_write_inst1(assembly, "setne", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_NOT_EQUALS_F64:
            asm_write_inst2(assembly, "movq", "xmm1", "rdx");
            asm_write_inst2(assembly, "movq", "xmm0", "rax");
            asm_write_inst2(assembly, "ucomisd", "xmm0", "xmm1");
            asm_write_inst1(assembly, "setne", "al");
            asm_write_inst2(assembly, "movzx", "edx", "al");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_OR:
            asm_write_inst2(assembly, "test", "rax", "rax");
            asm_write_inst2(assembly, "cmovnz", "rdx", "rax");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_PRINT:
            asm_write_inst2(assembly, "mov", "r12", "rax");
            // NOTE: rdx already holds word to be printed (2nd printf arg).
            asm_write_inst2(assembly, "lea", "rcx", "[fmt_u64]");
            asm_write_inst2(assembly, "mov", "rbp", "rsp");
            asm_write_inst2(assembly, "and", "spl", "0F0h");
            asm_write_inst2(assembly, "sub", "rsp", "32\t");
            asm_write_inst1(assembly, "call", "[printf]");
            asm_write_inst2(assembly, "mov", "rsp", "rbp");
            asm_write_inst2(assembly, "mov", "rdx", "r12");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_PRINT_BOOL:
            asm_write_inst2(assembly, "mov", "r12", "rax");
            asm_write_inst2(assembly, "lea", "rcx", "[fmt_bool_false]");
            asm_write_inst2(assembly, "lea", "rax", "[fmt_bool_true]");
            asm_write_inst2(assembly, "test", "rdx", "rdx");
            asm_write_inst2(assembly, "cmovnz", "rcx", "rax");
            asm_write_inst2(assembly, "mov", "rbp", "rsp");
            asm_write_inst2(assembly, "and", "spl", "0F0h");
            asm_write_inst2(assembly, "sub", "rsp", "32");
            asm_write_inst1(assembly, "call", "[printf]");
            asm_write_inst2(assembly, "mov", "rsp", "rbp");
            asm_write_inst2(assembly, "mov", "rdx", "r12");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_PRINT_CHAR:
            // NOTE: We treat a 'char' as an array of 4 bytes and print it as a string.
            // We get the null terminator for free since the upper 4 bytes will be zero,
            // as will any unused bytes in the UTF-8 encoding.
            asm_write_inst2(assembly, "mov", "r12", "rax");
            asm_write_inst2(assembly, "mov", "[char_print_buf]", "rdx");
            asm_write_inst2(assembly, "lea", "rdx", "[char_print_buf]");
            asm_write_inst2(assembly, "lea", "rcx", "[fmt_char]");
            asm_write_inst2(assembly, "mov", "rbp", "rsp");
            asm_write_inst2(assembly, "and", "spl", "0F0h");
            asm_write_inst2(assembly, "sub", "rsp", "32");
            asm_write_inst1(assembly, "call", "[printf]");
            asm_write_inst2(assembly, "mov", "rsp", "rbp");
            asm_write_inst2(assembly, "mov", "rdx", "r12");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_PRINT_STRING:
            // Length field already in rdx.
            asm_write_inst2(assembly, "mov", "r8", "rax");
            asm_write_inst2(assembly, "lea", "rcx", "[fmt_string]");
            asm_write_inst2(assembly, "mov", "rbp", "rsp");
            asm_write_inst2(assembly, "and", "spl", "0F0h");
            asm_write_inst2(assembly, "sub", "rsp", "32");
            asm_write_inst1(assembly, "call", "[printf]");
            asm_write_inst2(assembly, "mov", "rsp", "rbp");
            asm_write_inst1(assembly, "pop", "rdx");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_PRINT_FLOAT:
            asm_write_inst2(assembly, "mov", "r12", "rax");
            asm_write_inst2(assembly, "movq", "xmm1", "rdx");
            asm_write_inst2(assembly, "lea", "rcx", "[fmt_f64]");
            asm_write_inst2(assembly, "mov", "rbp", "rsp");
            asm_write_inst2(assembly, "and", "spl", "0F0h");
            asm_write_inst2(assembly, "sub", "rsp", "32");
            asm_write_inst1(assembly, "call", "[printf]");
            asm_write_inst2(assembly, "mov", "rsp", "rbp");
            asm_write_inst2(assembly, "mov", "rdx", "r12");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_PRINT_INT:
            asm_write_inst2(assembly, "mov", "r12", "rax");
            asm_write_inst2(assembly, "lea", "rcx", "[fmt_s64]");
            asm_write_inst2(assembly, "mov", "rbp", "rsp");
            asm_write_inst2(assembly, "and", "spl", "0F0h");
            asm_write_inst2(assembly, "sub", "rsp", "32");
            asm_write_inst1(assembly, "call", "[printf]");
            asm_write_inst2(assembly, "mov", "rsp", "rbp");
            asm_write_inst2(assembly, "mov", "rdx", "r12");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_SUB:
            // NOTE: we do a + (-b) instead of straight a - b.
            asm_write_inst1(assembly, "neg", "rdx");
            asm_write_inst2(assembly, "add", "rdx", "rax");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_SUBF32:
            asm_write_inst2(assembly, "movd", "xmm0", "eax");
            asm_write_inst2(assembly, "movd", "xmm1", "edx");
            asm_write_inst2(assembly, "subss", "xmm0", "xmm1");
            asm_write_inst2(assembly, "movd", "edx", "xmm0");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_SUBF64:
            asm_write_inst2(assembly, "movq", "xmm0", "rax");
            asm_write_inst2(assembly, "movq", "xmm1", "rax");
            asm_write_inst2(assembly, "subsd", "xmm0", "xmm1");
            asm_write_inst2(assembly, "movq", "rdx", "xmm0");
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_SWAP:
            // This could be replaced by 3 movs.
            asm_write_inst2(assembly, "xchg", "rdx", "rax");
            break;
        case W_OP_SWAP_COMPS8: {
            int lhs_size = read_s8(block, ip + 1);
            int rhs_size = read_s8(block, ip + 2);
            ip += 2;
            generate_swap_comps(generator, lhs_size, rhs_size);
            break;
        }
        case W_OP_SWAP_COMPS16: {
            int lhs_size = read_s16(block, ip + 1);
            int rhs_size = read_s16(block, ip + 2);
            ip += 4;
            generate_swap_comps(generator, lhs_size, rhs_size);
            break;
        }
        case W_OP_SWAP_COMPS32: {
            int lhs_size = read_s32(block, ip + 1);
            int rhs_size = read_s32(block, ip + 2);
            ip += 8;
            generate_swap_comps(generator, lhs_size, rhs_size);
            break;
        }
        case W_OP_SX8:
            asm_write_inst2(assembly, "movsx", "rdx", "dl");
            break;
        case W_OP_SX8L:
            asm_write_inst2(assembly, "movsx", "rax", "al");
            break;
        case W_OP_SX16:
            asm_write_inst2(assembly, "movsx", "rdx", "dx");
            break;
        case W_OP_SX16L:
            asm_write_inst2(assembly, "movsx", "rax", "ax");
            break;
        case W_OP_SX32:
            asm_write_inst2(assembly, "movsxd", "rdx", "edx");
            break;
        case W_OP_SX32L:
            asm_write_inst2(assembly, "movsxd", "rax", "eax");
            break;
        case W_OP_ZX8:
            asm_write_inst2(assembly, "movzx", "edx", "dl");
            break;
        case W_OP_ZX8L:
            asm_write_inst2(assembly, "movzx", "eax", "al");
            break;
        case W_OP_ZX16:
            asm_write_inst2(assembly, "movzx", "edx", "dx");
            break;
        case W_OP_ZX16L:
            asm_write_inst2(assembly, "movzx", "eax", "ax");
            break;
        case W_OP_ZX32:
            asm_write_inst2(assembly, "mov", "edx", "edx");
            break;
        case W_OP_ZX32L:
            asm_write_inst2(assembly, "mov", "eax", "eax");
            break;
        case W_OP_FPROM:
            asm_write_inst2(assembly, "movd", "xmm2", "edx");
            asm_write_inst2(assembly, "cvtss2sd", "xmm1", "xmm2");
            asm_write_inst2(assembly, "movq", "rdx", "xmm1");
            break;
        case W_OP_FPROML:
            asm_write_inst2(assembly, "movd", "xmm2", "edx");
            asm_write_inst2(assembly, "cvtss2sd", "xmm1", "xmm2");
            asm_write_inst2(assembly, "movq", "rdx", "xmm1");
            break;
        case W_OP_FDEM:
            asm_write_inst2(assembly, "movq", "xmm2", "rdx");
            asm_write_inst2(assembly, "cvtsd2ss", "xmm1", "xmm2");
            asm_write_inst2(assembly, "movd", "edx", "xmm1");
            break;
        case W_OP_ICONVF32:
            asm_write_inst2(assembly, "cvtsi2ss", "xmm0", "rdx");
            asm_write_inst2(assembly, "movd", "edx", "xmm0");
            break;
        case W_OP_ICONVF32L:
            asm_write_inst2(assembly, "cvtsi2ss", "xmm0", "rax");
            asm_write_inst2(assembly, "movd", "eax", "xmm0");
            break;
        case W_OP_ICONVF64:
            asm_write_inst2(assembly, "cvtsi2sd", "xmm0", "rdx");
            asm_write_inst2(assembly, "movq", "rdx", "xmm0");
            break;
        case W_OP_ICONVF64L:
            asm_write_inst2(assembly, "cvtsi2sd", "xmm0", "rax");
            asm_write_inst2(assembly, "movd", "rax", "xmm0");
            break;
        case W_OP_FCONVI32:
            asm_write_inst2(assembly, "movd", "xmm0", "edx");
            asm_write_inst2(assembly, "cvtss2si", "rdx", "xmm0");
            break;
        case W_OP_FCONVI64:
            asm_write_inst2(assembly, "movq", "xmm0", "rdx");
            asm_write_inst2(assembly, "cvtsd2si", "rdx", "xmm0");
            break;
        case W_OP_ICONVB:
            asm_write_inst2(assembly, "test", "rdx", "rdx");
            asm_write_inst1(assembly, "setnz", "dl");
            asm_write_inst2(assembly, "movzx", "edx", "dl");
            break;
        case W_OP_FCONVB32:
            asm_write_inst2(assembly, "xor", "ecx", "ecx");  // 0f32.
            asm_write_inst2(assembly, "movd", "xmm0", "eax");
            asm_write_inst2(assembly, "movd", "xmm1", "ecx");
            asm_write_inst2(assembly, "ucomiss", "xmm0", "xmm1");
            asm_write_inst1(assembly, "setne", "dl");  // Also handles unordered case.
            asm_write_inst2(assembly, "movzx", "edx", "dl");
            break;
        case W_OP_FCONVB64:
            asm_write_inst2(assembly, "xor", "ecx", "ecx");  // 0f64.
            asm_write_inst2(assembly, "movq", "xmm0", "rax");
            asm_write_inst2(assembly, "movq", "xmm1", "rcx");
            asm_write_inst2(assembly, "ucomisd", "xmm0", "xmm1");
            asm_write_inst1(assembly, "setne", "dl");  // Also handles unordered case.
            asm_write_inst2(assembly, "movzx", "edx", "dl");
            break;
        case W_OP_ICONVC32:
            // 10ffffh is the highest Unicode codepoint
            asm_write_inst2(assembly, "xor", "ecx", "ecx");
            asm_write_inst2(assembly, "test", "rdx", "rdx");
            asm_write_inst2(assembly, "cmovs", "edx", "ecx");
            asm_write_inst2(assembly, "mov", "rcx", "10ffffh");
            asm_write_inst2(assembly, "cmp", "edx", "ecx");
            asm_write_inst2(assembly, "cmova", "rdx", "rcx");
            break;
        case W_OP_CHAR_8CONV32:
            // NOTE: We use the Bude calling convention here since this is an internal function.
            asm_write_inst1(assembly, "call", "decode_utf8");
            break;
        case W_OP_CHAR_32CONV8:
            // NOTE: As above, we use the Bude calling convention here.
            asm_write_inst1(assembly, "call", "encode_utf8");
            break;
        case W_OP_CHAR_16CONV32:
            asm_write_inst1(assembly, "call", "decode_utf16");
            break;
        case W_OP_CHAR_32CONV16:
            asm_write_inst1(assembly, "call", "encode_utf16");
            break;
        case W_OP_PACK1:
            ++ip;
            break;
        case W_OP_PACK2: {
            uint8_t sizes[] = {
                read_u8(block, ip + 1),
                read_u8(block, ip + 2),
            };
            ip += 2;
            generate_pack_instruction(generator, 2, sizes);
            break;
        }
        case W_OP_PACK3: {
            uint8_t sizes[] = {
                read_u8(block, ip + 1),
                read_u8(block, ip + 2),
                read_u8(block, ip + 3),
            };
            ip += 3;
            generate_pack_instruction(generator, 3, sizes);
            break;
        }
        case W_OP_PACK4: {
            uint8_t sizes[] = {
                read_u8(block, ip + 1),
                read_u8(block, ip + 2),
                read_u8(block, ip + 3),
                read_u8(block, ip + 4),
            };
            ip += 4;
            generate_pack_instruction(generator, 4, sizes);
            break;
        }
        case W_OP_PACK5: {
            uint8_t sizes[] = {
                read_u8(block, ip + 1),
                read_u8(block, ip + 2),
                read_u8(block, ip + 3),
                read_u8(block, ip + 4),
                read_u8(block, ip + 5),
            };
            ip += 5;
            generate_pack_instruction(generator, 5, sizes);
            break;
        }
        case W_OP_PACK6: {
            uint8_t sizes[] = {
                read_u8(block, ip + 1),
                read_u8(block, ip + 2),
                read_u8(block, ip + 3),
                read_u8(block, ip + 4),
                read_u8(block, ip + 5),
                read_u8(block, ip + 6),
            };
            ip += 6;
            generate_pack_instruction(generator, 6, sizes);
            break;
        }
        case W_OP_PACK7: {
            uint8_t sizes[] = {
                read_u8(block, ip + 1),
                read_u8(block, ip + 2),
                read_u8(block, ip + 3),
                read_u8(block, ip + 4),
                read_u8(block, ip + 5),
                read_u8(block, ip + 6),
                read_u8(block, ip + 7),
            };
            ip += 7;
            generate_pack_instruction(generator, 7, sizes);
            break;
        }
        case W_OP_PACK8: {
            uint8_t sizes[] = {
                read_u8(block, ip + 1),
                read_u8(block, ip + 2),
                read_u8(block, ip + 3),
                read_u8(block, ip + 4),
                read_u8(block, ip + 5),
                read_u8(block, ip + 6),
                read_u8(block, ip + 7),
                read_u8(block, ip + 8),
            };
            ip += 8;
            generate_pack_instruction(generator, 8, sizes);
            break;
        }
        case W_OP_UNPACK1:
            ++ip;
            break;
        case W_OP_UNPACK2: {
            uint8_t sizes[] = {
                read_u8(block, ip + 1),
                read_u8(block, ip + 2),
            };
            ip += 2;
            generate_unpack_instruction(generator, 2, sizes);
            break;
        }
        case W_OP_UNPACK3: {
            uint8_t sizes[] = {
                read_u8(block, ip + 1),
                read_u8(block, ip + 2),
                read_u8(block, ip + 3),
            };
            ip += 3;
            generate_unpack_instruction(generator, 3, sizes);
            break;
        }
        case W_OP_UNPACK4: {
            uint8_t sizes[] = {
                read_u8(block, ip + 1),
                read_u8(block, ip + 2),
                read_u8(block, ip + 3),
                read_u8(block, ip + 4),
            };
            ip += 4;
            generate_unpack_instruction(generator, 4, sizes);
            break;
        }
        case W_OP_UNPACK5: {
            uint8_t sizes[] = {
                read_u8(block, ip + 1),
                read_u8(block, ip + 2),
                read_u8(block, ip + 3),
                read_u8(block, ip + 4),
                read_u8(block, ip + 5),
            };
            ip += 5;
            generate_unpack_instruction(generator, 5, sizes);
            break;
        }
        case W_OP_UNPACK6: {
            uint8_t sizes[] = {
                read_u8(block, ip + 1),
                read_u8(block, ip + 2),
                read_u8(block, ip + 3),
                read_u8(block, ip + 4),
                read_u8(block, ip + 5),
                read_u8(block, ip + 6),
            };
            ip += 6;
            generate_unpack_instruction(generator, 6, sizes);
            break;
        }
        case W_OP_UNPACK7: {
            uint8_t sizes[] = {
                read_u8(block, ip + 1),
                read_u8(block, ip + 2),
                read_u8(block, ip + 3),
                read_u8(block, ip + 4),
                read_u8(block, ip + 5),
                read_u8(block, ip + 6),
                read_u8(block, ip + 7),
            };
            ip += 7;
            generate_unpack_instruction(generator, 7, sizes);
            break;
        }
        case W_OP_UNPACK8: {
            uint8_t sizes[] = {
                read_u8(block, ip + 1),
                read_u8(block, ip + 2),
                read_u8(block, ip + 3),
                read_u8(block, ip + 4),
                read_u8(block, ip + 5),
                read_u8(block, ip + 6),
                read_u8(block, ip + 7),
                read_u8(block, ip + 8),
            };
            ip += 8;
            generate_unpack_instruction(generator, 8, sizes);
            break;
        }
        case W_OP_PACK_FIELD_GET: {
            int offset = read_s8(block, ip + 1);
            int size = read_s8(block, ip + 2);
            ip += 2;
            generate_pack_field_get(generator, offset, size);
            break;
        }
        case W_OP_COMP_FIELD_GET8: {
            int offset = read_s8(block, ip + 1);
            ip += 1;
            generate_comp_field_get(generator, offset);
            break;
        }
        case W_OP_COMP_FIELD_GET16: {
            int offset = read_s16(block, ip + 1);
            ip += 2;
            generate_comp_field_get(generator, offset);
            break;
        }
        case W_OP_COMP_FIELD_GET32: {
            int offset = read_s32(block, ip + 1);
            ip += 4;
            generate_comp_field_get(generator, offset);
            break;
        }
        case W_OP_PACK_FIELD_SET: {
            int offset = read_s8(block, ip + 1);
            int size = read_s8(block, ip + 2);
            ip += 2;
            generate_pack_field_set(generator, offset, size);
            break;
        }
        case W_OP_COMP_FIELD_SET8: {
            int offset = read_s8(block, ip + 1);
            ip += 1;
            generate_comp_field_set(generator, offset);
            break;
        }
        case W_OP_COMP_FIELD_SET16: {
            int offset = read_s16(block, ip + 1);
            ip += 2;
            generate_comp_field_set(generator, offset);
            break;
        }
        case W_OP_COMP_FIELD_SET32: {
            int offset = read_s32(block, ip + 1);
            ip += 4;
            generate_comp_field_set(generator, offset);
            break;
        }
        case W_OP_COMP_SUBCOMP_GET8: {
            int offset = read_s8(block, ip + 1);
            int size = read_s8(block, ip + 2);
            ip += 2;
            generate_subcomp_get(generator, offset, size);
            break;
        }
        case W_OP_COMP_SUBCOMP_GET16: {
            int offset = read_s16(block, ip + 1);
            int size = read_s16(block, ip + 3);
            ip += 4;
            generate_subcomp_get(generator, offset, size);
            break;
        }
        case W_OP_COMP_SUBCOMP_GET32: {
            int offset = read_s32(block, ip + 1);
            int size = read_s32(block, ip + 5);
            ip += 8;
            generate_subcomp_get(generator, offset, size);
            break;
        }
        case W_OP_COMP_SUBCOMP_SET8: {
            int offset = read_s8(block, ip + 1);
            int size = read_s8(block, ip + 2);
            ip += 2;
            generate_subcomp_set(generator, offset, size);
            break;
        }
        case W_OP_COMP_SUBCOMP_SET16: {
            int offset = read_s16(block, ip + 1);
            int size = read_s16(block, ip + 3);
            ip += 4;
            generate_subcomp_set(generator, offset, size);
            break;
        }
        case W_OP_COMP_SUBCOMP_SET32: {
            int offset = read_s32(block, ip + 1);
            int size = read_s32(block, ip + 5);
            ip += 8;
            generate_subcomp_set(generator, offset, size);
            break;
        }
        case W_OP_ARRAY_GET8: {
            int element_count = read_u8(block, ip + 1);
            int word_count = read_u8(block, ip + 2);
            ip += 2;
            generate_array_get(generator, element_count, word_count);
            break;
        }
        case W_OP_ARRAY_GET16: {
            int element_count = read_u8(block, ip + 1);
            int word_count = read_u8(block, ip + 3);
            ip += 4;
            generate_array_get(generator, element_count, word_count);
            break;
        }
        case W_OP_ARRAY_GET32: {
            int element_count = read_u8(block, ip + 1);
            int word_count = read_u8(block, ip + 5);
            ip += 8;
            generate_array_get(generator, element_count, word_count);
            break;
        }
        case W_OP_ARRAY_SET8: {
            int element_count = read_u8(block, ip + 1);
            int word_count = read_u8(block, ip + 2);
            ip += 2;
            generate_array_set(generator, element_count, word_count);
            break;
        }
        case W_OP_ARRAY_SET16: {
            int element_count = read_u8(block, ip + 1);
            int word_count = read_u8(block, ip + 3);
            ip += 4;
            generate_array_set(generator, element_count, word_count);
            break;
        }
        case W_OP_ARRAY_SET32: {
            int element_count = read_u8(block, ip + 1);
            int word_count = read_u8(block, ip + 5);
            ip += 8;
            generate_array_set(generator, element_count, word_count);
            break;
        }
        case W_OP_CALL8: {
            int func_index = read_u8(block, ip + 1);
            ip += 1;
            generate_function_call(generator, func_index);
            break;
        }
        case W_OP_CALL16: {
            int func_index = read_u16(block, ip + 1);
            ip += 2;
            generate_function_call(generator, func_index);
            break;
        }
        case W_OP_CALL32: {
            int func_index = read_u32(block, ip + 1);
            ip += 4;
            generate_function_call(generator, func_index);
            break;
        }
        case W_OP_EXTCALL8: {
            int extfunc_index = read_u8(block, ip + 1);
            ip += 1;
            struct ext_function *external = \
                get_external(&generator->module->externals, extfunc_index);
            generate_external_call(generator, external);
            break;
        }
        case W_OP_EXTCALL16: {
            int extfunc_index = read_u16(block, ip + 1);
            ip += 2;
            struct ext_function *external = \
                get_external(&generator->module->externals, extfunc_index);
            generate_external_call(generator, external);
            break;
        }
        case W_OP_EXTCALL32: {
            int extfunc_index = read_u32(block, ip + 1);
            ip += 4;
            struct ext_function *external = \
                get_external(&generator->module->externals, extfunc_index);
            generate_external_call(generator, external);
            break;
        }
        case W_OP_RET:
            generate_function_return(generator);
            break;
        }
    }
}

static void generate_decode_utf8(struct generator *generator) {
    struct asm_block *assembly = generator->assembly;
    asm_label(assembly, "decode_utf8");
    asm_write_inst1c(assembly, "pop", "rbp", "Return address.");
    asm_write_inst2(assembly, "mov", "r8", "rdx");
    asm_write_inst2(assembly, "shr", "r8", "8");
    asm_write_inst2(assembly, "movzx", "edx", "dl");
    asm_write_inst2(assembly, "test", "dl", "dl");
    // 1 byte: jump to end.
    asm_write_inst1(assembly, "jns", ".func_end");
    // 2+ bytes.
    asm_write_inst2c(assembly, "mov", "ecx", "1", "Number of continuation bytes.");
    // CF is set/cleared based on the last bit shifted.
    // For multibyte UTF-8, the first byte has a prefix of at least 3 bits
    // (110, 1110, 11110 for 1, 2, and 3 continuation bytes).
    // By left-shifting dl by 3 bits, we can see if there is one continuation
    // byte (CF=0) or more (CF=1). If we had more continuation bytes, we keep left-shifting
    // by 1 until CF is cleared. We assume the UTF-8 bytes are well-formed for this.
    asm_write_inst2(assembly, "shl", "dl", "3");
    // 2 bytes: handle continuation bytes.
    asm_write_inst1(assembly, "jnc", ".start_cont_bytes");
    asm_write_inst1(assembly, "inc", "ecx");
    asm_write_inst2(assembly, "shl", "dl", "1");
    // 3 bytes: handle continuation bytes.
    asm_write_inst1(assembly, "jnc", ".start_cont_bytes");
    asm_write_inst1(assembly, "inc", "ecx");
    // 4 bytes: discard final 0 in 'header'.
    asm_write_inst2(assembly, "shl", "dl", "1");
    asm_label(assembly, ".start_cont_bytes");
    asm_write_inst2(assembly, "shr", "dl", "cl");
    asm_write_inst2(assembly, "shr", "dl", "2");
    asm_write_inst2(assembly, "movzx", "edx", "dl");  // Avoid partial register stall.
    asm_label(assembly, ".cont_bytes");
    asm_write_inst2(assembly, "shl", "edx", "6");
    asm_write_inst2(assembly, "mov", "r9", "r8");
    asm_write_inst2(assembly, "and", "r9", "3fh");  // Keep only the last 6 bits.
    asm_write_inst2(assembly, "xor", "edx", "r9d");
    asm_write_inst2(assembly, "shr", "r8", "8");
    asm_write_inst1(assembly, "dec", "ecx");
    asm_write_inst1(assembly, "jnz", ".cont_bytes");
    asm_label(assembly, ".func_end");
    asm_write_inst1(assembly, "push", "rbp");
    asm_write_inst0(assembly, "ret");
}

static void generate_encode_utf8(struct generator *generator) {
    struct asm_block *assembly = generator->assembly;
    asm_label(assembly, "encode_utf8");
    asm_write_inst1c(assembly, "pop", "rbp", "Return address.");
    asm_write_inst2(assembly, "mov", "r8", "rdx");
    asm_write_inst2(assembly, "movzx", "edx", "dl");
    asm_write_inst2(assembly, "cmp", "r8", "80h");
    asm_write_inst1(assembly, "jl", ".func_end");
    asm_write_inst2c(assembly, "mov", "ecx", "1", "Number of continuation bytes.");
    asm_write_inst2c(assembly, "mov", "r9", "-0e1h", "First byte prefix mask.");
    asm_write_inst2(assembly, "cmp", "r8", "800h");
    asm_write_inst1(assembly, "jl", ".cont_bytes");
    asm_write_inst1(assembly, "inc", "ecx");
    asm_write_inst2(assembly, "shr", "r9", "1");
    asm_write_inst2(assembly, "cmp", "r8", "10000h");
    asm_write_inst1(assembly, "jl", ".cont_bytes");
    asm_write_inst1(assembly, "inc", "ecx");
    asm_write_inst2(assembly, "shr", "r9", "1");
    asm_label(assembly, ".cont_bytes");
    asm_write_inst2(assembly, "and", "edx", "-0c1h");  // Mask off first two bits (-0c1h = 11...100111111b).
    asm_write_inst2(assembly, "xor", "edx", "80h");   // Set first 2 bits to '10'.
    asm_write_inst2(assembly, "shl", "edx", "8");
    asm_write_inst2(assembly, "shr", "r8", "6");
    asm_write_inst2(assembly, "movzx", "r10", "r8b");
    asm_write_inst2(assembly, "xor", "rdx", "r10");  // Move next byte into rdx.
    asm_write_inst1(assembly, "dec", "ecx");
    asm_write_inst1(assembly, "jnz", ".cont_bytes");
    // Add prefix to first byte.
    asm_write_inst2(assembly, "and", "rdx", "r9");
    asm_write_inst1(assembly, "not", "r9");
    asm_write_inst2(assembly, "shl", "r9", "1");
    asm_write_inst2(assembly, "movzx", "r9", "r9b");  // Clear upper bits of r9.
    asm_write_inst2(assembly, "xor", "rdx", "r9");
    asm_label(assembly, ".func_end");
    asm_write_inst1(assembly, "push", "rbp");
    asm_write_inst0(assembly, "ret");
}

static void generate_decode_utf16(struct generator *generator) {
    struct asm_block *assembly = generator->assembly;
    asm_label(assembly, "decode_utf16");
    asm_write_inst1c(assembly, "pop", "rbp", "Return address.");
    asm_write_inst2(assembly, "mov", "r8", "rdx");
    asm_write_inst2(assembly, "movzx", "edx", "dx");
    asm_write_inst2(assembly, "and", "r8", "-400h");
    asm_write_inst2(assembly, "cmp", "r8w", "0d800h");
    asm_write_inst1(assembly, "jne", ".func_end");
    // Surrogate pairs.
    asm_write_inst2(assembly, "sub", "edx", "0d800h");
    asm_write_inst2(assembly, "shl", "edx", "10");
    asm_write_inst2(assembly, "shr", "r8", "16");
    // NOTE: We don't check to make sure the second unit is a low surrogate.
    asm_write_inst2(assembly, "and", "r8", "3ffh");
    asm_write_inst2(assembly, "xor", "rdx", "r8");
    asm_write_inst2(assembly, "add", "edx", "10000h");  // Convert complement to codepoint.
    asm_label(assembly, ".func_end");
    asm_write_inst1(assembly, "push", "rbp");
    asm_write_inst0(assembly, "ret");
}

static void generate_encode_utf16(struct generator *generator) {
    struct asm_block *assembly = generator->assembly;
    asm_label(assembly, "encode_utf16");
    asm_write_inst1c(assembly, "pop", "rbp", "Return address.");
    asm_write_inst2(assembly, "mov", "r8", "rdx");
    asm_write_inst2(assembly, "sub", "r8", "10000h");
    asm_write_inst1(assembly, "jl", ".func_end");
    // Need surrogate pairs.
    // eax now contains the complement.
    asm_write_inst2(assembly, "movzx", "edx", "r8w");
    asm_write_inst2(assembly, "and", "edx", "3ffh");  // Clear high bits of edx (not just dx).
    asm_write_inst2(assembly, "xor", "edx", "0dc00h");  // Low surrogate.
    asm_write_inst2(assembly, "shl", "edx", "16");
    asm_write_inst2(assembly, "shr", "r8", "10");
    asm_write_inst2(assembly, "xor", "r8", "0d800h");  // High surrogate.
    asm_write_inst2(assembly, "xor", "rdx", "r8");  // Combine surrogates.
    asm_label(assembly, ".func_end");
    asm_write_inst1(assembly, "push", "rbp");
    asm_write_inst0(assembly, "ret");
}

void generate_code(struct generator *generator) {
    struct asm_block *assembly = generator->assembly;
    asm_section(assembly, ".code", "code", "readable", "executable");
    asm_write(assembly, "\n");
    asm_label(assembly, "start");
    asm_write(assembly, "\n");
    asm_write(assembly, "  ;;\t=== INITIALISATION ===\n");
    // Global registers.
    asm_write_inst2c(assembly, "lea", "rsi", "[aux]", "Auxiliary stack pointer.");
    asm_write_inst2c(assembly, "mov", "rbx", "rsi", "Auxiliary base pointer.");
    asm_write_inst2c(assembly, "xor", "rdi", "rdi", "Loop counter.");
    // Entry point.
    asm_write(assembly, "  ;;\t=== ENTRY POINT ===\n");
    generate_function_call(generator, 0);
    // End.
    asm_write(assembly, "  ;;\t=== END ===\n");
    asm_write_inst2c(assembly, "xor", "rcx", "rcx", "Successful exit.");
    asm_write_inst2(assembly, "and", "spl", "0F0h");
    asm_write_inst2(assembly, "sub", "rsp", "32");
    asm_write_inst1(assembly, "call", "[ExitProcess]");
    asm_write(assembly, "\n");
    // Built in functions.
    generate_decode_utf8(generator);
    generate_encode_utf8(generator);
    generate_decode_utf16(generator);
    generate_encode_utf16(generator);
    // Functions.
    for (int i = 0; i < generator->module->functions.count; ++i) {
        generate_function(generator, i);
    }
}

static void generate_imports(struct generator *generator) {
    struct asm_block *assembly = generator->assembly;
    asm_section(assembly, ".idata", "import", "data", "readable");
    asm_write(assembly, "\n");
    asm_write(assembly, "  library\\\n");
    asm_write(assembly, "\tkernel, 'kernel32.dll',\\\n");
    asm_write(assembly, "\tmsvcrt, 'msvcrt.dll'");
    struct ext_lib_table *libraries = &generator->module->ext_libraries;
    for (int i = 0; i < libraries->count; ++i) {
        struct ext_library *lib = &libraries->items[i];
        asm_write(assembly, ",\\\n\textlib_%d, '%"PRI_SV"'", i, SV_FMT(lib->filename));
    }
    asm_write(assembly, "\n\n");
    asm_write(assembly, "  import msvcrt,\\\n");
    asm_write(assembly, "\tprintf, 'printf'\n");
    asm_write(assembly, "\n");
    asm_write(assembly, "  import kernel,\\\n");
    asm_write(assembly, "\tExitProcess, 'ExitProcess'\n");
    asm_write(assembly, "\n");
    for (int i = 0; i < libraries->count; ++i) {
        struct ext_library *lib = &libraries->items[i];
        assert(lib->count > 0);
        asm_write(assembly, "  import extlib_%d", i);
        for (int j = 0; j < lib->count; ++j) {
            int ext_index = lib->items[j];
            struct ext_function *external = &generator->module->externals.items[ext_index];
            asm_write(assembly, ",\\\n\t%"PRI_SV", '%"PRI_SV"'",
                      SV_FMT(external->name), SV_FMT(external->name));
        }
        asm_write(assembly, "\n\n");
    }
}

static void generate_constants(struct generator *generator) {
    struct asm_block *assembly = generator->assembly;
    asm_section(assembly, ".rdata", "data", "readable");
    asm_write(assembly, "\n");
    asm_label(assembly, "fmt_s64");
    asm_write_inst2c(assembly, "db", "'%%I64d'", "0",
                     "NOTE: I64 is a Non-ISO Microsoft extension.");
    asm_write(assembly, "\n");
    asm_label(assembly, "fmt_u64");
    asm_write_inst2(assembly, "db", "'%%I64u'", "0");
    asm_write(assembly, "\n");
    asm_label(assembly, "fmt_f64");
    asm_write_inst2(assembly, "db", "'%%g'", "0");
    asm_write(assembly, "\n");
    asm_label(assembly, "fmt_bool_false");
    asm_write_inst2(assembly, "db", "'false'", "0");
    asm_write(assembly, "\n");
    asm_label(assembly, "fmt_bool_true");
    asm_write_inst2(assembly, "db", "'true'", "0");
    asm_write(assembly, "\n");
    asm_label(assembly, "fmt_char");
    asm_write_inst2(assembly, "db", "'%%s'", "0");
    asm_write(assembly, "\n");
    asm_label(assembly, "fmt_string");
    asm_write_inst2(assembly, "db", "'%%.*s'", "0");
    asm_write(assembly, "\n");
    struct string_table *strings = &generator->module->strings;
    for (int i = 0; i < strings->count; ++i) {
        asm_label(assembly, "str%u", i);
        asm_write(assembly, "\tdb\t");
        asm_write_sv(assembly, &strings->items[i]);
        asm_write(assembly, "\n\n");
    }
}

static void generate_bss(struct generator *generator) {
    struct asm_block *assembly = generator->assembly;
    asm_section(assembly, ".bss", "data", "readable", "writeable");
    asm_label(assembly, "char_print_buf");
    asm_write_inst1(assembly, "rq", "1");
    asm_label(assembly, "aux");
    asm_write_inst1(assembly, "rq", "1024*1024");
}

enum generate_result generate(struct module *module, struct asm_block *assembly) {
    struct generator generator = {
        .assembly = assembly,
        .module = module,
        .loop_level = 0,
    };
    generate_header(&generator);
    generate_code(&generator);
    generate_constants(&generator);
    generate_imports(&generator);
    generate_bss(&generator);
    return (!asm_had_error(generator.assembly)) ? GENERATE_OK : GENERATE_ERROR;
}
