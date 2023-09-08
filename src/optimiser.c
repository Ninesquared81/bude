#include <assert.h>
#include <stdbool.h>

#include "ir.h"
#include "optimiser.h"

static bool check_next(struct ir_block *block, int offset, int operand_size, enum opcode opcode) {
    int next_offset = offset + 1;
    switch (operand_size) {
    case 0:
        // Simple instruction.
        break;
    case 8:
        next_offset = offset + 2;
        break;
    case 16:
        next_offset = offset + 3;
        break;
    case 32:
        next_offset = offset + 5;
        break;
    default:
        assert(0 && "Invalid operand size. Allowed sizes are 0, 8, 16, 32.");
    }

    if (next_offset >= block->count) return false;
    if (is_jump_dest(block, next_offset)) return false;

    // Note: we assume the passed offset is to the start of an instruction.
    return block->code[next_offset] == opcode;
}

void optimise(struct ir_block *block) {
    for (int ip = 0; ip < block->count; ++ip) {
        switch (block->code[ip]) {
        case OP_PUSH8:
            if (check_next(block, ip, 8, OP_POP)) {
                overwrite_instruction(block, ip, OP_NOP);
                overwrite_instruction(block, ip + 1, OP_NOP);
                ++ip;
            }
            break;
        case OP_PUSH16:
            if (check_next(block, ip, 16, OP_POP)) {
                overwrite_instruction(block, ip, OP_NOP);
                overwrite_instruction(block, ip + 1, OP_NOP);
                overwrite_instruction(block, ip + 2, OP_NOP);
                ip += 2;
            }
            break;
        case OP_PUSH32:
            if (check_next(block, ip, 32, OP_POP)) {
                overwrite_instruction(block, ip, OP_NOP);
                overwrite_instruction(block, ip + 1, OP_NOP);
                overwrite_instruction(block, ip + 2, OP_NOP);
                overwrite_instruction(block, ip + 3, OP_NOP);
                overwrite_instruction(block, ip + 4, OP_NOP);
                ip += 4;
            }
            break;
        case OP_LOAD8:
            if (check_next(block, ip, 8, OP_POP)) {
                overwrite_instruction(block, ip, OP_NOP);
                overwrite_instruction(block, ip + 1, OP_NOP);
                ++ip;
            }
            break;
        case OP_LOAD16:
            if (check_next(block, ip, 16, OP_POP)) {
                overwrite_instruction(block, ip, OP_NOP);
                overwrite_instruction(block, ip + 1, OP_NOP);
                overwrite_instruction(block, ip + 2, OP_NOP);
                ip += 2;
            }
            break;
        case OP_LOAD32:
            if (check_next(block, ip, 32, OP_POP)) {
                overwrite_instruction(block, ip, OP_NOP);
                overwrite_instruction(block, ip + 1, OP_NOP);
                overwrite_instruction(block, ip + 2, OP_NOP);
                overwrite_instruction(block, ip + 3, OP_NOP);
                overwrite_instruction(block, ip + 4, OP_NOP);
                ip += 4;
            }
            break;
        case OP_NOT:
            if (check_next(block, ip, 0, OP_NOT)) {
                overwrite_instruction(block, ip, OP_NOP);
                overwrite_instruction(block, ip + 1, OP_NOP);
                ++ip;
            }
            else if (check_next(block, ip, 0, OP_JUMP_NCOND)) {
                overwrite_instruction(block, ip, OP_NOP);
                overwrite_instruction(block, ip + 1, OP_JUMP_COND);
                ++ip;
            }
            else if (check_next(block, ip, 0, OP_JUMP_COND)) {
                overwrite_instruction(block, ip, OP_NOP);
                overwrite_instruction(block, ip + 1, OP_JUMP_NCOND);
                ++ip;
            }
            break;
        }
    }
}
