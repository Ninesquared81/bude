#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "disassembler.h"
#include "function.h"
#include "interpreter.h"
#include "ir.h"
#include "stack.h"
#include "type_punning.h"
#include "unicode.h"


#define BIN_OP(op, stack) do {     \
        stack_word b = pop(stack); \
        stack_word a = pop(stack); \
        push(stack, a op b);       \
    } while (0)


bool init_interpreter(struct interpreter *interpreter, struct function_table *functions) {
    interpreter->functions = functions;
    struct function *main_func = get_function(functions, 0);
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
}

static void jump(struct ir_block *block, int offset, int *ip) {
    int new_address = *ip + offset;
    // Address -1 means to jump to the start.
    assert(-1 <= new_address && new_address < block->count);
    *ip = new_address;
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
    stack_word *rhs = malloc(rhs_size * sizeof *rhs);
    pop_all(interpreter->main_stack, rhs_size, rhs);
    pop_all(interpreter->main_stack, lhs_size, lhs);
    push_all(interpreter->main_stack, rhs_size, rhs);
    push_all(interpreter->main_stack, lhs_size, lhs);
    free(lhs);
    free(rhs);
}

static void call(struct interpreter *interpreter, int index, int *ip) {
    struct pair32 retinfo = {interpreter->current_function, *ip};
    push(interpreter->call_stack, pair32_to_u64(retinfo));
    struct function *callee = get_function(interpreter->functions, index);
    interpreter->block = &callee->w_code;
    interpreter->current_function = index;
    *ip = -1;  // -1 since ip will be incremented.
}

static int ret(struct interpreter *interpreter) {
    struct pair32 retinfo = u64_to_pair32(pop(interpreter->call_stack));
    int index = retinfo.a;
    int ip = retinfo.b;
    struct function *caller = get_function(interpreter->functions, index);
    interpreter->block = &caller->w_code;
    interpreter->current_function = index;
    return ip;
}

enum interpret_result interpret(struct interpreter *interpreter) {
    for (int ip = 0; ip < interpreter->block->count; ++ip) {
        enum w_opcode instruction = interpreter->block->code[ip];
        switch (instruction) {
        case W_OP_NOP:
            // Do nothing.
            break;
        case W_OP_PUSH8: {
            ++ip;
            uint8_t value = read_u8(interpreter->block, ip);
            push(interpreter->main_stack, value);
            break;
        }
        case W_OP_PUSH16: {
            ip += 2;
            uint16_t value = read_u16(interpreter->block, ip - 1);
            push(interpreter->main_stack, value);
            break;
        }
        case W_OP_PUSH32: {
            ip += 4;
            uint32_t value = read_u32(interpreter->block, ip - 3);
            push(interpreter->main_stack, value);
            break;
        }
        case W_OP_PUSH64: {
            ip += 8;
            uint64_t value = read_u64(interpreter->block, ip - 7);
            push(interpreter->main_stack, value);
            break;
        }
        case W_OP_PUSH_INT8: {
            ++ip;
            int8_t value = read_s8(interpreter->block, ip);
            push(interpreter->main_stack, s64_to_u64(value));
            break;
        }
        case W_OP_PUSH_INT16: {
            ip += 2;
            int16_t value = read_s16(interpreter->block, ip - 1);
            push(interpreter->main_stack, s64_to_u64(value));
            break;
        }
        case W_OP_PUSH_INT32: {
            ip += 4;
            int32_t value = read_s32(interpreter->block, ip - 3);
            push(interpreter->main_stack, s64_to_u64(value));
            break;
        }
        case W_OP_PUSH_INT64: {
            ip += 8;
            int64_t value = read_s64(interpreter->block, ip - 7);
            push(interpreter->main_stack, s64_to_u64(value));
            break;
        }
        case W_OP_PUSH_CHAR8: {
            uint8_t value = read_u8(interpreter->block, ip + 1);
            ip += 1;
            stack_word chr = encode_utf8_u32(value);
            push(interpreter->main_stack, chr);
            break;
        }
        case W_OP_PUSH_CHAR16: {
            uint16_t value = read_u16(interpreter->block, ip + 1);
            ip += 2;
            stack_word chr = encode_utf8_u32(value);
            push(interpreter->main_stack, chr);
            break;
        }
        case W_OP_PUSH_CHAR32: {
            uint32_t value = read_u32(interpreter->block, ip + 1);
            ip += 4;
            stack_word chr = encode_utf8_u32(value);
            push(interpreter->main_stack, chr);
            break;
        }
        case W_OP_LOAD_STRING8: {
            ++ip;
            uint8_t index = read_u8(interpreter->block, ip);
            struct string_view *view = read_string(interpreter->block, index);
            push(interpreter->main_stack, (uintptr_t)view->start);
            push(interpreter->main_stack, view->length);
            break;
        }
        case W_OP_LOAD_STRING16: {
            ip += 2;
            uint16_t index = read_u16(interpreter->block, ip - 1);
            struct string_view *view = read_string(interpreter->block, index);
            push(interpreter->main_stack, (uintptr_t)view->start);
            push(interpreter->main_stack, view->length);
            break;
        }
        case W_OP_LOAD_STRING32: {
            ip += 4;
            uint32_t index = read_u32(interpreter->block, ip - 3);
            struct string_view *view = read_string(interpreter->block, index);
            push(interpreter->main_stack, (uintptr_t)view->start);
            push(interpreter->main_stack, view->length);
            break;
        }
        case W_OP_POP: pop(interpreter->main_stack); break;
        case W_OP_POPN8: {
            int8_t n = read_s8(interpreter->block, ip + 1);
            ip += 1;
            popn(interpreter->main_stack, n);
            break;
        }
        case W_OP_POPN16: {
            int16_t n = read_s16(interpreter->block, ip + 1);
            ip += 2;
            popn(interpreter->main_stack, n);
            break;
        }
        case W_OP_POPN32: {
            int32_t n = read_s32(interpreter->block, ip + 1);
            ip += 4;
            popn(interpreter->main_stack, n);
            break;
        }
        case W_OP_ADD: BIN_OP(+, interpreter->main_stack); break;
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
            int8_t n = read_s8(interpreter->block, ip + 1);
            ip += 1;
            const stack_word *words = peekn(interpreter->main_stack, n);
            push_all(interpreter->main_stack, n, words);
            break;
        }
        case W_OP_DUPEN16: {
            int16_t n = read_s16(interpreter->block, ip + 1);
            ip += 2;
            const stack_word *words = peekn(interpreter->main_stack, n);
            push_all(interpreter->main_stack, n, words);
            break;
        }
        case W_OP_DUPEN32: {
            int32_t n = read_s32(interpreter->block, ip + 1);
            ip += 4;
            const stack_word *words = peekn(interpreter->main_stack, n);
            push_all(interpreter->main_stack, n, words);
            break;
        }
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
            int offset = read_s16(interpreter->block, ip + 1);
            jump(interpreter->block, offset, &ip);
            break;
        }
        case W_OP_JUMP_COND: {
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
        case W_OP_JUMP_NCOND: {
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
        case W_OP_FOR_DEC_START: {
            int skip_jump = read_s16(interpreter->block, ip + 1);
            stack_word counter = pop(interpreter->main_stack);
            if (counter != 0) {
                ip += 2;
                push(interpreter->loop_stack, counter);
            }
            else {
                jump(interpreter->block, skip_jump, &ip);
            }
            break;
        }
        case W_OP_FOR_DEC: {
            int loop_jump = read_s16(interpreter->block, ip + 1);
            stack_word counter = pop(interpreter->loop_stack);
            if (--counter != 0) {
                push(interpreter->loop_stack, counter);
                jump(interpreter->block, loop_jump, &ip);
            }
            else {
                ip += 2;
            }
            break;
        }
        case W_OP_FOR_INC_START: {
            int skip_jump = read_s16(interpreter->block, ip + 1);
            stack_word counter = pop(interpreter->main_stack);
            if (counter != 0) {
                ip += 2;
                push(interpreter->loop_stack, 0);
                push(interpreter->auxiliary_stack, counter);
            }
            else {
                jump(interpreter->block, skip_jump, &ip);
            }
            break;
        }
        case W_OP_FOR_INC: {
            int loop_jump = read_s16(interpreter->block, ip + 1);
            stack_word target = peek(interpreter->auxiliary_stack);
            stack_word counter = pop(interpreter->loop_stack);
            if (++counter < target) {
                push(interpreter->loop_stack, counter);
                jump(interpreter->block, loop_jump, &ip);
            }
            else {
                pop(interpreter->auxiliary_stack);
                ip += 2;
            }
            break;
        }
        case W_OP_GET_LOOP_VAR: {
            ip += 2;
            uint16_t offset = read_u16(interpreter->block, ip - 1);
            stack_word loop_var = peek_nth(interpreter->loop_stack, offset);
            push(interpreter->main_stack, loop_var);
            break;
        }
        case W_OP_MULT: BIN_OP(*, interpreter->main_stack); break;
        case W_OP_NOT: {
            bool condition = pop(interpreter->main_stack);
            push(interpreter->main_stack, !condition);
            break;
        }
        case W_OP_SUB: BIN_OP(-, interpreter->main_stack); break;
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
            int lhs_size = read_s8(interpreter->block, ip + 1);
            int rhs_size = read_s8(interpreter->block, ip + 2);
            ip += 2;
            swap_comps(interpreter, lhs_size, rhs_size);
            break;
        }
        case W_OP_SWAP_COMPS16: {
            int lhs_size = read_s16(interpreter->block, ip + 1);
            int rhs_size = read_s16(interpreter->block, ip + 3);
            ip += 4;
            swap_comps(interpreter, lhs_size, rhs_size);
            break;
        }
        case W_OP_SWAP_COMPS32: {
            int lhs_size = read_s32(interpreter->block, ip + 1);
            int rhs_size = read_s32(interpreter->block, ip + 5);
            ip += 8;
            swap_comps(interpreter, lhs_size, rhs_size);
            break;
        }
        case W_OP_PRINT:
            printf("%"PRIsw"\n", pop(interpreter->main_stack));
            break;
        case W_OP_PRINT_CHAR: {
            stack_word value = pop(interpreter->main_stack);
            char bytes[4];
            memcpy(bytes, &value, sizeof bytes);
            printf("%s", bytes);
            break;
        }
        case W_OP_PRINT_INT:
            printf("%"PRIssw"\n", u64_to_s64(pop(interpreter->main_stack)));
            break;
        case W_OP_PRINT_STRING: {
            stack_word length = pop(interpreter->main_stack);
            char *start = (char *)(uintptr_t)pop(interpreter->main_stack);
            assert(length < INT_MAX);
            printf("%*s", (int)length, start);
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
        case W_OP_PACK1: {
            ++ip;
            // We don't need to actually do anything here.
            break;
        }
        case W_OP_PACK2: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, ip + 1),
                read_u8(interpreter->block, ip + 2),
            };
            ip += 2;
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
                read_u8(interpreter->block, ip + 1),
                read_u8(interpreter->block, ip + 2),
                read_u8(interpreter->block, ip + 3),
            };
            ip += 3;
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
                read_u8(interpreter->block, ip + 1),
                read_u8(interpreter->block, ip + 2),
                read_u8(interpreter->block, ip + 3),
                read_u8(interpreter->block, ip + 4),
            };
            ip += 4;
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
                read_u8(interpreter->block, ip + 1),
                read_u8(interpreter->block, ip + 2),
                read_u8(interpreter->block, ip + 3),
                read_u8(interpreter->block, ip + 4),
                read_u8(interpreter->block, ip + 5),
            };
            ip += 5;
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
                read_u8(interpreter->block, ip + 1),
                read_u8(interpreter->block, ip + 2),
                read_u8(interpreter->block, ip + 3),
                read_u8(interpreter->block, ip + 4),
                read_u8(interpreter->block, ip + 5),
                read_u8(interpreter->block, ip + 6),
            };
            ip += 6;
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
                read_u8(interpreter->block, ip + 1),
                read_u8(interpreter->block, ip + 2),
                read_u8(interpreter->block, ip + 3),
                read_u8(interpreter->block, ip + 4),
                read_u8(interpreter->block, ip + 5),
                read_u8(interpreter->block, ip + 6),
                read_u8(interpreter->block, ip + 7),
            };
            ip += 7;
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
                read_u8(interpreter->block, ip + 1),
                read_u8(interpreter->block, ip + 2),
                read_u8(interpreter->block, ip + 3),
                read_u8(interpreter->block, ip + 4),
                read_u8(interpreter->block, ip + 5),
                read_u8(interpreter->block, ip + 6),
                read_u8(interpreter->block, ip + 7),
                read_u8(interpreter->block, ip + 8),
            };
            ip += 8;
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
            ++ip;
            // We don't need to actually do anything here.
            break;
        }
        case W_OP_UNPACK2: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, ip + 1),
                read_u8(interpreter->block, ip + 2),
            };
            ip += 2;
            stack_word fields[2] = {0};
            stack_word pack = pop(interpreter->main_stack);
            unpack_fields(2, fields, sizes, pack);
            push_all(interpreter->main_stack, 2, fields);
            break;
        }
        case W_OP_UNPACK3: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, ip + 1),
                read_u8(interpreter->block, ip + 2),
                read_u8(interpreter->block, ip + 3),
            };
            ip += 3;
            stack_word fields[3] = {0};
            stack_word pack = pop(interpreter->main_stack);
            unpack_fields(3, fields, sizes, pack);
            push_all(interpreter->main_stack, 3, fields);
            break;
        }
        case W_OP_UNPACK4: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, ip + 1),
                read_u8(interpreter->block, ip + 2),
                read_u8(interpreter->block, ip + 3),
                read_u8(interpreter->block, ip + 4),
            };
            ip += 4;
            stack_word fields[4] = {0};
            stack_word pack = pop(interpreter->main_stack);
            unpack_fields(4, fields, sizes, pack);
            push_all(interpreter->main_stack, 4, fields);
            break;
        }
        case W_OP_UNPACK5: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, ip + 1),
                read_u8(interpreter->block, ip + 2),
                read_u8(interpreter->block, ip + 3),
                read_u8(interpreter->block, ip + 4),
                read_u8(interpreter->block, ip + 5),
            };
            ip += 5;
            stack_word fields[5] = {0};
            stack_word pack = pop(interpreter->main_stack);
            unpack_fields(5, fields, sizes, pack);
            push_all(interpreter->main_stack, 5, fields);
            break;
        }
        case W_OP_UNPACK6: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, ip + 1),
                read_u8(interpreter->block, ip + 2),
                read_u8(interpreter->block, ip + 3),
                read_u8(interpreter->block, ip + 4),
                read_u8(interpreter->block, ip + 5),
                read_u8(interpreter->block, ip + 6),
            };
            ip += 6;
            stack_word fields[6] = {0};
            stack_word pack = pop(interpreter->main_stack);
            unpack_fields(6, fields, sizes, pack);
            push_all(interpreter->main_stack, 6, fields);
            break;
        }
        case W_OP_UNPACK7: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, ip + 1),
                read_u8(interpreter->block, ip + 2),
                read_u8(interpreter->block, ip + 3),
                read_u8(interpreter->block, ip + 4),
                read_u8(interpreter->block, ip + 5),
                read_u8(interpreter->block, ip + 6),
                read_u8(interpreter->block, ip + 7),
            };
            ip += 7;
            stack_word fields[7] = {0};
            stack_word pack = pop(interpreter->main_stack);
            unpack_fields(7, fields, sizes, pack);
            push_all(interpreter->main_stack, 7, fields);
            break;
        }
        case W_OP_UNPACK8: {
            uint8_t sizes[] = {
                read_u8(interpreter->block, ip + 1),
                read_u8(interpreter->block, ip + 2),
                read_u8(interpreter->block, ip + 3),
                read_u8(interpreter->block, ip + 4),
                read_u8(interpreter->block, ip + 5),
                read_u8(interpreter->block, ip + 6),
                read_u8(interpreter->block, ip + 7),
                read_u8(interpreter->block, ip + 8),
            };
            ip += 8;
            stack_word fields[8] = {0};
            stack_word pack = pop(interpreter->main_stack);
            unpack_fields(8, fields, sizes, pack);
            push_all(interpreter->main_stack, 8, fields);
            break;
        }
        case W_OP_PACK_FIELD_GET: {
            uint8_t offset = read_u8(interpreter->block, ++ip);
            uint8_t size = read_u8(interpreter->block, ++ip);
            stack_word pack = peek(interpreter->main_stack);
            stack_word field = 0;
            memcpy(&field, (unsigned char *)&pack + offset, size);
            push(interpreter->main_stack, field);
            break;
        }
        case W_OP_COMP_FIELD_GET8: {
            uint8_t offset = read_u8(interpreter->block, ip + 1);
            ip += 1;
            stack_word field = peek_nth(interpreter->main_stack, offset - 1);
            push(interpreter->main_stack, field);
            break;
        }
        case W_OP_COMP_FIELD_GET16: {
            uint16_t offset = read_u16(interpreter->block, ip + 1);
            ip += 2;
            stack_word field = peek_nth(interpreter->main_stack, offset - 1);
            push(interpreter->main_stack, field);
            break;
        }
        case W_OP_COMP_FIELD_GET32: {
            uint32_t offset = read_u32(interpreter->block, ip + 1);
            ip += 4;
            stack_word field = peek_nth(interpreter->main_stack, offset - 1);
            push(interpreter->main_stack, field);
            break;
        }
        case W_OP_PACK_FIELD_SET: {
            int8_t offset = read_s8(interpreter->block, ip + 1);
            int8_t size = read_s8(interpreter->block, ip + 2);
            ip += 2;
            stack_word field = pop(interpreter->main_stack);
            stack_word pack = pop(interpreter->main_stack);
            memcpy((unsigned char *)&pack + offset, &field, size);
            push(interpreter->main_stack, pack);
            break;
        }
        case W_OP_COMP_FIELD_SET8: {
            int8_t offset = read_s8(interpreter->block, ip + 1);
            ip += 1;
            stack_word field = pop(interpreter->main_stack);
            set_nth(interpreter->main_stack, offset - 1, field);
            break;
        }
        case W_OP_COMP_FIELD_SET16: {
            int16_t offset = read_s16(interpreter->block, ip + 1);
            ip += 2;
            stack_word field = pop(interpreter->main_stack);
            set_nth(interpreter->main_stack, offset - 1, field);
            break;
        }
        case W_OP_COMP_FIELD_SET32: {
            int32_t offset = read_s32(interpreter->block, ip + 1);
            ip += 4;
            stack_word field = pop(interpreter->main_stack);
            set_nth(interpreter->main_stack, offset - 1, field);
            break;
        }
        case W_OP_COMP_SUBCOMP_GET8: {
            int8_t offset = read_s8(interpreter->block, ip + 1);
            int8_t word_count = read_s8(interpreter->block, ip + 2);
            ip += 2;
            const stack_word *words = peekn(interpreter->main_stack, offset);
            push_all(interpreter->main_stack, word_count, words);
            break;
        }
        case W_OP_COMP_SUBCOMP_GET16: {
            int16_t offset = read_s16(interpreter->block, ip + 1);
            int16_t word_count = read_s16(interpreter->block, ip + 3);
            ip += 4;
            const stack_word *words = peekn(interpreter->main_stack, offset);
            push_all(interpreter->main_stack, word_count, words);
            break;
        }
        case W_OP_COMP_SUBCOMP_GET32: {
            int32_t offset = read_s32(interpreter->block, ip + 1);
            int32_t word_count = read_s32(interpreter->block, ip + 5);
            ip += 8;
            const stack_word *words = peekn(interpreter->main_stack, offset);
            push_all(interpreter->main_stack, word_count, words);
            break;
        }
        case W_OP_COMP_SUBCOMP_SET8: {
            int8_t offset = read_s8(interpreter->block, ip + 1);
            int8_t word_count = read_s8(interpreter->block, ip + 2);
            ip += 2;
            stack_word *subcomp = malloc(word_count * sizeof *subcomp);
            stack_word *words = malloc(offset * sizeof *words);
            pop_all(interpreter->main_stack, word_count, subcomp);
            pop_all(interpreter->main_stack, offset, words);
            memcpy(words, subcomp, word_count);
            push_all(interpreter->main_stack, offset, words);
            free(subcomp);
            free(words);
            break;
        }
        case W_OP_COMP_SUBCOMP_SET16: {
            int16_t offset = read_s16(interpreter->block, ip + 1);
            int16_t word_count = read_s16(interpreter->block, ip + 3);
            ip += 4;
            stack_word *subcomp = malloc(word_count * sizeof *subcomp);
            stack_word *words = malloc(offset * sizeof *words);
            pop_all(interpreter->main_stack, word_count, subcomp);
            pop_all(interpreter->main_stack, offset, words);
            memcpy(words, subcomp, word_count);
            push_all(interpreter->main_stack, offset, words);
            free(subcomp);
            free(words);
            break;
        }
        case W_OP_COMP_SUBCOMP_SET32: {
            int32_t offset = read_s32(interpreter->block, ip + 1);
            int32_t word_count = read_s32(interpreter->block, ip + 5);
            ip += 8;
            stack_word *subcomp = malloc(word_count * sizeof *subcomp);
            stack_word *words = malloc(offset * sizeof *words);
            pop_all(interpreter->main_stack, word_count, subcomp);
            pop_all(interpreter->main_stack, offset, words);
            memcpy(words, subcomp, word_count);
            push_all(interpreter->main_stack, offset, words);
            free(subcomp);
            free(words);
            break;
        }
        case W_OP_CALL8: {
            uint8_t index = read_u8(interpreter->block, ip + 1);
            ip += 1;
            call(interpreter, index, &ip);
            break;
        }
        case W_OP_CALL16: {
            uint16_t index = read_u16(interpreter->block, ip + 1);
            ip += 2;
            call(interpreter, index, &ip);
            break;
        }
        case W_OP_CALL32: {
            uint32_t index = read_u32(interpreter->block, ip + 1);
            ip += 4;
            call(interpreter, index, &ip);
            break;
        }
        case W_OP_RET:
            ip = ret(interpreter);
            break;
        }
    }
    return INTERPRET_OK;
}

