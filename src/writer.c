#include <stdio.h>

#include "module.h"
#include "writer.h"

#define BWF_version_number 1

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
    /* Structure:
     * HEADER
     *   magic-number:`BudeBWF` version-number:char[] `\n`
     * DATA-INFO
     *   string-count:s32
     *   function-count:s32
     * DATA
     *   STRING-TABLE
     *     size:u32 contents:byte[]
     *     ...
     *   FUNCTION-TABLE
     *     size:s32 contents:byte[]
     *     ...
     */
    /* HEADER */
    fprintf(f, "BudeBWFv%d\n", BWF_version_number);
    /* DATA-INFO */
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
        struct ir_block *block = &function->w_code;
        if (fwrite(&block->count, sizeof block->count, 1, f) != 1) return errno;
        if (fwrite(block->code, 1, block->count, f) != (size_t)block->count) return errno;
    }
    return 0;
}
