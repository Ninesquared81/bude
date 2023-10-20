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


bool init_interpreter(struct interpreter *interpreter, struct ir_block *block) {
    interpreter->block = block;
    interpreter->main_stack = malloc(sizeof *interpreter->main_stack);
    interpreter->auxiliary_stack = malloc(sizeof *interpreter->auxiliary_stack);
    if (interpreter->main_stack == NULL || interpreter->auxiliary_stack == NULL) {
        return false;
    }
    init_stack(interpreter->main_stack);
    init_stack(interpreter->auxiliary_stack);
    return true;
}

void free_interpreter(struct interpreter *interpreter) {
    free(interpreter->main_stack);
    free(interpreter->auxiliary_stack);
    interpreter->main_stack = NULL;
    interpreter->auxiliary_stack = NULL;
}

static void jump(struct ir_block *block, int offset, int *ip) {
    int new_address = *ip + offset;
    // Address -1 means to jump to the start.
    assert(-1 <= new_address && new_address < block->count);
    *ip = new_address;
}

enum interpret_result interpret(struct interpreter *interpreter) {
    for (int ip = 0; ip < interpreter->block->count; ++ip) {
        enum opcode instruction = interpreter->block->code[ip];
        switch (instruction) {
        case OP_NOP:
            // Do nothing.
            break;
        case OP_PUSH8: {
            ++ip;
            int8_t value = read_s8(interpreter->block, ip);
            push(interpreter->main_stack, s64_to_u64(value));
            break;
        }
        case OP_PUSH16: {
            ip += 2;
            int16_t value = read_s16(interpreter->block, ip - 1);
            push(interpreter->main_stack, s64_to_u64(value));
            break;
        }
        case OP_PUSH32: {
            ip += 4;
            int32_t value = read_s32(interpreter->block, ip - 3);
            push(interpreter->main_stack, s64_to_u64(value));
            break;
        }
        case OP_LOAD8: {
            ++ip;
            uint8_t index = read_u8(interpreter->block, ip);
            uint64_t constant = read_constant(interpreter->block, index);
            push(interpreter->main_stack, constant);
            break;
        }
        case OP_LOAD16: {
            ip += 2;
            uint16_t index = read_u16(interpreter->block, ip - 1);
            uint64_t constant = read_constant(interpreter->block, index);
            push(interpreter->main_stack, constant);
            break;
        }
        case OP_LOAD32: {
            ip += 4;
            uint32_t index = read_u32(interpreter->block, ip - 3);
            uint64_t constant = read_constant(interpreter->block, index);
            push(interpreter->main_stack, constant);
            break;
        }
        case OP_LOAD_STRING8: {
            ++ip;
            uint8_t index = read_u8(interpreter->block, ip);
            struct string_view *view = read_string(interpreter->block, index);
            push(interpreter->main_stack, (uintptr_t)view->start);
            push(interpreter->main_stack, view->length);
            break;
        }
        case OP_LOAD_STRING16: {
            ip += 2;
            uint16_t index = read_u16(interpreter->block, ip);
            struct string_view *view = read_string(interpreter->block, index);
            push(interpreter->main_stack, (uintptr_t)view->start);
            push(interpreter->main_stack, view->length);
            break;
        }
        case OP_LOAD_STRING32: {
            ip += 4;
            uint32_t index = read_u32(interpreter->block, ip);
            struct string_view *view = read_string(interpreter->block, index);
            push(interpreter->main_stack, (uintptr_t)view->start);
            push(interpreter->main_stack, view->length);
            break;
        }
        case OP_POP: pop(interpreter->main_stack); break;
        case OP_ADD: BIN_OP(+, interpreter->main_stack); break;
        case OP_DEREF: {
            stack_word addr = pop(interpreter->main_stack);
            push(interpreter->main_stack, *(unsigned char *)(uintptr_t)addr);
            break;
        }
        case OP_DUPE: {
            stack_word a = pop(interpreter->main_stack);
            push(interpreter->main_stack, a);
            push(interpreter->main_stack, a);
            break;
        }
        case OP_EXIT: {
            int64_t exit_code = u64_to_s64(pop(interpreter->main_stack));
            if (exit_code < INT_MIN) exit_code = INT_MIN;
            if (exit_code > INT_MAX) exit_code = INT_MAX;
            exit(exit_code);
        }
        case OP_AND: {
            stack_word b = pop(interpreter->main_stack);
            stack_word a = pop(interpreter->main_stack);
            stack_word result = (!a) ? a : b;
            push(interpreter->main_stack, result);
            break;
        }
        case OP_OR: {
            stack_word b = pop(interpreter->main_stack);
            stack_word a = pop(interpreter->main_stack);
            stack_word result = (a) ? a : b;
            push(interpreter->main_stack, result);
            break;
        }
        case OP_JUMP: {
            int offset = read_s16(interpreter->block, ip + 1);
            jump(interpreter->block, offset, &ip);
            break;
        }
        case OP_JUMP_COND: {
            int offset = read_s16(interpreter->block, ip + 1);
            bool condition = pop(interpreter->main_stack);
            if (condition) {
                jump(interpreter->block, offset, &ip);
            }
            else {
                ip += 2;  // Consume the operand.
            }
            break;
        }
        case OP_JUMP_NCOND: {
            int offset = read_s16(interpreter->block, ip + 1);
            bool condition = pop(interpreter->main_stack);
            if (!condition) {
                jump(interpreter->block, offset, &ip);
            }
            else {
                ip += 2;
            }
            break;
        }
        case OP_FOR_LOOP_START: {
            int skip_jump = read_s16(interpreter->block, ip + 1);
            stack_word counter = pop(interpreter->main_stack);
            if (counter != 0) {
                ip += 2;
                push(interpreter->auxiliary_stack, counter);
            }
            else {
                jump(interpreter->block, skip_jump, &ip);
            }
            break;
        }
        case OP_FOR_LOOP_UPDATE: {
            int loop_jump = read_s16(interpreter->block, ip + 1);
            stack_word counter = pop(interpreter->auxiliary_stack);
            if (--counter != 0) {
                push(interpreter->auxiliary_stack, counter);
                jump(interpreter->block, loop_jump, &ip);
            }
            else {
                ip += 2;
            }
            break;
        }
        case OP_MULT: BIN_OP(*, interpreter->main_stack); break;
        case OP_NOT: {
            bool condition = pop(interpreter->main_stack);
            push(interpreter->main_stack, !condition);
            break;
        }
        case OP_SUB: BIN_OP(-, interpreter->main_stack); break;
        case OP_DIVMOD: {
            stack_word b = pop(interpreter->main_stack);
            stack_word a = pop(interpreter->main_stack);
            push(interpreter->main_stack, a / b);
            push(interpreter->main_stack, a % b);
            break;
        }
        case OP_SWAP: {
            stack_word b = pop(interpreter->main_stack);
            stack_word a = pop(interpreter->main_stack);
            push(interpreter->main_stack, b);
            push(interpreter->main_stack, a);
            break;
        }
        case OP_PRINT:
            printf("%"PRIsw"\n", u64_to_s64(pop(interpreter->main_stack)));
            break;
        case OP_PRINT_CHAR:
            printf("%c", (char)(uint8_t)pop(interpreter->main_stack));
            break;
        }
    }
    return INTERPRET_OK;
}

