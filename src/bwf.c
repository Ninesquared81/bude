#include <assert.h>

#include "bwf.h"
#include "function.h"
#include "type.h"


int get_field_count(int version_number) {
    switch (version_number) {
    case 1:
    case 2:
    case 3:
        return 2;
    case 4:
        return 3;
    default:
        static_assert(BWF_version_number <= 4);
        assert(0 && "Unreachable");
        return 0;
    }
}

int get_function_entry_size(struct function *function, int version_number) {
    int function_code_size = function->w_code.count;
    int locals_count = function->locals.count;
    switch (version_number) {
    case 1:
    case 2:
        return function_code_size;
    case 3: return 4 + function_code_size;
    case 4: return 4 + function_code_size + 3*4 + locals_count*4;
    default:
        static_assert(BWF_version_number <= 4);
        assert(0 && "Unreachable");
        return 0;
    }
}

int get_type_entry_size(const struct type_info *info, int version_number) {
    switch (version_number) {
    case 1:
    case 2:
    case 3:
        return 0;
    case 4:
        switch (info->kind) {
        case KIND_UNINIT:
        case KIND_SIMPLE:
            return 3*8;
        case KIND_PACK:
            return 3*8 + info->pack.field_count;
        case KIND_COMP:
            return 3*8 + info->comp.field_count;
        }
        break;
    default:
        static_assert(BWF_version_number <= 4);
        assert(0 && "Unreachable");
    }
    return 0;
}
