#include <assert.h>
#include <stdint.h>
#include <inttypes.h>

#include "asm.h"
#include "generator.h"
#include "ir.h"


enum generate_result generate(struct ir_block *block, struct asm_block *assembly) {
#define BIN_OP(OP)                                               \
    do {                                                         \
        asm_write(assembly, "\tpop\trbx\t\t; RHS.\n");           \
        asm_write(assembly, "\tpop\trax\t\t; LHS.\n");           \
        asm_write(assembly, "\t"OP"\trax, rbx\n");               \
        asm_write(assembly, "\tpush\trax\n");                    \
    } while (0)

    asm_start_asm(assembly);
    asm_start_code(assembly, "start");
    for (int ip = 0; ip < block->count; ++ip) {
        if (is_jump_dest(block, ip)) {
            // We need a label.
            asm_write(assembly, "  addr_%d:\n", ip);
        }
        enum opcode instruction = block->code[ip];
        switch (instruction) {
        case OP_NOP:
            // Do nothing.
            break;
        case OP_PUSH8: {
            ++ip;
            int8_t value = read_s8(block, ip);
            asm_write(assembly, "  ;; === OP_PUSH8 ===\n");
            asm_write(assembly, "\tmov\trax, %"PRId8"\n", value);
            asm_write(assembly, "\tpush\trax\n");
            break;
        }
        case OP_ADD:
            asm_write(assembly, "  ;; === OP_ADD ===\n");
            BIN_OP("add");
            break;
        case OP_EXIT:
            asm_write(assembly, "  ;; === OP_EXIT ===\n");
            asm_write(assembly, "\tpop\trcx\t\t; Exit code.\n");
            asm_write(assembly, "\tcall\t[ExitProcess]\n");
            break;
        case OP_JUMP: {
            ip += 2;
            int16_t jump = read_s16(block, ip - 1);
            int jump_addr = ip - 1 + jump;  // -1 since jumps are calculated from the opcode.
            asm_write(assembly, "  ;; === OP_JUMP ===\n");
            asm_write(assembly, "\tjmp\taddr_%d\n", jump_addr);
            break;
        }
        case OP_JUMP_COND: {
            ip += 2;
            int16_t jump = read_s16(block, ip - 1);
            int jump_addr = ip - 1 + jump;
            asm_write(assembly, "  ;; === OP_JUMP_COND ===\n");
            asm_write(assembly, "\tpop\trax\t\t; Condition.\n");
            asm_write(assembly, "\ttest rax, rax\n");
            asm_write(assembly, "\tjnz\taddr_%d\n", jump_addr);
            break;
        }
        case OP_JUMP_NCOND: {
            ip += 2;
            int16_t jump = read_s16(block, ip - 1);
            int jump_addr = ip -1 + jump;
            asm_write(assembly, "  ;; === OP_JUMP_NCOND ===\n");
            asm_write(assembly, "\tpop\trax\t\t; Condition.\n");
            asm_write(assembly, "\ttest\trax, rax\n");
            asm_write(assembly, "\tjz\taddr_%d\n", jump_addr);
            break;
        }
        case OP_NOT:
            asm_write(assembly, "  ;; === OP_NOT ===\n");
            asm_write(assembly, "\tpop\trax\n");
            asm_write(assembly, "\ttest\trax, rax\n");
            asm_write(assembly, "\tsetz\trax\n");
            asm_write(assembly, "\tpush\trax\n");
            break;
        case OP_SUB:
            asm_write(assembly, "  ;; === OP_SUB ===\n");
            BIN_OP("sub");
            break;
        default:
            assert(0 && "Not implemented you silly goose!");
        }
    }
    asm_write(assembly, "  ;; === END ===\n");
    asm_write(assembly, "\tinvoke\tExitProcess, 0\t; Successful exit.\n");
    asm_end_code(assembly);
    return GENERATE_OK;
#undef BIN_OP
}
