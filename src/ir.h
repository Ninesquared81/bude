#ifndef IR_H
#define IR_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "string_builder.h"
#include "string_view.h"

/* Documenation:
 * Below is a mini-grammar for the language used to informally document each IR instruction:
 *
 * instruction  = <mnemonic> [operand-type]
 *   mnemonic   = NOP, PUSHn (n: size variable), POP, ...
 * operand-type = <operand-kind>_<operand-sign><operand-size>
 * operand-kind = Imm (immediate), Idx (array index), Off (relative offset)
 * operand-sign = u, s
 * operand-size = 8, 16, 32, 64, n (n: size variable)
 */

#define T_OPCODES                                                       \
    /* NOP -- no operation. */                                          \
    X(T_OP_NOP)                                                         \
    /* PUSHn Imm_un -- Push a word to the stack. */                     \
    X(T_OP_PUSH8)                                                       \
    X(T_OP_PUSH16)                                                      \
    X(T_OP_PUSH32)                                                      \
    X(T_OP_PUSH64)                                                      \
    /* PUSH_INTn Imm_sn -- Push a word-sized signed integer to the stack. */ \
    X(T_OP_PUSH_INT8)                                                   \
    X(T_OP_PUSH_INT16)                                                  \
    X(T_OP_PUSH_INT32)                                                  \
    X(T_OP_PUSH_INT64)                                                  \
    /* PUSH_FLOATn Imm_fn -- Push a floating-point value to the stack. */ \
    X(T_OP_PUSH_FLOAT32)                                                \
    X(T_OP_PUSH_FLOAT64)                                                \
    /* PUSH_CHARn Imm_un -- Push a Unicode codepoint to the stack. */   \
    X(T_OP_PUSH_CHAR8)                                                  \
    X(T_OP_PUSH_CHAR16)                                                 \
    X(T_OP_PUSH_CHAR32)                                                 \
    /* LOAD_STRINGn Idx_un -- Load a string (ptr word) onto the stack from the strings table. */ \
    X(T_OP_LOAD_STRING8)                                                \
    X(T_OP_LOAD_STRING16)                                               \
    X(T_OP_LOAD_STRING32)                                               \
    /* POP -- Pop and discard the top stack element. */                 \
    X(T_OP_POP)                                                         \
    /* ADD -- Add top two stack elements. */                            \
    X(T_OP_ADD)                                                         \
    /* AND -- Logical (value-preserving) and operation of top two elements. */ \
    X(T_OP_AND)                                                         \
    /* DEREF -- Dereference (byte) ptr at top of stack. */              \
    X(T_OP_DEREF)                                                       \
    /* DIV -- Divide next element by top. For integers, this is equivalent to DIVMOD; POP. */ \
    X(T_OP_DIV)                                                         \
    /* DIVMOD -- Unsigned division and reaminder. */                    \
    X(T_OP_DIVMOD)                                                      \
    /* IDIVMOD -- Signed (truncated) division and remainder. */         \
    X(T_OP_IDIVMOD)                                                     \
    /* EDIVMOD -- Signed (Euclidean) divion and remainder (remaninder non-negative). */ \
    X(T_OP_EDIVMOD)                                                     \
    /* DUPE -- Duplicate top stack element. */                          \
    X(T_OP_DUPE)                                                        \
    /* EQUALS -- Pop top two stack elements and push 1 if they're equal or 0 if not. */ \
    X(T_OP_EQUALS)                                                      \
    /* EXIT -- Exit the program, using the top of the stack as the exit code. */ \
    X(T_OP_EXIT)                                                        \
    /* FOR_DEC_START Off_s16 -- Initialise a for loop counter to the top element. */ \
    X(T_OP_FOR_DEC_START)                                               \
    /* FOR_DEC Off_s16 -- Decrement a for loop counter by 1 and loop while it's not zero. */ \
    X(T_OP_FOR_DEC)                                                     \
    /* FOR_INC_START Off_s16 -- Initialise a for loop counter to zero and the target to the \
       top element. */                                                  \
    X(T_OP_FOR_INC_START)                                               \
    /* FOR_INC Off_s16 -- Increment a for loop counter by 1 and jump a given distance if \
       the counter is not equal to the target. */                       \
    X(T_OP_FOR_INC)                                                     \
    /* GET_LOOP_VAR Idx_u16 -- Get the loop variable a given distance from the current loop. */ \
    X(T_OP_GET_LOOP_VAR)                                                \
    /* GREATER_EQUALS -- Pop top two stack elements and push 1 if next element is greater than \
       or equal to the top element, or 0 if not. */                     \
    X(T_OP_GREATER_EQUALS)                                              \
    /* GREATER_THAN -- Pop top two stack elements and push 1 if next element is greater than \
       the top element, or 0 if not. */                                 \
    X(T_OP_GREATER_THAN)                                                \
    /* JUMP Off_s16 -- Jump a given distance. */                        \
    X(T_OP_JUMP)                                                        \
    /* JUMP_COND Off_s16 -- Jump the given distance if the top element is non-zero (true). */ \
    X(T_OP_JUMP_COND)                                                   \
    /* JUMP_NCOND Off_s16 -- Jump the given distance if the top element is zero (false). */ \
    X(T_OP_JUMP_NCOND)                                                  \
    /* LESS_EQUALS -- Pop top two stack elements and push 1 if next element is less than \
       or equal to the top element, or 0 if not. */                     \
    X(T_OP_LESS_EQUALS)                                                 \
    /* LESS_THAN -- Pop top two stack elements and push 1 if next element is less than the \
       top element, or 0 if not. */                                     \
    X(T_OP_LESS_THAN)                                                   \
    /* LOCAL_GET Idx_u16 -- Get the local variable with the given index. */ \
    X(T_OP_LOCAL_GET)                                                   \
    /* LOCAL_SET Idx_u16 -- Set the local variable with the given index. */ \
    X(T_OP_LOCAL_SET)                                                   \
    /* MULT -- Multiply the top two stack elements. */                  \
    X(T_OP_MULT)                                                        \
    /* NOT -- Logical not operation of the top stack element. */        \
    X(T_OP_NOT)                                                         \
    /* NOT_EQUALS -- Pop top two stack elements and push 1 if they're different or 0 if not. */ \
    X(T_OP_NOT_EQUALS)                                                  \
    /* OR -- Logical (value-preserving) or operation of top two stack elements. */ \
    X(T_OP_OR)                                                          \
    /* (T)PRINT -- Print the top element of the stack in a format surmised from its type. */ \
    X(T_OP_PRINT)                                                       \
    /* OVER -- Copy the next element over the top element. */           \
    X(T_OP_OVER)                                                        \
    /* PRINT_CHAR -- Print the top element of the stack as a character. */ \
    X(T_OP_PRINT_CHAR)                                                  \
    /* PRINT_INT -- Print the top element of the stack as a signed integer. */ \
    X(T_OP_PRINT_INT)                                                   \
    /* ROT -- Rotate the top three stack elements. */                   \
    X(T_OP_ROT)                                                         \
    /* SUB -- Subtract the top stack element from the next element. */  \
    X(T_OP_SUB)                                                         \
    /* SWAP -- Swap the top two stack elements. */                      \
    X(T_OP_SWAP)                                                        \
    /* AS_type -- Clear any excess bits and treat as an value of that type. */ \
    X(T_OP_AS_WORD)                                                     \
    X(T_OP_AS_BYTE)                                                     \
    X(T_OP_AS_PTR)                                                      \
    X(T_OP_AS_INT)                                                      \
    X(T_OP_AS_U8)                                                       \
    X(T_OP_AS_U16)                                                      \
    X(T_OP_AS_U32)                                                      \
    X(T_OP_AS_S8)                                                       \
    X(T_OP_AS_S16)                                                      \
    X(T_OP_AS_S32)                                                      \
    X(T_OP_AS_F32)                                                      \
    X(T_OP_AS_F64)                                                      \
    X(T_OP_AS_CHAR)                                                     \
    X(T_OP_AS_CHAR16)                                                   \
    X(T_OP_AS_CHAR32)                                                   \
    /* TO_type -- Convert to the closest representable value of that type. */ \
    X(T_OP_TO_WORD)                                                     \
    X(T_OP_TO_BYTE)                                                     \
    X(T_OP_TO_PTR)                                                      \
    X(T_OP_TO_INT)                                                      \
    X(T_OP_TO_U8)                                                       \
    X(T_OP_TO_U16)                                                      \
    X(T_OP_TO_U32)                                                      \
    X(T_OP_TO_S8)                                                       \
    X(T_OP_TO_S16)                                                      \
    X(T_OP_TO_S32)                                                      \
    X(T_OP_TO_F32)                                                      \
    X(T_OP_TO_F64)                                                      \
    X(T_OP_TO_CHAR)                                                     \
    X(T_OP_TO_CHAR16)                                                   \
    X(T_OP_TO_CHAR32)                                                   \
    /* (T)PACKn Idx_un -- Construct a pack with the given type index. */ \
    X(T_OP_PACK8)                                                       \
    X(T_OP_PACK16)                                                      \
    X(T_OP_PACK32)                                                      \
    /* (T)COMPn Idx_un -- Construct a comp with the given type index. */ \
    X(T_OP_COMP8)                                                       \
    X(T_OP_COMP16)                                                      \
    X(T_OP_COMP32)                                                      \
    /* (T)UNPACK -- Unpack the current pack on the top of the stack. */ \
    X(T_OP_UNPACK)                                                      \
    /* DECOMP -- Decompose the current pack on the top of the stack. */ \
    X(T_OP_DECOMP)                                                      \
    /* (T)PACK_FIELD_GETn Idx_un Imm_u8 -- Retrieve the field of the pack at the given offset. */ \
    X(T_OP_PACK_FIELD_GET8)                                             \
    X(T_OP_PACK_FIELD_GET16)                                            \
    X(T_OP_PACK_FIELD_GET32)                                            \
    /* (T)COMP_FIELD_GETn Idx_un Imm_un -- Retrieve the field of the comp at the given offset. */ \
    X(T_OP_COMP_FIELD_GET8)                                             \
    X(T_OP_COMP_FIELD_GET16)                                            \
    X(T_OP_COMP_FIELD_GET32)                                            \
    /* (T)PACK_FIELD_SETn Idx_sn Imm_sn -- Set the field of the pack at the given offset. */ \
    X(T_OP_PACK_FIELD_SET8)                                             \
    X(T_OP_PACK_FIELD_SET16)                                            \
    X(T_OP_PACK_FIELD_SET32)                                            \
    /* (T)COMP_FIELD_SETn Idx_un Imm_un -- Set the field of the comp at the given offset. */ \
    X(T_OP_COMP_FIELD_SET8)                                             \
    X(T_OP_COMP_FIELD_SET16)                                            \
    X(T_OP_COMP_FIELD_SET32)                                            \
    /* CALLn Idx_un -- Call the function specified by the index in the function table. */ \
    X(T_OP_CALL8)                                                       \
    X(T_OP_CALL16)                                                      \
    X(T_OP_CALL32)                                                      \
    /* RET -- Return from the current function. */                      \
    X(T_OP_RET)

#define W_OPCODES                                                       \
    /* NOP -- no operation. */                                          \
    X(W_OP_NOP)                                                         \
    /* PUSHn Imm_un -- Push a word to the stack. */                     \
    X(W_OP_PUSH8)                                                       \
    X(W_OP_PUSH16)                                                      \
    X(W_OP_PUSH32)                                                      \
    X(W_OP_PUSH64)                                                      \
    /* PUSH_INTn Imm_sn -- Push a word-sized signed integer to the stack. */ \
    X(W_OP_PUSH_INT8)                                                   \
    X(W_OP_PUSH_INT16)                                                  \
    X(W_OP_PUSH_INT32)                                                  \
    X(W_OP_PUSH_INT64)                                                  \
    /* PUSH_FLOATn Imm_fn -- Push a floating-point value to the stack. */ \
    X(W_OP_PUSH_FLOAT32)                                                \
    X(W_OP_PUSH_FLOAT64)                                                \
    /* PUSH_CHARn Imm_un -- Push a Unicode codepoint to the stack. */   \
    X(W_OP_PUSH_CHAR8)                                                  \
    X(W_OP_PUSH_CHAR16)                                                 \
    X(W_OP_PUSH_CHAR32)                                                 \
    /* LOAD_STRINGn Imm_un -- Load a string (ptr word) onto the stack from the strings table. */ \
    X(W_OP_LOAD_STRING8)                                                \
    X(W_OP_LOAD_STRING16)                                               \
    X(W_OP_LOAD_STRING32)                                               \
    /* POP -- Pop and discard the top stack element. */                 \
    X(W_OP_POP)                                                         \
    /* POPNn Imm_sn -- Pop the top N elements from the stack. */        \
    X(W_OP_POPN8)                                                       \
    X(W_OP_POPN16)                                                      \
    X(W_OP_POPN32)                                                      \
    /* ADD -- Add top two stack elements. */                            \
    X(W_OP_ADD)                                                         \
    /* ADDF32 -- Add top two IEEE 754 single-precision (binary 32-bit) floating-point values */ \
    X(W_OP_ADDF32)                                                      \
    /* ADDF64 -- Add top two IEEE 754 double-precision (binary 64-bit) floating-point values */ \
    X(W_OP_ADDF64)                                                      \
    /* AND -- Logical (value-preserving) and operation of top two elements. */ \
    X(W_OP_AND)                                                         \
    /* DEREF -- Dereference (byte) ptr at top of stack. */              \
    X(W_OP_DEREF)                                                       \
    /* DIVF32 -- Single-precsion floating-point division. */            \
    X(W_OP_DIVF32)                                                      \
    /* DIVF64 -- Double-precision floating-point division. */           \
    X(W_OP_DIVF64)                                                      \
    /* DIVMOD -- Unsigned division and reaminder. */                    \
    X(W_OP_DIVMOD)                                                      \
    /* IDIVMOD -- Signed (truncated) division and remainder. */         \
    X(W_OP_IDIVMOD)                                                     \
    /* EDIVMOD -- Signed (Euclidean) divion and remainder (remaninder non-negative). */ \
    X(W_OP_EDIVMOD)                                                     \
    /* DUPE -- Duplicate top stack element. */                          \
    X(W_OP_DUPE)                                                        \
    /* DUPENn -- Duplicate the top N elements on the stack. */          \
    X(W_OP_DUPEN8)                                                      \
    X(W_OP_DUPEN16)                                                     \
    X(W_OP_DUPEN32)                                                     \
    /* EQUALS -- Pop top two stack elements and push 1 if they're equal or 0 if not. */ \
    X(W_OP_EQUALS)                                                      \
    /* EXIT -- Exit the program, using the top of the stack as the exit code. */ \
    X(W_OP_EXIT)                                                        \
    /* FOR_DEC_START Off_s16 -- Initialise a for loop counter to the top element. */ \
    X(W_OP_FOR_DEC_START)                                               \
    /* FOR_DEC Off_s16 -- Decrement a for loop counter by 1 and loop while it's not zero. */ \
    X(W_OP_FOR_DEC)                                                     \
    /* FOR_INC_START Off_s16 -- Initialise a for loop counter to zero and the target to the \
       top element. */                                                  \
    X(W_OP_FOR_INC_START)                                               \
    /* FOR_INC Off_s16 -- Increment a for loop counter by 1 and jump a given distance if \
       the counter is not equal to the target. */                       \
    X(W_OP_FOR_INC)                                                     \
    /* GET_LOOP_VAR Idx_u16 -- Get the loop variable a given distance from the current loop. */ \
    X(W_OP_GET_LOOP_VAR)                                                \
    /* GREATER_EQUALS -- Pop top two (signed) stack elements and push 1 if next element is greater \
       than or equal to the top element, or 0 if not. */                \
    X(W_OP_GREATER_EQUALS)                                              \
    /* GREATER_THAN -- Pop top two (signed) stack elements and push 1 if next element is greater \
       than the top element, or 0 if not. */                            \
    X(W_OP_GREATER_THAN)                                                \
    /* HIGHER_SAME -- Pop top two (unsigned) stack elements and push 1 if next element is higher \
       or the same as the top element, or 0 if not. */                  \
    X(W_OP_HIGHER_SAME)                                                 \
    /* HIGHER_THAN -- Pop top two (unsigned) stack elements and push 1 if next element is higher \
       than the top element, or 0 if not. */                            \
    X(W_OP_HIGHER_THAN)                                                 \
    /* JUMP Off_s16 -- Jump a given distance. */                        \
    X(W_OP_JUMP)                                                        \
    /* JUMP_COND Off_s16 -- Jump the given distance if the top element is non-zero (true). */ \
    X(W_OP_JUMP_COND)                                                   \
    /* JUMP_NCOND Off_s16 -- Jump the given distance if the top element is zero (false). */ \
    X(W_OP_JUMP_NCOND)                                                  \
    /* LESS_EQUALS -- Pop top two (signed) stack elements and push 1 if next element is less than \
       or equal to the top element, or 0 if not. */                     \
    X(W_OP_LESS_EQUALS)                                                 \
    /* LESS_THAN -- Pop top two (signed) stack elements and push 1 if next element is less than the \
       top element, or 0 if not. */                                     \
    X(W_OP_LESS_THAN)                                                   \
    /* LOCAL_GET Idx_u16 -- Get the local variable with the given index. */ \
    X(W_OP_LOCAL_GET)                                                   \
    /* LOCAL_SET Idx_u16 -- Set the local variable with the given index. */ \
    X(W_OP_LOCAL_SET)                                                   \
    /* LOWER_SAME -- Pop top two (unsigned) stack elements and push 1 if next element is lower or the \
       same as the top element, or 0 if not. */                         \
    X(W_OP_LOWER_SAME)                                                  \
    /* LOWER_THAN -- Pop top two (unsigned) stack elements and push 1 if next element is lower than the \
       top element, or 0 if not. */                                     \
    X(W_OP_LOWER_THAN)                                                  \
    /* MULT -- Multiply the top two stack elements. */                  \
    X(W_OP_MULT)                                                        \
    /* MULTF32 -- Single-precision floating-point multiplication of top two stack elements. */ \
    X(W_OP_MULTF32)                                                     \
    /* MULTF64 -- Double-precision floating-point multiplication of top two stack elements. */ \
    X(W_OP_MULTF64)                                                     \
    /* NOT -- Logical not operation of the top two stack elements. */   \
    X(W_OP_NOT)                                                         \
    /* NOT_EQUALS -- Pop top two stack elements and push 1 if they're different or 0 if not. */ \
    X(W_OP_NOT_EQUALS)                                                  \
    /* OR -- Logical (value-preserving) or operation of top two stack elements. */ \
    X(W_OP_OR)                                                          \
    /* (W)PRINT -- Print the top element of the stack as a word. */     \
    X(W_OP_PRINT)                                                       \
    /* PRINT_CHAR -- Print the top element of the stack as a character. */ \
    X(W_OP_PRINT_CHAR)                                                  \
    /* PRINT_FLOAT -- Print the top element of the stack as an IEEE 754 double-precision (binary 64-bit) floating-point value. */ \
    X(W_OP_PRINT_FLOAT)                                                 \
    /* PRINT_INT -- Print the top element of the stack as a signed integer. */ \
    X(W_OP_PRINT_INT)                                                   \
    /* PRINT_STRING -- Print the top two stack elements (start pointer, length) as a string. */ \
    X(W_OP_PRINT_STRING)                                                \
    /* SUB -- Subtract the top stack element from the next element. */  \
    X(W_OP_SUB)                                                         \
    /* SUBF32 -- Single-precision floating-point subtraction of top element from next element. */ \
    X(W_OP_SUBF32)                                                      \
    /* SUBF64 -- Double-precision floating-point subtraction of top element from next element. */ \
    X(W_OP_SUBF64)                                                      \
    /* SWAP -- Swap the top two stack words. */                         \
    X(W_OP_SWAP)                                                        \
    /* SWAP_COMPSn Imm_sn Imm_sn -- Swap the two comps at the top of the stack with the \
       specified sizes. */                                              \
    X(W_OP_SWAP_COMPS8)                                                 \
    X(W_OP_SWAP_COMPS16)                                                \
    X(W_OP_SWAP_COMPS32)                                                \
    /* SXn, SXnL -- sign extend an n-bit integer. The -L versions operate on the \
       element under the top (i.e. the left-hand side of a binary operation). */ \
    X(W_OP_SX8)                                                         \
    X(W_OP_SX8L)                                                        \
    X(W_OP_SX16)                                                        \
    X(W_OP_SX16L)                                                       \
    X(W_OP_SX32)                                                        \
    X(W_OP_SX32L)                                                       \
    /* ZXn, ZXnL -- zero extend an n-bit integer. The -L versions operate on the \
       element under the top (i.e. the left-hand side of a binary operation). */ \
    X(W_OP_ZX8)                                                         \
    X(W_OP_ZX8L)                                                        \
    X(W_OP_ZX16)                                                        \
    X(W_OP_ZX16L)                                                       \
    X(W_OP_ZX32)                                                        \
    X(W_OP_ZX32L)                                                       \
    /* FPROM, FPROML -- promote a single-precision floating-point value to double-precision. \
       The -L version operates on the element under the top. */         \
    X(W_OP_FPROM)                                                       \
    X(W_OP_FPROML)                                                      \
    /* FDEM -- demote a double-precision floating-point value to single-precision. */ \
    X(W_OP_FDEM)                                                        \
    /* ICONVF32, ICONVF32L -- convert a 64-bit integer to a 32-bit single-precision \
       floating-point value. The -L version operates on the element under the top. */ \
    X(W_OP_ICONVF32)                                                    \
    X(W_OP_ICONVF32L)                                                   \
    /* ICONVF64, ICONVF64L -- convert a 64-bit integer to a 64-bit double-precision \
       floating-point value. The -L version operates on the element under the top. */ \
    X(W_OP_ICONVF64)                                                    \
    X(W_OP_ICONVF64L)                                                   \
    /* FCONVI32 -- convert a 32-bit single-precision floating-point value to a \
       64-bit integer. */                                               \
    X(W_OP_FCONVI32)                                                    \
    /* FCONVI64 -- convert a 64-bit double-precision floating-point value to a \
       64-bit intger. */                                                \
    X(W_OP_FCONVI64)                                                    \
    /* ICONVC32 -- convert a signed 64-bit integer to a UTF-32 codepoint. Values \
       not in the range 0--0x10ffff are clamped. */                     \
    X(W_OP_ICONVC32)                                                    \
    /* CHAR_8CONV32 -- convert a UTF-8--encoded character to UTF-32. */ \
    X(W_OP_CHAR_8CONV32)                                                \
    /* CHAR_32CONV8 -- convert a UTF-32--encoded character to UTF-8. */ \
    X(W_OP_CHAR_32CONV8)                                                \
    /* CHAR_16CONV32 -- convert a UTF-16--encoded character to UTF-32. */ \
    X(W_OP_CHAR_16CONV32)                                               \
    /* CHAR_32CONV16 -- convert a UTF-32--encoded character to UTF-16. */ \
    X(W_OP_CHAR_32CONV16)                                               \
    /* (W)PACKn Imm_u8... -- Construct a pack with n fields of the provided sizes. */ \
    X(W_OP_PACK1)                                                       \
    X(W_OP_PACK2)                                                       \
    X(W_OP_PACK3)                                                       \
    X(W_OP_PACK4)                                                       \
    X(W_OP_PACK5)                                                       \
    X(W_OP_PACK6)                                                       \
    X(W_OP_PACK7)                                                       \
    X(W_OP_PACK8)                                                       \
    /* (W)UNPACKn Imm_u8... -- Deconstruct a pack with n fields of the provided sizes. */ \
    X(W_OP_UNPACK1)                                                     \
    X(W_OP_UNPACK2)                                                     \
    X(W_OP_UNPACK3)                                                     \
    X(W_OP_UNPACK4)                                                     \
    X(W_OP_UNPACK5)                                                     \
    X(W_OP_UNPACK6)                                                     \
    X(W_OP_UNPACK7)                                                     \
    X(W_OP_UNPACK8)                                                     \
    /* (W)PACK_FIELD_GET Imm_u8 Imm_u8 -- Retrieve the field of a pack at the given offset \
       and of the given size. */                                        \
    X(W_OP_PACK_FIELD_GET)                                              \
    /* (W)COMP_FIELD_GETn Imm_un -- Retrieve the field of a comp at the given offset. */ \
    X(W_OP_COMP_FIELD_GET8)                                             \
    X(W_OP_COMP_FIELD_GET16)                                            \
    X(W_OP_COMP_FIELD_GET32)                                            \
    /* (W)PACK_FIELD_SET Imm_u8 Imm_u8 -- Set the field of a pack at the given offset \
       and of the given size. */                                        \
    X(W_OP_PACK_FIELD_SET)                                              \
    /* (W)COMP_FIELD_SETn Imm_un -- Set the field of a comp at the given offset. */ \
    X(W_OP_COMP_FIELD_SET8)                                             \
    X(W_OP_COMP_FIELD_SET16)                                            \
    X(W_OP_COMP_FIELD_SET32)                                            \
    /* (W)COMP_SUBCOMP_GETn Imm_sn Imm_sn -- Get the value of a subcomp. */ \
    X(W_OP_COMP_SUBCOMP_GET8)                                           \
    X(W_OP_COMP_SUBCOMP_GET16)                                          \
    X(W_OP_COMP_SUBCOMP_GET32)                                          \
    /* (W)COMP_SUBCOMP_SETn Imm_sn Imm_sn -- Set the value of a subcomp. */ \
    X(W_OP_COMP_SUBCOMP_SET8)                                           \
    X(W_OP_COMP_SUBCOMP_SET16)                                          \
    X(W_OP_COMP_SUBCOMP_SET32)                                          \
    /* CALLn Idx_un -- Call the function specified by the index in the function table. */ \
    X(W_OP_CALL8)                                                       \
    X(W_OP_CALL16)                                                      \
    X(W_OP_CALL32)                                                      \
    /* RET -- Return from the current function. */                      \
    X(W_OP_RET)

#define X(opcode) opcode,
enum t_opcode {
    T_OPCODES
};

enum w_opcode {
    W_OPCODES
};
#undef X

static_assert(T_OP_NOP == 0 && W_OP_NOP == 0);

enum ir_instruction_set {
    IR_TYPED,
    IR_WORD_ORIENTED,
};

struct jump_info_table {
    int capacity;
    int count;
    int *dests;
};

struct ir_block {
    int capacity;
    int count;
    uint8_t *code;
    struct location *locations;
    enum ir_instruction_set instruction_set;
    struct jump_info_table jumps;
};

typedef int opcode;

const char *get_t_opcode_name(enum t_opcode opcode);
const char *get_w_opcode_name(enum w_opcode opcode);

#define get_opcode_name(opcode)                 \
    _Generic((opcode),                          \
             enum t_opcode: get_t_opcode_name,  \
             enum w_opcode: get_w_opcode_name   \
        )(opcode)

bool is_t_jump(enum t_opcode instruction);
bool is_w_jump(enum w_opcode instruction);

#define is_jump(instruction)\
    _Generic((instruction),\
             enum t_opcode: is_t_jump,  \
             enum w_opcode: is_w_jump   \
        )(instruction)

void init_block(struct ir_block *block, enum ir_instruction_set instruction_set);
void free_block(struct ir_block *block);
void init_jump_info_table(struct jump_info_table *table);
void free_jump_info_table(struct jump_info_table *table);

void copy_metadata(struct ir_block *restrict dest, struct ir_block *restrict src);

void write_simple(struct ir_block *block, opcode instruction, struct location *location);

void write_immediate_u8(struct ir_block *block, opcode instruction, uint8_t operand,
                        struct location *location);
void write_immediate_s8(struct ir_block *block, opcode instruction, int8_t operand,
                        struct location *location);
void write_immediate_u16(struct ir_block *block, opcode instruction, uint16_t operand,
                         struct location *location);
void write_immediate_s16(struct ir_block *block, opcode instruction, int16_t operand,
                         struct location *location);
void write_immediate_u32(struct ir_block *block, opcode instruction, uint32_t operand,
                         struct location *location);
void write_immediate_s32(struct ir_block *block, opcode instruction, int32_t operand,
                         struct location *location);
void write_immediate_u64(struct ir_block *block, opcode instruction, uint64_t operand,
                         struct location *location);
void write_immediate_s64(struct ir_block *block, opcode instruction, int64_t operand,
                         struct location *location);

void write_u8(struct ir_block *block, uint8_t value, struct location *location);
void write_s8(struct ir_block *block, int8_t value, struct location *location);
void write_u16(struct ir_block *block, uint16_t value, struct location *location);
void write_s16(struct ir_block *block, int16_t value, struct location *location);
void write_u32(struct ir_block *block, uint32_t value, struct location *location);
void write_s32(struct ir_block *block, int32_t value, struct location *location);
void write_u64(struct ir_block *block, uint64_t value, struct location *location);
void write_s64(struct ir_block *block, int64_t value, struct location *location);

void overwrite_u8(struct ir_block *block, int start, uint8_t value);
void overwrite_s8(struct ir_block *block, int start, int8_t value);
void overwrite_u16(struct ir_block *block, int start, uint16_t value);
void overwrite_s16(struct ir_block *block, int start, int16_t value);
void overwrite_u32(struct ir_block *block, int start, uint32_t value);
void overwrite_s32(struct ir_block *block, int start, int32_t value);
void overwrite_u64(struct ir_block *block, int start, uint64_t value);
void overwrite_s64(struct ir_block *block, int start, int64_t value);


void overwrite_instruction(struct ir_block *block, int index, opcode instruction);

uint8_t read_u8(struct ir_block *block, int index);
int8_t read_s8(struct ir_block *block, int index);
uint16_t read_u16(struct ir_block *block, int index);
int16_t read_s16(struct ir_block *block, int index);
uint32_t read_u32(struct ir_block *block, int index);
int32_t read_s32(struct ir_block *block, int index);
uint64_t read_u64(struct ir_block *block, int index);
int64_t read_s64(struct ir_block *block, int index);

int add_jump(struct ir_block *block, int dest);
int find_jump(struct ir_block *block, int dest);
bool is_jump_dest(struct ir_block *block, int dest);

void ir_error(const char *restrict filename, struct ir_block *block,
              size_t index, const char *restrict message);

#endif
