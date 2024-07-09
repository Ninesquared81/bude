#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "bwf.h"
#include "ir.h"
#include "function.h"
#include "location.h"
#include "memory.h"
#include "reader.h"
#include "region.h"
#include "string_view.h"


#define reader_version_number 2


static int parse_header(FILE *f) {
    static char header_buffer[1024] = {0};
    if (fgets(header_buffer, sizeof header_buffer, f) == NULL) {
        perror("Failed to read header line");
        return -1;
    }
    int version_number = -1;
    if (sscanf(header_buffer, "BudeBWFv%d", &version_number) != 1) {
        fprintf(stderr, "Invalid BudeBWF header\n");
        return -1;
    }
    return version_number;
}

static bool parse_data_info(FILE *f, int version_number, int *string_count, int *function_count) {
    if (version_number > 1) return false;
    return fread(string_count, sizeof *string_count, 1, f) == 1
        && fread(function_count, sizeof *function_count, 1, f) == 1;
}

static bool parse_data(FILE *f, int string_count, int function_count,
                       struct string_view strings[string_count],
                       struct function functions[function_count],
                       struct region *region) {
    for (int i = 0; i < string_count; ++i) {
        uint32_t size = 0;
        if (fread(&size, sizeof size, 1, f) != 1) return false;
        char *string = region_alloc(region, size);
        if (string == NULL) return false;
        if (fread(string, 1, size, f) != size) return false;
        strings[i] = (struct string_view) {.length = size, .start = string};
    }
    for (int i = 0; i < function_count; ++i) {
        int32_t size = 0;
        if (fread(&size, sizeof size, 1, f) != 1) return false;
        uint8_t *code = allocate_array(size, 1);
        struct location *locations = allocate_array(size, sizeof *locations);
        if (fread(code, 1, size, f) != (size_t)size) return false;
        // NOTE: We set all other field to 0/NULL since we don't care about them.
        functions[i] = (struct function) {
            .w_code = {
                .instruction_set = IR_WORD_ORIENTED,
                .capacity = size,
                .count = size,
                .code = code,
                .locations = locations,  // NOTE: All initialised to 0.
            }
        };
    }
    return true;
}

struct module read_bytecode(const char *filename) {
    struct module module;
    init_module(&module, filename);
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        perror("Failed to open file");
        exit(1);
    }
    int version_number = parse_header(f);
    if (version_number <= 0) goto error;  // Error parsing header.
    if (version_number > reader_version_number) {
        fprintf(stderr, "BWF version number %d not supported.\n", version_number);
        goto error;
    }
    int string_count = 0;
    int function_count = 0;
    if (!parse_data_info(f, version_number, &string_count, &function_count)) goto error;
    struct string_table *string_table = &module.strings;
    struct function_table *function_table = &module.functions;
    if (string_table->capacity < string_count) {
        reallocate_array(string_table->items, string_table->capacity, string_count, 1);
        string_table->capacity = string_count;
    }
    string_table->count = string_count;
    if (function_table->capacity < function_count) {
        reallocate_array(function_table->items, function_table->capacity, function_count, 1);
        function_table->capacity = function_count;
    }
    function_table->count = function_count;
    if (!parse_data(f, string_count, function_count, string_table->items,
                    function_table->items, module.region)) goto error;
    fclose(f);
    return module;
error:
    fclose(f);
    exit(1);
}
