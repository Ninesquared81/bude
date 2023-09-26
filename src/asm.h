#ifndef ASM_H
#define ASM_H

#include <stdarg.h>

#define ASM_CODE_SIZE 4 * 1024 *1024

struct asm_block {
    size_t count;
    const char *entry_point;
    char code[ASM_CODE_SIZE];
};

void init_assembly(struct asm_block *assembly);

void asm_write(struct asm_block *assembly, const char *restrict code, ...);
/*
void asm_write_comment(struct asm_block *assembly, const char *restrict comment, ...);

void asm_write_inst1(struct asm_block *assembly, const char *restrict arg1, ...);

void asm_write_inst1c(struct asm_block *assembly,
                      const char *restrict arg1, const char *restrict comment, ...);

void asm_write_inst2(struct asm_block *assembly,
                     const char *restrict arg1, const char *restrict arg2, ...);

void asm_write_inst2c(struct asm_block *assembly,
                      const char *restrict arg1, const char *restrict arg2,
                      const char *comment, ...);
*/

void asm_start_asm(struct asm_block *assembly);

void asm_start_code(struct asm_block *assembly, const char *entry_point);

void asm_end_code(struct asm_block *assembly);



void asm_vwrite(struct asm_block *assembly, const char *restrict code, va_list args);
/*
void asm_vwrite_comment(struct asm_block *assembly, const char *restrict comment, va_list args);

void asm_vwrite_inst1(struct asm_block *assembly, const char *restrict arg1, va_list args);

void asm_vwrite_inst1c(struct asm_block *assembly,
                      const char *restrict arg1, const char *restrict comment, va_list args);

void asm_vwrite_inst2(struct asm_block *assembly,
                     const char *restrict arg1, const char *restrict arg2, va_list args);

void asm_vwrite_inst2c(struct asm_block *assembly,
                      const char *restrict arg1, const char *restrict arg2,
                      const char *comment, va_list args);
*/
#endif
