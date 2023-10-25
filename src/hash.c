#include "hash.h"

uint32_t hash_sv(const struct string_view *key) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < key->length; ++i) {
        hash ^= key->start[i];
        hash *= 16777619;
    }
    return hash;
}
