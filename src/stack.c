#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "stack.h"


void init_stack(struct stack *stack) {
    reset_stack(stack);
}

void reset_stack(struct stack *stack) {
    stack->top = stack->elements;
}

void push(struct stack *stack, stack_word value) {
    if (stack->top == &stack->elements[STACK_SIZE-1]) {
        fprintf(stderr, "Stack overflow\n");
        exit(1);
    }
    *stack->top++ = value;
}

stack_word pop(struct stack *stack) {
    if (stack->top == &stack->elements[0]) {
        fprintf(stderr, "Stack underflow\n");
        exit(1);
    }
    return *--stack->top;
}

stack_word peek(struct stack *stack) {
    if (stack->top == &stack->elements[0]) {
        fprintf(stderr, "Stack underflow\n");
        exit(1);
    }
    return stack->top[-1];
}

stack_word peek_nth(struct stack *stack, uint32_t n) {
    if (stack->top - stack->elements <= (int64_t)n) {
        fprintf(stderr, "Stack underflow\n");
        exit(1);
    }
    return stack->top[-1 - (int64_t)n];
}
