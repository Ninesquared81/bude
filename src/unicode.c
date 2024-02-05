#include <assert.h>
#include <string.h>

#include "unicode.h"


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
        /* Note: UTF-8 allows values up to 0x3fffff, but we can't assume
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
    memcpy(&u32, utf8.bytes, 4);
    return u32;
}
