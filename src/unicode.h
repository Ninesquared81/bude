#ifndef UNICODE_H
#define UNICODE_H

#include <stdint.h>

#define UTF8_DECODE_ERROR 0xffffffffu

#define UNICODE_MAX 0x10ffff

#define HAS_PREFIX(byte, prefix, mask)               \
    ((byte & mask) == prefix)

struct utf8 {
    int n_bytes;
    uint8_t bytes[4];
};

struct utf16 {
    int n_units;
    uint16_t units[2];
};

struct utf8 encode_utf8_codepoint(uint32_t codepoint);
uint32_t encode_utf8_u32(uint32_t codepoint);
uint32_t decode_utf8(const char *start, const char **end);

struct utf16 encode_utf16_codepoint(uint32_t codepoint);
uint32_t encode_utf16_u32(uint32_t codepoint);
uint32_t decode_utf16(const char *start, const char **end);

const char *escape_unicode(uint32_t codepoint);

#endif
