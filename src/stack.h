#ifndef STACK_H
#define STACK_H

#include <stdint.h>
#include <inttypes.h>


#define STACK_SIZE 4 * 1024 * 1024
#define PRIsw  PRIu64
#define PRIssw PRId64

typedef uint64_t stack_word;

struct stack {
    stack_word *top;
    stack_word elements[STACK_SIZE];
};

void init_stack(struct stack *stack);
void reset_stack(struct stack *stack);

void push(struct stack *stack, stack_word value);
stack_word pop(struct stack *stack);
stack_word peek(struct stack *stack);
stack_word peek_nth(struct stack *stack, uint32_t n);

#endif
