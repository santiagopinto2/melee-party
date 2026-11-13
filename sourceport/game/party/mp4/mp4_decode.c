/* Melee Party, Mario Party 4 runtime: the decoders of MP4's data archives, from the MP4
 * decompilation (github.com/mariopartyrd/partyboard src/game/decode.c, CC0).
 *
 * The same six types and the same output as HuDecodeData, with the input and output bounded: a
 * file that runs out of input or names an unknown type is refused instead of read past its end.
 * The SLIDE flag words are read big-endian, as on the console. */
#include "mp4_data.h"

typedef struct Reader {
    const u8* src;
    u32 pos, size;
} Reader;

static int take(Reader* r, u32* out)
{
    if (r->pos >= r->size) {
        return 0;
    }
    *out = r->src[r->pos++];
    return 1;
}

static int decode_none(Reader* r, u8* dst, u32 size)
{
    u32 i;
    if (r->size - r->pos < size) {
        return -1;
    }
    for (i = 0; i < size; i++) {
        dst[i] = r->src[r->pos++];
    }
    return 0;
}

/* A 1 KB ring of the last bytes out, starting at 958; each flag bit says literal (1) or a
 * 10-bit ring position with a length of 3 to 66 (0). */
static int decode_lz(Reader* r, u8* dst, u32 size)
{
    u8 text[1024];
    u32 flag = 0, pos = 958, out = 0, i, j, b0, b1, len;
    for (i = 0; i < 1024; i++) {
        text[i] = 0;
    }
    while (out < size) {
        flag >>= 1;
        if (!(flag & 0x100)) {
            if (!take(r, &b0)) {
                return -1;
            }
            flag = b0 | 0xFF00;
        }
        if (flag & 1) {
            if (!take(r, &b0)) {
                return -1;
            }
            text[pos++] = dst[out++] = (u8) b0;
            pos &= 0x3FF;
        } else {
            if (!take(r, &b0) || !take(r, &b1)) {
                return -1;
            }
            i = b0 | ((b1 & ~0x3Fu) << 2);
            len = (b1 & 0x3F) + 3;
            for (j = 0; j < len && out < size; j++) {
                text[pos++] = dst[out++] = text[(i + j) & 0x3FF];
                pos &= 0x3FF;
            }
        }
    }
    return 0;
}

/* SLIDE and FSLIDE: a 4-byte size, then 32 flags per big-endian word, literal (1) or a 12-bit
 * distance back with a length of 3 to 17, or 18 to 273 from an extra byte (0). SLIDE reads zeros
 * before the start of the output; FSLIDE never refers there in a valid file, and gets the same. */
static int decode_slide(Reader* r, u8* dst, u32 size)
{
    u32 flag = 0, flag_len = 0, out = 0, b0, b1, b2, b3, dist, len;
    s32 from;
    if (r->size - r->pos < 4) {
        return -1;
    }
    r->pos += 4;
    while (out < size) {
        if (flag_len == 0) {
            if (!take(r, &b0) || !take(r, &b1) || !take(r, &b2) || !take(r, &b3)) {
                return -1;
            }
            flag = (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
            flag_len = 32;
        }
        if (flag >> 31) {
            if (!take(r, &b0)) {
                return -1;
            }
            dst[out++] = (u8) b0;
        } else {
            if (!take(r, &b0) || !take(r, &b1)) {
                return -1;
            }
            dist = (b0 << 8) | b1;
            len = (dist >> 12) & 0xF;
            dist &= 0xFFF;
            from = (s32) out - (s32) dist;
            if (len == 0) {
                if (!take(r, &b2)) {
                    return -1;
                }
                len = b2 + 18;
            } else {
                len += 2;
            }
            while (len != 0 && out < size) {
                dst[out++] = from - 1 < 0 ? 0 : dst[from - 1];
                len--;
                from++;
            }
        }
        flag <<= 1;
        flag_len--;
    }
    return 0;
}

/* Runs: a count below 128 repeats the next byte, 128 + n copies the next n bytes. */
static int decode_rle(Reader* r, u8* dst, u32 size)
{
    u32 out = 0, count, fill, i;
    while (out < size) {
        if (!take(r, &count)) {
            return -1;
        }
        if (count < 128) {
            if (!take(r, &fill)) {
                return -1;
            }
            for (i = 0; i < count && out < size; i++) {
                dst[out++] = (u8) fill;
            }
        } else {
            for (i = 0; i < count - 128 && out < size; i++) {
                if (!take(r, &fill)) {
                    return -1;
                }
                dst[out++] = (u8) fill;
            }
        }
    }
    return 0;
}

int mp4_decode(const void* src, u32 src_size, void* dst, u32 raw_len, u32 type)
{
    Reader r = { (const u8*) src, 0, src_size };
    switch (type) {
    case MP4_DECODE_NONE:
        return decode_none(&r, dst, raw_len);
    case MP4_DECODE_LZ:
        return decode_lz(&r, dst, raw_len);
    case MP4_DECODE_SLIDE:
    case MP4_DECODE_FSLIDE_ALT:
    case MP4_DECODE_FSLIDE:
        return decode_slide(&r, dst, raw_len);
    case MP4_DECODE_RLE:
        return decode_rle(&r, dst, raw_len);
    default:
        return -1;
    }
}
