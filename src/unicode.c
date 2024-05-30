#include <assert.h>
#include <string.h>

#include "unicode.h"

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

#define UTF16_MAX1 0xffff
#define UTF16_SURR_CMPL 0x10000
#define UTF16_SURR_BITS_HIGH 10
#define UTF16_SURR_MASK_LOW 0x3ff
#define UTF16_HIGH_START 0xd800
#define UTF16_LOW_START 0xdc00


struct utf8 encode_utf8_codepoint(uint32_t codepoint) {
    struct utf8 utf8 = {0};
    if (codepoint <= UTF8_MAX1) {
        utf8.n_bytes = 1;
        utf8.bytes[0] = codepoint;
    }
    else if (codepoint <= UTF8_MAX2) {
        utf8.n_bytes = 2;
        utf8.bytes[1] = UTF8_PRE_CONT | (codepoint & UTF8_MASK_CONT);
        codepoint >>= UTF8_BITS_CONT;
        /* This assertion ought to be implied by the if condition. Nevertheless,
           we assert it to be certain. */
        assert((codepoint & UTF8_MASK2) == codepoint);
        utf8.bytes[0] = UTF8_PRE2 | codepoint;
    }
    else if (codepoint <= UTF8_MAX3) {
        utf8.n_bytes = 3;
        for (int i = 2; i >= 1; --i) {
            utf8.bytes[i] = UTF8_PRE_CONT | (codepoint & UTF8_MASK_CONT);
            codepoint >>= UTF8_BITS_CONT;
        }
        assert((codepoint & UTF8_MASK3) == codepoint);
        utf8.bytes[0] = UTF8_PRE3 | codepoint;
    }
    else {
        utf8.n_bytes = 4;
        for (int i = 3; i >= 1; --i) {
            utf8.bytes[i] = UTF8_PRE_CONT | (codepoint & UTF8_MASK_CONT);
            codepoint >>= UTF8_BITS_CONT;
        }
        /* Note: UTF-8 allows values up to 0x1fffff, but we can't assume
           the user passed a value in that range, so we trancate.
           Strictly, UTF-8 can only encode valid codepoints, so 0x10FFFF
           is the de jure maximum, but here we allow up the de facto maximum. */
        utf8.bytes[0] = UTF8_PRE4 | (codepoint & UTF8_MASK4);
    }
    return utf8;
}

uint32_t encode_utf8_u32(uint32_t codepoint) {
    uint32_t u32 = 0;
    struct utf8 utf8 = encode_utf8_codepoint(codepoint);
    memcpy(&u32, utf8.bytes, utf8.n_bytes);
    return u32;
}


uint32_t decode_utf8(const char *start, const char **end) {
    uint8_t byte = *start++;
    if ((byte & ~UTF8_MASK_CONT) == UTF8_PRE_CONT) {
        // Continuation byte: invalid start point
        return UTF8_DECODE_ERROR;
    }
    if (byte <= UTF8_MAX1) return byte;
    uint32_t codepoint = 0;
    if ((byte & ~UTF8_MASK2) == UTF8_PRE2) {
        codepoint = byte & UTF8_MASK2;
        byte = *start++;
        if ((byte & ~UTF8_MASK_CONT) != UTF8_PRE_CONT) {
            // Expected a continuation byte.
            return UTF8_DECODE_ERROR;
        }
        codepoint <<= UTF8_BITS_CONT;
        codepoint |= (byte & UTF8_MASK_CONT);
    }
    else if ((byte & ~UTF8_MASK3) == UTF8_PRE3) {
        codepoint = byte & UTF8_MASK3;
        for (int i = 0; i < 2; ++i) {
            byte = *start++;
            if ((byte & ~UTF8_MASK_CONT) != UTF8_PRE_CONT) {
                // Expected a continuation byte.
                return UTF8_DECODE_ERROR;
            }
            codepoint <<= UTF8_BITS_CONT;
            codepoint |= (byte & UTF8_MASK_CONT);
        }
    }
    else if ((byte & ~UTF8_MASK4) == UTF8_PRE4) {
        codepoint = byte & UTF8_MASK4;
        for (int i = 0; i < 3; ++i) {
            byte = *start++;
            if ((byte & ~UTF8_MASK_CONT) != UTF8_PRE_CONT) {
                // Expected a continuation byte.
                return UTF8_DECODE_ERROR;
            }
            codepoint <<= UTF8_BITS_CONT;
            codepoint |= (byte & UTF8_MASK_CONT);
        }

    }
    else {
        // Invalid prefix.
        return UTF8_DECODE_ERROR;
    }
    if (end != NULL) *end = start;
    return codepoint;
}

struct utf16 encode_utf16_codepoint(uint32_t codepoint) {
    struct utf16 utf16 = {0};
    if (codepoint <= UTF16_MAX1) {
        utf16.n_units = 1;
        utf16.units[0] = codepoint;
    }
    else {
        // Surrogate pair.
        if (codepoint > UNICODE_MAX) codepoint = UNICODE_MAX;
        uint32_t complement = codepoint - UTF16_SURR_CMPL;
        uint32_t high_bits = complement >> UTF16_SURR_BITS_HIGH;
        uint32_t low_bits = complement & UTF16_SURR_MASK_LOW;
        utf16.n_units = 2;
        utf16.units[0] = UTF16_HIGH_START + high_bits;
        utf16.units[1] = UTF16_LOW_START + low_bits;
    }
    return utf16;
}

uint32_t encode_utf16_u32(uint32_t codepoint) {
    uint32_t u32 = 0;
    struct utf16 utf16 = encode_utf16_codepoint(codepoint);
    memcpy(&u32, utf16.units, sizeof utf16.units);
    return u32;
}

uint32_t decode_utf16(const char *start, const char **end) {
    uint32_t codepoint = 0;
    uint16_t first_unit = 0;
    memcpy(&first_unit, start, sizeof first_unit);
    start += sizeof first_unit;
    if ((first_unit & ~UTF16_SURR_MASK_LOW) != UTF16_HIGH_START) {
        codepoint = first_unit;
    }
    else {
        first_unit -= UTF16_HIGH_START;
        uint16_t second_unit = 0;
        memcpy(&second_unit, start, sizeof second_unit);
        start += sizeof second_unit;
        second_unit -= UTF16_LOW_START;
        uint32_t complement = (first_unit << UTF16_SURR_BITS_HIGH) | second_unit;
        codepoint = complement + UTF16_SURR_CMPL;

    }
    if (end != NULL) *end = start;
    return codepoint;
}
