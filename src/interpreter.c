#include <assert.h>
#include <limits.h>
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

enum interpret_result interpret(struct ir_block *block, struct stack *stack) {
    init_stack(stack);
    for (int ip = 0; ip < block->count; ++ip) {
        enum opcode instruction = block->code[ip];
        switch (instruction) {
        case OP_NOP:
            // Do nothing.
            break;
        case OP_PUSH8: {
            ++ip;
            int8_t value = read_s8(block, ip);
            push(stack, s64_to_u64(value));
            break;
        }
        case OP_PUSH16: {
            ip += 2;
            int16_t value = read_s16(block, ip - 1);
            push(stack, s64_to_u64(value));
            break;
        }
        case OP_PUSH32: {
            ip += 4;
            int32_t value = read_s32(block, ip - 3);
            push(stack, s64_to_u64(value));
            break;
        }
        case OP_LOAD8: {
            ++ip;
            uint8_t index = read_u8(block, ip);
            uint64_t constant = read_constant(block, index);
            push(stack, constant);
            break;
        }
        case OP_LOAD16: {
            ip += 2;
            uint16_t index = read_u16(block, ip - 1);
            uint64_t constant = read_constant(block, index);
            push(stack, constant);
            break;
        }
        case OP_LOAD32: {
            ip += 4;
            uint32_t index = read_u32(block, ip - 3);
            uint64_t constant = read_constant(block, index);
            push(stack, constant);
            break;
        }
        case OP_LOAD_STRING8: {
            ++ip;
            uint8_t index = read_u8(block, ip);
            struct string_view *view = read_string(block, index);
            push(stack, (uintptr_t)view->start);
            push(stack, view->length);
            break;
        }
        case OP_LOAD_STRING16: {
            ip += 2;
            uint16_t index = read_u16(block, ip);
            struct string_view *view = read_string(block, index);
            push(stack, (uintptr_t)view->start);
            push(stack, view->length);
            break;
        }
        case OP_LOAD_STRING32: {
            ip += 4;
            uint32_t index = read_u32(block, ip);
            struct string_view *view = read_string(block, index);
            push(stack, (uintptr_t)view->start);
            push(stack, view->length);
            break;
        }
        case OP_POP: pop(stack); break;
        case OP_ADD: BIN_OP(+, stack); break;
        case OP_DEREF: {
            stack_word addr = pop(stack);
            push(stack, *(unsigned char *)(uintptr_t)addr);
            break;
        }
        case OP_DUPE: {
            stack_word a = pop(stack);
            push(stack, a);
            push(stack, a);
            break;
        }
        case OP_EXIT: {
            int64_t exit_code = u64_to_s64(pop(stack));
            if (exit_code < INT_MIN) exit_code = INT_MIN;
            if (exit_code > INT_MAX) exit_code = INT_MAX;
            exit(exit_code);
        }
        case OP_AND: {
            stack_word b = pop(stack);
            stack_word a = pop(stack);
            stack_word result = (!a) ? a : b;
            push(stack, result);
            break;
        }
        case OP_OR: {
            stack_word b = pop(stack);
            stack_word a = pop(stack);
            stack_word result = (a) ? a : b;
            push(stack, result);
            break;
        }
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
            }
            else {
                ip += 2;  // Consume the operand.
            }
            break;
        }
        case OP_JUMP_NCOND: {
            int offset = read_s16(block, ip + 1);
            bool condition = pop(stack);
            if (!condition) {
                jump(block, offset, &ip);
            }
            else {
                ip += 2;
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
        case OP_PRINT_CHAR:
            printf("%c", (char)(uint8_t)pop(stack));
            break;
        }
    }
    return INTERPRET_OK;
}

