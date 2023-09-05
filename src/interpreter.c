#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "disassembler.h"
#include "interpreter.h"
#include "ir.h"
#include "stack.h"
#include "type_punning.h"


#define BIN_OP(op, stack) do {     \
        stack_word b = pop(stack); \
        stack_word a = pop(stack); \
        push(stack, a op b);       \
    } while (0)


static void jump(struct ir_block *block, int offset, int *ip) {
    int new_address = *ip + offset;
    // Address -1 means to jump to the start.
    assert(-1 <= new_address && new_address < block->count);
    *ip = new_address;
}

enum interpret_result interpret(struct ir_block *block) {
    disassemble_block(block);
    printf("-----------------------------\n");
    struct stack *stack = malloc(sizeof *stack);
    init_stack(stack);
    for (int ip = 0; ip < block->count; ++ip) {
        enum opcode instruction = block->code[ip];
        switch (instruction) {
        case OP_PUSH: {
            int8_t value = u8_to_s8(block->code[++ip]);
            push(stack, s64_to_u64(value));
            break;
        }
        case OP_POP: pop(stack); break;
        case OP_ADD: BIN_OP(+, stack); break;
        case OP_JUMP: {
            int offset = read_s16(block, ip + 1);
            jump(block, offset, &ip);
            break;
        }
        case OP_JUMP_COND: {
            int offset = read_s16(block, ip + 1);
            bool condition = pop(stack);
            if (condition) {
                jump(block, offset, &ip);
            } else {
                ip += 2;  // Consume the operand.
            }
            break;
        }
        case OP_MULT: BIN_OP(*, stack); break;
        case OP_NOT: {
            bool condition = pop(stack);
            push(stack, !condition);
            break;
        }
        case OP_SUB: BIN_OP(-, stack); break;
        case OP_DIVMOD: {
            stack_word b = pop(stack);
            stack_word a = pop(stack);
            push(stack, a / b);
            push(stack, a % b);
            break;
        }
        case OP_SWAP: {
            stack_word b = pop(stack);
            stack_word a = pop(stack);
            push(stack, b);
            push(stack, a);
            break;
        }
        case OP_PRINT:
            printf("%"PRIsw"\n", u64_to_s64(pop(stack)));
            break;
        }
    }
    free(stack);
    return INTERPRET_OK;
}

