#ifndef ASM_H
#define ASM_H

#include <stdarg.h>
#include <stddef.h>

#define ASM_CODE_SIZE 4 * 1024 *1024

struct asm_block {
    size_t count;
    const char *entry_point;
    char code[ASM_CODE_SIZE];
};

void init_assembly(struct asm_block *assembly);

void asm_write(struct asm_block *assembly, const char *restrict code, ...);

void asm_start_asm(struct asm_block *assembly);

void asm_start_code(struct asm_block *assembly, const char *entry_point);

void asm_end_code(struct asm_block *assembly);



void asm_vwrite(struct asm_block *assembly, const char *restrict code, va_list args);

void asm_start_asm(struct asm_block *assembly);
void asm_section(struct asm_block *assembly, const char *section_name, ...);
void asm_label(struct asm_block *assembly, const char *label);

#define asm_write_inst0(assembly, inst) \
    asm_write(assembly, "\t" inst "\n")
#define asm_write_inst0c(assembly, inst, comment) \
    asm_write(assembly, "\t" inst "\t\t; " comment "\n")
#define asm_write_inst1(assembly, inst, arg1) \
    asm_write(assembly, "\t" inst "\t" arg1 "\n")
#define asm_write_inst1c(assembly, inst, arg1, comment) \
    asm_write(assembly, "\t" inst "\t" arg1 "\t\t; " comment "\n")
#define asm_write_inst2(assembly, inst, arg1, arg2) \
    asm_write(assembly, "\t" inst "\t" arg1 ", " arg2 "\n")
#define asm_write_inst2c(assembly, inst, arg1, arg2, comment) \
    asm_write(assembly, "\t" inst "\t" arg1 ", " arg2 "\t\t; " comment "\n")

#define asm_write_inst0f(assembly, inst, ...)            \
    asm_write(assembly, "\t" inst "\n", __VA_ARGS__)
#define asm_write_inst0cf(assembly, inst, comment, ...) \
    asm_write(assembly, "\t" inst "\t\t; " comment "\n", VA_ARGS)
#define asm_write_inst1f(assembly, inst, arg1, ...) \
    asm_write(assembly, "\t" inst "\t" arg1 "\n", __VA_ARGS__)
#define asm_write_inst1cf(assembly, inst, arg1, comment, ...) \
    asm_write(assembly, "\t" inst "\t" arg1 "\t\t; " comment "\n", __VA_ARGS__)
#define asm_write_inst2f(assembly, inst, arg1, arg2, ...) \
    asm_write(assembly, "\t" inst "\t" arg1 ", " arg2 "\n", __VA_ARGS__)
#define asm_write_inst2cf(assembly, inst, arg1, arg2, comment, ...) \
    asm_write(assembly, "\t" inst "\t" arg1 ", " arg2 "\t\t; " comment "\n", __VA_ARGS__)

#endif
