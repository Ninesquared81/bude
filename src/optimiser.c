#include "optimiser.h"

void optimise(struct ir_block *block) {
    for (int ip = 0; ip < block->count; ++ip) {
        switch (block->code[ip]) {
        case OP_PUSH8:
            if (check_next(block, ip, OP_POP)) {
                rewrite_instruction(block, ip, OP_NOP);
                rewrite_instruction(block, ip + 1, OP_NOP);
                ++ip;
            }
            break;
        case OP_PUSH16:
            if (check_next(block, ip + 1, OP_POP)) {
                rewrite_instruction(block, ip, OP_NOP);
                rewrite_instruction(block, ip + 1, OP_NOP);
                rewrite_instruction(block, ip + 2, OP_NOP);
                ip += 2;
            }
            break;
        case OP_PUSH32:
            if (check_next(block, ip + 3, OP_POP)) {
                rewrite_instruction(block, ip, OP_NOP);
                rewrite_instruction(block, ip + 1, OP_NOP);
                rewrite_instruction(block, ip + 2, OP_NOP);
                rewrite_instruction(block, ip + 3, OP_NOP);
                rewrite_instruction(block, ip + 4, OP_NOP);
                ip += 4;
            }
            break;
        case OP_LOAD8:
            if (check_next(block, ip, OP_POP)) {
                rewrite_instruction(block, ip, OP_NOP);
                rewrite_instruction(block, ip + 1, OP_NOP);
                ++ip;
            }
            break;
        case OP_LOAD16:
            if (check_next(block, ip + 1, OP_POP)) {
                rewrite_instruction(block, ip, OP_NOP);
                rewrite_instruction(block, ip + 1, OP_NOP);
                rewrite_instruction(block, ip + 2, OP_NOP);
                ip += 2;
            }
            break;
        case OP_LOAD32:
            if (check_next(block, ip + 3, OP_POP)) {
                rewrite_instruction(block, ip, OP_NOP);
                rewrite_instruction(block, ip + 1, OP_NOP);
                rewrite_instruction(block, ip + 2, OP_NOP);
                rewrite_instruction(block, ip + 3, OP_NOP);
                rewrite_instruction(block, ip + 4, OP_NOP);
                ip += 4;
            }
            break;
        case OP_NOT:
            if (check_next(block, ip, OP_NOT)) {
                rewrite_instruction(block, ip, OP_NOP);
                rewrite_instruction(block, ip + 1, OP_NOP);
                ++ip;
            }
            else if (check_next(block, ip, OP_JUMP_NCOND)) {
                rewrite_instruction(block, ip, OP_NOP);
                rewrite_instruction(block, ip + 1, OP_JUMP_COND);
                ++ip;
            }
            else if (check_next(block, ip, OP_JUMP_COND)) {
                rewrite_instruction(block, ip, OP_NOP);
                rewrite_instruction(block, ip + 1, OP_JUMP_NCOND);
                ++ip;
            }
            break;
        }
    }
}
