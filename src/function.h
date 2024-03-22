#ifndef FUNCTION_H
#define FUNCTION_H

#include "ir.h"
#include "region.h"
#include "type.h"

struct signature {
    int param_count;
    int ret_count;
    type_index *params;
    type_index *rets;
};

struct function {
    struct ir_block t_code;
    struct ir_block w_code;
    struct signature sig;
};

struct function_table {
    int count;
    int capacity;
    struct function *functions;
    struct region *region;
};

#endif
