#ifndef UNICODE_H
#define UNICODE_H

#include <stdint.h>

#define UTF8_MAX1 0x007f
#define UTF8_MAX2 0x07ff
#define UTF8_MAX3 0xffff
#define UTF8_PRE2 0xc0
#define UTF8_PRE3 0xe0
#define UTF8_PRE4 0xf0
#define UTF8_PRE_CONT 0x80
#define UTF8_MASK2 0x1f
#define UTF8_MASK3 0x0f
#define UTF8_MASK4 0x07
#define UTF8_MASK_CONT 0x3f
#define UTF8_BITS2 5
#define UTF8_BITS3 4
#define UTF8_BITS4 3
#define UTF8_BITS_CONT 6

struct utf8 {
    int n_bytes;
    uint8_t bytes[4];
};

struct utf8 encode_utf8_codepoint(uint32_t codepoint);
uint32_t encode_utf8_u32(uint32_t codepoint);

#endif
