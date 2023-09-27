#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "asm.h"


void init_assembly(struct asm_block *assembly) {
    assembly->count = 0;
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

void asm_section_(struct asm_block *assembly, const char *section_name, ...) {
    asm_write(assembly, "section '%s'", section_name);
    va_list args;
    va_start(args, section_name);
    // Section permissions.
    const char *perm;
    while ((perm = va_arg(args, const char *))) {
        asm_write(assembly, " %s", perm);
    }
    va_end(args);
    asm_write(assembly, "\n");
}

void asm_label(struct asm_block *assembly, const char *restrict label, ...) {
    asm_write(assembly, "  ");
    va_list args;
    va_start(args, label);
    asm_vwrite(assembly, label, args);
    va_end(args);
    asm_write(assembly, ":\n");
}

