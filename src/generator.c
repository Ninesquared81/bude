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
        case OP_SUB:
            asm_write(assembly, "  ;; === OP_SUB ===\n");
            BIN_OP("sub");
            break;
        default:
            assert(0 && "Not implemented you silly goose!");
        }
    }
    asm_end_code(assembly);
    return GENERATE_OK;
#undef BIN_OP
}
