
#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "interpreter.h"
#include "ir.h"


int main(void) {
    const char *code = "34 35 +\nprint";
    struct ir_block *block = allocate_block(0);
    /*
    write_immediate(block, OP_PUSH, 34);
    write_immediate(block, OP_PUSH, 35);
    write_simple(block, OP_ADD);
    write_simple(block, OP_PRINT);
    */
    compile(code, block);
    interpret(block);
    free_block(block);
    return 0;
}
