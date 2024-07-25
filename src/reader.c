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


#define reader_version_number 5


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

static bool parse_data_info(FILE *f, int version_number, struct data_info *di) {
    int32_t field_count = 2;
    long start_pos = ftell(f);
    if (version_number >= 2) {
        // Read data-field-count field
        if (fread(&field_count, sizeof field_count, 1, f) != 1) return false;
        if (field_count < 2) {
            fprintf(stderr, "Bad `data-info-field-count`: %d.\n", field_count);
            return false;
        }
    }
    if (fread(&di->string_count, sizeof di->string_count, 1, f) != 1) return false;
    if (fread(&di->function_count, sizeof di->function_count, 1, f) != 1) return false;
    if (version_number < 4) goto skip_rest;
    // Version 4+ fields.
    if (fread(&di->ud_type_count, sizeof di->ud_type_count, 1, f) != 1) return false;
    if (version_number < 5) goto skip_rest;
    // Version 5+ fields.
    if (fread(&di->ext_function_count, sizeof di->ext_function_count, 1, f) != 1) return false;
    if (fread(&di->ext_library_count, sizeof di->ext_library_count, 1, f) != 1) return false;
skip_rest:
    long bytes_left = start_pos + (field_count + 1)*4 - ftell(f);
    assert(bytes_left >= 0);
    if (bytes_left > 0) {
        fprintf(stderr, "Warning: extra fields not read\n.");
        if (fseek(f, bytes_left, SEEK_CUR) != 0) return false;
    }
    return true;
}

static bool parse_function(FILE *f, int version_number, struct function *function) {
    long start_pos = ftell(f);
    int32_t entry_size = 0;
    if (version_number >= 3) {
        if (fread(&entry_size, sizeof entry_size, 1, f) != 1) return false;
    }
    int32_t size = 0;
    if (fread(&size, sizeof size, 1, f) != 1) return false;
    if (entry_size == 0) entry_size = size;
    if (size < 0) return false;
    uint8_t *code = allocate_array(size, 1);
    struct location *locations = allocate_array(size, sizeof *locations);
    int32_t max_for_loop_level = 0;
    int32_t locals_size = 0;
    int32_t local_count = 0;
    struct local *locals = NULL;
    if (fread(code, 1, size, f) != (size_t)size) return false;
    if (version_number < 4) goto skip_rest;
    // Version 4+ fields.
    if (fread(&max_for_loop_level, sizeof max_for_loop_level, 1, f) != 1) return false;
    if (fread(&locals_size, sizeof locals_size, 1, f) != 1) return false;
    if (fread(&local_count, sizeof local_count, 1, f) != 1) return false;
    if (local_count < 0) return false;
    locals = allocate_array(local_count, sizeof *locals);
    if (fread(locals, sizeof locals[0], local_count, f) != (size_t)local_count) return false;
skip_rest:
    long bytes_left = (start_pos + entry_size + 4) - ftell(f);
    if (bytes_left < 0) return false;
    if (fseek(f, bytes_left, SEEK_CUR) != 0) return false;
    // NOTE: We set all other field to 0/NULL since we don't care about them.
    *function = (struct function) {
        .w_code = {
            .instruction_set = IR_WORD_ORIENTED,
            .capacity = size,
            .count = size,
            .code = code,
            .locations = locations,  // NOTE: All initialised to 0.
        },
        .locals = {
            .capacity = local_count,
            .count = local_count,
            .items = locals,
        },
        .max_for_loop_level = max_for_loop_level,
        .locals_size = locals_size,
    };
    init_jump_info_table(&function->w_code.jumps);
    recompute_jump_dests(&function->w_code);
    return true;
}

static bool parse_type(FILE *f, int version_number, struct type_info *info,
                       struct region *region) {
    (void)version_number;
    int32_t entry_size = 0;
    int32_t kind = KIND_UNINIT;
    int32_t field_count = 0;
    int32_t word_count = 1;
    type_index *fields = NULL;
    long start_pos = ftell(f);
    if (fread(&entry_size, sizeof entry_size, 1, f) != 1) return false;
    if (fread(&kind, sizeof kind, 1, f) != 1) return false;
    if (fread(&field_count, sizeof field_count, 1, f) != 1) return false;
    if (fread(&word_count, sizeof word_count, 1, f) != 1) return false;
    if (field_count < 0 || word_count < 0) return false;
    info->kind = kind;
    switch (info->kind) {
    case KIND_PACK:
        if (field_count > 8) return false;
        fields = info->pack.fields;
        info->pack.field_count = field_count;
        break;
    case KIND_COMP:
        fields = region_calloc(region, field_count, sizeof fields[0]);
        info->comp.fields = fields;
        info->comp.field_count = field_count;
        info->comp.word_count = word_count;
        break;
    case KIND_UNINIT:
    case KIND_SIMPLE:
        // Do nothing.
        break;
    }
    if (fields != NULL) {
        if (fread(fields, sizeof fields[0], field_count, f) != (size_t)field_count) return false;
    }
    long bytes_left = start_pos + 4 + entry_size - ftell(f);
    if (bytes_left < 0) return false;
    if (fseek(f, bytes_left, SEEK_CUR) != 0) return false;
    return true;
}

static bool parse_ext_function(FILE *f, int version_number, struct ext_function *external,
                               struct module *module) {
    (void)version_number;
    int32_t entry_size = 0;
    int32_t param_count = 0;
    int32_t ret_count = 0;
    long start_pos = ftell(f);
    if (fread(&entry_size, sizeof entry_size, 1, f) != 1) return false;
    if (fread(&param_count, sizeof param_count, 1, f) != 1) return false;
    if (fread(&ret_count, sizeof ret_count, 1, f) != 1) return false;
    if (param_count < 0 || ret_count < 0) return false;
    type_index *params = region_calloc(module->region, param_count, sizeof params[0]);
    CHECK_ARRAY_ALLOCATION(params, param_count);
    type_index *rets = region_calloc(module->region, ret_count, sizeof rets[0]);
    CHECK_ARRAY_ALLOCATION(rets, ret_count);
    for (int i = 0; i < param_count; ++i) {
        int32_t param_type = 0;
        if (fread(&param_type, sizeof param_type, 1, f) != 1) return false;
        params[i] = param_type;
    }
    for (int i = 0; i < ret_count; ++i) {
        int32_t ret_type = 0;
        if (fread(&ret_type, sizeof ret_type, 1, f) != 1) return false;
        rets[i] = ret_type;
    }
    int32_t name_index = 0;
    int32_t call_conv = 0;
    if (fread(&name_index, sizeof name_index, 1, f) != 1) return false;
    if (fread(&call_conv, sizeof call_conv, 1, f) != 1) return false;
    if (name_index < 0 || name_index >= module->strings.count) return false;
    long bytes_left = start_pos + 4 + entry_size - ftell(f);
    if (bytes_left < 0) return false;
    if (fseek(f, bytes_left, SEEK_CUR) != 0) return false;
    *external = (struct ext_function) {
        .sig = {param_count, ret_count, params, rets},
        .name = module->strings.items[name_index],
        .call_conv = call_conv,
    };
    return true;
}

static bool parse_ext_library(FILE *f, int version_number, struct ext_library *library,
                              struct module *module) {
    (void)version_number;
    int32_t entry_size = 0;
    int32_t external_count = 0;
    long start_pos = ftell(f);
    if (fread(&entry_size, sizeof entry_size, 1, f) != 1) return false;
    if (fread(&external_count, sizeof external_count, 1, f) != 1) return false;
    type_index *externals = allocate_array(external_count, sizeof externals[0]);
    CHECK_ARRAY_ALLOCATION(externals, external_count);
    for (int i = 0; i < external_count; ++i) {
        int32_t external_index = 0;
        if (fread(&external_index, sizeof external_index, 1, f) != 1) return false;
        externals[i] = external_index;
    }
    int32_t filename_index = 0;
    if (fread(&filename_index, sizeof filename_index, 1, f) != 1) return false;
    if (filename_index < 0 || filename_index >= module->strings.count) return false;
    long bytes_left = start_pos + 4 + entry_size - ftell(f);
    if (bytes_left < 0) return false;
    // TODO: print a diagnostic like in `parse_data_info()`.
    if (fseek(f, bytes_left, SEEK_CUR) != 0) return false;
    *library = (struct ext_library) {
        .capacity = external_count,
        .count = external_count,
        .items = externals,
        .filename = module->strings.items[filename_index],
    };
    return true;
}

static bool parse_data(FILE *f, int version_number, struct module *module) {
    for (int i = 0; i < module->strings.count; ++i) {
        uint32_t size = 0;
        if (fread(&size, sizeof size, 1, f) != 1) return false;
        char *string = region_alloc(module->region, size);
        if (string == NULL) return false;
        if (fread(string, 1, size, f) != size) return false;
        module->strings.items[i] = (struct string_view) {.length = size, .start = string};
    }
    for (int i = 0; i < module->functions.count; ++i) {
        struct function *function = &module->functions.items[i];
        if (!parse_function(f, version_number, function)) return false;
    }
    if (version_number < 4) return true;
    for (int i = BUILTIN_TYPE_COUNT; i < module->types.count; ++i) {
        struct type_info *info = &module->types.items[i];
        if (!parse_type(f, version_number, info, module->types.extra_info)) return false;
    }
    if (version_number < 5) return true;
    for (int i = 0; i < module->externals.count; ++i) {
        struct ext_function *external = &module->externals.items[i];
        if (!parse_ext_function(f, version_number, external, module)) return false;
    }
    for (int i = 0; i < module->ext_libraries.count; ++i) {
        struct ext_library *library = &module->ext_libraries.items[i];
        if (!parse_ext_library(f, version_number, library, module)) return false;
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
    struct data_info di = {0};
    if (!parse_data_info(f, version_number, &di)) goto error;
    struct string_table *strings = &module.strings;
    struct function_table *functions = &module.functions;
    struct type_table *types = &module.types;
    struct external_table *externals = &module.externals;
    struct ext_lib_table *ext_libraries = &module.ext_libraries;
    if (strings->capacity < di.string_count) {
        reallocate_array(strings->items, strings->capacity, di.string_count, 1);
        strings->capacity = di.string_count;
    }
    strings->count = di.string_count;
    if (functions->capacity < di.function_count) {
        reallocate_array(functions->items, functions->capacity, di.function_count,
                         sizeof functions->items[0]);
        functions->capacity = di.function_count;
    }
    functions->count = di.function_count;
    int type_count = di.ud_type_count + BUILTIN_TYPE_COUNT;
    if (types->capacity < type_count) {
        reallocate_array(types->items, types->capacity, type_count, 1);
        types->capacity = type_count;
    }
    types->count = type_count;
    if (externals->capacity < di.ext_function_count) {
        reallocate_array(externals->items, externals->capacity, di.ext_function_count,
                         sizeof externals->items[0]);
        externals->capacity = di.ext_function_count;
    }
    externals->count = di.ext_function_count;
    if (ext_libraries->capacity < di.ext_library_count) {
        reallocate_array(ext_libraries->items, ext_libraries->capacity, di.ext_library_count,
                         sizeof ext_libraries->items[0]);
        ext_libraries->capacity = di.ext_library_count;
    }
    ext_libraries->count = di.ext_library_count;
    if (!parse_data(f, version_number, &module)) goto error;
    fclose(f);
    return module;
error:
    fclose(f);
    exit(1);
}
