#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "../src/function.h"
#include "../src/region.h"
#include "../src/string_view.h"
#include "../src/writer.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "ERROR: Not enough arguments.\n");
        exit(1);
    }
    if (argc > 2) {
        fprintf(stderr, "WARNING: Extra arguments ignored.\n");
    }
    FILE *f = fopen(argv[1], "rb");
    if (f == NULL) {
        perror("Failed to open file");
        exit(1);
    }
    char header_buffer[1024];
    if (!fgets(header_buffer, sizeof header_buffer - 1, f)) {
        perror("Failed to read header line");
        exit(1);
    }
    int version_number = 0;
    if (sscanf(header_buffer, "BudeBWFv%d", &version_number) != 1) {
        perror("Failed to parse header line");
        exit(1);
    }
    int32_t string_count = 0;
    int32_t function_count = 0;
    if (fread(&string_count, sizeof string_count, 1, f) != 1) {
        perror("Failed to read `string_count` field");
        exit(1);
    }
    if (fread(&function_count, sizeof function_count, 1, f) != 1) {
        perror("Failed to read `function_count` field");
        exit(1);
    }
    struct string_view *strings = calloc(string_count, sizeof *strings);
    struct function *functions = calloc(function_count, sizeof *functions);
    struct region *region = new_region(4 * 1204);
    assert(strings && functions);
    for (int i = 0; i < string_count; ++i) {
        uint32_t length = 0;
        if (fread(&length, sizeof length, 1, f) != 1) {
            perror("Failed to read string length field");
            exit(1);
        }
        char *string_chars = region_alloc(region, length);
        if (fread(string_chars, 1, length, f) != length) {
            perror("Failed to read string contents");
            exit(1);
        }
        strings[i] = (struct string_view) {.start = string_chars, .length = length};
    }
    for (int i = 0; i < function_count; ++i) {
        int32_t size = 0;
        if (fread(&size, sizeof size, 1, f) != 1) {
            perror("Failed to read function size field");
            exit(1);
        }
        uint8_t *bytes = region_alloc(region, size);
        if (fread(bytes, 1, size, f) != size) {
            perror("Failed to read bytecode of function");
            exit(1);
        }
        functions[i].w_code = (struct ir_block) {
            .capacity = size,
            .count = size,
            .code = bytes,
            .instruction_set = IR_WORD_ORIENTED,
        };
    }
    struct module module = {
        .strings = {
            .capacity = string_count,
            .count = string_count,
            .items = strings},
        .functions = {
            .count = function_count,
            .capacity = function_count,
            .items = functions,
        },
        .region = region,
        .filename = argv[1],
    };
    display_bytecode(&module, stdout);
}
