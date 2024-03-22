#ifndef TYPE_PUNNING_H
#define TYPE_PUNNING_H

#include <stdint.h>

union pun8  {uint8_t  u8;  int8_t  s8; };
union pun16 {uint16_t u16; int16_t s16;};
union pun32 {uint32_t u32; int32_t s32;};
union pun64 {uint64_t u64; int64_t s64;};

struct pair32 {uint32t a, b;};
union pun_pair32 {struct pair32 pair32; uint64_t u64;};

#define u8_to_s8(value) ((union pun8) {.u8 = value}).s8
#define s8_to_u8(value) ((union pun8) {.s8 = value}).u8

#define u16_to_s16(value) ((union pun16) {.u16 = value}).s16
#define s16_to_u16(value) ((union pun16) {.s16 = value}).u16

#define u32_to_s32(value) ((union pun32) {.u32 = value}).s32
#define s32_to_u32(value) ((union pun32) {.s32 = value}).u32

#define u64_to_s64(value) ((union pun64) {.u64 = value}).s64
#define s64_to_u64(value) ((union pun64) {.s64 = value}).u64

#define pair32_to_u64(value) ((union pun_tup32) {.pair32 = value}).u64
#define u64_to_pair32(value) ((union pun_tup32) {.u64 = value}).pair32

#endif
