#ifndef BWF_H
#define BWF_H

#define BWF_version_number 5

/* Bude Binary Word-oriented Format version 5
 *
 * BudeBWF is a file format for storing word-oriented Bude IR code.
 * The format is structured as a series of fixed-sized fields and variable-sized data entries
 * organised into different sections. For variable-size entries, the data is preceeded by a
 * fixed-size field holding its size.
 *
 * The sections are as follows:
 *  - HEADER section comprising the file format's "magic number" (a series of ASCII characters
 *    spelling out "BudeBWF" and the version number (the ASCII character "v" followed by 1 or
 *    more ASCII digits). The current version number for this standard is version 5. The HEADER
 *    section is terminated by an ASCII line feed character.
 *  - DATA-INFO section holding information pertaining to the data section and -- from version
 *    2 onwards -- the data-info-field-count which holds the number of other fields in this
 *    section.
 *  - DATA section comprising the main data of the IR.
 *
 * Structure:
 *
 * HEADER
 *   magic-number:`BudeBWF` version-number:`v`..digit[] `\n`
 * DATA-INFO
 *   data-info-field-count:s32   } version >= 2
 *   string-count:s32
 *   function-count:s32
 *   user-defined-type-count:s32 } version >= 4
 *   external-function-count:s32 \ version >= 5
 *   external-library-count:s32  /
 * DATA
 *   STRING-TABLE
 *     size:u32 contents:byte[]
 *     ...
 *   FUNCTION-TABLE
 *     entry-size:s32            } version >= 3
 *     code-size:s32 code:byte[]
 *     max-for-loop-level:s32    \
 *     locals-size:s32           |
 *     local-count:s32           | version >= 4
 *     LOCAL-TABLE               |
 *       type-index:s32          |
 *       ...                     /
 *     ...
 *   USER-DEFINED-TYPE-TABLE     \
 *     entry-size:s32            |
 *     kind:s32                  |
 *     field-count:s32           |
 *     word-count:s32            | version >= 4
 *     FIELD-LIST                |
 *       type-index:s32          |
 *       ...                     |
 *     ...                       /
 *   EXTERNAL-FUNCTION-TABLE     \
 *     entry-size:s32            |
 *     param-count:s32           |
 *     ret-count:s32             |
 *     PARAM-LIST                |
 *       type-index:s32          |
 *       ...                     |
 *     RET-LIST                  |
 *       type-index:s32          |
 *       ...                     |
 *     name-index:s32            | version >= 5
 *     calling-convention:s32    |
 *     ...                       |
 *   EXTERNAL-LIBRARY-TABLE      |
 *     entry-size:s32            |
 *     external-count:s32        |
 *     EXTERNAL-LIST             |
 *       external-index:s32      |
 *       ...                     |
 *     filename-index:s32        |
 *     ...                       /
 *
 */

#include "ext_function.h"
#include "function.h"
#include "type.h"


struct data_info {
    int32_t string_count;
    int32_t function_count;
    int32_t ud_type_count;
    int32_t ext_function_count;
    int32_t ext_library_count;
};


int get_field_count(int version_number);
int get_function_entry_size(struct function *function, int version_number);
int get_type_entry_size(const struct type_info *info, int version_number);
int get_ext_function_entry_size(struct ext_function *external, int version_number);
int get_ext_library_entry_size(struct ext_library *library, int version_number);

#endif
