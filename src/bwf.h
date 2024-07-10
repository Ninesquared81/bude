#ifndef BWF_H
#define BWF_H

#define BWF_version_number 4

/* Bude Binary Word-oriented Format version 4
 *
 * BudeBWF is a file format for storing word-oriented Bude IR code.
 * The format is structured as a series of fixed-sized fields and variable-sized data entries
 * organised into different sections. For variable-size entries, the data is preceeded by a
 * fixed-size field holding its size.
 *
 * The sections are as follows:
 *  - HEADER section comprising the file format's "magic number" (a series of ASCII characters
 *    spelling out "BudeBWF" and the version number (the ASCII character "v" followed by 1 or
 *    more ASCII digits). The current version number for this standard is version 4. The HEADER
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
 *
 */

#include "function.h"
#include "type.h"


int get_field_count(int version_number);
int get_function_entry_size(struct function *function, int version_number);
int get_type_entry_size(const struct type_info *info, int version_number);

#endif
