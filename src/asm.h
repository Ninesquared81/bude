#ifndef ASM_H
#define ASM_H

#include <stdarg.h>
#include <stddef.h>

#define ASM_CODE_SIZE 4 * 1024 *1024

struct asm_block {
    size_t count;
    enum asm_status {
        ASM_OK,
        ASM_WRITE_ERROR,
    } status;
    char code[ASM_CODE_SIZE];
};

void init_assembly(struct asm_block *assembly);

void asm_write(struct asm_block *assembly, const char *restrict code, ...);
void asm_vwrite(struct asm_block *assembly, const char *restrict code, va_list args);

void asm_write_string(struct asm_block *assembly, const char *restrict string);

void asm_start_asm(struct asm_block *assembly);
void asm_section_(struct asm_block *assembly, const char *section_name, ...);
void asm_label(struct asm_block *assembly, const char *label, ...);

#define asm_had_error(assembly) ((assembly)->status != ASM_OK)
#define asm_reset_status(assembly) ((assembly)->status = ASM_OK)

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
    asm_write(assembly, "\t" inst "\t" arg1 ", " arg2 "\t; " comment "\n")
#define asm_write_inst3(assembly, inst, arg1, arg2, arg3) \
    asm_write(assembly, "\t" inst "\t" arg1 ", " arg2 ", " arg3 "\n")
#define asm_write_inst3c(assembly, inst, arg1, arg2, arg3, comment) \
    asm_write(assembly, "\t" inst "\t" arg1 ", " arg2 ", " arg3 "\t; " comment "\n")
#define asm_write_inst4(assembly, inst, arg1, arg2, arg3, arg4) \
    asm_write(assembly, "\t" inst "\t" arg1 ", " arg2 ", " arg3 ", " arg4 "\n")
#define asm_write_inst4c(assembly, inst, arg1, arg2, arg3, arg4, comment) \
    asm_write(assembly, "\t" inst "\t" arg1 ", " arg2 ", " arg3 ", " arg4 "\t; " comment "\n")


#define asm_write_inst0f(assembly, inst, ...)            \
    asm_write(assembly, "\t" inst "\n", __VA_ARGS__)
#define asm_write_inst0cf(assembly, inst, comment, ...) \
    asm_write(assembly, "\t" inst "\t\t; " comment "\n", VA_ARGS)
#define asm_write_inst1f(assembly, inst, arg1, ...) \
    asm_write(assembly, "\t" inst "\t" arg1 "\n", __VA_ARGS__)
#define asm_write_inst1cf(assembly, inst, arg1, comment, ...) \
    asm_write(assembly, "\t" inst "\t" arg1 "\t; " comment "\n", __VA_ARGS__)
#define asm_write_inst2f(assembly, inst, arg1, arg2, ...) \
    asm_write(assembly, "\t" inst "\t" arg1 ", " arg2 "\n", __VA_ARGS__)
#define asm_write_inst2cf(assembly, inst, arg1, arg2, comment, ...) \
    asm_write(assembly, "\t" inst "\t" arg1 ", " arg2 "\t; " comment "\n", __VA_ARGS__)
#define asm_write_inst3f(assembly, inst, arg1, arg2, arg3, ...) \
    asm_write(assembly, "\t" inst "\t" arg1 ", " arg2 ", " arg3 "\n", __VA_ARGS__)
#define asm_write_inst3cf(assembly, inst, arg1, arg2, arg3, comment, ...) \
    asm_write(assembly, "\t" inst "\t" arg1 ", " arg2 ", " arg3 "\t; " comment "\n",\
              __VA_ARGS__)
#define asm_write_inst4f(assembly, inst, arg1, arg2, arg3, arg4, ...) \
    asm_write(assembly, "\t" inst "\t" arg1 ", " arg2 ", " arg3 ", " arg4 "\n", __VA_ARGS__)
#define asm_write_inst4cf(assembly, inst, arg1, arg2, arg3, arg4, comment, ...) \
    asm_write(assembly, "\t" inst "\t" arg1 ", " arg2 ", " arg3 ", " arg4 "\t; " comment "\n",\
              __VA_ARGS__)

#define asm_section(assembly, ...) asm_section_(assembly, __VA_ARGS__, NULL)

#endif
