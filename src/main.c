#include <stdio.h>
#include <stdlib.h>

#include "interpreter.h"
#include "ir.h"


int main(void) {
    struct ir_block *block = allocate_block(0);
    write_immediate(block, OP_PUSH, 1);
    write_immediate(block, OP_PUSH, 2);
    write_simple(block, OP_ADD);
    write_simple(block, OP_PRINT);
    interpret(block);
    free_block(block);
    return 0;
}
