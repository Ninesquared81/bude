#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "symbol.h"

#define HASH2_P 113u  // Prime number < table size.
static_assert(HASH2_P < SYMDICT_INIT_SIZE);

static uint32_t hash2(uint32_t hash) {
    return HASH2_P - (hash % HASH2_P);
}

void init_symbol_dictionary(struct symbol_dictionary *dict) {
    dict->capacity = SYMDICT_INIT_SIZE;
    dict->count = 0;
    dict->slots = calloc(SYMDICT_INIT_SIZE, sizeof dict->slots[0]);
}

void free_symbol_dictionary(struct symbol_dictionary *dict) {
    free(dict->slots);
    dict->capacity = 0;
    dict->count = 0;
}

static bool check_name(const struct symdict_slot *slot, uint32_t hash,
                       const struct string_view *name) {
    return slot->hash == hash && sv_eq(&slot->symbol.name, name);
}

static struct symdict_slot *find_slot(const struct symbol_dictionary *dict,
                                      const struct string_view *name, uint32_t hash) {
    size_t index = hash % dict->capacity;
    struct symdict_slot *slot = &dict->slots[index];
    size_t stride = hash2(hash);
    size_t visit_count = 1;
    while (slot->is_occupied && !check_name(slot, hash, name)) {
        if (visit_count >= dict->capacity) return NULL;  // Not enough space.
        ++visit_count;
        if (slot < &dict->slots[dict->capacity] - stride) {
            slot += stride;
        } else {
            index = slot - dict->slots;
            index += stride;
            slot = &dict->slots[index];
        }
    }
    return slot;
}

void insert_symbol(struct symbol_dictionary *dict, struct symbol *symbol) {
    uint32_t hash = hash_sv(&symbol->name);
    struct symdict_slot *slot = find_slot(dict, &symbol->name, hash);
    assert(slot);  // Todo: grow dict when there isn't space.
    memcpy(&slot->symbol, symbol, sizeof *symbol);
    slot->is_occupied = true;
    slot->hash = hash;
}

struct symbol *lookup_symbol(const struct symbol_dictionary *dict,
                             const struct string_view *name) {
    uint32_t hash = hash_sv(name);
    struct symdict_slot *slot = find_slot(dict, name, hash);
    if (slot == NULL || !slot->is_occupied) return NULL;
    return &slot->symbol;
}
