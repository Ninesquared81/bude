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
        fprintf(stderr, "Stack overflow in push()\n");
        exit(1);
    }
    *stack->top++ = value;
}

stack_word pop(struct stack *stack) {
    if (stack->top == &stack->elements[0]) {
        fprintf(stderr, "Stack underflow in pop()\n");
        exit(1);
    }
    return *--stack->top;
}

void popn(struct stack *stack, int n) {
    assert(n >= 0);
    if (n == 0) return;
    if (stack->top - stack->elements < n) {
        fprintf(stderr, "Stack underflow in popn()\n");
        exit(1);
    }
    stack->top -= n;
}

void push_all(struct stack *stack, size_t n, const stack_word values[n]) {
    if (&stack->elements[STACK_SIZE] - stack->top <= (int64_t)n) {
        fprintf(stderr, "Stack overflow in push_all()\n");
        exit(1);
    }
    memcpy(stack->top, values, sizeof(stack_word[n]));
    stack->top += n;
}

void pop_all(struct stack *stack, size_t n, stack_word buffer[n]) {
    if (stack->top - stack->elements < (int64_t)n) {
        fprintf(stderr, "Stack underflow in pop_all()\n");
        exit(1);
    }
    memcpy(buffer, stack->top - n, sizeof(stack_word[n]));
    stack->top -= n;
}

stack_word peek(struct stack *stack) {
    if (stack->top == &stack->elements[0]) {
        fprintf(stderr, "Stack underflow in peek()\n");
        exit(1);
    }
    return stack->top[-1];
}

stack_word peek_nth(struct stack *stack, uint32_t n) {
    if (stack->top - stack->elements <= (int64_t)n) {
        fprintf(stderr, "Stack underflow in peek_nth()\n");
        exit(1);
    }
    return stack->top[-1 - (int64_t)n];
}

const stack_word *peekn(struct stack *stack, int n) {
    if (stack->top - stack->elements < n) {
        fprintf(stderr, "Stack underflow in peekn()\n");
        exit(1);
    }
    assert(n > 0);
    return stack->top - n;
}

void set_nth(struct stack *stack, int n, stack_word value) {
    if (stack->top - stack->elements < n) {
        fprintf(stderr, "Stack underflow in set_nth()\n");
        exit(1);
    }
    assert(n >= 0);
    stack->top[-1 - n] = value;
}

stack_word *reserve(struct stack *stack, int count) {
    if (&stack->elements[STACK_SIZE] - stack->top < count) {
        fprintf(stderr, "Stack overflow in reserve()\n");
        exit(1);
    }
    assert(count >= 0);
    stack_word *start = stack->top;
    stack->top += count;
    return start;
}

int restore(struct stack *stack, stack_word *start) {
    assert(stack->elements <= start && start <= stack->top);
    int size = stack->top - start;
    stack->top = start;
    return size;
}
