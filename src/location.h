#ifndef LOCATION_H
#define LOCATION_H

#include <stddef.h>

#ifndef LINE_START
#define LINE_START 1
#endif

#ifndef COLUMN_START
#define COLUMN_START 1
#endif

struct location {
    size_t line, column;
};

void report_location(const char *restrict filename, const struct location *location);

#endif
