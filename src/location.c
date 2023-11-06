#include <stdio.h>

#include "location.h"

void report_location(const char *restrict filename, const struct location *location) {
    fprintf(stderr, "%s:%zu:%zu: ", filename, location->line, location->column);
}
