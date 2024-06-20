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

#endif
