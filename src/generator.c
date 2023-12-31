#include <assert.h>
#include <stdint.h>
#include <inttypes.h>

#include "asm.h"
#include "generator.h"
#include "ir.h"


void generate_header(struct asm_block *assembly) {
    asm_write(assembly, "format PE64 console\n");
    asm_write(assembly, "include 'win64ax.inc'\n");
    asm_write(assembly, "\n");
}

void generate_code(struct asm_block *assembly, struct ir_block *block) {
#define BIN_OP(OP)                                                      \
    do {                                                                \
        asm_write_inst1c(assembly, "pop", "rdx", "RHS.");               \
        asm_write_inst2c(assembly, OP, "[rsp]", "rdx", "LHS left on stack."); \
    } while (0)

    asm_section(assembly, ".code", "code", "readable", "executable");
    asm_write(assembly, "\n");
    asm_label(assembly, "start");
    asm_write(assembly, "\n");
    asm_write(assembly, "  ;;\tInitialisation.\n");
    // Global registers.
    asm_write_inst2c(assembly, "lea", "rsi", "[aux]", "Loop stack pointer.");
    asm_write_inst2cf(assembly, "lea", "rbx", "[rsi + %zu*8]",
                      "Auxiliary stack pointer (space reserved for loop stack).",
                      block->max_for_loop_level);
    asm_write_inst2c(assembly, "xor", "rdi", "rdi", "Loop counter.");
    // Instructions.
    for (int ip = 0; ip < block->count; ++ip) {
        if (is_jump_dest(block, ip)) {
            // We need a label.
            asm_label(assembly, "addr_%d", ip);
        }
        enum w_opcode instruction = block->code[ip];
        if (instruction == W_OP_NOP) continue;
        asm_write(assembly, "  ;;\t=== %s ===\n", get_opcode_name(instruction));
        switch (instruction) {
        case W_OP_NOP:
            // Do nothing.
            break;
        case W_OP_PUSH8: {
            ++ip;
            int8_t value = read_u8(block, ip);
            asm_write_inst2f(assembly, "mov", "rax", "%"PRIu8, value);
            asm_write_inst1(assembly, "push", "rax");
            break;
        }
        case W_OP_PUSH16: {
            ip += 2;
            int16_t value = read_u16(block, ip - 1);
            asm_write_inst2f(assembly, "mov", "rax", "%"PRIu16, value);
            asm_write_inst1(assembly, "push", "rax");
            break;
        }
        case W_OP_PUSH32: {
            ip += 4;
            int32_t value = read_u32(block, ip - 3);
            asm_write_inst2f(assembly, "mov", "rax", "%"PRIu32, value);
            asm_write_inst1(assembly, "push", "rax");
            break;
        }
        case W_OP_PUSH64: {
            ip += 8;
            int64_t value = read_u64(block, ip - 7);
            asm_write_inst2f(assembly, "mov", "rax", "%"PRIu64, value);
            asm_write_inst1(assembly, "push", "rax");
            break;
        }
        case W_OP_PUSH_INT8: {
            ++ip;
            int8_t value = read_s8(block, ip);
            asm_write_inst2f(assembly, "mov", "rax", "%"PRId8, value);
            asm_write_inst1(assembly, "push", "rax");
            break;
        }
        case W_OP_PUSH_INT16: {
            ip += 2;
            int16_t value = read_s16(block, ip - 1);
            asm_write_inst2f(assembly, "mov", "rax", "%"PRId16, value);
            asm_write_inst1(assembly, "push", "rax");
            break;
        }
        case W_OP_PUSH_INT32: {
            ip += 4;
            int32_t value = read_s32(block, ip - 3);
            asm_write_inst2f(assembly, "mov", "rax", "%"PRId32, value);
            asm_write_inst1(assembly, "push", "rax");
            break;
        }
        case W_OP_PUSH_INT64: {
            ip += 8;
            int64_t value = read_s64(block, ip - 7);
            asm_write_inst2f(assembly, "mov", "rax", "%"PRId64, value);
            asm_write_inst1(assembly, "push", "rax");
            break;
        }
        case W_OP_PUSH_CHAR8: {
            ++ip;
            uint8_t value = read_u8(block, ip);
            asm_write_inst2f(assembly, "mov", "rax", "%"PRIu8, value);
            asm_write_inst1(assembly, "push", "rax");
            break;
        }
        case W_OP_LOAD_STRING8: {
            ++ip;
            uint8_t index = read_u8(block, ip);
            asm_write_inst2f(assembly, "lea", "rax", "[str%"PRIu8"]", index);
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst1f(assembly, "push", "%u", read_string(block, index)->length);
            break;
        }
        case W_OP_LOAD_STRING16: {
            ip += 2;
            uint16_t index = read_u16(block, ip - 1);
            asm_write_inst2f(assembly, "lea", "rax", "[str%"PRIu16"]", index);
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst1f(assembly, "push", "%u", read_string(block, index)->length);
            break;
        }
        case W_OP_LOAD_STRING32: {
            ip += 4;
            uint32_t index = read_u32(block, ip - 3);
            asm_write_inst2f(assembly, "lea", "rax", "[str%"PRIu32"]", index);
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst1f(assembly, "push", "%u", read_string(block, index)->length);
            break;
        }
        case W_OP_POP:
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case W_OP_ADD:
            BIN_OP("add");
            break;
        case W_OP_AND:
            asm_write_inst1c(assembly, "pop", "rdx", "'Then' value.");
            asm_write_inst2c(assembly, "mov", "rax", "[rsp]", "'Else' value.");
            asm_write_inst2(assembly, "test", "rax", "rax");
            asm_write_inst2(assembly, "cmovnz", "rax", "rdx");
            asm_write_inst2(assembly, "mov", "[rsp]", "rax");
            break;
        case W_OP_DEREF:
            asm_write_inst1(assembly, "pop", "rax");
            asm_write_inst2(assembly, "movzx", "rdx", "byte [rax]");
            asm_write_inst1(assembly, "push", "rdx");
            break;
        case W_OP_DIVMOD:
            asm_write_inst1c(assembly, "pop", "rcx", "Divisor.");
            asm_write_inst1c(assembly, "pop", "rax", "Dividend.");
            asm_write_inst2c(assembly, "xor", "rdx", "rdx", "Zero out extra bytes in dividend.");
            asm_write_inst1(assembly, "div", "rcx");
            asm_write_inst1c(assembly, "push", "rax", "Quotient.");
            asm_write_inst1c(assembly, "push", "rdx", "Remainder.");
            break;
        case W_OP_IDIVMOD:
            asm_write_inst1c(assembly, "pop", "rcx", "Divisor.");
            asm_write_inst1c(assembly, "pop", "rax", "Dividend.");
            asm_write_inst2(assembly, "xor", "rdx", "rdx");
            asm_write_inst1(assembly, "idiv", "rcx");
            asm_write_inst1c(assembly, "push", "rax", "Quotient.");
            asm_write_inst1c(assembly, "push", "rdx", "Remainder.");
            break;
        case W_OP_EDIVMOD:
            asm_write_inst1c(assembly, "pop", "rcx", "Divisor.");
            asm_write_inst1c(assembly, "pop", "rax", "Dividend.");
            asm_write_inst2c(assembly, "mov", "r8", "rcx", "Save divisor.");
            asm_write_inst1(assembly, "neg", "r8");
            asm_write_inst2c(assembly, "cmovg", "r8", "rcx", "r8 = -abs(rcx).");
            asm_write_inst2(assembly, "mov", "r9", "rcx");
            asm_write_inst2c(assembly, "sal", "r9", "63", "r9 = sign(rcx).");
            asm_write_inst2(assembly, "xor", "rdx", "rdx");
            asm_write_inst1(assembly, "idiv", "rcx");
            asm_write_inst2c(assembly, "add", "r8", "rax", "q - sign(b)");
            asm_write_inst2c(assembly, "add", "r9", "rdx", "r + abs(b)");
            asm_write_inst2c(assembly, "test", "rdx", "rdx",
                             "Ensure r >= 0 and adjust q accordingly.");
            asm_write_inst2(assembly, "cmovl", "rax", "r8");
            asm_write_inst2(assembly, "cmovl", "rdx", "r9");
            asm_write_inst1c(assembly, "push", "rax", "Quotient.");
            asm_write_inst1c(assembly, "push", "rdx", "Remainder.");
            break;
        case W_OP_DUPE:
            asm_write_inst1(assembly, "push", "qword [rsp]");
            break;
        case W_OP_EXIT:
            asm_write_inst1c(assembly, "pop", "rcx", "Exit code.");
            asm_write_inst1(assembly, "call", "[ExitProcess]");
            break;
        case W_OP_FOR_DEC_START: {
            ip += 2;
            int16_t skip_jump = read_s16(block, ip - 1);
            int skip_jump_addr = ip - 1 + skip_jump;
            asm_write_inst1c(assembly, "pop", "rdi", "Load loop counter.");
            asm_write_inst2(assembly, "test", "rdi", "rdi");
            asm_write_inst1f(assembly, "jz", "addr_%d", skip_jump_addr);
            asm_write_inst2c(assembly, "mov", "[rsi]", "rdi",
                             "Push old loop counter onto loop stack.");
            asm_write_inst2(assembly, "add", "rsi", "8");
            break;
        }
        case W_OP_FOR_DEC: {
            ip += 2;
            int16_t loop_jump = read_s16(block, ip - 1);
            int loop_jump_addr = ip - 1 + loop_jump;
            asm_write_inst1(assembly, "dec", "rdi");
            asm_write_inst2(assembly, "test", "rdi", "rdi");
            asm_write_inst1f(assembly, "jnz", "addr_%d", loop_jump_addr);
            asm_write_inst2c(assembly, "sub", "rsi", "8", "Pop old loop counter into rdi.");
            asm_write_inst2(assembly, "mov", "rdi", "[rsi]");
            break;
        }
        case W_OP_FOR_INC_START: {
            ip += 2;
            int16_t skip_jump = read_s16(block, ip - 1);
            int skip_jump_addr = ip - 1 + skip_jump;
            asm_write_inst1c(assembly, "pop", "rax", "Load loop target.");
            asm_write_inst2(assembly, "test", "rax", "rax");
            asm_write_inst1f(assembly, "jz", "addr_%d", skip_jump_addr);
            asm_write_inst2c(assembly, "mov", "[rbx]", "rax", "Push loop target to aux.");
            asm_write_inst2(assembly, "add", "rbx", "8");
            asm_write_inst2c(assembly, "mov", "[rsi]", "rdi",
                             "Push old loop counter onto loop stack.");
            asm_write_inst2(assembly, "add", "rsi", "8");
            asm_write_inst2c(assembly, "xor", "rdi", "rdi", "Zero out loop counter.");
            break;
        }
        case W_OP_FOR_INC: {
            ip += 2;
            int16_t loop_jump = read_s16(block, ip - 1);
            int loop_jump_addr = ip - 1 + loop_jump;
            asm_write_inst1(assembly, "inc", "rdi");
            asm_write_inst2(assembly, "cmp", "rdi", "[rbx-8]");
            asm_write_inst1f(assembly, "jl", "addr_%d", loop_jump_addr);
            asm_write_inst2c(assembly, "sub", "rbx", "8", "Pop target.");
            asm_write_inst2c(assembly, "sub", "rsi", "8", "Pop old loop counter into rdi.");
            asm_write_inst2(assembly, "mov", "rdi", "[rsi]");
            break;
        }
        case W_OP_GET_LOOP_VAR: {
            ip += 2;
            uint16_t offset = read_u16(block, ip - 1);
            if (offset == 0) {
                // Current loop.
                asm_write_inst1(assembly, "push", "rdi");
            }
            else {
                // Outer loop.
                asm_write_inst2cf(assembly, "mov", "rax", "[rsi - %"PRIu16"*8]",
                                  "Offset of loop variable.", offset);
                asm_write_inst1(assembly, "push", "rax");
            }
            break;
        }
        case W_OP_JUMP: {
            ip += 2;
            int16_t jump = read_s16(block, ip - 1);
            int jump_addr = ip - 1 + jump;  // -1 since jumps are calculated from the opcode.
            asm_write_inst1f(assembly, "jmp", "addr_%d", jump_addr);
            break;
        }
        case W_OP_JUMP_COND: {
            ip += 2;
            int16_t jump = read_s16(block, ip - 1);
            int jump_addr = ip - 1 + jump;
            asm_write_inst1c(assembly, "pop", "rax", "Condition.");
            asm_write_inst2(assembly, "test", "rax", "rax");
            asm_write_inst1f(assembly, "jnz", "addr_%d", jump_addr);
            break;
        }
        case W_OP_JUMP_NCOND: {
            ip += 2;
            int16_t jump = read_s16(block, ip - 1);
            int jump_addr = ip -1 + jump;
            asm_write_inst1c(assembly, "pop", "rax", "Condition.");
            asm_write_inst2(assembly, "test", "rax", "rax");
            asm_write_inst1f(assembly, "jz", "addr_%d", jump_addr);
            break;
        }
        case W_OP_MULT:
            asm_write_inst1(assembly, "pop", "rax");
            asm_write_inst2c(assembly, "imul", "rax", "[rsp]", "Multiplication is commutative.");
            asm_write_inst2(assembly, "mov", "[rsp]", "rax");
            break;
        case W_OP_NOT:
            asm_write_inst1(assembly, "pop", "rax");
            asm_write_inst2c(assembly, "xor", "edx", "edx", "Zero out rdx.");
            asm_write_inst2(assembly, "test", "rax", "rax");
            asm_write_inst1(assembly, "setz", "dl");
            asm_write_inst1(assembly, "push", "rdx");
            break;
        case W_OP_OR:
            asm_write_inst1c(assembly, "pop", "rdx", "'Else' value.");
            asm_write_inst2c(assembly, "mov", "rax", "[rsp]", "'Then' value.");
            asm_write_inst2(assembly, "test", "rax", "rax");
            asm_write_inst2(assembly, "cmovz", "rax", "rdx");
            asm_write_inst2(assembly, "mov", "[rsp]", "rax");
            break;
        case W_OP_PRINT:
            asm_write_inst1c(assembly, "pop", "rdx", "Value to be printed.");
            asm_write_inst2c(assembly, "lea", "rcx", "[fmt_u64]", "Format string.");
            asm_write_inst2c(assembly, "mov", "rbp", "rsp",
                            "Save rsp for later (rbp is non-volatile in MS x64)");
            asm_write_inst2c(assembly, "and", "spl", "0F0h", "Align stack.");
            asm_write_inst2c(assembly, "sub", "rsp", "32\t", "Shadow space.");
            asm_write_inst1(assembly, "call", "[printf]");
            asm_write_inst2c(assembly, "mov", "rsp", "rbp", "Restore cached version of rsp.");
            break;
        case W_OP_PRINT_CHAR:
            asm_write_inst1(assembly, "pop", "rdx");
            asm_write_inst2(assembly, "lea", "rcx", "[fmt_char]");
            asm_write_inst2(assembly, "mov", "rbp", "rsp");
            asm_write_inst2(assembly, "and", "spl", "0F0h");
            asm_write_inst2(assembly, "sub", "rsp", "32");
            asm_write_inst1(assembly, "call", "[printf]");
            asm_write_inst2(assembly, "mov", "rsp", "rbp");
            break;
        case W_OP_PRINT_INT:
            asm_write_inst1(assembly, "pop", "rdx");
            asm_write_inst2(assembly, "lea", "rcx", "[fmt_s64]");
            asm_write_inst2(assembly, "mov", "rbp", "rsp");
            asm_write_inst2(assembly, "and", "spl", "0F0h");
            asm_write_inst2(assembly, "sub", "rsp", "32");
            asm_write_inst1(assembly, "call", "[printf]");
            asm_write_inst2(assembly, "mov", "rsp", "rbp");
            break;
        case W_OP_SUB:
            BIN_OP("sub");
            break;
        case W_OP_SWAP:
            asm_write_inst2(assembly, "mov", "rax", "[rsp]");
            asm_write_inst2(assembly, "mov", "rdx", "[rsp+8]");
            asm_write_inst2(assembly, "mov", "[rsp+8]", "rax");
            asm_write_inst2(assembly, "mov", "[rsp]", "rdx");
            break;
        case W_OP_SX8:
            asm_write_inst2(assembly, "movsx", "rax", "byte [rsp]");
            asm_write_inst2(assembly, "mov", "[rsp]", "rax");
            break;
        case W_OP_SX8L:
            asm_write_inst2(assembly, "movsx", "rax", "byte [rsp+8]");
            asm_write_inst2(assembly, "mov", "[rsp+8]", "rax");
            break;
        case W_OP_SX16:
            asm_write_inst2(assembly, "movsx", "rax", "word [rsp]");
            asm_write_inst2(assembly, "mov", "[rsp]", "rax");
            break;
        case W_OP_SX16L:
            asm_write_inst2(assembly, "movsx", "rax", "word [rsp+8]");
            asm_write_inst2(assembly, "mov", "[rsp+8]", "rax");
            break;
        case W_OP_SX32:
            asm_write_inst2(assembly, "movsx", "rax", "dword [rsp]");
            asm_write_inst2(assembly, "mov", "[rsp]", "rax");
            break;
        case W_OP_SX32L:
            asm_write_inst2(assembly, "movsx", "rax", "dword [rsp+8]");
            asm_write_inst2(assembly, "mov", "[rsp+8]", "rax");
            break;
        case W_OP_ZX8:
            asm_write_inst2(assembly, "movzx", "rax", "byte [rsp]");
            asm_write_inst2(assembly, "mov", "[rsp]", "rax");
            break;
        case W_OP_ZX8L:
            asm_write_inst2(assembly, "movzx", "rax", "byte [rsp+8]");
            asm_write_inst2(assembly, "mov", "[rsp+8]", "rax");
            break;
        case W_OP_ZX16:
            asm_write_inst2(assembly, "movzx", "rax", "word [rsp]");
            asm_write_inst2(assembly, "mov", "[rsp]", "rax");
            break;
        case W_OP_ZX16L:
            asm_write_inst2(assembly, "movzx", "rax", "word [rsp+8]");
            asm_write_inst2(assembly, "mov", "[rsp+8]", "rax");
            break;
        case W_OP_ZX32:
            asm_write_inst2(assembly, "movzx", "rax", "dword [rsp]");
            asm_write_inst2(assembly, "mov", "[rsp]", "rax");
            break;
        case W_OP_ZX32L:
            asm_write_inst2(assembly, "mov", "rax", "dword [rsp+8]");
            asm_write_inst2(assembly, "mov", "[rsp+8]", "rax");
            break;
        }
    }
    asm_write(assembly, "  ;;\t=== END ===\n");
    asm_write_inst2c(assembly, "xor", "rcx", "rcx", "Successful exit.");
    asm_write_inst2(assembly, "and", "spl", "0F0h");
    asm_write_inst2(assembly, "sub", "rsp", "32");
    asm_write_inst1(assembly, "call", "[ExitProcess]");
    asm_write(assembly, "\n");
#undef BIN_OP
}

void generate_imports(struct asm_block *assembly) {
    asm_section(assembly, ".idata", "import", "data", "readable");
    asm_write(assembly, "\n");
    asm_write(assembly, "  library\\\n");
    asm_write(assembly, "\tkernel, 'kernel32.dll',\\\n");
    asm_write(assembly, "\tmsvcrt, 'msvcrt.dll'\n");
    asm_write(assembly, "\n");
    asm_write(assembly, "  import msvcrt,\\\n");
    asm_write(assembly, "\tprintf, 'printf'\n");
    asm_write(assembly, "\n");
    asm_write(assembly, "  import kernel,\\\n");
    asm_write(assembly, "\tExitProcess, 'ExitProcess'\n");
    asm_write(assembly, "\n");
}

void generate_constants(struct asm_block *assembly, struct ir_block *block) {
    asm_section(assembly, ".rdata", "data", "readable");
    asm_write(assembly, "\n");
    asm_label(assembly, "fmt_s64");
    asm_write_inst3c(assembly, "db", "'%%I64d'", "10", "0",
                     "NOTE: I64 is a Non-ISO Microsoft extension.");
    asm_write(assembly, "\n");
    asm_label(assembly, "fmt_u64");
    asm_write_inst3(assembly, "db", "'%%I64u'", "10", "0");
    asm_write(assembly, "\n");
    asm_label(assembly, "fmt_char");
    asm_write_inst2(assembly, "db", "'%%c'", "0");
    asm_write(assembly, "\n");
    for (size_t i = 0; i < block->strings.count; ++i) {
        asm_label(assembly, "str%u", i);
        asm_write(assembly, "\tdb\t");
        asm_write_string(assembly, block->strings.views[i].start);
        asm_write(assembly, "\n\n");
    }
}

void generate_bss(struct asm_block *assembly) {
    asm_section(assembly, ".bss", "data", "readable", "writeable");
    asm_label(assembly, "aux");
    asm_write_inst1(assembly, "rq", "1024*1024");
}

enum generate_result generate(struct ir_block *block, struct asm_block *assembly) {
    generate_header(assembly);
    generate_code(assembly, block);
    generate_constants(assembly, block);
    generate_imports(assembly);
    generate_bss(assembly);
    return (!asm_had_error(assembly)) ? GENERATE_OK : GENERATE_ERROR;
}
