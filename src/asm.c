#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "asm.h"


void init_assembly(struct asm_block *assembly) {
    assembly->count = 0;
    assembly->status = ASM_OK;
    memset(&assembly->code, 0, ASM_CODE_SIZE);
}

void asm_vwrite(struct asm_block *assembly, const char *restrict code, va_list args) {
    assert(assembly->count + 1 <= ASM_CODE_SIZE);  // +1 for null byte.
    if (asm_had_error(assembly)) return;  // If there was an error, do nothing.
    size_t max_count = ASM_CODE_SIZE - assembly->count;
    int count = vsnprintf(&assembly->code[assembly->count], max_count, code, args);
    if (count < 0 || (size_t)count > max_count) {
        assembly->status = ASM_WRITE_ERROR;
        return;
    }
    assembly->count += count;
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

static bool can_be_in_fasm_string(char c) {
    return ' ' <= c && c <= '~';  // ASCII only.
}

void asm_write_sv(struct asm_block *assembly, const struct string_view *sv) {
    const char *string = sv->start;
    const char *end = SV_END(*sv);
    char opener = (*string == '"') ? '\'' : '"';
    asm_write(assembly, "%c", opener);
    for (; string != end; ++string) {
        char c = *string;
        if (can_be_in_fasm_string(c)) {
            if (c != opener) {
                asm_write(assembly, "%c", c);
            }
            else {
                char new_opener = (opener == '"') ? '\'' : '"';
                asm_write(assembly, "%c, %c%c", opener, new_opener, c);
                opener = new_opener;
            }
        }
        else {
            asm_write(assembly, "%c", opener);
            do {
                asm_write(assembly, ", %d", c);
                if (string == end) return;
            } while (!can_be_in_fasm_string(c = *++string));
            asm_write(assembly, ", %c%c", opener, c);
        }
    }
    asm_write(assembly, "%c, 0", opener);  // Null terminate string.
}
