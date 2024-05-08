#include <stdio.h>

#include "module.h"
#include "writer.h"

void display_bytecode(struct module *module, FILE *f) {
    for (int i = 0; i < (int)module->strings.count; ++i) {
        struct string_view *sv = &module->strings.views[i];
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
        struct function *function = &module->functions.functions[i];
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
    (void)module, (void)f;
    return 1;
}

