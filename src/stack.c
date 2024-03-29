#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

void popn(struct stack *stack, int n) {
    assert(n > 0);
    if (stack->top - stack->elements < n) {
        fprintf(stderr, "Stack underflow");
        exit(1);
    }
    stack->top -= n;
}

void push_all(struct stack *stack, size_t n, const stack_word values[n]) {
    if (&stack->elements[STACK_SIZE] - stack->top <= (int64_t)n) {
        fprintf(stderr, "Stack overflow\n");
        exit(1);
    }
    memcpy(stack->top, values, sizeof(stack_word[n]));
    stack->top += n;
}

void pop_all(struct stack *stack, size_t n, stack_word buffer[n]) {
    if (stack->top - stack->elements < (int64_t)n) {
        fprintf(stderr, "Stack underflow\n");
        exit(1);
    }
    memcpy(buffer, stack->top - n, sizeof(stack_word[n]));
    stack->top -= n;
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

const stack_word *peekn(struct stack *stack, int n) {
    if (stack->top - stack->elements < n) {
        fprintf(stderr, "Stack underflow\n");
        exit(1);
    }
    assert(n > 0);
    return stack->top - n;
}

void set_nth(struct stack *stack, int n, stack_word value) {
    if (stack->top - stack->elements < n) {
        fprintf(stderr, "Stack underflow\n");
        exit(1);
    }
    assert(n > 0);
    stack->top[-1 - n] = value; 
}
