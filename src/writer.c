#include <stdio.h>

#include "bwf.h"
#include "module.h"
#include "writer.h"

#define writer_version_number 4
#define WRITE(obj, f) \
    fwrite(&obj, sizeof obj, 1, f)

#define WRITE_OR_ERR(obj, f, err_ret) \
    if (WRITE(obj, f) != 1) return err_ret


void display_bytecode(struct module *module, FILE *f) {
    for (int i = 0; i < module->strings.count; ++i) {
        struct string_view *sv = &module->strings.items[i];
        fprintf(f, "str_%d:\n\t\"", i);
        for (const char *p = sv->start; p < SV_END(*sv); ++p) {
            char c = *p;
            switch (c) {
            case '"':
                fprintf(f,"\\\"");
                break;
            case '\n':
                fprintf(f,"\\n");
                break;
            case '\t':
                fprintf(f,"\\t");
                break;
            case '\r':
                fprintf(f,"\\r");
                break;
            case '\\':
                fprintf(f,"\\\\");
                break;
            default:
                fprintf(f,"%c", *p);
            }
        }
        fprintf(f,"\"\n");
    }
#define BYTECODE_COLUMN_COUNT 16
    for (int i = 0; i < module->functions.count; ++i) {
        struct function *function = &module->functions.items[i];
        struct ir_block *block = &function->w_code;
        fprintf(f,"func_%d:\n\t", i);
        int line_count = block->count / BYTECODE_COLUMN_COUNT;
        int leftover_count = block->count % BYTECODE_COLUMN_COUNT;
        for (int j = 0; j < line_count; ++j) {
            for (int k = 0; k < BYTECODE_COLUMN_COUNT; ++k) {
                fprintf(f,"%.2x ", block->code[j*BYTECODE_COLUMN_COUNT + k]);
            }
            fprintf(f,"\n\t");
        }
        for (int k = 0; k < leftover_count; ++k) {
            fprintf(f,"%.2x ", block->code[line_count*BYTECODE_COLUMN_COUNT + k]);
        }
        if (leftover_count > 0) {
            fprintf(f,"\n");
        }
    }
#undef BYTECODE_COLUMN_COUNT
}

int write_bytecode(struct module *module, FILE *f) {
    return write_bytecode_ex(module, f, BWF_version_number);
}

static int write_function_entry(struct module *module, struct function *function,
                                FILE *f, int version_number) {
    (void)module;
    struct ir_block *block = &function->w_code;
    int32_t entry_size = get_function_entry_size(function, version_number);
    if (version_number >= 3) {
        WRITE_OR_ERR(entry_size, f, errno);
    }
    if (fwrite(&block->count, sizeof block->count, 1, f) != 1) return errno;
    if (fwrite(block->code, 1, block->count, f) != (size_t)block->count) return errno;
    if (version_number < 4) return 0;
    int32_t max_for_loop_level = function->max_for_loop_level;
    int32_t locals_size = function->locals_size;
    int32_t local_count = function->locals.count;
    WRITE_OR_ERR(max_for_loop_level, f, errno);
    WRITE_OR_ERR(locals_size, f, errno);
    WRITE_OR_ERR(local_count, f, errno);
    for (int i = 0; i < local_count; ++i) {
        int32_t type = function->locals.items[i].type;
        WRITE_OR_ERR(type, f, errno);
    }
    return 0;
}

static int write_type_entry(struct module *module, type_index type, FILE *f, int version_number) {
    const struct type_info *info = lookup_type(&module->types, type);
    int32_t entry_size = get_type_entry_size(info, version_number);
    WRITE_OR_ERR(entry_size, f, errno);
    int32_t kind = info->kind;
    WRITE_OR_ERR(kind, f, errno);
    int32_t field_count = 0;
    int32_t word_count = 1;
    const type_index *fields = NULL;
    switch (info->kind) {
    case KIND_PACK:
        field_count = info->pack.field_count;
        fields = info->pack.fields;
        break;
    case KIND_COMP:
        field_count = info->comp.field_count;
        word_count = info->comp.word_count;
        fields = info->comp.fields;
        break;
    case KIND_UNINIT:
    case KIND_SIMPLE:
        // Do nothing.
        break;
    }
    WRITE_OR_ERR(field_count, f, errno);
    WRITE_OR_ERR(word_count, f, errno);
    for (int i = 0; i < field_count; ++i) {
        int32_t field_type = fields[i];
        WRITE_OR_ERR(field_type, f, errno);
    }
    return 0;
}

int write_bytecode_ex(struct module *module, FILE *f, int version_number) {
    /* HEADER */
    fprintf(f, "BudeBWFv%d\n", version_number);
    /* DATA-INFO */
    int32_t field_count = get_field_count(version_number);
    if (version_number >= 2) {
        if (fwrite(&field_count, sizeof field_count, 1, f) != 1) return errno;
    }
    int32_t string_count = module->strings.count;
    int32_t function_count = module->functions.count;
    if (fwrite(&string_count, sizeof string_count, 1, f) != 1) return errno;
    if (fwrite(&function_count, sizeof function_count, 1, f) != 1) return errno;
    int32_t ud_type_count = module->types.count - BUILTIN_TYPE_COUNT;
    if (version_number < 4) goto data_section;
    // Version 4+ fields.
    if (fwrite(&ud_type_count, sizeof ud_type_count, 1, f) != 1) return errno;
data_section:
    /* DATA */
    /* STRING-TABLE */
    for (int i = 0; i < module->strings.count; ++i) {
        struct string_view *sv = &module->strings.items[i];
        uint32_t length = sv->length;
        if (fwrite(&length, sizeof length, 1, f) != 1) return errno;
        if (fprintf(f, "%"PRI_SV, SV_FMT(*sv)) != (int)sv->length) return errno;
    }
    /* FUNCTION-TABLE */
    for (int i = 0; i < module->functions.count; ++i) {
        struct function *function = &module->functions.items[i];
        int ret = write_function_entry(module, function, f, version_number);
        if (ret != 0) return ret;
    }
    if (version_number < 4) return 0;
    /* USER-DEFINED-TYPE-TABLE */
    for (int i = 0; i < ud_type_count; ++i) {
        type_index type = i + SIMPLE_TYPE_COUNT + BUILTIN_TYPE_COUNT;
        int ret = write_type_entry(module, type, f, version_number);
        if (ret != 0) return ret;
    }
    return 0;
}
