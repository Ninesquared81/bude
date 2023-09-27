#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "asm.h"


void init_assembly(struct asm_block *assembly) {
    assembly->count = 0;
    assembly->entry_point = NULL;
}

void asm_vwrite(struct asm_block *assembly, const char *restrict code, va_list args) {
    assert(assembly->count + 1 <= ASM_CODE_SIZE);  // +1 for null byte.
    size_t max_count = ASM_CODE_SIZE - assembly->count;
    int count = vsnprintf(&assembly->code[assembly->count], max_count, code, args);
    if (count < 0 || (size_t)count > max_count) {
        fprintf(stderr, "Could not write assembly code.\n");
        exit(1);
    }
    assembly->count += count;
    assembly->code[assembly->count] = '\0';
}

void asm_write(struct asm_block *assembly, const char *restrict code, ...) {
    va_list args;
    va_start(args, code);
    asm_vwrite(assembly, code, args);
    va_end(args);
}

void asm_start_asm(struct asm_block *assembly) {
    asm_write(assembly, "format PE64 console\n");
    asm_write(assembly, "include 'win64ax.inc'\n\n");
}

void asm_start_code(struct asm_block *assembly, const char *entry_point) {
    assembly->entry_point = entry_point;
    asm_write(assembly, ".code\n");
    asm_write(assembly, "  %s:\n\n", entry_point);
}

void asm_end_code(struct asm_block *assembly) {
    asm_write(assembly, "\n.end %s\n", assembly->entry_point);
}

