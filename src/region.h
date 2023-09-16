#ifndef REGION_H
#define REGION_H

#include <stddef.h>

struct region {
    struct region *next;
    struct region *parent;
    size_t size;
    char bytes[];
};

#endif
