#include "builtins.h"
#include "string_view.h"
#include "type.h"

static const struct symbol builtins[] = {
    {.name = SV_LIT("start"), .type = SYM_COMP_FIELD, .comp_field = {
                .comp = TYPE_STRING, .field_offset = 0}},
    {.name = SV_LIT("length"), .type = SYM_COMP_FIELD, .comp_field = {
                .comp = TYPE_STRING, .field_offset = 1}},
};

#define BUILTINS_COUNT                          \
    (sizeof builtins / sizeof buitlins[0])

void init_builtins(struct symbol_dictionary *symbols) {
    for (int i = 0; i < BUILTINS_COUNT; ++i) {
        insert_symbol(symbols, buitlins[i]);
    }
}
