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
    case 5:
        return 5;
    default:
        static_assert(BWF_version_number <= 5);
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
    case 3:
        return 4 + function_code_size;
    case 4:
    case 5:
        return 4 + function_code_size + 3*4 + locals_count*4;
    default:
        static_assert(BWF_version_number <= 5);
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
    case 5:
        switch (info->kind) {
        case KIND_UNINIT:
        case KIND_SIMPLE:
            return 3*4;
        case KIND_PACK:
            return 3*4 + info->pack.field_count*4;
        case KIND_COMP:
            return 3*4 + info->comp.field_count*4;
        case KIND_ARRAY:
            return 3*4 + 1*4;  // field-count = 1; word-count = element_count;
                               // fields[0] = element_type.
        }
        break;
    default:
        static_assert(BWF_version_number <= 5);
        assert(0 && "Unreachable");
    }
    return 0;
}

int get_ext_function_entry_size(struct ext_function *external, int version_number) {
    switch (version_number) {
    case 1:
    case 2:
    case 3:
    case 4:
        return 0;
    case 5:
        return 2*4 + (external->sig.param_count + external->sig.ret_count)*4 + 2*4;
    default:
        static_assert(BWF_version_number <= 5);
        assert(0 && "Unreachable");
    }
    return 0;
}

int get_ext_library_entry_size(struct ext_library *library, int version_number) {
    switch (version_number) {
    case 1:
    case 2:
    case 3:
    case 4:
        return 0;
    case 5:
        return 4 + library->count*4 + 4;
    default:
        static_assert(BWF_version_number <= 5);
        assert(0 && "Unreachable");
    }
    return 0;
}
