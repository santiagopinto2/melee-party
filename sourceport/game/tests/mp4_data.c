/* Melee Party's Mario Party 4 runtime (party/mp4): the disc header check, the file system table,
 * the archive tables and the six decoders. Every input is made here: hand-made bytes, and the
 * output of small encoders for MP4's LZ, SLIDE and RLE formats. No disc data. */
#include <stdio.h>
#include <string.h>

#include "mp4_data.h"

static int failures;

#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                 \
            failures++;                                                            \
        }                                                                          \
    } while (0)

static void put32(u8* p, u32 v)
{
    p[0] = (u8) (v >> 24);
    p[1] = (u8) (v >> 16);
    p[2] = (u8) (v >> 8);
    p[3] = (u8) v;
}

/* ---- hand-made vectors ---- */

static void test_hand_vectors(void)
{
    /* LZ: literals A and B land at ring 958 and 959; then 5 bytes from ring 958 (0x3BE: low byte
     * 0xBE, top two bits 3 in the length byte's top bits, length 5 - 3 = 2), which overlap the
     * bytes they write: ABABABA. Flags, low bit first: literal, literal, copy. */
    static const u8 lz[] = { 0x03, 'A', 'B', 0xBE, 0xC2 };
    /* LZ from the untouched ring: three zeros, then a literal. */
    static const u8 lz_zero[] = { 0x02, 0x00, 0x00, 'Z' };
    /* SLIDE: a size word, then flags 1 1 0 (MSB first); the copy is distance 1, length 3 + 2 = 5,
     * starting two bytes back. */
    static const u8 slide[] = { 0, 0, 0, 7, 0xC0, 0, 0, 0, 'A', 'B', 0x30, 0x01 };
    /* SLIDE: length nibble 0 takes 18 + the next byte (2: 20 bytes) of the byte before. */
    static const u8 slide_long[] = { 0, 0, 0, 21, 0x80, 0, 0, 0, 'Q', 0x00, 0x00, 0x02 };
    /* SLIDE reads zeros from before the output's start: distance 5 at the start. */
    static const u8 slide_before[] = { 0, 0, 0, 3, 0x00, 0, 0, 0, 0x10, 0x05 };
    /* RLE: four As, then four bytes as they are. */
    static const u8 rle[] = { 0x04, 'A', 0x84, 'B', 'x', 'y', 'z' };
    static const u8 none[] = { 1, 2, 3 };
    u8 out[64];
    int i, ok;

    memset(out, 0xEE, sizeof out);
    CHECK(mp4_decode(lz, sizeof lz, out, 7, MP4_DECODE_LZ) == 0);
    CHECK(memcmp(out, "ABABABA", 7) == 0);
    CHECK(out[7] == 0xEE);   /* nothing past the unpacked size */

    CHECK(mp4_decode(lz_zero, sizeof lz_zero, out, 4, MP4_DECODE_LZ) == 0);
    CHECK(out[0] == 0 && out[1] == 0 && out[2] == 0 && out[3] == 'Z');

    memset(out, 0xEE, sizeof out);
    CHECK(mp4_decode(slide, sizeof slide, out, 7, MP4_DECODE_SLIDE) == 0);
    CHECK(memcmp(out, "ABABABA", 7) == 0);
    CHECK(out[7] == 0xEE);
    memset(out, 0, sizeof out);
    CHECK(mp4_decode(slide, sizeof slide, out, 7, MP4_DECODE_FSLIDE) == 0);
    CHECK(memcmp(out, "ABABABA", 7) == 0);
    memset(out, 0, sizeof out);
    CHECK(mp4_decode(slide, sizeof slide, out, 7, MP4_DECODE_FSLIDE_ALT) == 0);
    CHECK(memcmp(out, "ABABABA", 7) == 0);

    CHECK(mp4_decode(slide_long, sizeof slide_long, out, 21, MP4_DECODE_SLIDE) == 0);
    for (i = 0, ok = 1; i < 21; i++) {
        ok &= out[i] == 'Q';
    }
    CHECK(ok);

    memset(out, 0xEE, sizeof out);
    CHECK(mp4_decode(slide_before, sizeof slide_before, out, 3, MP4_DECODE_SLIDE) == 0);
    CHECK(out[0] == 0 && out[1] == 0 && out[2] == 0);

    CHECK(mp4_decode(rle, sizeof rle, out, 8, MP4_DECODE_RLE) == 0);
    CHECK(memcmp(out, "AAAABxyz", 8) == 0);

    CHECK(mp4_decode(none, sizeof none, out, 3, MP4_DECODE_NONE) == 0);
    CHECK(out[0] == 1 && out[1] == 2 && out[2] == 3);

    /* refused: input that runs out, and an unknown type */
    CHECK(mp4_decode(lz, sizeof lz - 1, out, 7, MP4_DECODE_LZ) != 0);
    CHECK(mp4_decode(slide, sizeof slide - 1, out, 7, MP4_DECODE_SLIDE) != 0);
    CHECK(mp4_decode(slide, 3, out, 7, MP4_DECODE_SLIDE) != 0);
    CHECK(mp4_decode(rle, 3, out, 8, MP4_DECODE_RLE) != 0);
    CHECK(mp4_decode(none, sizeof none, out, 4, MP4_DECODE_NONE) != 0);
    CHECK(mp4_decode(none, sizeof none, out, 3, 6) != 0);
}

/* ---- encoders ---- */

/* LZ: greedy, each candidate copy simulated on a copy of the ring so overlapping copies are
 * judged by what the decoder will really produce. */
static u32 encode_lz(const u8* in, u32 n, u8* out)
{
    u8 ring[1024], sim[1024];
    u32 pos = 958, o = 0, k = 0, flag_at = 0, bit = 8, i, j;
    memset(ring, 0, sizeof ring);
    while (k < n) {
        u32 best_len = 0, best_i = 0;
        if (bit == 8) {
            flag_at = o++;
            out[flag_at] = 0;
            bit = 0;
        }
        for (i = 0; i < 1024; i++) {
            u32 len = 0;
            if (ring[i] != in[k]) {
                continue;
            }
            memcpy(sim, ring, sizeof sim);
            for (j = 0; j < 66 && k + j < n; j++) {
                u8 b = sim[(i + j) & 0x3FF];
                if (b != in[k + j]) {
                    break;
                }
                sim[(pos + j) & 0x3FF] = b;
                len++;
            }
            if (len > best_len) {
                best_len = len;
                best_i = i;
            }
        }
        if (best_len >= 3) {
            out[o++] = (u8) best_i;
            out[o++] = (u8) (((best_i >> 8) << 6) | (best_len - 3));
            for (j = 0; j < best_len; j++) {
                ring[pos] = in[k++];
                pos = (pos + 1) & 0x3FF;
            }
        } else {
            out[flag_at] |= (u8) (1 << bit);
            out[o++] = in[k];
            ring[pos] = in[k++];
            pos = (pos + 1) & 0x3FF;
        }
        bit++;
    }
    return o;
}

/* SLIDE: greedy over the last 4096 bytes; a copy of distance d starts d + 1 bytes back. */
static u32 encode_slide(const u8* in, u32 n, u8* out)
{
    u32 o = 4, k = 0, flag_at = 0, bits = 32, flag = 0, d, j;
    put32(out, n);
    while (k < n) {
        u32 best_len = 0, best_d = 0;
        if (bits == 32) {
            if (k != 0) {
                put32(out + flag_at, flag);
            }
            flag_at = o;
            o += 4;
            flag = 0;
            bits = 0;
        }
        for (d = 0; d < 4096 && d + 1 <= k; d++) {
            u32 from = k - d - 1, len = 0;
            for (j = 0; j < 273 && k + j < n && in[from + j] == in[k + j]; j++) {
                len++;
            }
            if (len > best_len) {
                best_len = len;
                best_d = d;
            }
        }
        if (best_len >= 3) {
            if (best_len <= 17) {
                out[o++] = (u8) (((best_len - 2) << 4) | (best_d >> 8));
                out[o++] = (u8) best_d;
            } else {
                out[o++] = (u8) (best_d >> 8);
                out[o++] = (u8) best_d;
                out[o++] = (u8) (best_len - 18);
            }
            k += best_len;
        } else {
            flag |= 0x80000000u >> bits;
            out[o++] = in[k++];
        }
        bits++;
    }
    put32(out + flag_at, flag);
    return o;
}

/* RLE: runs of 3 or more as a count and a byte, the rest in literal groups of up to 127. */
static u32 encode_rle(const u8* in, u32 n, u8* out)
{
    u32 o = 0, k = 0, run, lit;
    while (k < n) {
        for (run = 1; k + run < n && run < 127 && in[k + run] == in[k]; run++) {
        }
        if (run >= 3) {
            out[o++] = (u8) run;
            out[o++] = in[k];
            k += run;
            continue;
        }
        for (lit = 0; k + lit < n && lit < 127; lit++) {
            if (k + lit + 2 < n && in[k + lit] == in[k + lit + 1] && in[k + lit] == in[k + lit + 2]) {
                break;
            }
        }
        out[o++] = (u8) (128 + lit);
        memcpy(out + o, in + k, lit);
        o += lit;
        k += lit;
    }
    return o;
}

/* Data with runs, repeats near and far, and noise. */
static void make_data(u8* buf, u32 n, u32 seed)
{
    u32 x = seed, k = 0;
    while (k < n) {
        u32 kind, len, j;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        kind = x & 3;
        len = 1 + ((x >> 8) & 63);
        for (j = 0; j < len && k < n; j++, k++) {
            if (kind == 0) {
                buf[k] = (u8) (x >> 16);                            /* a run */
            } else if (kind == 1 && k > 300) {
                buf[k] = buf[k - 300 + (x >> 24) % 5];              /* a far repeat */
            } else if (kind == 2 && k > 3) {
                buf[k] = buf[k - 3];                                /* a near repeat */
            } else {
                buf[k] = (u8) ((x >> (j & 7)) * 2654435761u >> 24); /* noise */
            }
        }
    }
}

static u8 plain[6000], packed[9000], unpacked[6100];

static void test_round_trips(void)
{
    static const u32 sizes[] = { 1, 2, 3, 17, 18, 300, 1100, 5000 };
    u32 s, n, size;
    for (s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
        n = sizes[s];
        make_data(plain, n, 0x1234567u + s);

        size = encode_lz(plain, n, packed);
        memset(unpacked, 0xEE, sizeof unpacked);
        CHECK(mp4_decode(packed, size, unpacked, n, MP4_DECODE_LZ) == 0);
        CHECK(memcmp(unpacked, plain, n) == 0 && unpacked[n] == 0xEE);

        size = encode_slide(plain, n, packed);
        memset(unpacked, 0xEE, sizeof unpacked);
        CHECK(mp4_decode(packed, size, unpacked, n, MP4_DECODE_SLIDE) == 0);
        CHECK(memcmp(unpacked, plain, n) == 0 && unpacked[n] == 0xEE);
        memset(unpacked, 0xEE, sizeof unpacked);
        CHECK(mp4_decode(packed, size, unpacked, n, MP4_DECODE_FSLIDE) == 0);
        CHECK(memcmp(unpacked, plain, n) == 0);
        if (n > 1) {
            CHECK(mp4_decode(packed, size - 1, unpacked, n, MP4_DECODE_SLIDE) != 0 ||
                  memcmp(unpacked, plain, n) != 0);
        }

        size = encode_rle(plain, n, packed);
        memset(unpacked, 0xEE, sizeof unpacked);
        CHECK(mp4_decode(packed, size, unpacked, n, MP4_DECODE_RLE) == 0);
        CHECK(memcmp(unpacked, plain, n) == 0 && unpacked[n] == 0xEE);
    }
}

/* ---- archives ---- */

static u8 archive[20000];

static void test_archive(void)
{
    static u8 files[3][700];
    static const u32 lens[3] = { 40, 700, 333 };
    static const u32 types[3] = { MP4_DECODE_NONE, MP4_DECODE_SLIDE, MP4_DECODE_RLE };
    Mp4ArchiveEntry e;
    u32 at = 4 + 3 * 4, i, size;
    put32(archive, 3);
    for (i = 0; i < 3; i++) {
        make_data(files[i], lens[i], 99 + i);
        put32(archive + 4 + i * 4, at);
        put32(archive + at, lens[i]);
        put32(archive + at + 4, types[i]);
        if (types[i] == MP4_DECODE_NONE) {
            memcpy(archive + at + 8, files[i], lens[i]);
            size = lens[i];
        } else if (types[i] == MP4_DECODE_SLIDE) {
            size = encode_slide(files[i], lens[i], archive + at + 8);
        } else {
            size = encode_rle(files[i], lens[i], archive + at + 8);
        }
        at += 8 + size;
        at = (at + 3) & ~3u;   /* records start on words, as MP4's do */
    }

    CHECK(mp4_archive_count(archive, at) == 3);
    CHECK(mp4_archive_count(archive, 12) == -1);   /* the table does not fit */
    for (i = 0; i < 3; i++) {
        CHECK(mp4_archive_entry(archive, 16, at, i, &e) == 0);
        CHECK(mp4_archive_raw_len(archive + e.offset, e.size) == lens[i]);
        memset(unpacked, 0xEE, sizeof unpacked);
        CHECK(mp4_archive_unpack(archive + e.offset, e.size, unpacked, sizeof unpacked) == 0);
        CHECK(memcmp(unpacked, files[i], lens[i]) == 0);
        CHECK(mp4_archive_unpack(archive + e.offset, e.size, unpacked, lens[i] - 1) != 0);
    }
    CHECK(mp4_archive_entry(archive, 16, at, 2, &e) == 0 && e.offset + e.size == at);
    CHECK(mp4_archive_entry(archive, 16, at, 3, &e) != 0);        /* past the count */
    CHECK(mp4_archive_entry(archive, 16, 30, 0, &e) != 0);        /* past the archive's end */
    put32(archive + 4, 8);                                        /* inside the table */
    CHECK(mp4_archive_entry(archive, 16, at, 0, &e) != 0);
    CHECK(mp4_archive_raw_len(archive, 7) == 0xFFFFFFFFu);
}

/* ---- the file system table ---- */

static u8 fst[512];
static u32 fst_names;

static void fst_entry(u32 i, int dir, const char* name, u32 a, u32 b)
{
    u8* e = fst + i * 12;
    u32 name_at = fst_names;
    strcpy((char*) fst + 12 * 9 + name_at, name);
    fst_names += (u32) strlen(name) + 1;
    put32(e, (dir ? 0x01000000u : 0) | name_at);
    put32(e + 4, a);
    put32(e + 8, b);
}

static void test_fst(void)
{
    Mp4Fst t;
    u32 off = 0, size = 0;
    /* 0 root (9 entries), 1 opening.bnr, 2 data/ (to 6), 3 data/m440.bin, 4 data/sub/ (to 6),
     * 5 data/sub/x.bin, 6 data2/ (to 8), 7 data2/m440.bin, 8 zz.bin */
    memset(fst, 0, sizeof fst);
    fst_names = 0;
    fst_entry(0, 1, "", 0, 9);
    fst_entry(1, 0, "opening.bnr", 0x1000, 0x20);
    fst_entry(2, 1, "data", 0, 6);
    fst_entry(3, 0, "m440.bin", 0x2000, 0x300);
    fst_entry(4, 1, "sub", 2, 6);
    fst_entry(5, 0, "x.bin", 0x4000, 0x10);
    fst_entry(6, 1, "data2", 0, 8);
    fst_entry(7, 0, "m440.bin", 0x5000, 0x40);
    fst_entry(8, 0, "zz.bin", 0x6000, 0x50);

    CHECK(mp4_fst_parse(&t, fst, 12 * 9 + fst_names) == 0);
    CHECK(t.count == 9);
    CHECK(mp4_fst_find(&t, "data/m440.bin", &off, &size) == 1 && off == 0x2000 && size == 0x300);
    CHECK(mp4_fst_find(&t, "/DATA/M440.BIN", &off, &size) == 1 && off == 0x2000);
    CHECK(mp4_fst_find(&t, "data2/m440.bin", &off, &size) == 1 && off == 0x5000);
    CHECK(mp4_fst_find(&t, "data/sub/x.bin", &off, &size) == 1 && off == 0x4000 && size == 0x10);
    CHECK(mp4_fst_find(&t, "opening.bnr", &off, &size) == 1 && off == 0x1000);
    CHECK(mp4_fst_find(&t, "zz.bin", &off, &size) == 1 && off == 0x6000);
    CHECK(mp4_fst_find(&t, "x.bin", &off, &size) == 0);           /* not at the root */
    CHECK(mp4_fst_find(&t, "data/x.bin", &off, &size) == 0);      /* in data/sub, not data */
    CHECK(mp4_fst_find(&t, "data", &off, &size) == 0);            /* a directory */
    CHECK(mp4_fst_find(&t, "data/m440.bi", &off, &size) == 0);
    CHECK(mp4_fst_find(&t, "opening.bnr/x", &off, &size) == 0);   /* a file is no directory */

    CHECK(mp4_fst_parse(&t, fst, 11) != 0);
    put32(fst + 8, 100);                                          /* more entries than bytes */
    CHECK(mp4_fst_parse(&t, fst, 12 * 9 + fst_names) != 0);
    put32(fst + 8, 9);
    put32(fst + 2 * 12 + 8, 20);                                  /* a directory past the end */
    CHECK(mp4_fst_parse(&t, fst, 12 * 9 + fst_names) != 0);
}

/* ---- the disc header ---- */

static void test_header(void)
{
    static u8 h[MP4_DISC_HEADER_SIZE];
    int rev = -1;
    memset(h, 0, sizeof h);
    memcpy(h, "GMPE01", 6);
    put32(h + 0x1C, 0xC2339F3Du);
    put32(h + 0x424, 0x123400);
    put32(h + 0x428, 0x5678);
    CHECK(mp4_disc_check_header(h, sizeof h, &rev) == MP4_DISC_OK && rev == 0);
    h[7] = 1;
    CHECK(mp4_disc_check_header(h, sizeof h, &rev) == MP4_DISC_OK && rev == 1);
    CHECK(mp4_disc_fst_offset(h) == 0x123400 && mp4_disc_fst_size(h) == 0x5678);
    h[7] = 2;
    CHECK(mp4_disc_check_header(h, sizeof h, &rev) == MP4_DISC_WRONG_REV);
    h[7] = 0;
    CHECK(mp4_disc_check_header(h, sizeof h - 1, NULL) == MP4_DISC_SHORT);
    memcpy(h, "GMPP01", 6);   /* the PAL disc */
    CHECK(mp4_disc_check_header(h, sizeof h, NULL) == MP4_DISC_WRONG_GAME);
    memcpy(h, "GALE01", 6);   /* Melee */
    CHECK(mp4_disc_check_header(h, sizeof h, NULL) == MP4_DISC_WRONG_GAME);
    memcpy(h, "GMPE01", 6);
    memcpy(h + 0x200, "NKIT", 4);
    CHECK(mp4_disc_check_header(h, sizeof h, NULL) == MP4_DISC_NKIT);
    memset(h + 0x200, 0, 4);
    put32(h + 0x1C, 0);       /* a compressed image starts with its own header */
    CHECK(mp4_disc_check_header(h, sizeof h, NULL) == MP4_DISC_NOT_GC);
}

int main(void)
{
    test_hand_vectors();
    test_round_trips();
    test_archive();
    test_fst();
    test_header();
    if (failures != 0) {
        printf("mp4_data: %d failures\n", failures);
        return 1;
    }
    printf("mp4_data: ok\n");
    return 0;
}
