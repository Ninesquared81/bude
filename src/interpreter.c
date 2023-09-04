#include <stdlib.h>
#include <stdio.h>

#include "interpreter.h"
#include "disassembler.h"
#include "ir.h"
#include "stack.h"


#define BIN_OP(op, stack) do {     \
        stack_word b = pop(stack); \
        stack_word a = pop(stack); \
        push(stack, a op b);       \
    } while (0)


enum interpret_result interpret(struct ir_block *block) {
    disassemble_block(block);
    printf("-----------------------------\n");
    struct stack *stack = malloc(sizeof *stack);
    init_stack(stack);
    for (int ip = 0; ip < block->count; ++ip) {
        enum opcode instruction = block->code[ip];
        switch (instruction) {
        case OP_PUSH: {
            uint8_t value = block->code[++ip];
            push(stack, value);
            break;
        }
        case OP_POP: pop(stack); break;
        case OP_ADD: BIN_OP(+, stack); break;
        case OP_MULT: BIN_OP(*, stack); break;
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
            printf("%"PRIsw"\n", pop(stack));
            break;
        }
    }
    free(stack);
    return INTERPRET_OK;
}

