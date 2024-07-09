#include <assert.h>

#include "bwf.h"


int get_field_count(int version_number) {
    switch (version_number) {
    case 1:
    case 2:
        return 2;
    default:
        assert(0 && "Unreachable");
        return 0;
    }
}
