#ifndef HASH_H
#define HASH_H

#include <stdint.h>

#include "string_view.h"

uint32_t hash_sv(const struct string_view *key);

#endif
