#include <assert.h>

#include "bwf.h"


int get_field_count(int version_number) {
    switch (version_number) {
    case 1:
    case 2:
    case 3:
        return 2;
    default:
        static_assert(BWF_version_number <= 3);
        assert(0 && "Unreachable");
        return 0;
    }
}

int get_function_entry_size(int function_code_size, int version_number) {
    switch (version_number) {
    case 1:
    case 2:
        return function_code_size;
    case 3: return function_code_size + 4;
    default:
        static_assert(BWF_version_number <= 3);
        assert(0 && "Unreachable");
        return 0;
    }
}
