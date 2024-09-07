#include <stdio.h>

#include "bwf.h"
#include "module.h"
#include "writer.h"

#define writer_version_number 5


#define WRITE(obj, f) \
    fwrite(&obj, sizeof obj, 1, f)

#define WRITE_OR_ERR(obj, f, err_ret) \
    if (WRITE(obj, f) != 1) return err_ret


void display_bytecode(struct module *module, FILE *f) {
    for (int i = 0; i < module->strings.count; ++i) {
        struct string_view *sv = &module->strings.items[i];
        fprintf(f, "str_%d:\n\t\"", i);
        for (const char *p = sv->start; p < SV_END(*sv); ++p) {
            char c = *p;
            switch (c) {
            case '"':
                fprintf(f,"\\\"");
                break;
            case '\n':
                fprintf(f,"\\n");
                break;
            case '\t':
                fprintf(f,"\\t");
                break;
            case '\r':
                fprintf(f,"\\r");
                break;
            case '\\':
                fprintf(f,"\\\\");
                break;
            default:
                fprintf(f,"%c", *p);
            }
        }
        fprintf(f,"\"\n");
    }
#define BYTECODE_COLUMN_COUNT 16
    for (int i = 0; i < module->functions.count; ++i) {
        struct function *function = &module->functions.items[i];
        struct ir_block *block = &function->w_code;
        fprintf(f,"func_%d:\n\t", i);
        int line_count = block->count / BYTECODE_COLUMN_COUNT;
        int leftover_count = block->count % BYTECODE_COLUMN_COUNT;
        for (int j = 0; j < line_count; ++j) {
            for (int k = 0; k < BYTECODE_COLUMN_COUNT; ++k) {
                fprintf(f,"%.2x ", block->code[j*BYTECODE_COLUMN_COUNT + k]);
            }
            fprintf(f,"\n\t");
        }
        for (int k = 0; k < leftover_count; ++k) {
            fprintf(f,"%.2x ", block->code[line_count*BYTECODE_COLUMN_COUNT + k]);
        }
        if (leftover_count > 0) {
            fprintf(f,"\n");
        }
    }
#undef BYTECODE_COLUMN_COUNT
}

int write_bytecode(struct module *module, FILE *f) {
    return write_bytecode_ex(module, f, BWF_version_number);
}

static int write_data_info(struct data_info di, FILE *f, int version_number) {
    int32_t field_count = get_field_count(version_number);
    if (version_number >= 2) {
        WRITE_OR_ERR(field_count, f, errno);
    }
    WRITE_OR_ERR(di.string_count, f, errno);
    WRITE_OR_ERR(di.function_count, f, errno);
    if (version_number < 4) return 0;
    // Version 4+ fields.
    WRITE_OR_ERR(di.ud_type_count, f, errno);
    if (version_number < 5) return 0;
    // Version 5+ fields.
    WRITE_OR_ERR(di.ext_function_count, f, errno);
    WRITE_OR_ERR(di.ext_library_count , f, errno);
    return 0;
}

static int write_function_entry(struct module *module, struct function *function,
                                FILE *f, int version_number) {
    (void)module;
    struct ir_block *block = &function->w_code;
    int32_t entry_size = get_function_entry_size(function, version_number);
    if (version_number >= 3) {
        WRITE_OR_ERR(entry_size, f, errno);
    }
    if (fwrite(&block->count, sizeof block->count, 1, f) != 1) return errno;
    if (fwrite(block->code, 1, block->count, f) != (size_t)block->count) return errno;
    if (version_number < 4) return 0;
    int32_t max_for_loop_level = function->max_for_loop_level;
    int32_t locals_size = function->locals_size;
    int32_t local_count = function->locals.count;
    WRITE_OR_ERR(max_for_loop_level, f, errno);
    WRITE_OR_ERR(locals_size, f, errno);
    WRITE_OR_ERR(local_count, f, errno);
    for (int i = 0; i < local_count; ++i) {
        int32_t type = function->locals.items[i].type;
        WRITE_OR_ERR(type, f, errno);
    }
    return 0;
}

static int write_type_entry(struct module *module, type_index type, FILE *f, int version_number) {
    const struct type_info *info = lookup_type(&module->types, type);
    int32_t entry_size = get_type_entry_size(info, version_number);
    WRITE_OR_ERR(entry_size, f, errno);
    int32_t kind = info->kind;
    WRITE_OR_ERR(kind, f, errno);
    int32_t field_count = 0;
    int32_t word_count = 1;
    const type_index *fields = NULL;
    switch (info->kind) {
    case KIND_PACK:
        field_count = info->pack.field_count;
        fields = info->pack.fields;
        break;
    case KIND_COMP:
        field_count = info->comp.field_count;
        word_count = info->comp.word_count;
        fields = info->comp.fields;
        break;
    case KIND_ARRAY:
        field_count = 1;
        word_count = info->array.element_count;
        fields = &info->array.element_type;
        break;
    case KIND_UNINIT:
    case KIND_SIMPLE:
        // Do nothing.
        break;
    }
    WRITE_OR_ERR(field_count, f, errno);
    WRITE_OR_ERR(word_count, f, errno);
    for (int i = 0; i < field_count; ++i) {
        int32_t field_type = fields[i];
        WRITE_OR_ERR(field_type, f, errno);
    }
    return 0;
}

static int write_ext_function_entry(struct module *module, struct ext_function *external,
                                    FILE *f, int version_number) {
    int32_t entry_size = get_ext_function_entry_size(external, version_number);
    WRITE_OR_ERR(entry_size, f, errno);
    int32_t param_count = external->sig.param_count;
    int32_t ret_count = external->sig.ret_count;
    WRITE_OR_ERR(param_count, f, errno);
    WRITE_OR_ERR(ret_count, f, errno);
    for (int i = 0; i < param_count; ++i) {
        int32_t param_type = external->sig.params[i];
        WRITE_OR_ERR(param_type, f, errno);
    }
    for (int i = 0; i < ret_count; ++i) {
        int32_t ret_type = external->sig.rets[i];
        WRITE_OR_ERR(ret_type, f, errno);
    }
    int32_t name_index = find_string(module, &external->name);
    assert(name_index > 0);
    int32_t call_conv = external->call_conv;
    WRITE_OR_ERR(name_index, f, errno);
    WRITE_OR_ERR(call_conv, f, errno);
    return 0;
}

static int write_ext_library_entry(struct module *module, struct ext_library *library,
                                   FILE *f, int version_number) {
    int32_t entry_size = get_ext_library_entry_size(library, version_number);
    WRITE_OR_ERR(entry_size, f, errno);
    int32_t external_count = library->count;
    WRITE_OR_ERR(external_count, f, errno);
    for (int i = 0; i < external_count; ++i) {
        int32_t external_index = library->items[i];
        WRITE_OR_ERR(external_index, f, errno);
    }
    int32_t filename_index = find_string(module, &library->filename);
    assert(filename_index > 0);
    WRITE_OR_ERR(filename_index, f, errno);
    return 0;
}

int write_bytecode_ex(struct module *module, FILE *f, int version_number) {
    /* HEADER */
    fprintf(f, "BudeBWFv%d\n", version_number);
    /* DATA-INFO */
    struct data_info di = {
        .string_count       = module->strings.count,
        .function_count     = module->functions.count,
        .ud_type_count      = module->types.count - BUILTIN_TYPE_COUNT,
        .ext_function_count = module->externals.count,
        .ext_library_count  = module->ext_libraries.count,
    };
    int ret = write_data_info(di, f, version_number);
    if (ret != 0) return ret;
    /* DATA */
    /* STRING-TABLE */
    for (int i = 0; i < di.string_count; ++i) {
        struct string_view *sv = &module->strings.items[i];
        uint32_t length = sv->length;
        WRITE_OR_ERR(length, f, errno);
        if (fprintf(f, "%"PRI_SV, SV_FMT(*sv)) != (int)sv->length) return errno;
    }
    /* FUNCTION-TABLE */
    for (int i = 0; i < di.function_count; ++i) {
        struct function *function = &module->functions.items[i];
        int ret = write_function_entry(module, function, f, version_number);
        if (ret != 0) return ret;
    }
    if (version_number < 4) return 0;
    /* USER-DEFINED-TYPE-TABLE */
    for (int i = 0; i < di.ud_type_count; ++i) {
        type_index type = i + SIMPLE_TYPE_COUNT + BUILTIN_TYPE_COUNT;
        int ret = write_type_entry(module, type, f, version_number);
        if (ret != 0) return ret;
    }
    if (version_number < 5) return 0;
    /* EXTERNAL-FUNCTION-TABLE */
    for (int i = 0; i < di.ext_function_count; ++i) {
        struct ext_function *external = &module->externals.items[i];
        int ret = write_ext_function_entry(module, external, f, version_number);
        if (ret != 0) return ret;
    }
    /* EXTERNAL-LIBRARY-TABLE */
    for (int i = 0; i < di.ext_library_count; ++i) {
        struct ext_library *library = &module->ext_libraries.items[i];
        int ret = write_ext_library_entry(module, library, f, version_number);
        if (ret != 0) return ret;
    }
    return 0;
}
