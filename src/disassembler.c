#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "disassembler.h"
#include "ir.h"
#include "string_view.h"
#include "type.h"
#include "type_punning.h"

#define OPCODE_FORMAT "-21s"

static void print_bytes(struct ir_block *block, int offset, int length) {
#define MAX_INSTRUCTION_LENGTH 9  // Maximum number of bytes in an instruction.
#define WIDTH_LIMIT (3 * MAX_INSTRUCTION_LENGTH + 3)
    int count = 0;
    count += printf("[ ");
    for (int i = 0; i < length; ++i) {
        int byte = block->code[offset + i];
        if (count + 3 < WIDTH_LIMIT) {
            count += printf("%02x ", byte);
        }
        else {
            // Truncate overlong instructions.
            printf("\b\b\b.. ");  // Count not updated since we aren't adding any characters.
            break;
        }
    }
    for (int i = length; i < MAX_INSTRUCTION_LENGTH; ++i) {
        count += printf("-- ");
    }
    count += printf("%*s: ", WIDTH_LIMIT - count, "]");
#undef MAX_INSTRUCTION_LENGTH
#undef WIDTH_LIMIT
}

static void print_instruction(const char *name, struct ir_block *block, int offset, int length) {
    printf("%06d ", offset);
    print_bytes(block, offset, length);
    printf("%c %"OPCODE_FORMAT" ", ((is_jump_dest(block, offset)) ? '*': ' '), name);
}

static int immediate_u8_instruction(const char *name, struct ir_block *block, int offset) {
    print_instruction(name, block, offset, 2);
    printf("%"PRIu8"\n", read_u8(block, offset + 1));
    return offset + 2;
}

static int immediate_u16_instruction(const char *name, struct ir_block *block, int offset) {
    print_instruction(name, block, offset, 3);
    printf("%"PRIu16"\n", read_u16(block, offset + 1));
    return offset + 3;
}

static int immediate_u32_instruction(const char *name, struct ir_block *block, int offset) {
    print_instruction(name, block, offset, 5);
    printf("%"PRIu32"\n", read_u32(block, offset + 1));
    return offset + 5;
}

static int immediate_u64_instruction(const char *name, struct ir_block *block, int offset) {
    print_instruction(name, block, offset, 9);
    printf("%"PRIu64"\n", read_u64(block, offset + 1));
    return offset + 9;
}

static int immediate_s8_instruction(const char *name, struct ir_block *block, int offset) {
    print_instruction(name, block, offset, 2);
    printf("%"PRId8"\n", read_s8(block, offset + 1));
    return offset + 2;
}

static int immediate_s16_instruction(const char *name, struct ir_block *block, int offset) {
    print_instruction(name, block, offset, 3);
    printf("%"PRId16"\n", read_s16(block, offset + 1));
    return offset + 3;
}

static int immediate_s32_instruction(const char *name, struct ir_block *block, int offset) {
    print_instruction(name, block, offset, 5);
    printf("%"PRId32"\n", read_s32(block, offset + 1));
    return offset + 5;
}

static int immediate_s64_instruction(const char *name, struct ir_block *block, int offset) {
    print_instruction(name, block, offset, 9);
    printf("%"PRId64"\n", read_s64(block, offset + 1));
    return offset + 9;
}

static int immediate_f32_instruction(const char *name, struct ir_block *block, int offset) {
    print_instruction(name, block, offset, 5);
    float value = u32_to_f32(read_u32(block, offset + 1));
    printf("%g\n", value);
    return offset + 5;
}

static int immediate_f64_instruction(const char *name, struct ir_block *block, int offset) {
    print_instruction(name, block, offset, 9);
    double value = u64_to_f64(read_u64(block, offset + 1));
    printf("%g\n", value);
    return offset + 9;
}

static int jump_instruction(const char *name, struct ir_block *block, int offset) {
    print_instruction(name, block, offset, 3);
    int jump = read_s16(block, offset + 1);
    printf("%-6d (%d -> %d)\n", jump, offset, offset + jump + 1);
    return offset + 3;
}

static int simple_instruction(const char *name, struct ir_block *block, int offset) {
    print_instruction(name, block, offset, 1);
    printf("\n");
    return offset + 1;
}

static int t_packcomp8_instruction(const char *name, struct ir_block *block,
                                   struct module *module, int offset) {
    print_instruction(name, block, offset, 2);
    int packcomp = read_s8(block, offset + 1);
    struct string_view packcomp_name = type_name(&module->types, packcomp);
    printf("%d '%"PRI_SV"'\n", packcomp, SV_FMT(packcomp_name));
    return offset + 2;
}

static int t_packcomp16_instruction(const char *name, struct ir_block *block,
                                    struct module *module, int offset) {
    print_instruction(name, block, offset, 2);
    int packcomp = read_s16(block, offset + 1);
    struct string_view packcomp_name = type_name(&module->types, packcomp);
    printf("%d '%"PRI_SV"'\n", packcomp, SV_FMT(packcomp_name));
    return offset + 3;
}

static int t_packcomp32_instruction(const char *name, struct ir_block *block,
                                    struct module *module, int offset) {
    print_instruction(name, block, offset, 2);
    int packcomp = read_s32(block, offset + 1);
    struct string_view packcomp_name = type_name(&module->types, packcomp);
    printf("%d '%"PRI_SV"'\n", packcomp, SV_FMT(packcomp_name));
    return offset + 5;
}

static int w_pack_instruction(const char *name, struct ir_block *block, int offset,
                              int field_count) {
    print_instruction(name, block, offset, 1 + field_count);
    printf("%u", read_u8(block, offset + 1));
    for (int i = 1; i < field_count; ++i) {
        printf(", %u", read_u8(block, offset + 1 + i));
    }
    printf("\n");
    return offset + field_count + 1;
}

static int t_pack_field8_instruction(const char *name, struct ir_block *block,
                                     struct module *module, int offset) {
    print_instruction(name, block, offset, 1 + 1 + 1);
    type_index pack = read_s8(block, offset + 1);
    int field = read_u8(block, offset + 2);
    struct string_view pack_name = type_name(&module->types, pack);
    printf("%d '%"PRI_SV"', %d\n", pack, SV_FMT(pack_name), field);
    return offset + 3;
}

static int t_pack_field16_instruction(const char *name, struct ir_block *block,
                                      struct module *module, int offset) {
    print_instruction(name, block, offset, 1 + 2 + 1);
    type_index pack = read_s16(block, offset + 1);
    int field = read_u8(block, offset + 3);
    struct string_view pack_name = type_name(&module->types, pack);
    printf("%d '%"PRI_SV"', %d\n", pack, SV_FMT(pack_name), field);
    return offset + 4;
}

static int t_pack_field32_instruction(const char *name, struct ir_block *block,
                                      struct module *module, int offset) {
    print_instruction(name, block, offset, 1 + 4 + 1);
    type_index pack = read_s32(block, offset + 1);
    int field = read_u8(block, offset + 5);
    struct string_view pack_name = type_name(&module->types, pack);
    printf("%d '%"PRI_SV"', %d\n", pack, SV_FMT(pack_name), field);
    return offset + 6;
}

static int t_comp_field8_instruction(const char *name, struct ir_block *block,
                                     struct module *module, int offset) {
    print_instruction(name, block, offset, 1 + 1 + 1);
    type_index comp = read_s8(block, offset + 1);
    int field = read_u8(block, offset + 2);
    struct string_view comp_name = type_name(&module->types, comp);
    printf("%d '%"PRI_SV"', %d\n", comp, SV_FMT(comp_name), field);
    return offset + 3;
}

static int t_comp_field16_instruction(const char *name, struct ir_block *block,
                                      struct module *module, int offset) {
    print_instruction(name, block, offset, 1 + 2 + 2);
    type_index comp = read_s16(block, offset + 1);
    int field = read_u16(block, offset + 3);
    struct string_view comp_name = type_name(&module->types, comp);
    printf("%d '%"PRI_SV"', %d\n", comp, SV_FMT(comp_name), field);
    return offset + 5;
}

static int t_comp_field32_instruction(const char *name, struct ir_block *block,
                                      struct module *module, int offset) {
    print_instruction(name, block, offset, 1 + 4 + 4);
    type_index comp = read_s32(block, offset + 1);
    int field = read_u32(block, offset + 5);
    struct string_view comp_name = type_name(&module->types, comp);
    printf("%d '%"PRI_SV"', %d\n", comp, SV_FMT(comp_name), field);
    return offset + 9;
}

static int w_pack_field_instruction(const char *name, struct ir_block *block, int offset) {
    print_instruction(name, block, offset, 1 + 1 + 1);
    int field = read_u8(block, offset + 1);
    int size = read_u8(block, offset + 2);
    printf("%d, %d\n", field, size);
    return offset + 3;
}

static int w_comp_field8_instruction(const char *name, struct ir_block *block, int offset) {
    print_instruction(name, block, offset, 1 + 1);
    int field = read_s8(block, offset + 1);
    printf("%d\n", field);
    return offset + 2;
}

static int w_comp_field16_instruction(const char *name, struct ir_block *block, int offset) {
    print_instruction(name, block, offset, 1 + 2);
    int field = read_s16(block, offset + 1);
    printf("%d\n", field);
    return offset + 3;
}

static int w_comp_field32_instruction(const char *name, struct ir_block *block, int offset) {
    print_instruction(name, block, offset, 1 + 4);
    int field = read_s32(block, offset + 1);
    printf("%d\n", field);
    return offset + 5;
}

static int w_comp_subcomp8_instruction(const char *name, struct ir_block *block, int offset) {
    print_instruction(name, block, offset, 1 + 1 + 1);
    int subcomp = read_s8(block, offset + 1);
    int length = read_s8(block, offset + 2);
    printf("%d, %d\n", subcomp, length);
    return offset + 3;
}

static int w_comp_subcomp16_instruction(const char *name, struct ir_block *block, int offset) {
    print_instruction(name, block, offset, 1 + 2 + 2);
    int subcomp = read_s16(block, offset + 1);
    int length = read_s16(block, offset + 3);
    printf("%d, %d\n", subcomp, length);
    return offset + 5;
}

static int w_comp_subcomp32_instruction(const char *name, struct ir_block *block, int offset) {
    print_instruction(name, block, offset, 1 + 4 + 4);
    int subcomp = read_s32(block, offset + 1);
    int length = read_s32(block, offset + 5);
    printf("%d, %d\n", subcomp, length);
    return offset + 9;
}

static int disassemble_t_instruction(struct ir_block *block, struct module *module, int offset) {
    enum t_opcode instruction = block->code[offset];

    switch (instruction) {
    case T_OP_NOP:
        return simple_instruction("T_OP_NOP", block, offset);
    case T_OP_PUSH8:
        return immediate_u8_instruction("T_OP_PUSH8", block, offset);
    case T_OP_PUSH16:
        return immediate_u16_instruction("T_OP_PUSH16", block, offset);
    case T_OP_PUSH32:
        return immediate_u32_instruction("T_OP_PUSH32", block, offset);
    case T_OP_PUSH64:
        return immediate_u64_instruction("T_OP_PUSH64", block, offset);
    case T_OP_PUSH_INT8:
        return immediate_s8_instruction("T_OP_PUSH_INT8", block, offset);
    case T_OP_PUSH_INT16:
        return immediate_s16_instruction("T_OP_PUSH_INT16", block, offset);
    case T_OP_PUSH_INT32:
        return immediate_s32_instruction("T_OP_PUSH_INT32", block, offset);
    case T_OP_PUSH_INT64:
        return immediate_s64_instruction("T_OP_PUSH_INT64", block, offset);
    case T_OP_PUSH_FLOAT32:
        return immediate_f32_instruction("T_OP_PUSH_FLOAT32", block, offset);
    case T_OP_PUSH_FLOAT64:
        return immediate_f64_instruction("T_OP_PUSH_FLOAT64", block, offset);
    case T_OP_PUSH_CHAR8:
        return immediate_u8_instruction("T_OP_PUSH_CHAR8", block, offset);
    case T_OP_PUSH_CHAR16:
        return immediate_u16_instruction("T_OP_PUSH_CHAR16", block, offset);
    case T_OP_PUSH_CHAR32:
        return immediate_u32_instruction("T_OP_PUSH_CHAR32", block, offset);
    case T_OP_LOAD_STRING8:
        return immediate_u8_instruction("T_OP_LOAD_STRING8", block, offset);
    case T_OP_LOAD_STRING16:
        return immediate_u16_instruction("T_OP_LOAD_STRING16", block, offset);
    case T_OP_LOAD_STRING32:
        return immediate_u32_instruction("T_OP_LOAD_STRING32", block, offset);
    case T_OP_POP:
        return simple_instruction("T_OP_POP", block, offset);
    case T_OP_ADD:
        return simple_instruction("T_OP_ADD", block, offset);
    case T_OP_AND:
        return simple_instruction("T_OP_AND", block, offset);
    case T_OP_DEREF:
        return simple_instruction("T_OP_DEREF", block, offset);
    case T_OP_DIV:
        return simple_instruction("T_OP_DIV", block, offset);
    case T_OP_DIVMOD:
        return simple_instruction("T_OP_DIVMOD", block, offset);
    case T_OP_IDIVMOD:
        return simple_instruction("T_OP_IDIVMOD", block, offset);
    case T_OP_EDIVMOD:
        return simple_instruction("T_OP_EDIVMOD", block, offset);
    case T_OP_DUPE:
        return simple_instruction("T_OP_DUPE", block, offset);
    case T_OP_EQUALS:
        return simple_instruction("T_OP_EQUALS", block, offset);
    case T_OP_EXIT:
        return simple_instruction("T_OP_EXIT", block, offset);
    case T_OP_FOR_DEC_START:
        return jump_instruction("T_OP_FOR_DEC_START", block, offset);
    case T_OP_FOR_DEC:
        return jump_instruction("T_OP_FOR_DEC", block, offset);
    case T_OP_FOR_INC_START:
        return jump_instruction("T_OP_FOR_INC_START", block, offset);
    case T_OP_FOR_INC:
        return jump_instruction("T_OP_FOR_INC", block, offset);
    case T_OP_GET_LOOP_VAR:
        return immediate_u16_instruction("T_OP_GET_LOOP_VAR", block, offset);
    case T_OP_GREATER_EQUALS:
        return simple_instruction("T_OP_GREATER_EQUALS", block, offset);
    case T_OP_GREATER_THAN:
        return simple_instruction("T_OP_GREATER_THAN", block, offset);
    case T_OP_JUMP:
        return jump_instruction("T_OP_JUMP", block, offset);
    case T_OP_JUMP_COND:
        return jump_instruction("T_OP_JUMP_COND", block, offset);
    case T_OP_JUMP_NCOND:
        return jump_instruction("T_OP_JUMP_NCOND", block, offset);
    case T_OP_LESS_EQUALS:
        return simple_instruction("T_OP_LESS_EQUALS", block, offset);
    case T_OP_LESS_THAN:
        return simple_instruction("T_OP_LESS_THAN", block, offset);
    case T_OP_LOCAL_GET:
        return immediate_u16_instruction("T_OP_LOCAL_GET", block, offset);
    case T_OP_LOCAL_SET:
        return immediate_u16_instruction("T_OP_LOCAL_SET", block, offset);
    case T_OP_MULT:
        return simple_instruction("T_OP_MULT", block, offset);
    case T_OP_NOT:
        return simple_instruction("T_OP_NOT", block, offset);
    case T_OP_NOT_EQUALS:
        return simple_instruction("T_OP_NOT_EQUALS", block, offset);
    case T_OP_OR:
        return simple_instruction("T_OP_OR", block, offset);
    case T_OP_OVER:
        return simple_instruction("T_OP_OVER", block, offset);
    case T_OP_PRINT:
        return simple_instruction("T_OP_PRINT", block, offset);
    case T_OP_PRINT_CHAR:
        return simple_instruction("T_OP_PRINT_CHAR", block, offset);
    case T_OP_PRINT_INT:
        return simple_instruction("T_OP_PRINT_INT", block, offset);
    case T_OP_ROT:
        return simple_instruction("T_OP_ROT", block, offset);
    case T_OP_SUB:
        return simple_instruction("T_OP_SUB", block, offset);
    case T_OP_SWAP:
        return simple_instruction("T_OP_SWAP", block, offset);
    case T_OP_AS_WORD:
        return simple_instruction("T_OP_AS_WORD", block, offset);
    case T_OP_AS_BYTE:
        return simple_instruction("T_OP_AS_BYTE", block, offset);
    case T_OP_AS_PTR:
        return simple_instruction("T_OP_AS_PTR", block, offset);
    case T_OP_AS_INT:
        return simple_instruction("T_OP_AS_INT", block, offset);
    case T_OP_AS_U8:
        return simple_instruction("T_OP_AS_U8", block, offset);
    case T_OP_AS_U16:
        return simple_instruction("T_OP_AS_U16", block, offset);
    case T_OP_AS_U32:
        return simple_instruction("T_OP_AS_U32", block, offset);
    case T_OP_AS_S8:
        return simple_instruction("T_OP_AS_S8", block, offset);
    case T_OP_AS_S16:
        return simple_instruction("T_OP_AS_S16", block, offset);
    case T_OP_AS_S32:
        return simple_instruction("T_OP_AS_S32", block, offset);
    case T_OP_AS_F32:
        return simple_instruction("T_OP_AS_F32", block, offset);
    case T_OP_AS_F64:
        return simple_instruction("T_OP_AS_F64", block, offset);
    case T_OP_AS_CHAR:
        return simple_instruction("T_OP_AS_CHAR", block, offset);
    case T_OP_AS_CHAR16:
        return simple_instruction("T_OP_AS_CHAR16", block, offset);
    case T_OP_AS_CHAR32:
        return simple_instruction("T_OP_AS_CHAR32", block, offset);
    case T_OP_TO_WORD:
        return simple_instruction("T_OP_TO_WORD", block, offset);
    case T_OP_TO_BYTE:
        return simple_instruction("T_OP_TO_BYTE", block, offset);
    case T_OP_TO_PTR:
        return simple_instruction("T_OP_TO_PTR", block, offset);
    case T_OP_TO_INT:
        return simple_instruction("T_OP_TO_INT", block, offset);
    case T_OP_TO_U8:
        return simple_instruction("T_OP_TO_U8", block, offset);
    case T_OP_TO_U16:
        return simple_instruction("T_OP_TO_U16", block, offset);
    case T_OP_TO_U32:
        return simple_instruction("T_OP_TO_U32", block, offset);
    case T_OP_TO_S8:
        return simple_instruction("T_OP_TO_S8", block, offset);
    case T_OP_TO_S16:
        return simple_instruction("T_OP_TO_S16", block, offset);
    case T_OP_TO_S32:
        return simple_instruction("T_OP_TO_S32", block, offset);
    case T_OP_TO_F32:
        return simple_instruction("T_OP_TO_F32", block, offset);
    case T_OP_TO_F64:
        return simple_instruction("T_OP_TO_F64", block, offset);
    case T_OP_TO_CHAR:
        return simple_instruction("T_OP_TO_CHAR", block, offset);
    case T_OP_TO_CHAR16:
        return simple_instruction("T_OP_TO_CHAR16", block, offset);
    case T_OP_TO_CHAR32:
        return simple_instruction("T_OP_TO_CHAR32", block, offset);
    case T_OP_PACK8:
        return t_packcomp8_instruction("T_OP_PACK8", block, module, offset);
    case T_OP_PACK16:
        return t_packcomp16_instruction("T_OP_PACK16", block, module, offset);
    case T_OP_PACK32:
        return t_packcomp32_instruction("T_OP_PACK32", block, module, offset);
    case T_OP_COMP8:
        return t_packcomp8_instruction("T_OP_COMP8", block, module, offset);
    case T_OP_COMP16:
        return t_packcomp16_instruction("T_OP_COMP16", block, module, offset);
    case T_OP_COMP32:
        return t_packcomp32_instruction("T_OP_COMP32", block, module, offset);
    case T_OP_UNPACK:
        return simple_instruction("T_OP_UNPACK", block, offset);
    case T_OP_DECOMP:
        return simple_instruction("T_OP_DECOMP", block, offset);
    case T_OP_PACK_FIELD_GET8:
        return t_pack_field8_instruction("T_OP_PACK_FIELD_GET8", block, module, offset);
    case T_OP_PACK_FIELD_GET16:
        return t_pack_field16_instruction("T_OP_PACK_FIELD_GET16", block, module, offset);
    case T_OP_PACK_FIELD_GET32:
        return t_pack_field32_instruction("T_OP_PACK_FIELD_GET32", block, module, offset);
    case T_OP_COMP_FIELD_GET8:
        return t_comp_field8_instruction("T_OP_COMP_FIELD_GET8", block, module, offset);
    case T_OP_COMP_FIELD_GET16:
        return t_comp_field16_instruction("T_OP_COMP_FIELD_GET16", block, module, offset);
    case T_OP_COMP_FIELD_GET32:
        return t_comp_field32_instruction("T_OP_COMP_FIELD_GET32", block, module, offset);
    case T_OP_PACK_FIELD_SET8:
        return t_pack_field8_instruction("T_OP_PACK_FIELD_SET8", block, module, offset);
    case T_OP_PACK_FIELD_SET16:
        return t_pack_field16_instruction("T_OP_PACK_FIELD_SET16", block, module, offset);
    case T_OP_PACK_FIELD_SET32:
        return t_pack_field32_instruction("T_OP_PACK_FIELD_SET32", block, module, offset);
    case T_OP_COMP_FIELD_SET8:
        return t_comp_field8_instruction("T_OP_COMP_FIELD_SET8", block, module, offset);
    case T_OP_COMP_FIELD_SET16:
        return t_comp_field16_instruction("T_OP_COMP_FIELD_SET16", block, module, offset);
    case T_OP_COMP_FIELD_SET32:
        return t_comp_field32_instruction("T_OP_COMP_FIELD_SET32", block, module, offset);
    case T_OP_CALL8:
        return immediate_u8_instruction("T_OP_CALL8", block, offset);
    case T_OP_CALL16:
        return immediate_u16_instruction("T_OP_CALL16", block, offset);
    case T_OP_CALL32:
        return immediate_u32_instruction("T_OP_CALL32", block, offset);
    case T_OP_EXTCALL8:
        return immediate_u8_instruction("T_OP_EXTCALL8", block, offset);
    case T_OP_EXTCALL16:
        return immediate_u16_instruction("T_OP_EXTCALL16", block, offset);
    case T_OP_EXTCALL32:
        return immediate_u32_instruction("T_OP_EXTCALL32", block, offset);
    case T_OP_RET:
        return simple_instruction("T_OP_RET", block, offset);
    }
    // Not in switch so that the compiler can ensure all cases are handled.
    printf("<Unknown opcode>\n");
    return block->count;
}

static int disassemble_w_instruction(struct ir_block *block, struct module *module, int offset) {
    (void)module;
    enum w_opcode instruction = block->code[offset];

    switch (instruction) {
    case W_OP_NOP:
        return simple_instruction("W_OP_NOP", block, offset);
    case W_OP_PUSH8:
        return immediate_u8_instruction("W_OP_PUSH8", block, offset);
    case W_OP_PUSH16:
        return immediate_u16_instruction("W_OP_PUSH16", block, offset);
    case W_OP_PUSH32:
        return immediate_u32_instruction("W_OP_PUSH32", block, offset);
    case W_OP_PUSH64:
        return immediate_u64_instruction("W_OP_PUSH64", block, offset);
    case W_OP_PUSH_INT8:
        return immediate_s8_instruction("W_OP_PUSH_INT8", block, offset);
    case W_OP_PUSH_INT16:
        return immediate_s16_instruction("W_OP_PUSH_INT16", block, offset);
    case W_OP_PUSH_INT32:
        return immediate_s32_instruction("W_OP_PUSH_INT32", block, offset);
    case W_OP_PUSH_INT64:
        return immediate_s64_instruction("W_OP_PUSH_INT64", block, offset);
    case W_OP_PUSH_FLOAT32:
        return immediate_f32_instruction("W_OP_PUSH_FLOAT32", block, offset);
    case W_OP_PUSH_FLOAT64:
        return immediate_f64_instruction("W_OP_PUSH_FLOAT64", block, offset);
    case W_OP_PUSH_CHAR8:
        return immediate_u8_instruction("W_OP_PUSH_CHAR8", block, offset);
    case W_OP_PUSH_CHAR16:
        return immediate_u16_instruction("W_OP_PUSH_CHAR16", block, offset);
    case W_OP_PUSH_CHAR32:
        return immediate_u32_instruction("W_OP_PUSH_CHAR32", block, offset);
    case W_OP_LOAD_STRING8:
        return immediate_u8_instruction("W_OP_LOAD_STRING8", block, offset);
    case W_OP_LOAD_STRING16:
        return immediate_u16_instruction("W_OP_LOAD_STRING16", block, offset);
    case W_OP_LOAD_STRING32:
        return immediate_u32_instruction("W_OP_LOAD_STRING32", block, offset);
    case W_OP_POP:
        return simple_instruction("W_OP_POP", block, offset);
    case W_OP_POPN8:
        return immediate_s8_instruction("W_OP_POPN8", block, offset);
    case W_OP_POPN16:
        return immediate_s16_instruction("W_OP_POPN16", block, offset);
    case W_OP_POPN32:
        return immediate_s32_instruction("W_OP_POPN32", block, offset);
    case W_OP_ADD:
        return simple_instruction("W_OP_ADD", block, offset);
    case W_OP_ADDF32:
        return simple_instruction("W_OP_ADDF32", block, offset);
    case W_OP_ADDF64:
        return simple_instruction("W_OP_ADDF64", block, offset);
    case W_OP_AND:
        return simple_instruction("W_OP_AND", block, offset);
    case W_OP_DEREF:
        return simple_instruction("W_OP_DEREF", block, offset);
    case W_OP_DIVF32:
        return simple_instruction("W_OP_DIVF32", block, offset);
    case W_OP_DIVF64:
        return simple_instruction("W_OP_DIVF64", block, offset);
    case W_OP_DIVMOD:
        return simple_instruction("W_OP_DIVMOD", block, offset);
    case W_OP_IDIVMOD:
        return simple_instruction("W_OP_IDIVMOD", block, offset);
    case W_OP_EDIVMOD:
        return simple_instruction("W_OP_EDIVMOD", block, offset);
    case W_OP_DUPE:
        return simple_instruction("W_OP_DUPE", block, offset);
    case W_OP_DUPEN8:
        return immediate_s8_instruction("W_OP_DUPEN8", block, offset);
    case W_OP_DUPEN16:
        return immediate_s16_instruction("W_OP_DUPEN16", block, offset);
    case W_OP_DUPEN32:
        return immediate_s32_instruction("W_OP_DUPEN32", block, offset);
    case W_OP_EQUALS:
        return simple_instruction("W_OP_EQUALS", block, offset);
    case W_OP_EQUALS_F32:
        return simple_instruction("W_OP_EQUALS_F32", block, offset);
    case W_OP_EQUALS_F64:
        return simple_instruction("W_OP_EQUALS_F64", block, offset);
    case W_OP_EXIT:
        return simple_instruction("W_OP_EXIT", block, offset);
    case W_OP_FOR_DEC_START:
        return jump_instruction("W_OP_FOR_DEC_START", block, offset);
    case W_OP_FOR_DEC:
        return jump_instruction("W_OP_FOR_DEC", block, offset);
    case W_OP_FOR_INC_START:
        return jump_instruction("W_OP_FOR_INC_START", block, offset);
    case W_OP_FOR_INC:
        return jump_instruction("W_OP_FOR_INC", block, offset);
    case W_OP_GET_LOOP_VAR:
        return immediate_u16_instruction("W_OP_GET_LOOP_VAR", block, offset);
    case W_OP_GREATER_EQUALS:
        return simple_instruction("W_OP_GREATER_EQUALS", block, offset);
    case W_OP_GREATER_EQUALS_F32:
        return simple_instruction("W_OP_GREATER_EQUALS_F32", block, offset);
    case W_OP_GREATER_EQUALS_F64:
        return simple_instruction("W_OP_GREATER_EQUALS_F64", block, offset);
    case W_OP_GREATER_THAN:
        return simple_instruction("W_OP_GREATER_THAN", block, offset);
    case W_OP_GREATER_THAN_F32:
        return simple_instruction("W_OP_GREATER_THAN_F32", block, offset);
    case W_OP_GREATER_THAN_F64:
        return simple_instruction("W_OP_GREATER_THAN_F64", block, offset);
    case W_OP_HIGHER_SAME:
        return simple_instruction("W_OP_HIGHER_SAME", block, offset);
    case W_OP_HIGHER_THAN:
        return simple_instruction("W_OP_HIGHER_THAN", block, offset);
    case W_OP_JUMP:
        return jump_instruction("W_OP_JUMP", block, offset);
    case W_OP_JUMP_COND:
        return jump_instruction("W_OP_JUMP_COND", block, offset);
    case W_OP_JUMP_NCOND:
        return jump_instruction("W_OP_JUMP_NCOND", block, offset);
    case W_OP_LESS_EQUALS:
        return simple_instruction("W_OP_LESS_EQUALS", block, offset);
    case W_OP_LESS_EQUALS_F32:
        return simple_instruction("W_OP_LESS_EQUALS_F32", block, offset);
    case W_OP_LESS_EQUALS_F64:
        return simple_instruction("W_OP_LESS_EQUALS_F64", block, offset);
    case W_OP_LESS_THAN:
        return simple_instruction("W_OP_LESS_THAN", block, offset);
    case W_OP_LESS_THAN_F32:
        return simple_instruction("W_OP_LESS_THAN_F32", block, offset);
    case W_OP_LESS_THAN_F64:
        return simple_instruction("W_OP_LESS_THAN_F64", block, offset);
    case W_OP_LOCAL_GET:
        return immediate_u16_instruction("W_OP_LOCAL_GET", block, offset);
    case W_OP_LOCAL_SET:
        return immediate_u16_instruction("W_OP_LOCAL_SET", block, offset);
    case W_OP_LOWER_SAME:
        return simple_instruction("W_OP_LOWER_SAME", block, offset);
    case W_OP_LOWER_THAN:
        return simple_instruction("W_OP_LOWER_THAN", block, offset);
    case W_OP_MULT:
        return simple_instruction("W_OP_MULT", block, offset);
    case W_OP_MULTF32:
        return simple_instruction("W_OP_MULTF32", block, offset);
    case W_OP_MULTF64:
        return simple_instruction("W_OP_MULTF64", block, offset);
    case W_OP_NOT:
        return simple_instruction("W_OP_NOT", block, offset);
    case W_OP_NOT_EQUALS:
        return simple_instruction("W_OP_NOT_EQUALS", block, offset);
    case W_OP_NOT_EQUALS_F32:
        return simple_instruction("W_OP_NOT_EQUALS_F32", block, offset);
    case W_OP_NOT_EQUALS_F64:
        return simple_instruction("W_OP_NOT_EQUALS_F64", block, offset);
    case W_OP_OR:
        return simple_instruction("W_OP_OR", block, offset);
    case W_OP_PRINT:
        return simple_instruction("W_OP_PRINT", block, offset);
    case W_OP_PRINT_CHAR:
        return simple_instruction("W_OP_PRINT_CHAR", block, offset);
    case W_OP_PRINT_FLOAT:
        return simple_instruction("W_OP_PRINT_FLOAT", block, offset);
    case W_OP_PRINT_INT:
        return simple_instruction("W_OP_PRINT_INT", block, offset);
    case W_OP_PRINT_STRING:
        return simple_instruction("W_OP_PRINT_STRING", block, offset);
    case W_OP_SUB:
        return simple_instruction("W_OP_SUB", block, offset);
    case W_OP_SUBF32:
        return simple_instruction("W_OP_SUBF32", block, offset);
    case W_OP_SUBF64:
        return simple_instruction("W_OP_SUBF64", block, offset);
    case W_OP_SWAP:
        return simple_instruction("W_OP_SWAP", block, offset);
    case W_OP_SWAP_COMPS8:
        return w_comp_subcomp8_instruction("W_OP_SWAP_COMPS8", block, offset);
    case W_OP_SWAP_COMPS16:
        return w_comp_subcomp16_instruction("W_OP_SWAP_COMPS16", block, offset);
    case W_OP_SWAP_COMPS32:
        return w_comp_subcomp32_instruction("W_OP_SWAP_COMPS32", block, offset);
    case W_OP_SX8:
        return simple_instruction("W_OP_SX8", block, offset);
    case W_OP_SX8L:
        return simple_instruction("W_OP_SX8L", block, offset);
    case W_OP_SX16:
        return simple_instruction("W_OP_SX16", block, offset);
    case W_OP_SX16L:
        return simple_instruction("W_OP_SX16L", block, offset);
    case W_OP_SX32:
        return simple_instruction("W_OP_SX32", block, offset);
    case W_OP_SX32L:
        return simple_instruction("W_OP_SX32L", block, offset);
    case W_OP_ZX8:
        return simple_instruction("W_OP_ZX8", block, offset);
    case W_OP_ZX8L:
        return simple_instruction("W_OP_ZX8L", block, offset);
    case W_OP_ZX16:
        return simple_instruction("W_OP_ZX16", block, offset);
    case W_OP_ZX16L:
        return simple_instruction("W_OP_ZX16L", block, offset);
    case W_OP_ZX32:
        return simple_instruction("W_OP_ZX32", block, offset);
    case W_OP_ZX32L:
        return simple_instruction("W_OP_ZX32L", block, offset);
    case W_OP_FPROM:
        return simple_instruction("W_OP_FPROM", block, offset);
    case W_OP_FPROML:
        return simple_instruction("W_OP_FPROML", block, offset);
    case W_OP_FDEM:
        return simple_instruction("W_OP_FDEM", block, offset);
    case W_OP_ICONVF32:
        return simple_instruction("W_OP_ICONVF32", block, offset);
    case W_OP_ICONVF32L:
        return simple_instruction("W_OP_ICONVF32L", block, offset);
    case W_OP_ICONVF64:
        return simple_instruction("W_OP_ICONVF64", block, offset);
    case W_OP_ICONVF64L:
        return simple_instruction("W_OP_ICONVF64L", block, offset);
    case W_OP_FCONVI32:
        return simple_instruction("W_OP_FCONVI32", block, offset);
    case W_OP_FCONVI64:
        return simple_instruction("W_OP_FCONVI64", block, offset);
    case W_OP_ICONVC32:
        return simple_instruction("W_OP_ICONVC32", block, offset);
    case W_OP_CHAR_8CONV32:
        return simple_instruction("W_OP_CHAR_8CONV32", block, offset);
    case W_OP_CHAR_32CONV8:
        return simple_instruction("W_OP_CHAR_32CONV8", block, offset);
    case W_OP_CHAR_16CONV32:
        return simple_instruction("W_OP_CHAR_16CONV32", block, offset);
    case W_OP_CHAR_32CONV16:
        return simple_instruction("W_OP_CHAR_32CONV16", block, offset);
    case W_OP_PACK1:
        return w_pack_instruction("W_OP_PACK1", block, offset, 1);
    case W_OP_PACK2:
        return w_pack_instruction("W_OP_PACK2", block, offset, 2);
    case W_OP_PACK3:
        return w_pack_instruction("W_OP_PACK3", block, offset, 3);
    case W_OP_PACK4:
        return w_pack_instruction("W_OP_PACK4", block, offset, 4);
    case W_OP_PACK5:
        return w_pack_instruction("W_OP_PACK5", block, offset, 5);
    case W_OP_PACK6:
        return w_pack_instruction("W_OP_PACK6", block, offset, 6);
    case W_OP_PACK7:
        return w_pack_instruction("W_OP_PACK7", block, offset, 7);
    case W_OP_PACK8:
        return w_pack_instruction("W_OP_PACK8", block, offset, 8);
    case W_OP_UNPACK1:
        return w_pack_instruction("W_OP_UNPACK1", block, offset, 1);
    case W_OP_UNPACK2:
        return w_pack_instruction("W_OP_UNPACK2", block, offset, 2);
    case W_OP_UNPACK3:
        return w_pack_instruction("W_OP_UNPACK3", block, offset, 3);
    case W_OP_UNPACK4:
        return w_pack_instruction("W_OP_UNPACK4", block, offset, 4);
    case W_OP_UNPACK5:
        return w_pack_instruction("W_OP_UNPACK5", block, offset, 5);
    case W_OP_UNPACK6:
        return w_pack_instruction("W_OP_UNPACK6", block, offset, 6);
    case W_OP_UNPACK7:
        return w_pack_instruction("W_OP_UNPACK7", block, offset, 7);
    case W_OP_UNPACK8:
        return w_pack_instruction("W_OP_UNPACK8", block, offset, 8);
    case W_OP_PACK_FIELD_GET:
        return w_pack_field_instruction("W_OP_PACK_FIELD_GET", block, offset);
    case W_OP_COMP_FIELD_GET8:
        return w_comp_field8_instruction("W_OP_COMP_FIELD_GET8", block, offset);
    case W_OP_COMP_FIELD_GET16:
        return w_comp_field16_instruction("W_OP_COMP_FIELD_GET16", block, offset);
    case W_OP_COMP_FIELD_GET32:
        return w_comp_field32_instruction("W_OP_COMP_FIELD_GET32", block, offset);
    case W_OP_PACK_FIELD_SET:
        return w_pack_field_instruction("W_OP_PACK_FIELD_SET", block, offset);
    case W_OP_COMP_FIELD_SET8:
        return w_comp_field8_instruction("W_OP_COMP_FIELD_SET8", block, offset);
    case W_OP_COMP_FIELD_SET16:
        return w_comp_field16_instruction("W_OP_COMP_FIELD_SET16", block, offset);
    case W_OP_COMP_FIELD_SET32:
        return w_comp_field32_instruction("W_OP_COMP_FIELD_SET32", block, offset);
    case W_OP_COMP_SUBCOMP_GET8:
        return w_comp_subcomp8_instruction("W_OP_COMP_SUBCOMP_GET8", block, offset);
    case W_OP_COMP_SUBCOMP_GET16:
        return w_comp_subcomp16_instruction("W_OP_COMP_SUBCOMP_GET16", block, offset);
    case W_OP_COMP_SUBCOMP_GET32:
        return w_comp_subcomp32_instruction("W_OP_COMP_SUBCOMP_GET32", block, offset);
    case W_OP_COMP_SUBCOMP_SET8:
        return w_comp_subcomp8_instruction("W_OP_COMP_SUBCOMP_SET8", block, offset);
    case W_OP_COMP_SUBCOMP_SET16:
        return w_comp_subcomp16_instruction("W_OP_COMP_SUBCOMP_SET16", block, offset);
    case W_OP_COMP_SUBCOMP_SET32:
        return w_comp_subcomp32_instruction("W_OP_COMP_SUBCOMP_SET32", block, offset);
    case W_OP_CALL8:
        return immediate_u8_instruction("W_OP_CALL8", block, offset);
    case W_OP_CALL16:
        return immediate_u16_instruction("W_OP_CALL16", block, offset);
    case W_OP_CALL32:
        return immediate_u32_instruction("W_OP_CALL32", block, offset);
    case W_OP_EXTCALL8:
        return immediate_u8_instruction("W_OP_EXTCALL8", block, offset);
    case W_OP_EXTCALL16:
        return immediate_u16_instruction("W_OP_EXTCALL16", block, offset);
    case W_OP_EXTCALL32:
        return immediate_u32_instruction("W_OP_EXTCALL32", block, offset);
    case W_OP_RET:
        return simple_instruction("W_OP_RET", block, offset);
    }
    // Not in switch so that the compiler can ensure all cases are handled.
    printf("<Unknown opcode>\n");
    return block->count;
}

typedef int (*instr_disasm)(struct ir_block *block, struct module *module, int offset);

static instr_disasm get_disassembler(struct ir_block *block) {
    switch (block->instruction_set) {
    case IR_TYPED: return disassemble_t_instruction;
    case IR_WORD_ORIENTED: return disassemble_w_instruction;
    }
    // Invalid instruction set.
    __builtin_unreachable();
}

void disassemble_block(struct ir_block *block, struct module *module) {
    instr_disasm disassemble_instruction = get_disassembler(block);
    for (int offset = 0; offset < block->count; ) {
        offset = disassemble_instruction(block, module, offset);
    }
}

void disassemble_tir(struct module *module) {
    for (int i = 0; i < module->functions.count; ++i) {
        struct function *function = get_function(&module->functions, i);
        printf("== func_%d ==\n", i);
        disassemble_block(&function->t_code, module);
    }
}

void disassemble_wir(struct module *module) {
    for (int i = 0; i < module->functions.count; ++i) {
        struct function *function = get_function(&module->functions, i);
        printf("== func_%d ==\n", i);
        disassemble_block(&function->w_code, module);
    }
}
