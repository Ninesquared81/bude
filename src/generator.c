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
        case OP_ADD:
            BIN_OP("add");
            break;
        case OP_DIVMOD:
            asm_write_inst1c(assembly, "pop", "rbx", "Divisor.");
            asm_write_inst1c(assembly, "pop", "rax", "Dividend.");
            asm_write_inst2c(assembly, "xor", "rdx", "rdx", "Zero out extra bytes in dividend.");
            asm_write_inst1(assembly, "div", "rbx");
            asm_write_inst1c(assembly, "push", "rax", "Quotient.");
            asm_write_inst1c(assembly, "push", "rdx", "Remainder.");
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
            asm_write_inst1c(assembly, "pop", "rdx", "Second argument to printf.");
            asm_write_inst2c(assembly, "mov", "rcx", "fmt_s64",
                             "First argument to printf (format string).");
            asm_write_inst1(assembly, "call", "[printf]");
            break;
        case OP_SUB:
            BIN_OP("sub");
            break;
        default:
            assert(0 && "Not implemented you silly goose!");
        }
    }
    asm_write(assembly, "  ;;\t=== END ===\n");
    asm_write_inst2c(assembly, "invoke", "ExitProcess", "0", "Successful exit.");
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
    (void)block;
    asm_section(assembly, ".rodata", "data", "readable");
    asm_write(assembly, "\n");
    asm_label(assembly, "fmt_s64");
    asm_write_inst3c(assembly, "db", "'%%I64d'", "10", "0",
                     "NOTE: I64 is a Non-ISO Microsoft extension.");
    asm_write(assembly, "\n");
    asm_label(assembly, "fmt_u64");
    asm_write_inst3(assembly, "db", "'%%I64u'", "10", "0");
    asm_write(assembly, "\n");
}

enum generate_result generate(struct ir_block *block, struct asm_block *assembly) {
    generate_header(assembly);
    generate_code(assembly, block);
    generate_constants(assembly, block);
    generate_imports(assembly);
    return GENERATE_OK;
}
