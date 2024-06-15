#ifndef STACK_H
#define STACK_H

#include <stdint.h>
#include <inttypes.h>


#define STACK_SIZE (4 * 1024 * 1024)
#define PRIsw  PRIu64
#define PRIssw PRId64

typedef uint64_t stack_word;
typedef int64_t sstack_word;

struct stack {
    stack_word *top;
    stack_word elements[STACK_SIZE];
};

void init_stack(struct stack *stack);
void reset_stack(struct stack *stack);

void push(struct stack *stack, stack_word value);
stack_word pop(struct stack *stack);
void popn(struct stack *stack, int n);
void push_all(struct stack *stack, size_t n, const stack_word values[n]);
void pop_all(struct stack *stack, size_t n, stack_word buffer[n]);
stack_word peek(struct stack *stack);
stack_word peek_nth(struct stack *stack, uint32_t n);
const stack_word *peekn(struct stack *stack, int n);
void set_nth(struct stack *stack, int n, stack_word value);

stack_word *reserve(struct stack *stack, int count);
int restore(struct stack *stack, stack_word *start);

#endif
