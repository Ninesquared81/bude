#ifndef EXT_FUNCTION_H
#define EXT_FUNCTION_H

#include "function.h"
#include "string_view.h"

enum calling_convention {
    CC_BUDE,        /* Bude Calling Convention (args and return on stack). */
    CC_NATIVE,      /* Platform native calling convnetion. */
    CC_MS_X64,      /* Microsoft x64 Calling Convention (1st 4 args in regs, return in rax). */
    CC_SYSV_AMD64,  /* System V AMD64 Calling Convention (1st 6 args in regs, return in rax). */
};

struct ext_function {
    struct signature sig;
    struct string_view name;
    enum calling_convention call_conv;
};

struct external_table {
    int capacity;
    int count;
    struct ext_function *items;
};

struct ext_library {
    int capacity;
    int count;
    int *items;  // Array of external function indices in this library.
    struct string_view filename;
};

struct ext_lib_table {
    int capacity;
    int count;
    struct ext_library *items;
};

void init_external_table(struct external_table *externals);
void free_external_table(struct external_table *externals);

void init_ext_lib_table(struct ext_lib_table *libraries);
void free_ext_lib_table(struct ext_lib_table *libraries);

int add_external(struct external_table *externals, struct ext_library *library,
                 struct ext_function *external);
struct ext_function *get_external(struct external_table *externals, int index);

#endif
