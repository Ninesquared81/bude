#include <stdio.h>

#include "bwf.h"
#include "module.h"
#include "writer.h"

#define writer_version_number 3


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
    int32_t entry_size = get_function_entry_size(block->count, version_number);
    if (version_number >= 3) {
        if (fwrite(&entry_size, sizeof entry_size, 1, f) != 1) return errno;
    }
    if (fwrite(&block->count, sizeof block->count, 1, f) != 1) return errno;
    if (fwrite(block->code, 1, block->count, f) != (size_t)block->count) return errno;
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
    if (version_number < 3) return 0;
    // Version 3+ fields here...
    return 0;
}
