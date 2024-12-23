#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "disassembler.h"
#include "function.h"
#include "interpreter.h"
#include "ir.h"
#include "memory.h"
#include "module.h"
#include "stack.h"
#include "type_punning.h"
#include "unicode.h"


#define BIN_OP(op, stack) do {     \
        stack_word b = pop(stack); \
        stack_word a = pop(stack); \
        push(stack, a op b);       \
    } while (0)

#define IBIN_OP(op, stack) do {                 \
        sstack_word b = u64_to_s64(pop(stack)); \
        sstack_word a = u64_to_s64(pop(stack)); \
        push(stack, s64_to_u64(a op b));        \
    } while (0)

#define BINF32_OP(op, stack) do {         \
        float b = u32_to_f32(pop(stack)); \
        float a = u32_to_f32(pop(stack)); \
        push(stack, f32_to_u32(a op b));  \
    } while (0)

#define BINF64_OP(op, stack) do {          \
        double b = u64_to_f64(pop(stack)); \
        double a = u64_to_f64(pop(stack)); \
        push(stack, f64_to_u64(a op b));   \
    } while (0)

bool init_interpreter(struct interpreter *interpreter, struct module *module) {
    interpreter->module = module;
    struct function *main_func = get_function(&module->functions, 0);
    interpreter->current_function = 0;  // Function 0 is the entry point.
    interpreter->ip = 0;
    interpreter->for_loop_level = 0;
    interpreter->block = &main_func->w_code;
    interpreter->main_stack = malloc(sizeof *interpreter->main_stack);
    interpreter->auxiliary_stack = malloc(sizeof *interpreter->auxiliary_stack);
    interpreter->loop_stack = malloc(sizeof *interpreter->loop_stack);
    interpreter->call_stack = malloc(sizeof *interpreter->call_stack);
    if (interpreter->main_stack == NULL || interpreter->auxiliary_stack == NULL
        || interpreter->loop_stack == NULL || interpreter->call_stack == NULL) {
        return false;
    }
    init_stack(interpreter->main_stack);
    init_stack(interpreter->auxiliary_stack);
    init_stack(interpreter->loop_stack);
    init_stack(interpreter->call_stack);
    // Needs to happen after aux has been initialised.
    interpreter->locals = interpreter->auxiliary_stack->elements;
    // Dummy return address to simulate entry point code.
    /* struct pair32 dummy_retinfo = {0, main_func->w_code.count}; */
    /* push(interpreter->call_stack, pair32_to_u64(dummy_retinfo)); */
    return true;
}

void free_interpreter(struct interpreter *interpreter) {
    free(interpreter->main_stack);
    free(interpreter->auxiliary_stack);
    free(interpreter->loop_stack);
    free(interpreter->call_stack);
    interpreter->main_stack = NULL;
    interpreter->auxiliary_stack = NULL;
    interpreter->loop_stack = NULL;
    interpreter->call_stack = NULL;
    interpreter->locals = NULL;
}

static void jump(struct interpreter *interpreter, int offset) {
    interpreter->ip += offset;
    // Address -1 means to jump to the start.
    assert(-1 <= interpreter->ip && interpreter->ip < interpreter->block->count);
}

static stack_word pack_fields(int count, stack_word fields[count], uint8_t sizes[count]) {
    assert(0 < count && count <= 8);
    stack_word pack = 0;
    unsigned char *write_ptr = (unsigned char *)&pack;
    for (int i = 0; i < count; ++i) {
        uint8_t size = sizes[i];
        assert((unsigned char *)&(&pack)[1] - write_ptr >= (int)size);
        memcpy(write_ptr, &fields[i], size);
        write_ptr += size;
    }
    return pack;
}

static void unpack_fields(int count, stack_word fields[count], uint8_t sizes[count],
                          stack_word pack) {
    assert(0 < count && count <= 8);
    const unsigned char *read_ptr = (unsigned char *)&pack;
    for (int i = 0; i < count; ++i) {
        uint8_t size = sizes[i];
        assert((unsigned char *)&(&pack)[1] - read_ptr >= (int)size);
        memcpy(&fields[i], read_ptr, size);
        read_ptr += size;
    }
}

static void swap_comps(struct interpreter *interpreter, int lhs_size, int rhs_size) {
    assert(lhs_size > 0);
    assert(rhs_size > 0);
    stack_word *lhs = malloc(lhs_size * sizeof *lhs);
    CHECK_ALLOCATION(lhs);
    stack_word *rhs = malloc(rhs_size * sizeof *rhs);
    CHECK_ALLOCATION(rhs);
    pop_all(interpreter->main_stack, rhs_size, rhs);
    pop_all(interpreter->main_stack, lhs_size, lhs);
    push_all(interpreter->main_stack, rhs_size, rhs);
    push_all(interpreter->main_stack, lhs_size, lhs);
    free(lhs);
    free(rhs);
}

static void comp_get_subcomp(struct interpreter *interpreter, int offset, int word_count) {
    const stack_word *words = peekn(interpreter->main_stack, offset);
    push_all(interpreter->main_stack, word_count, words);
}

static void comp_set_subcomp(struct interpreter *interpreter, int offset, int word_count) {
    stack_word *subcomp = malloc(word_count * sizeof *subcomp);
    CHECK_ALLOCATION(subcomp);
    stack_word *words = malloc(offset * sizeof *words);
    CHECK_ALLOCATION(words);
    pop_all(interpreter->main_stack, word_count, subcomp);
    pop_all(interpreter->main_stack, offset, words);
    memcpy(words, subcomp, word_count);
    push_all(interpreter->main_stack, offset, words);
    free(subcomp);
    free(words);
}

static void array_get(struct interpreter *interpreter, int element_count, int word_count) {
    sstack_word index = u64_to_s64(pop(interpreter->main_stack));
    sstack_word offset = (element_count - index) * word_count;
    comp_get_subcomp(interpreter, offset, word_count);
}

static void array_set(struct interpreter *interpreter, int element_count, int word_count) {
    sstack_word index = u64_to_s64(pop(interpreter->main_stack));
    sstack_word offset = (element_count - index) * word_count;
    comp_set_subcomp(interpreter, offset, word_count);
}

static void call(struct interpreter *interpreter, int index) {
    struct pair32 retinfo = {interpreter->current_function, interpreter->ip};
    push(interpreter->call_stack, pair32_to_u64(retinfo));
    push(interpreter->loop_stack, interpreter->for_loop_level);
    interpreter->for_loop_level = 0;
    struct function *callee = get_function(&interpreter->module->functions, index);
    interpreter->block = &callee->w_code;
    interpreter->current_function = index;
    push(interpreter->auxiliary_stack, (stack_word)interpreter->locals);
    interpreter->locals = reserve(interpreter->auxiliary_stack, callee->locals_size);
    interpreter->ip = -1;  // -1 since ip will be incremented.
}

static void ret(struct interpreter *interpreter) {
    restore(interpreter->auxiliary_stack, interpreter->locals);
    interpreter->locals = (stack_word *)pop(interpreter->auxiliary_stack);
    popn(interpreter->loop_stack, interpreter->for_loop_level);
    interpreter->for_loop_level = pop(interpreter->loop_stack);
    struct pair32 retinfo = u64_to_pair32(pop(interpreter->call_stack));
    int index = retinfo.a;
    interpreter->ip = retinfo.b;
    struct function *caller = get_function(&interpreter->module->functions, index);
    interpreter->block = &caller->w_code;
    interpreter->current_function = index;
}

enum interpret_result interpret(struct interpreter *interpreter) {
    interpreter->ip = interpreter->block->count;  // For final return.
    call(interpreter, 0);
    for (interpreter->ip = 0; interpreter->ip < interpreter->block->count; ++interpreter->ip) {
        enum w_opcode instruction = interpreter->block->code[interpreter->ip];
        switch (instruction) {
        case W_OP_NOP:
            // Do nothing.
            break;
        case W_OP_PUSH8: {
            ++interpreter->ip;
            uint8_t value = read_u8(interpreter->block, interpreter->ip);
            push(interpreter->main_stack, value);
            break;
        }
        case W_OP_PUSH16: {
            interpreter->ip += 2;
            uint16_t value = read_u16(interpreter->block, interpreter->ip - 1);
            push(interpreter->main_stack, value);
            break;
        }
        case W_OP_PUSH32: {
            interpreter->ip += 4;
            uint32_t value = read_u32(interpreter->block, interpreter->ip - 3);
            push(interpreter->main_stack, value);
            break;
        }
        case W_OP_PUSH64: {
            interpreter->ip += 8;
            uint64_t value = read_u64(interpreter->block, interpreter->ip - 7);
            push(interpreter->main_stack, value);
            break;
        }
        case W_OP_PUSH_INT8: {
            ++interpreter->ip;
            int8_t value = read_s8(interpreter->block, interpreter->ip);
            push(interpreter->main_stack, s64_to_u64(value));
            break;
        }
        case W_OP_PUSH_INT16: {
            interpreter->ip += 2;
            int16_t value = read_s16(interpreter->block, interpreter->ip - 1);
            push(interpreter->main_stack, s64_to_u64(value));
            break;
        }
        case W_OP_PUSH_INT32: {
            interpreter->ip += 4;
            int32_t value = read_s32(interpreter->block, interpreter->ip - 3);
            push(interpreter->main_stack, s64_to_u64(value));
            break;
        }
        case W_OP_PUSH_INT64: {
            interpreter->ip += 8;
            int64_t value = read_s64(interpreter->block, interpreter->ip - 7);
            push(interpreter->main_stack, s64_to_u64(value));
            break;
        }
        case W_OP_PUSH_FLOAT32: {
            interpreter->ip += 4;
            uint32_t bits = read_u32(interpreter->block, interpreter->ip - 3);
            push(interpreter->main_stack, bits);
            break;
        }
        case W_OP_PUSH_FLOAT64: {
            interpreter->ip += 8;
            uint64_t bits = read_u64(interpreter->block, interpreter->ip - 7);
            push(interpreter->main_stack, bits);
            break;
        }
        case W_OP_PUSH_CHAR8: {
            uint8_t value = read_u8(interpreter->block, interpreter->ip + 1);
            interpreter->ip += 1;
            stack_word chr = encode_utf8_u32(value);
            push(interpreter->main_stack, chr);
            break;
        }
        case W_OP_PUSH_CHAR16: {
            uint16_t value = read_u16(interpreter->block, interpreter->ip + 1);
            interpreter->ip += 2;
            stack_word chr = encode_utf8_u32(value);
            push(interpreter->main_stack, chr);
            break;
        }
        case W_OP_PUSH_CHAR32: {
            uint32_t value = read_u32(interpreter->block, interpreter->ip + 1);
            interpreter->ip += 4;
            stack_word chr = encode_utf8_u32(value);
            push(interpreter->main_stack, chr);
            break;
        }
        case W_OP_LOAD_STRING8: {
            ++interpreter->ip;
            uint8_t index = read_u8(interpreter->block, interpreter->ip);
            struct string_view *view = read_string(interpreter->module, index);
            push(interpreter->main_stack, (uintptr_t)view->start);
            push(interpreter->main_stack, view->length);
            break;
        }
        case W_OP_LOAD_STRING16: {
            interpreter->ip += 2;
            uint16_t index = read_u16(interpreter->block, interpreter->ip - 1);
            struct string_view *view = read_string(interpreter->module, index);
            push(interpreter->main_stack, (uintptr_t)view->start);
            push(interpreter->main_stack, view->length);
            break;
        }
        case W_OP_LOAD_STRING32: {
            interpreter->ip += 4;
            uint32_t index = read_u32(interpreter->block, interpreter->ip - 3);
            struct string_view *view = read_string(interpreter->module, index);
            push(interpreter->main_stack, (uintptr_t)view->start);
            push(interpreter->main_stack, view->length);
            break;
        }
        case W_OP_POP: pop(interpreter->main_stack); break;
        case W_OP_POPN8: {
            int8_t n = read_s8(interpreter->block, interpreter->ip + 1);
            interpreter->ip += 1;
            popn(interpreter->main_stack, n);
            break;
        }
        case W_OP_POPN16: {
            int16_t n = read_s16(interpreter->block, interpreter->ip + 1);
            interpreter->ip += 2;
            popn(interpreter->main_stack, n);
            break;
        }
        case W_OP_POPN32: {
            int32_t n = read_s32(interpreter->block, interpreter->ip + 1);
            interpreter->ip += 4;
            popn(interpreter->main_stack, n);
            break;
        }
        case W_OP_ADD: BIN_OP(+, interpreter->main_stack); break;
        case W_OP_ADDF32: BINF32_OP(+, interpreter->main_stack); break;
        case W_OP_ADDF64: BINF64_OP(+, interpreter->main_stack); break;
        case W_OP_DEREF: {
            stack_word addr = pop(interpreter->main_stack);
            push(interpreter->main_stack, *(unsigned char *)(uintptr_t)addr);
            break;
        }
        case W_OP_DUPE: {
            stack_word a = pop(interpreter->main_stack);
            push(interpreter->main_stack, a);
            push(interpreter->main_stack, a);
            break;
        }
        case W_OP_DUPEN8: {
            int8_t n = read_s8(interpreter->block, interpreter->ip + 1);
            interpreter->ip += 1;
            const stack_word *words = peekn(interpreter->main_stack, n);
            push_all(interpreter->main_stack, n, words);
            break;
        }
        case W_OP_DUPEN16: {
            int16_t n = read_s16(interpreter->block, interpreter->ip + 1);
            interpreter->ip += 2;
            const stack_word *words = peekn(interpreter->main_stack, n);
            push_all(interpreter->main_stack, n, words);
            break;
        }
        case W_OP_DUPEN32: {
            int32_t n = read_s32(interpreter->block, interpreter->ip + 1);
            interpreter->ip += 4;
            const stack_word *words = peekn(interpreter->main_stack, n);
            push_all(interpreter->main_stack, n, words);
            break;
        }
        case W_OP_EQUALS: BIN_OP(==, interpreter->main_stack); break;
        case W_OP_EQUALS_F32: BINF32_OP(==, interpreter->main_stack); break;
        case W_OP_EQUALS_F64: BINF64_OP(==, interpreter->main_stack); break;
        case W_OP_EXIT: {
            int64_t exit_code = u64_to_s64(pop(interpreter->main_stack));
            if (exit_code < INT_MIN) exit_code = INT_MIN;
            if (exit_code > INT_MAX) exit_code = INT_MAX;
            exit(exit_code);
        }
        case W_OP_AND: {
            stack_word b = pop(interpreter->main_stack);
            stack_word a = pop(interpreter->main_stack);
            stack_word result = (!a) ? a : b;
            push(interpreter->main_stack, result);
            break;
        }
        case W_OP_OR: {
            stack_word b = pop(interpreter->main_stack);
            stack_word a = pop(interpreter->main_stack);
            stack_word result = (a) ? a : b;
            push(interpreter->main_stack, result);
            break;
        }
        case W_OP_JUMP: {
            int offset = read_s16(interpreter->block, interpreter->ip + 1);
            jump(interpreter, offset);
            break;
        }
        case W_OP_JUMP_COND: {
            int offset = read_s16(interpreter->block, interpreter->ip + 1);
            bool condition = pop(interpreter->main_stack);
            if (condition) {
                jump(interpreter, offset);
            }
            else {
                interpreter->ip += 2;  // Consume the operand.
            }
            break;
        }
        case W_OP_JUMP_NCOND: {
            int offset = read_s16(interpreter->block, interpreter->ip + 1);
            bool condition = pop(interpreter->main_stack);
            if (!condition) {
                jump(interpreter, offset);
            }
            else {
                interpreter->ip += 2;
            }
            break;
        }
        case W_OP_FOR_DEC_START: {
            int skip_jump = read_s16(interpreter->block, interpreter->ip + 1);
            stack_word counter = pop(interpreter->main_stack);
            if (counter > 0) {
                interpreter->ip += 2;
                push(interpreter->loop_stack, counter);
                interpreter->for_loop_level += 1;
            }
            else {
                jump(interpreter, skip_jump);
            }
            break;
        }
        case W_OP_FOR_DEC: {
            int loop_jump = read_s16(interpreter->block, interpreter->ip + 1);
            stack_word counter = pop(interpreter->loop_stack);
            if (--counter > 0) {
                push(interpreter->loop_stack, counter);
                jump(interpreter, loop_jump);
            }
            else {
                interpreter->ip += 2;
                interpreter->for_loop_level -= 1;
            }
            break;
        }
        case W_OP_FOR_INC_START: {
            int skip_jump = read_s16(interpreter->block, interpreter->ip + 1);
            stack_word target = pop(interpreter->main_stack);
            stack_word counter = 0;
            if (counter < target) {
                interpreter->ip += 2;
                push(interpreter->loop_stack, target);
                push(interpreter->loop_stack, counter);
                interpreter->for_loop_level += 2;
            }
            else {
                jump(interpreter, skip_jump);
            }
            break;
        }
        case W_OP_FOR_INC: {
            int loop_jump = read_s16(interpreter->block, interpreter->ip + 1);
            stack_word counter = pop(interpreter->loop_stack);
            stack_word target = peek(interpreter->loop_stack);
            if (++counter < target) {
                push(interpreter->loop_stack, counter);
                jump(interpreter, loop_jump);
            }
            else {
                pop(interpreter->loop_stack);
                interpreter->ip += 2;
                interpreter->for_loop_level -= 2;
            }
            break;
        }
        case W_OP_GET_LOOP_VAR: {
            interpreter->ip += 2;
            uint16_t offset = read_u16(interpreter->block, interpreter->ip - 1);
            stack_word loop_var = peek_nth(interpreter->loop_stack, offset);
            push(interpreter->main_stack, loop_var);
            break;
        }
        case W_OP_GREATER_EQUALS: IBIN_OP(>=, interpreter->main_stack); break;
        case W_OP_GREATER_EQUALS_F32: BINF32_OP(>=, interpreter->main_stack); break;
        case W_OP_GREATER_EQUALS_F64: BINF64_OP(>=, interpreter->main_stack); break;
        case W_OP_GREATER_THAN: IBIN_OP(>, interpreter->main_stack); break;
        case W_OP_GREATER_THAN_F32: BINF32_OP(>, interpreter->main_stack); break;
        case W_OP_GREATER_THAN_F64: BINF64_OP(>, interpreter->main_stack); break;
        case W_OP_HIGHER_SAME: BIN_OP(>=, interpreter->main_stack); break;
        case W_OP_HIGHER_THAN: BIN_OP(>, interpreter->main_stack); break;
        case W_OP_LESS_EQUALS: IBIN_OP(<=, interpreter->main_stack); break;
        case W_OP_LESS_EQUALS_F32: BINF32_OP(<=, interpreter->main_stack); break;
        case W_OP_LESS_EQUALS_F64: BINF64_OP(<=, interpreter->main_stack); break;
        case W_OP_LESS_THAN: IBIN_OP(<, interpreter->main_stack); break;
        case W_OP_LESS_THAN_F32: BINF32_OP(<, interpreter->main_stack); break;
        case W_OP_LESS_THAN_F64: BINF64_OP(<, interpreter->main_stack); break;
        case W_OP_LOCAL_GET: {
            interpreter->ip += 2;
            uint16_t index = read_u16(interpreter->block, interpreter->ip - 1);
            struct function *function = \
                get_function(&interpreter->module->functions, interpreter->current_function);
            struct local local = function->locals.items[index];
            push_all(interpreter->main_stack, local.size, &interpreter->locals[local.offset]);
            break;
        }
        case W_OP_LOCAL_SET: {
            interpreter->ip += 2;
            uint16_t index = read_u16(interpreter->block, interpreter->ip - 1);
            struct function *function = \
                get_function(&interpreter->module->functions, interpreter->current_function);
            struct local local = function->locals.items[index];
            pop_all(interpreter->main_stack, local.size, &interpreter->locals[local.offset]);
            break;
        }
        case W_OP_LOWER_SAME: BIN_OP(<=, interpreter->main_stack); break;
        case W_OP_LOWER_THAN: BIN_OP(<, interpreter->main_stack); break;
        case W_OP_MULT: BIN_OP(*, interpreter->main_stack); break;
        case W_OP_MULTF32: BINF32_OP(*, interpreter->main_stack); break;
        case W_OP_MULTF64: BINF64_OP(*, interpreter->main_stack); break;
        case W_OP_NEG: {
            stack_word a = pop(interpreter->main_stack);
            push(interpreter->main_stack, -a);
            break;
        }
        case W_OP_NEGF32: {
            stack_word a = pop(interpreter->main_stack);
            float x = u32_to_f32(a);
            push(interpreter->main_stack, f32_to_u32(-x));
            break;
        }
        case W_OP_NEGF64: {
            stack_word a = pop(interpreter->main_stack);
            double x = u64_to_f64(a);
            push(interpreter->main_stack, f64_to_u64(-x));
            break;
        }
        case W_OP_NOT: {
            bool condition = pop(interpreter->main_stack);
            push(interpreter->main_stack, !condition);
            break;
        }
        case W_OP_NOT_EQUALS: BIN_OP(!=, interpreter->main_stack); break;
        case W_OP_NOT_EQUALS_F32: BINF32_OP(!=, interpreter->main_stack); break;
        case W_OP_NOT_EQUALS_F64: BINF64_OP(!=, interpreter->main_stack); break;
        case W_OP_SUB: BIN_OP(-, interpreter->main_stack); break;
        case W_OP_SUBF32: BINF32_OP(-, interpreter->main_stack); break;
        case W_OP_SUBF64: BINF64_OP(-, interpreter->main_stack); break;
        case W_OP_DIVF32: BINF32_OP(/, interpreter->main_stack); break;
        case W_OP_DIVF64: BINF64_OP(/, interpreter->main_stack); break;
        case W_OP_DIVMOD: {
            stack_word b = pop(interpreter->main_stack);
            stack_word a = pop(interpreter->main_stack);
            push(interpreter->main_stack, a / b);
            push(interpreter->main_stack, a % b);
            break;
        }
        case W_OP_IDIVMOD: {
            int64_t b = u64_to_s64(pop(interpreter->main_stack));
            int64_t a = u64_to_s64(pop(interpreter->main_stack));
            push(interpreter->main_stack, a / b);
            push(interpreter->main_stack, a % b);
            break;
        }
        case W_OP_EDIVMOD: {
            int64_t b = u64_to_s64(pop(interpreter->main_stack));
            int64_t a = u64_to_s64(pop(interpreter->main_stack));
            int64_t q = a / b;
            int64_t r = a % b;
            if (r < 0) {
                // Adjust r to ensure r >= 0.
                r += llabs(b);
                // Adjust q to maintain a = b*q + r.
                q -= (b > 0) - (b < 0);  // Sign of b.
            }
            push(interpreter->main_stack, q);
            push(interpreter->main_stack, r);
            break;
        }
        case W_OP_SWAP: {
            stack_word b = pop(interpreter->main_stack);
            stack_word a = pop(interpreter->main_stack);
            push(interpreter->main_stack, b);
            push(interpreter->main_stack, a);
            break;
        }
        case W_OP_SWAP_COMPS8: {
            int lhs_size = read_s8(interpreter->block, interpreter->ip + 1);
            int rhs_size = read_s8(interpreter->block, interpreter->ip + 2);
            interpreter->ip += 2;
            swap_comps(interpreter, lhs_size, rhs_size);
            break;
        }
        case W_OP_SWAP_COMPS16: {
            int lhs_size = read_s16(interpreter->block, interpreter->ip + 1);
            int rhs_size = read_s16(interpreter->block, interpreter->ip + 3);
            interpreter->ip += 4;
            swap_comps(interpreter, lhs_size, rhs_size);
            break;
        }
        case W_OP_SWAP_COMPS32: {
            int lhs_size = read_s32(interpreter->block, interpreter->ip + 1);
            int rhs_size = read_s32(interpreter->block, interpreter->ip + 5);
            interpreter->ip += 8;
            swap_comps(interpreter, lhs_size, rhs_size);
            break;
        }
        case W_OP_PRINT:
            printf("%"PRIsw, pop(interpreter->main_stack));
            break;
        case W_OP_PRINT_CHAR: {
            stack_word value = pop(interpreter->main_stack);
            char bytes[8];
            memcpy(bytes, &value, sizeof bytes);
            printf("%s", bytes);
            break;
        }
        case W_OP_PRINT_BOOL:
            printf("%s", pop(interpreter->main_stack) ? "true" : "false");
            break;
        case W_OP_PRINT_FLOAT: {
            uint64_t bits = pop(interpreter->main_stack);
            double value = u64_to_f64(bits);
            printf("%g", value);
            break;
        }
        case W_OP_PRINT_INT:
            printf("%"PRIssw, u64_to_s64(pop(interpreter->main_stack)));
            break;
        case W_OP_PRINT_STRING: {
            stack_word length = pop(interpreter->main_stack);
            char *start = (char *)(uintptr_t)pop(interpreter->main_stack);
            assert(length < INT_MAX);
            printf("%.*s", (int)length, start);
            break;
        }
        case W_OP_SX8: {
            stack_word b = pop(interpreter->main_stack);
            b &= 0xFF;  // Mask off higher bits.
            uint64_t sign = b >> 7;
            uint64_t extension = -sign << 8;
            b |= extension;
            push(interpreter->main_stack, b);
            break;
        }
        case W_OP_SX8L: {
            stack_word b = pop(interpreter->main_stack);
            stack_word a = pop(interpreter->main_stack);
            a &= 0xFF;  // Mask off higher bits.
            uint64_t sign = a >> 7;
            uint64_t extension = -sign << 8;
            a |= extension;
            push(interpreter->main_stack, a);
            push(interpreter->main_stack, b);
            break;
        }
        case W_OP_SX16: {
            stack_word b = pop(interpreter->main_stack);
            b &= 0xFFFF;  // Mask off higher bits.
            uint64_t sign = b >> 15;
            uint64_t extension = -sign << 16;
            b |= extension;
            push(interpreter->main_stack, b);
            break;
        }
        case W_OP_SX16L: {
            stack_word b = pop(interpreter->main_stack);
            stack_word a = pop(interpreter->main_stack);
            a &= 0xFFFF;  // Mask off higher bits.
            uint64_t sign = a >> 15;
            uint64_t extension = -sign << 16;
            a |= extension;
            push(interpreter->main_stack, a);
            push(interpreter->main_stack, b);
            break;
        }
        case W_OP_SX32: {
            stack_word b = pop(interpreter->main_stack);
            b &= 0xFFFFFFFF;  // Mask off higher bits.
            uint64_t sign = b >> 31;
            uint64_t extension = -sign << 32;
            b |= extension;
            push(interpreter->main_stack, b);
            break;
        }
        case W_OP_SX32L: {
            stack_word b = pop(interpreter->main_stack);
            stack_word a = pop(interpreter->main_stack);
            a &= 0xFFFFFFFF;  // Mask off higher bits.
            uint64_t sign = a >> 31;
            uint64_t extension = -sign << 32;
            a |= extension;
            push(interpreter->main_stack, a);
            push(interpreter->main_stack, b);
            break;
        }
        case W_OP_ZX8: {
            stack_word b = pop(interpreter->main_stack);
            b &= 0xFF;
            push(interpreter->main_stack, b);
            break;
        }
        case W_OP_ZX8L: {
            stack_word b = pop(interpreter->main_stack);
            stack_word a = pop(interpreter->main_stack);
            a &= 0xFF;
            push(interpreter->main_stack, a);
            push(interpreter->main_stack, b);
            break;
        }
        case W_OP_ZX16: {
            stack_word b = pop(interpreter->main_stack);
            b &= 0xFFFF;
            push(interpreter->main_stack, b);
            break;
        }
        case W_OP_ZX16L: {
            stack_word b = pop(interpreter->main_stack);
            stack_word a = pop(interpreter->main_stack);
            a &= 0xFFFF;
            push(interpreter->main_stack, a);
            push(interpreter->main_stack, b);
            break;
        }
        case W_OP_ZX32: {
            stack_word b = pop(interpreter->main_stack);
            b &= 0xFFFFFFFF;
            push(interpreter->main_stack, b);
            break;
        }
        case W_OP_ZX32L: {
            stack_word b = pop(interpreter->main_stack);
            stack_word a = pop(interpreter->main_stack);
            a &= 0xFFFFFFFF;
            push(interpreter->main_stack, a);
            push(interpreter->main_stack, b);
            break;
        }
        case W_OP_FPROM: {
            stack_word bits = pop(interpreter->main_stack);
            double value = u32_to_f32(bits);
            push(interpreter->main_stack, f64_to_u64(value));
            break;
        }
        case W_OP_FPROML: {
            stack_word top = pop(interpreter->main_stack);
            stack_word bits = pop(interpreter->main_stack);
            double value = u32_to_f32(bits);
            push(interpreter->main_stack, f64_to_u64(value));
            push(interpreter->main_stack, top);
            break;
        }
        case W_OP_FDEM: {
            stack_word bits = pop(interpreter->main_stack);
            float value = u64_to_f64(bits);
            push(interpreter->main_stack, f32_to_u32(value));
            break;
        }
        case W_OP_ICONVF32: {
            sstack_word integer_value = u64_to_s64(pop(interpreter->main_stack));
            float floating_value = integer_value;
            push(interpreter->main_stack, f32_to_u32(floating_value));
            break;
        }
        case W_OP_ICONVF32L: {
            stack_word top = pop(interpreter->main_stack);
            sstack_word integer_value = u64_to_s64(pop(interpreter->main_stack));
            float floating_value = integer_value;
            push(interpreter->main_stack, f32_to_u32(floating_value));
            push(interpreter->main_stack, top);
            break;
        }
        case W_OP_ICONVF64: {
            sstack_word integer_value = u64_to_s64(pop(interpreter->main_stack));
            double floating_value = integer_value;
            push(interpreter->main_stack, f64_to_u64(floating_value));
            break;
        }
        case W_OP_ICONVF64L: {
            stack_word top = pop(interpreter->main_stack);
            sstack_word integer_value = u64_to_s64(pop(interpreter->main_stack));
            double floating_value = integer_value;
            push(interpreter->main_stack, f64_to_u64(floating_value));
            push(interpreter->main_stack, top);
            break;
        }
        case W_OP_FCONVI32: {
            float floating_value = u32_to_f32(pop(interpreter->main_stack));
            sstack_word integer_value = floating_value;
            push(interpreter->main_stack, s64_to_u64(integer_value));
            break;
        }
        case W_OP_FCONVI64: {
            double floating_value = u64_to_f64(pop(interpreter->main_stack));
            sstack_word integer_value = floating_value;
            push(interpreter->main_stack, s64_to_u64(integer_value));
            break;
        }
        case W_OP_ICONVB: {
            stack_word integer_value = pop(interpreter->main_stack);
            push(interpreter->main_stack, integer_value != 0);
            break;
        }
        case W_OP_FCONVB32: {
            float floating_value = u32_to_f32(pop(interpreter->main_stack));
            push(interpreter->main_stack, floating_value != 0.0f && !isnan(floating_value));
            break;
        }
        case W_OP_FCONVB64: {
            double floating_value = u64_to_f64(pop(interpreter->main_stack));
            push(interpreter->main_stack, floating_value != 0.0 && !isnan(floating_value));
            break;
        }
        case W_OP_ICONVC32: {
            sstack_word integer_value = u64_to_s64(pop(interpreter->main_stack));
            if (integer_value < 0) integer_value = 0;
            if (integer_value > UNICODE_MAX) integer_value = UNICODE_MAX;
            push(interpreter->main_stack, s64_to_u64(integer_value));
            break;
        }
        case W_OP_CHAR_8CONV32: {
            stack_word bytes = pop(interpreter->main_stack);
            uint32_t codepoint = decode_utf8((void *)&bytes, NULL);
            push(interpreter->main_stack, codepoint);
            break;
        }
        case W_OP_CHAR_32CONV8: {
            stack_word codepoint = pop(interpreter->main_stack);
            stack_word char_value = encode_utf8_u32(codepoint);
            push(interpreter->main_stack, char_value);
            break;
        }
        case W_OP_CHAR_16CONV32: {
            stack_word bytes = pop(interpreter->main_stack);
            stack_word codepoint = decode_utf16((void *)&bytes, NULL);
            push(interpreter->main_stack, codepoint);
            break;
        }
        case W_OP_CHAR_32CONV16: {
            stack_word codepoint = pop(interpreter->main_stack);
            stack_word char16_value = encode_utf16_u32(codepoint);
            push(interpreter->main_stack, char16_value);
            break;
        }
        case W_OP_PACK1: {
            ++interpreter->ip;
            // We don't need to actually do anything here.
            break;
        }
        case W_OP_PACK2: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, interpreter->ip + 1),
                read_u8(interpreter->block, interpreter->ip + 2),
            };
            interpreter->ip += 2;
            stack_word fields[] = {
                peek_nth(interpreter->main_stack, 1),
                peek(interpreter->main_stack)
            };
            popn(interpreter->main_stack, 2);
            stack_word pack = pack_fields(2, fields, sizes);
            push(interpreter->main_stack, pack);
            break;
        }
        case W_OP_PACK3: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, interpreter->ip + 1),
                read_u8(interpreter->block, interpreter->ip + 2),
                read_u8(interpreter->block, interpreter->ip + 3),
            };
            interpreter->ip += 3;
            stack_word fields[] = {
                peek_nth(interpreter->main_stack, 2),
                peek_nth(interpreter->main_stack, 1),
                peek(interpreter->main_stack)
            };
            popn(interpreter->main_stack, 3);
            stack_word pack = pack_fields(3, fields, sizes);
            push(interpreter->main_stack, pack);
            break;
        }
        case W_OP_PACK4: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, interpreter->ip + 1),
                read_u8(interpreter->block, interpreter->ip + 2),
                read_u8(interpreter->block, interpreter->ip + 3),
                read_u8(interpreter->block, interpreter->ip + 4),
            };
            interpreter->ip += 4;
            stack_word fields[] = {
                peek_nth(interpreter->main_stack, 3),
                peek_nth(interpreter->main_stack, 2),
                peek_nth(interpreter->main_stack, 1),
                peek(interpreter->main_stack)
            };
            popn(interpreter->main_stack, 4);
            stack_word pack = pack_fields(4, fields, sizes);
            push(interpreter->main_stack, pack);
            break;
        }
        case W_OP_PACK5: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, interpreter->ip + 1),
                read_u8(interpreter->block, interpreter->ip + 2),
                read_u8(interpreter->block, interpreter->ip + 3),
                read_u8(interpreter->block, interpreter->ip + 4),
                read_u8(interpreter->block, interpreter->ip + 5),
            };
            interpreter->ip += 5;
            stack_word fields[] = {
                peek_nth(interpreter->main_stack, 4),
                peek_nth(interpreter->main_stack, 3),
                peek_nth(interpreter->main_stack, 2),
                peek_nth(interpreter->main_stack, 1),
                peek(interpreter->main_stack)
            };
            popn(interpreter->main_stack, 5);
            stack_word pack = pack_fields(5, fields, sizes);
            push(interpreter->main_stack, pack);
            break;
        }
        case W_OP_PACK6: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, interpreter->ip + 1),
                read_u8(interpreter->block, interpreter->ip + 2),
                read_u8(interpreter->block, interpreter->ip + 3),
                read_u8(interpreter->block, interpreter->ip + 4),
                read_u8(interpreter->block, interpreter->ip + 5),
                read_u8(interpreter->block, interpreter->ip + 6),
            };
            interpreter->ip += 6;
            stack_word fields[] = {
                peek_nth(interpreter->main_stack, 5),
                peek_nth(interpreter->main_stack, 4),
                peek_nth(interpreter->main_stack, 3),
                peek_nth(interpreter->main_stack, 2),
                peek_nth(interpreter->main_stack, 1),
                peek(interpreter->main_stack)
            };
            popn(interpreter->main_stack, 6);
            stack_word pack = pack_fields(6, fields, sizes);
            push(interpreter->main_stack, pack);
            break;
        }
        case W_OP_PACK7: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, interpreter->ip + 1),
                read_u8(interpreter->block, interpreter->ip + 2),
                read_u8(interpreter->block, interpreter->ip + 3),
                read_u8(interpreter->block, interpreter->ip + 4),
                read_u8(interpreter->block, interpreter->ip + 5),
                read_u8(interpreter->block, interpreter->ip + 6),
                read_u8(interpreter->block, interpreter->ip + 7),
            };
            interpreter->ip += 7;
            stack_word fields[] = {
                peek_nth(interpreter->main_stack, 6),
                peek_nth(interpreter->main_stack, 5),
                peek_nth(interpreter->main_stack, 4),
                peek_nth(interpreter->main_stack, 3),
                peek_nth(interpreter->main_stack, 2),
                peek_nth(interpreter->main_stack, 1),
                peek(interpreter->main_stack)
            };
            popn(interpreter->main_stack, 7);
            stack_word pack = pack_fields(7, fields, sizes);
            push(interpreter->main_stack, pack);
            break;
        }
        case W_OP_PACK8: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, interpreter->ip + 1),
                read_u8(interpreter->block, interpreter->ip + 2),
                read_u8(interpreter->block, interpreter->ip + 3),
                read_u8(interpreter->block, interpreter->ip + 4),
                read_u8(interpreter->block, interpreter->ip + 5),
                read_u8(interpreter->block, interpreter->ip + 6),
                read_u8(interpreter->block, interpreter->ip + 7),
                read_u8(interpreter->block, interpreter->ip + 8),
            };
            interpreter->ip += 8;
            stack_word fields[] = {
                peek_nth(interpreter->main_stack, 7),
                peek_nth(interpreter->main_stack, 6),
                peek_nth(interpreter->main_stack, 5),
                peek_nth(interpreter->main_stack, 4),
                peek_nth(interpreter->main_stack, 3),
                peek_nth(interpreter->main_stack, 2),
                peek_nth(interpreter->main_stack, 1),
                peek(interpreter->main_stack)
            };
            popn(interpreter->main_stack, 8);
            stack_word pack = pack_fields(8, fields, sizes);
            push(interpreter->main_stack, pack);
            break;
        }
        case W_OP_UNPACK1: {
            ++interpreter->ip;
            // We don't need to actually do anything here.
            break;
        }
        case W_OP_UNPACK2: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, interpreter->ip + 1),
                read_u8(interpreter->block, interpreter->ip + 2),
            };
            interpreter->ip += 2;
            stack_word fields[2] = {0};
            stack_word pack = pop(interpreter->main_stack);
            unpack_fields(2, fields, sizes, pack);
            push_all(interpreter->main_stack, 2, fields);
            break;
        }
        case W_OP_UNPACK3: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, interpreter->ip + 1),
                read_u8(interpreter->block, interpreter->ip + 2),
                read_u8(interpreter->block, interpreter->ip + 3),
            };
            interpreter->ip += 3;
            stack_word fields[3] = {0};
            stack_word pack = pop(interpreter->main_stack);
            unpack_fields(3, fields, sizes, pack);
            push_all(interpreter->main_stack, 3, fields);
            break;
        }
        case W_OP_UNPACK4: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, interpreter->ip + 1),
                read_u8(interpreter->block, interpreter->ip + 2),
                read_u8(interpreter->block, interpreter->ip + 3),
                read_u8(interpreter->block, interpreter->ip + 4),
            };
            interpreter->ip += 4;
            stack_word fields[4] = {0};
            stack_word pack = pop(interpreter->main_stack);
            unpack_fields(4, fields, sizes, pack);
            push_all(interpreter->main_stack, 4, fields);
            break;
        }
        case W_OP_UNPACK5: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, interpreter->ip + 1),
                read_u8(interpreter->block, interpreter->ip + 2),
                read_u8(interpreter->block, interpreter->ip + 3),
                read_u8(interpreter->block, interpreter->ip + 4),
                read_u8(interpreter->block, interpreter->ip + 5),
            };
            interpreter->ip += 5;
            stack_word fields[5] = {0};
            stack_word pack = pop(interpreter->main_stack);
            unpack_fields(5, fields, sizes, pack);
            push_all(interpreter->main_stack, 5, fields);
            break;
        }
        case W_OP_UNPACK6: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, interpreter->ip + 1),
                read_u8(interpreter->block, interpreter->ip + 2),
                read_u8(interpreter->block, interpreter->ip + 3),
                read_u8(interpreter->block, interpreter->ip + 4),
                read_u8(interpreter->block, interpreter->ip + 5),
                read_u8(interpreter->block, interpreter->ip + 6),
            };
            interpreter->ip += 6;
            stack_word fields[6] = {0};
            stack_word pack = pop(interpreter->main_stack);
            unpack_fields(6, fields, sizes, pack);
            push_all(interpreter->main_stack, 6, fields);
            break;
        }
        case W_OP_UNPACK7: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, interpreter->ip + 1),
                read_u8(interpreter->block, interpreter->ip + 2),
                read_u8(interpreter->block, interpreter->ip + 3),
                read_u8(interpreter->block, interpreter->ip + 4),
                read_u8(interpreter->block, interpreter->ip + 5),
                read_u8(interpreter->block, interpreter->ip + 6),
                read_u8(interpreter->block, interpreter->ip + 7),
            };
            interpreter->ip += 7;
            stack_word fields[7] = {0};
            stack_word pack = pop(interpreter->main_stack);
            unpack_fields(7, fields, sizes, pack);
            push_all(interpreter->main_stack, 7, fields);
            break;
        }
        case W_OP_UNPACK8: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, interpreter->ip + 1),
                read_u8(interpreter->block, interpreter->ip + 2),
                read_u8(interpreter->block, interpreter->ip + 3),
                read_u8(interpreter->block, interpreter->ip + 4),
                read_u8(interpreter->block, interpreter->ip + 5),
                read_u8(interpreter->block, interpreter->ip + 6),
                read_u8(interpreter->block, interpreter->ip + 7),
                read_u8(interpreter->block, interpreter->ip + 8),
            };
            interpreter->ip += 8;
            stack_word fields[8] = {0};
            stack_word pack = pop(interpreter->main_stack);
            unpack_fields(8, fields, sizes, pack);
            push_all(interpreter->main_stack, 8, fields);
            break;
        }
        case W_OP_PACK_FIELD_GET: {
            uint8_t offset = read_u8(interpreter->block, ++interpreter->ip);
            uint8_t size = read_u8(interpreter->block, ++interpreter->ip);
            stack_word pack = peek(interpreter->main_stack);
            stack_word field = 0;
            memcpy(&field, (unsigned char *)&pack + offset, size);
            push(interpreter->main_stack, field);
            break;
        }
        case W_OP_COMP_FIELD_GET8: {
            uint8_t offset = read_u8(interpreter->block, interpreter->ip + 1);
            interpreter->ip += 1;
            stack_word field = peek_nth(interpreter->main_stack, offset - 1);
            push(interpreter->main_stack, field);
            break;
        }
        case W_OP_COMP_FIELD_GET16: {
            uint16_t offset = read_u16(interpreter->block, interpreter->ip + 1);
            interpreter->ip += 2;
            stack_word field = peek_nth(interpreter->main_stack, offset - 1);
            push(interpreter->main_stack, field);
            break;
        }
        case W_OP_COMP_FIELD_GET32: {
            uint32_t offset = read_u32(interpreter->block, interpreter->ip + 1);
            interpreter->ip += 4;
            stack_word field = peek_nth(interpreter->main_stack, offset - 1);
            push(interpreter->main_stack, field);
            break;
        }
        case W_OP_PACK_FIELD_SET: {
            int8_t offset = read_s8(interpreter->block, interpreter->ip + 1);
            int8_t size = read_s8(interpreter->block, interpreter->ip + 2);
            interpreter->ip += 2;
            stack_word field = pop(interpreter->main_stack);
            stack_word pack = pop(interpreter->main_stack);
            memcpy((unsigned char *)&pack + offset, &field, size);
            push(interpreter->main_stack, pack);
            break;
        }
        case W_OP_COMP_FIELD_SET8: {
            int8_t offset = read_s8(interpreter->block, interpreter->ip + 1);
            interpreter->ip += 1;
            stack_word field = pop(interpreter->main_stack);
            set_nth(interpreter->main_stack, offset - 1, field);
            break;
        }
        case W_OP_COMP_FIELD_SET16: {
            int16_t offset = read_s16(interpreter->block, interpreter->ip + 1);
            interpreter->ip += 2;
            stack_word field = pop(interpreter->main_stack);
            set_nth(interpreter->main_stack, offset - 1, field);
            break;
        }
        case W_OP_COMP_FIELD_SET32: {
            int32_t offset = read_s32(interpreter->block, interpreter->ip + 1);
            interpreter->ip += 4;
            stack_word field = pop(interpreter->main_stack);
            set_nth(interpreter->main_stack, offset - 1, field);
            break;
        }
        case W_OP_COMP_SUBCOMP_GET8: {
            int8_t offset = read_s8(interpreter->block, interpreter->ip + 1);
            int8_t word_count = read_s8(interpreter->block, interpreter->ip + 2);
            interpreter->ip += 2;
            comp_get_subcomp(interpreter, offset, word_count);
            break;
        }
        case W_OP_COMP_SUBCOMP_GET16: {
            int16_t offset = read_s16(interpreter->block, interpreter->ip + 1);
            int16_t word_count = read_s16(interpreter->block, interpreter->ip + 3);
            interpreter->ip += 4;
            comp_get_subcomp(interpreter, offset, word_count);
            break;
        }
        case W_OP_COMP_SUBCOMP_GET32: {
            int32_t offset = read_s32(interpreter->block, interpreter->ip + 1);
            int32_t word_count = read_s32(interpreter->block, interpreter->ip + 5);
            interpreter->ip += 8;
            comp_get_subcomp(interpreter, offset, word_count);
            break;
        }
        case W_OP_COMP_SUBCOMP_SET8: {
            int8_t offset = read_s8(interpreter->block, interpreter->ip + 1);
            int8_t word_count = read_s8(interpreter->block, interpreter->ip + 2);
            interpreter->ip += 2;
            comp_set_subcomp(interpreter, offset, word_count);
            break;
        }
        case W_OP_COMP_SUBCOMP_SET16: {
            int16_t offset = read_s16(interpreter->block, interpreter->ip + 1);
            int16_t word_count = read_s16(interpreter->block, interpreter->ip + 3);
            interpreter->ip += 4;
            comp_set_subcomp(interpreter, offset, word_count);
            break;
        }
        case W_OP_COMP_SUBCOMP_SET32: {
            int32_t offset = read_s32(interpreter->block, interpreter->ip + 1);
            int32_t word_count = read_s32(interpreter->block, interpreter->ip + 5);
            interpreter->ip += 8;
            comp_set_subcomp(interpreter, offset, word_count);
            break;
        }
        case W_OP_ARRAY_GET8: {
            int element_count = read_u8(interpreter->block, interpreter->ip + 1);
            int word_count = read_u8(interpreter->block, interpreter->ip + 2);
            interpreter->ip += 2;
            array_get(interpreter, element_count, word_count);
            break;
        }
        case W_OP_ARRAY_GET16: {
            int element_count = read_u8(interpreter->block, interpreter->ip + 1);
            int word_count = read_u16(interpreter->block, interpreter->ip + 3);
            interpreter->ip += 4;
            array_get(interpreter, element_count, word_count);
            break;
        }
        case W_OP_ARRAY_GET32: {
            int element_count = read_u8(interpreter->block, interpreter->ip + 1);
            int word_count = read_u32(interpreter->block, interpreter->ip + 5);
            interpreter->ip += 8;
            array_get(interpreter, element_count, word_count);
            break;
        }
        case W_OP_ARRAY_SET8: {
            int element_count = read_u8(interpreter->block, interpreter->ip + 1);
            int word_count = read_u8(interpreter->block, interpreter->ip + 2);
            interpreter->ip += 2;
            array_set(interpreter, element_count, word_count);
            break;
        }
        case W_OP_ARRAY_SET16: {
            int element_count = read_u8(interpreter->block, interpreter->ip + 1);
            int word_count = read_u16(interpreter->block, interpreter->ip + 3);
            interpreter->ip += 4;
            array_set(interpreter, element_count, word_count);
            break;
        }
        case W_OP_ARRAY_SET32: {
            int element_count = read_u8(interpreter->block, interpreter->ip + 1);
            int word_count = read_u32(interpreter->block, interpreter->ip + 5);
            interpreter->ip += 8;
            array_set(interpreter, element_count, word_count);
            break;
        }
        case W_OP_CALL8: {
            uint8_t index = read_u8(interpreter->block, interpreter->ip + 1);
            interpreter->ip += 1;
            call(interpreter, index);
            break;
        }
        case W_OP_CALL16: {
            uint16_t index = read_u16(interpreter->block, interpreter->ip + 1);
            interpreter->ip += 2;
            call(interpreter, index);
            break;
        }
        case W_OP_CALL32: {
            uint32_t index = read_u32(interpreter->block, interpreter->ip + 1);
            interpreter->ip += 4;
            call(interpreter, index);
            break;
        }
        case W_OP_EXTCALL8:
        case W_OP_EXTCALL16:
        case W_OP_EXTCALL32:
            assert(false && "Not implemented");
            break;
        case W_OP_RET:
            ret(interpreter);
            break;
        }
    }
    return INTERPRET_OK;
}
