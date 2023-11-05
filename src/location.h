#ifndef LOCATION_H
#define LOCATION_H

#include <stddef.h>

struct location {
    size_t line, column;
};

void report_location(const char *restrict filename, const struct location *location);

#endif
