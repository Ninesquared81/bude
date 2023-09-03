#include <stdlib.h>
#include <stdio.h>

#include "interpreter.h"
#include "disassembler.h"
#include "ir.h"
#include "stack.h"

enum interpret_result interpret(struct ir_block *block) {
    disassemble_block(block);
    printf("-----------------------------\n");
    struct stack *stack = malloc(sizeof *stack);
    init_stack(stack);
    for (int ip = 0; ip < block->count; ++ip) {
        uint8_t instruction = block->code[ip];
        switch (instruction) {
        case OP_PUSH: {
            uint8_t value = block->code[++ip];
            push(stack, value);
            break;
        }
        case OP_POP:
            pop(stack);
            break;
        case OP_ADD: {
            stack_word b = pop(stack);
            stack_word a = pop(stack);
            push(stack, a + b);
            break;
        }
        case OP_MULT: {
            stack_word b = pop(stack);
            stack_word a = pop(stack);
            push(stack, a * b);
            break;
        }        case OP_PRINT:
            printf("%"PRIsw"\n", pop(stack));
            break;
        }
    }
    free(stack);
    return INTERPRET_OK;
}

