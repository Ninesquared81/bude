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
        asm_write_inst1c(assembly, "pop", "rbx", "RHS.");               \
        asm_write_inst2c(assembly, OP, "[rsp]", "rbx", "LHS left on stack."); \
    } while (0)

    asm_section(assembly, ".code", "code", "readable", "executable");
    asm_write(assembly, "\n");
    asm_label(assembly, "start");
    asm_write(assembly, "\n");
    for (int ip = 0; ip < block->count; ++ip) {
        if (is_jump_dest(block, ip)) {
            // We need a label.
            asm_label(assembly, "addr_%d", ip);
        }
        enum opcode instruction = block->code[ip];
        if (instruction == OP_NOP) continue;
        asm_write(assembly, "  ;;\t=== %s ===\n", get_opcode_name(instruction));
        switch (instruction) {
        case OP_NOP:
            // Do nothing.
            break;
        case OP_PUSH8: {
            ++ip;
            int8_t value = read_s8(block, ip);
            asm_write_inst2f(assembly, "mov", "rax", "%"PRId8, value);
            asm_write_inst1(assembly, "push", "rax");
            break;
        }
        case OP_PUSH16: {
            ip += 2;
            int16_t value = read_s16(block, ip - 1);
            asm_write_inst2f(assembly, "mov", "rax", "%"PRId16, value);
            asm_write_inst1(assembly, "push", "rax");
            break;
        }
        case OP_PUSH32: {
            ip += 4;
            int32_t value = read_s32(block, ip - 3);
            asm_write_inst2f(assembly, "mov", "rax", "%"PRId16, value);
            asm_write_inst1(assembly, "push", "rax");
            break;
        }
        case OP_LOAD8: {
            ++ip;
            uint8_t index = read_u8(block, ip);
            asm_write_inst2f(assembly, "lea", "rax", "[constants + %"PRIu8"*8]", index);
            asm_write_inst1(assembly, "push", "qword [rax]");
            break;
        }
        case OP_LOAD16: {
            ip += 2;
            uint16_t index = read_u16(block, ip - 1);
            asm_write_inst2f(assembly, "lea", "rax", "[constants + %"PRIu16"*8]", index);
            asm_write_inst1(assembly, "push", "qword [rax]");
            break;
        }
        case OP_LOAD32: {
            ip += 4;
            uint32_t index = read_u32(block, ip - 3);
            asm_write_inst2f(assembly, "lea", "rax", "[constants + %"PRIu32"*8]", index);
            asm_write_inst1(assembly, "push", "qword [rax]");
            break;
        }
        case OP_LOAD_STRING8: {
            ++ip;
            int8_t index = read_u8(block, ip);
            asm_write_inst2f(assembly, "lea", "rax", "[str%"PRIu8"]", index);
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst1f(assembly, "push", "%u", read_string(block, index)->length);
            break;
        }
        case OP_LOAD_STRING16: {
            ++ip;
            int16_t index = read_u16(block, ip);
            asm_write_inst2f(assembly, "lea", "rax", "[str%"PRIu16"]", index);
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst1f(assembly, "push", "%u", read_string(block, index)->length);
            break;
        }
        case OP_LOAD_STRING32: {
            ++ip;
            int32_t index = read_u32(block, ip);
            asm_write_inst2f(assembly, "lea", "rax", "[str%"PRIu32"]", index);
            asm_write_inst1(assembly, "push", "rax");
            asm_write_inst1f(assembly, "push", "%u", read_string(block, index)->length);
            break;
        }
        case OP_POP:
            asm_write_inst1(assembly, "pop", "rax");
            break;
        case OP_ADD:
            BIN_OP("add");
            break;
        case OP_DEREF:
            asm_write_inst1(assembly, "pop", "rax");
            asm_write_inst2(assembly, "movzx", "rbx", "byte [rax]");
            asm_write_inst1(assembly, "push", "rbx");
            break;
        case OP_DIVMOD:
            asm_write_inst1c(assembly, "pop", "rbx", "Divisor.");
            asm_write_inst1c(assembly, "pop", "rax", "Dividend.");
            asm_write_inst2c(assembly, "xor", "rdx", "rdx", "Zero out extra bytes in dividend.");
            asm_write_inst1(assembly, "div", "rbx");
            asm_write_inst1c(assembly, "push", "rax", "Quotient.");
            asm_write_inst1c(assembly, "push", "rdx", "Remainder.");
            break;
        case OP_DUPE:
            asm_write_inst1(assembly, "push", "qword [rsp]");
            break;
        case OP_EXIT:
            asm_write_inst1c(assembly, "pop", "rcx", "Exit code.");
            asm_write_inst1(assembly, "call", "[ExitProcess]");
            break;
        case OP_JUMP: {
            ip += 2;
            int16_t jump = read_s16(block, ip - 1);
            int jump_addr = ip - 1 + jump;  // -1 since jumps are calculated from the opcode.
            asm_write_inst1f(assembly, "jmp", "addr_%d", jump_addr);
            break;
        }
        case OP_JUMP_COND: {
            ip += 2;
            int16_t jump = read_s16(block, ip - 1);
            int jump_addr = ip - 1 + jump;
            asm_write_inst1c(assembly, "pop", "rax", "Condition.");
            asm_write_inst2(assembly, "test", "rax", "rax");
            asm_write_inst1f(assembly, "jnz", "addr_%d", jump_addr);
            break;
        }
        case OP_JUMP_NCOND: {
            ip += 2;
            int16_t jump = read_s16(block, ip - 1);
            int jump_addr = ip -1 + jump;
            asm_write_inst1c(assembly, "pop", "rax", "Condition.");
            asm_write_inst2(assembly, "test", "rax", "rax");
            asm_write_inst1f(assembly, "jz", "addr_%d", jump_addr);
            break;
        }
        case OP_MULT:
            BIN_OP("mul");
            break;
        case OP_NOT:
            asm_write_inst1(assembly, "pop", "rax");
            asm_write_inst2(assembly, "test", "rax", "rax");
            asm_write_inst1(assembly, "setz", "rax");
            asm_write_inst1(assembly, "push", "rax");
            break;
        case OP_PRINT:
            asm_write_inst1c(assembly, "pop", "rdx", "Value to be printed.");
            asm_write_inst2c(assembly, "mov", "rcx", "fmt_s64", "Format string.");
            asm_write_inst2c(assembly, "mov", "rbx", "rsp",
                            "Save rsp for later (rbp is non-volatile in MS x64)");
            asm_write_inst2c(assembly, "and", "spl", "0F0h", "Align stack.");
            asm_write_inst2c(assembly, "sub", "rsp", "32\t", "Shadow space.");
            asm_write_inst1(assembly, "call", "[printf]");
            asm_write_inst2c(assembly, "mov", "rsp", "rbx", "Restore cached version of rsp.");
            break;
        case OP_PRINT_CHAR:
            asm_write_inst1(assembly, "pop", "rdx");
            asm_write_inst2(assembly, "lea", "rcx", "[fmt_char]");
            asm_write_inst2(assembly, "mov", "rbp", "rsp");
            asm_write_inst2(assembly, "and", "spl", "0F0h");
            asm_write_inst2(assembly, "sub", "rsp", "32");
            asm_write_inst1(assembly, "call", "[printf]");
            asm_write_inst2(assembly, "mov", "rsp", "rbp");
            break;
        case OP_SUB:
            BIN_OP("sub");
            break;
        default:
            assert(0 && "Not implemented you silly goose!");
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
    if (block->constants.count > 0) {
        struct constant_table *constants = &block->constants;
        asm_label(assembly, "constants");
        for (int i = 0; i < constants->count; ++i) {
            asm_write_inst1f(assembly, "dq", "%"PRIu64, constants->data[i]);
        }
    }
    
}

enum generate_result generate(struct ir_block *block, struct asm_block *assembly) {
    generate_header(assembly);
    generate_code(assembly, block);
    generate_constants(assembly, block);
    generate_imports(assembly);
    return GENERATE_OK;
}
