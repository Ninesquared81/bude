#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "disassembler.h"
#include "ir.h"
#include "type.h"

#define OPCODE_FORMAT "-20s"

static int immediate_u8_instruction(const char *name, struct ir_block *block, int offset) {
    printf("%"OPCODE_FORMAT" %"PRIu8"\n", name, read_u8(block, offset + 1));
    return offset + 2;
}

static int immediate_u16_instruction(const char *name, struct ir_block *block, int offset) {
    printf("%"OPCODE_FORMAT" %"PRIu16"\n", name, read_u16(block, offset + 1));
    return offset + 3;
}

static int immediate_u32_instruction(const char *name, struct ir_block *block, int offset) {
    printf("%"OPCODE_FORMAT" %"PRIu32"\n", name, read_u32(block, offset + 1));
    return offset + 5;
}

static int immediate_u64_instruction(const char *name, struct ir_block *block, int offset) {
    printf("%"OPCODE_FORMAT" %"PRIu64"\n", name, read_u64(block, offset + 1));
    return offset + 9;
}

static int immediate_s8_instruction(const char *name, struct ir_block *block, int offset) {
    printf("%"OPCODE_FORMAT" %"PRId8"\n", name, read_s8(block, offset + 1));
    return offset + 2;
}

static int immediate_s16_instruction(const char *name, struct ir_block *block, int offset) {
    printf("%"OPCODE_FORMAT" %"PRId16"\n", name, read_s16(block, offset + 1));
    return offset + 3;
}

static int immediate_s32_instruction(const char *name, struct ir_block *block, int offset) {
    printf("%"OPCODE_FORMAT" %"PRId32"\n", name, read_s32(block, offset + 1));
    return offset + 5;
}

static int immediate_s64_instruction(const char *name, struct ir_block *block, int offset) {
    printf("%"OPCODE_FORMAT" %"PRId64"\n", name, read_s64(block, offset + 1));
    return offset + 9;
}

static int jump_instruction(const char *name, struct ir_block *block, int offset) {
    int jump = read_s16(block, offset + 1);
    printf("%"OPCODE_FORMAT" %-6d (%d -> %d)\n", name, jump, offset, offset + jump + 1);
    return offset + 3;
}

static int simple_instruction(const char *name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int w_pack_instruction(const char *name, struct ir_block *block, int offset,
                              int field_count) {
    printf("%"OPCODE_FORMAT, name);
    printf(" %u", read_u8(block, offset + 1));
    for (int i = 1; i < field_count; ++i) {
        printf(", %u", read_u8(block, offset + 1 + i));
    }
    printf("\n");
    return offset + field_count + 1;
}

static int t_pack_field8_instruction(const char *name, struct ir_block *block, int offset) {
    type_index pack = read_s8(block, offset + 1);
    int field = read_u8(block, offset + 2);
    // TODO: print name of pack here.
    printf("%"OPCODE_FORMAT" %d, %d\n", name, pack, field);
    return offset + 3;
}

static int t_pack_field16_instruction(const char *name, struct ir_block *block, int offset) {
    type_index pack = read_s16(block, offset + 1);
    int field = read_u8(block, offset + 3);
    printf("%"OPCODE_FORMAT" %d, %d\n", name, pack, field);
    return offset + 4;
}

static int t_pack_field32_instruction(const char *name, struct ir_block *block, int offset) {
    type_index pack = read_s32(block, offset + 1);
    int field = read_u8(block, offset + 4);
    printf("%"OPCODE_FORMAT" %d, %d\n", name, pack, field);
    return offset + 6;
}

static int t_comp_field8_instruction(const char *name, struct ir_block *block, int offset) {
    type_index comp = read_s8(block, offset + 1);
    int field = read_u8(block, offset + 2);
    printf("%"OPCODE_FORMAT" %d, %d\n", name, comp, field);
    return offset + 3;
}

static int t_comp_field16_instruction(const char *name, struct ir_block *block, int offset) {
    type_index comp = read_s16(block, offset + 1);
    int field = read_u16(block, offset + 2);
    printf("%"OPCODE_FORMAT" %d, %d\n", name, comp, field);
    return offset + 5;
}

static int t_comp_field32_instruction(const char *name, struct ir_block *block, int offset) {
    type_index comp = read_s32(block, offset + 1);
    int field = read_u32(block, offset + 2);
    printf("%"OPCODE_FORMAT" %d, %d\n", name, comp, field);
    return offset + 9;
}

static int w_pack_field_instruction(const char *name, struct ir_block *block, int offset) {
    int field = read_u8(block, offset + 1);
    int size = read_u8(block, offset + 2);
    printf("%"OPCODE_FORMAT" %d, %d\n", name, field, size);
    return offset + 3;
}

static int w_comp_field8_instruction(const char *name, struct ir_block *block, int offset) {
    int field = read_s8(block, offset + 1);
    printf("%"OPCODE_FORMAT" %d\n", name, field);
    return offset + 2;
}

static int w_comp_field16_instruction(const char *name, struct ir_block *block, int offset) {
    int field = read_s16(block, offset + 1);
    printf("%"OPCODE_FORMAT" %d\n", name, field);
    return offset + 3;
}

static int w_comp_field32_instruction(const char *name, struct ir_block *block, int offset) {
    int field = read_s32(block, offset + 1);
    printf("%"OPCODE_FORMAT" %d\n", name, field);
    return offset + 5;
}

static int w_comp_subcomp8_instruction(const char *name, struct ir_block *block, int offset) {
    int subcomp = read_s8(block, offset + 1);
    int length = read_s8(block, offset + 2);
    printf("%"OPCODE_FORMAT" %d, %d\n", name, subcomp, length);
    return offset + 3;
}

static int w_comp_subcomp16_instruction(const char *name, struct ir_block *block, int offset) {
    int subcomp = read_s16(block, offset + 1);
    int length = read_s16(block, offset + 3);
    printf("%"OPCODE_FORMAT" %d, %d\n", name, subcomp, length);
    return offset + 5;
}

static int w_comp_subcomp32_instruction(const char *name, struct ir_block *block, int offset) {
    int subcomp = read_s32(block, offset + 1);
    int length = read_s32(block, offset + 5);
    printf("%"OPCODE_FORMAT" %d, %d\n", name, subcomp, length);
    return offset + 9;
}

static int disassemble_t_instruction(struct ir_block *block, int offset) {
    enum t_opcode instruction = block->code[offset];
    printf("%c %04d [%03d] ",
           (is_jump_dest(block, offset)) ? '*': ' ', offset, instruction);

    switch (instruction) {
    case T_OP_NOP:
        return simple_instruction("T_OP_NOP", offset);
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
        return simple_instruction("T_OP_POP", offset);
    case T_OP_ADD:
        return simple_instruction("T_OP_ADD", offset);
    case T_OP_AND:
        return simple_instruction("T_OP_AND", offset);
    case T_OP_DECOMP:
        return simple_instruction("T_OP_DECOMP", offset);
    case T_OP_DEREF:
        return simple_instruction("T_OP_DEREF", offset);
    case T_OP_DIVMOD:
        return simple_instruction("T_OP_DIVMOD", offset);
    case T_OP_IDIVMOD:
        return simple_instruction("T_OP_IDIVMOD", offset);
    case T_OP_EDIVMOD:
        return simple_instruction("T_OP_EDIVMOD", offset);
    case T_OP_DUPE:
        return simple_instruction("T_OP_DUPE", offset);
    case T_OP_EXIT:
        return simple_instruction("T_OP_EXIT", offset);
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
    case T_OP_JUMP:
        return jump_instruction("T_OP_JUMP", block, offset);
    case T_OP_JUMP_COND:
        return jump_instruction("T_OP_JUMP_COND", block, offset);
    case T_OP_JUMP_NCOND:
        return jump_instruction("T_OP_JUMP_NCOND", block, offset);
    case T_OP_MULT:
        return simple_instruction("T_OP_MULT", offset);
    case T_OP_NOT:
        return simple_instruction("T_OP_NOT", offset);
    case T_OP_OR:
        return simple_instruction("T_OP_OR", offset);
    case T_OP_PRINT:
        return simple_instruction("T_OP_PRINT", offset);
    case T_OP_PRINT_CHAR:
        return simple_instruction("T_OP_PRINT_CHAR", offset);
    case T_OP_PRINT_INT:
        return simple_instruction("T_OP_PRINT_INT", offset);
    case T_OP_SUB:
        return simple_instruction("T_OP_SUB", offset);
    case T_OP_SWAP:
        return simple_instruction("T_OP_SWAP", offset);
    case T_OP_AS_BYTE:
        return simple_instruction("T_OP_AS_BYTE", offset);
    case T_OP_AS_U8:
        return simple_instruction("T_OP_AS_U8", offset);
    case T_OP_AS_U16:
        return simple_instruction("T_OP_AS_U16", offset);
    case T_OP_AS_U32:
        return simple_instruction("T_OP_AS_U32", offset);
    case T_OP_AS_S8:
        return simple_instruction("T_OP_AS_S8", offset);
    case T_OP_AS_S16:
        return simple_instruction("T_OP_AS_S16", offset);
    case T_OP_AS_S32:
        return simple_instruction("T_OP_AS_S32", offset);
    case T_OP_PACK8:
        return immediate_u8_instruction("T_OP_PACK8", block, offset);
    case T_OP_PACK16:
        return immediate_u16_instruction("T_OP_PACK16", block, offset);
    case T_OP_PACK32:
        return immediate_u32_instruction("T_OP_PACK32", block, offset);
    case T_OP_COMP8:
        return immediate_u8_instruction("T_OP_COMP8", block, offset);
    case T_OP_COMP16:
        return immediate_u16_instruction("T_OP_COMP16", block, offset);
    case T_OP_COMP32:
        return immediate_u32_instruction("T_OP_COMP32", block, offset);
    case T_OP_UNPACK:
        return simple_instruction("T_OP_UNPACK", offset);
    case T_OP_PACK_FIELD_GET8:
        return t_pack_field8_instruction("T_OP_PACK_FIELD_GET8", block, offset);
    case T_OP_PACK_FIELD_GET16:
        return t_pack_field16_instruction("T_OP_PACK_FIELD_GET16", block, offset);
    case T_OP_PACK_FIELD_GET32:
        return t_pack_field32_instruction("T_OP_PACK_FIELD_GET32", block, offset);
    case T_OP_COMP_FIELD_GET8:
        return t_comp_field8_instruction("T_OP_COMP_FIELD_GET8", block, offset);
    case T_OP_COMP_FIELD_GET16:
        return t_comp_field16_instruction("T_OP_COMP_FIELD_GET16", block, offset);
    case T_OP_COMP_FIELD_GET32:
        return t_comp_field32_instruction("T_OP_COMP_FIELD_GET32", block, offset);
    case T_OP_PACK_FIELD_SET8:
        return t_pack_field8_instruction("T_OP_PACK_FIELD_SET8", block, offset);
    case T_OP_PACK_FIELD_SET16:
        return t_pack_field16_instruction("T_OP_PACK_FIELD_SET16", block, offset);
    case T_OP_PACK_FIELD_SET32:
        return t_pack_field32_instruction("T_OP_PACK_FIELD_SET32", block, offset);
    case T_OP_COMP_FIELD_SET8:
        return t_comp_field8_instruction("T_OP_COMP_FIELD_SET8", block, offset);
    case T_OP_COMP_FIELD_SET16:
        return t_comp_field16_instruction("T_OP_COMP_FIELD_SET16", block, offset);
    case T_OP_COMP_FIELD_SET32:
        return t_comp_field32_instruction("T_OP_COMP_FIELD_SET32", block, offset);
    }
    // Not in switch so that the compiler can ensure all cases are handled.
    printf("<Unknown opcode>\n");
    return block->count;
}

static int disassemble_w_instruction(struct ir_block *block, int offset) {
    enum w_opcode instruction = block->code[offset];
    printf("%c %04d [%03d] ",
           (is_jump_dest(block, offset)) ? '*': ' ', offset, instruction);

    switch (instruction) {
    case W_OP_NOP:
        return simple_instruction("W_OP_NOP", offset);
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
        return simple_instruction("W_OP_POP", offset);
    case W_OP_POPN8:
        return immediate_s8_instruction("W_OP_POPN8", block, offset);
    case W_OP_POPN16:
        return immediate_s16_instruction("W_OP_POPN16", block, offset);
    case W_OP_POPN32:
        return immediate_s32_instruction("W_OP_POPN32", block, offset);
    case W_OP_ADD:
        return simple_instruction("W_OP_ADD", offset);
    case W_OP_AND:
        return simple_instruction("W_OP_AND", offset);
    case W_OP_DEREF:
        return simple_instruction("W_OP_DEREF", offset);
    case W_OP_DIVMOD:
        return simple_instruction("W_OP_DIVMOD", offset);
    case W_OP_IDIVMOD:
        return simple_instruction("W_OP_IDIVMOD", offset);
    case W_OP_EDIVMOD:
        return simple_instruction("W_OP_EDIVMOD", offset);
    case W_OP_DUPE:
        return simple_instruction("W_OP_DUPE", offset);
    case W_OP_DUPEN8:
        return immediate_s8_instruction("W_OP_DUPEN8", block, offset);
    case W_OP_DUPEN16:
        return immediate_s16_instruction("W_OP_DUPEN16", block, offset);
    case W_OP_DUPEN32:
        return immediate_s32_instruction("W_OP_DUPEN32", block, offset);
    case W_OP_EXIT:
        return simple_instruction("W_OP_EXIT", offset);
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
    case W_OP_JUMP:
        return jump_instruction("W_OP_JUMP", block, offset);
    case W_OP_JUMP_COND:
        return jump_instruction("W_OP_JUMP_COND", block, offset);
    case W_OP_JUMP_NCOND:
        return jump_instruction("W_OP_JUMP_NCOND", block, offset);
    case W_OP_MULT:
        return simple_instruction("W_OP_MULT", offset);
    case W_OP_NOT:
        return simple_instruction("W_OP_NOT", offset);
    case W_OP_OR:
        return simple_instruction("W_OP_OR", offset);
    case W_OP_PRINT:
        return simple_instruction("W_OP_PRINT", offset);
    case W_OP_PRINT_CHAR:
        return simple_instruction("W_OP_PRINT_CHAR", offset);
    case W_OP_PRINT_INT:
        return simple_instruction("W_OP_PRINT_INT", offset);
    case W_OP_PRINT_STRING:
        return simple_instruction("W_OP_PRINT_STRING", offset);
    case W_OP_SUB:
        return simple_instruction("W_OP_SUB", offset);
    case W_OP_SWAP:
        return simple_instruction("W_OP_SWAP", offset);
    case W_OP_SWAP_COMPS8:
        return w_comp_subcomp8_instruction("W_OP_SWAP_COMPS8", block, offset);
    case W_OP_SWAP_COMPS16:
        return w_comp_subcomp16_instruction("W_OP_SWAP_COMPS16", block, offset);
    case W_OP_SWAP_COMPS32:
        return w_comp_subcomp32_instruction("W_OP_SWAP_COMPS32", block, offset);
    case W_OP_SX8:
        return simple_instruction("W_OP_SX8", offset);
    case W_OP_SX8L:
        return simple_instruction("W_OP_SX8L", offset);
    case W_OP_SX16:
        return simple_instruction("W_OP_SX16", offset);
    case W_OP_SX16L:
        return simple_instruction("W_OP_SX16L", offset);
    case W_OP_SX32:
        return simple_instruction("W_OP_SX32", offset);
    case W_OP_SX32L:
        return simple_instruction("W_OP_SX32L", offset);
    case W_OP_ZX8:
        return simple_instruction("W_OP_ZX8", offset);
    case W_OP_ZX8L:
        return simple_instruction("W_OP_ZX8L", offset);
    case W_OP_ZX16:
        return simple_instruction("W_OP_ZX16", offset);
    case W_OP_ZX16L:
        return simple_instruction("W_OP_ZX16L", offset);
    case W_OP_ZX32:
        return simple_instruction("W_OP_ZX32", offset);
    case W_OP_ZX32L:
        return simple_instruction("W_OP_ZX32L", offset);
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
    }
    // Not in switch so that the compiler can ensure all cases are handled.
    printf("<Unknown opcode>\n");
    return block->count;
}

typedef int (*instr_disasm)(struct ir_block *block, int offset);

static instr_disasm get_disassembler(struct ir_block *block) {
    switch (block->instruction_set) {
    case IR_TYPED: return disassemble_t_instruction;
    case IR_WORD_ORIENTED: return disassemble_w_instruction;
    }
    // Invalid instruction set.
    __builtin_unreachable();
}

void disassemble_block(struct ir_block *block) {
    instr_disasm disassemble_instruction = get_disassembler(block);
    for (int offset = 0; offset < block->count; ) {
        offset = disassemble_instruction(block, offset);
    }
}
