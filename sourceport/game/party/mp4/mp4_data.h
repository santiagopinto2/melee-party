/* Melee Party, Mario Party 4 runtime: the disc, its file system and MP4's data archives.
 *
 * The parsing (mp4_format.c, mp4_decode.c) works on bytes in memory and needs nothing else from
 * the game, so the tests (tests/mp4_data.c) run it on bytes they make themselves. mp4_disc.c
 * reads the player's disc through the host (MuHostApi.mp4_disc_read). */
#ifndef MU_PARTY_MP4_DATA_H
#define MU_PARTY_MP4_DATA_H

#include <dolphin/types.h>

/* ---- the disc header ---- */
enum {
    MP4_DISC_OK = 0,
    MP4_DISC_NONE,        /* the host has no MP4 disc */
    MP4_DISC_SHORT,       /* too short to hold a header */
    MP4_DISC_NOT_GC,      /* no GameCube disc magic: not a plain .iso (a .ciso, .gcz, .rvz...) */
    MP4_DISC_NKIT,        /* an NKit image, whose file offsets are not the disc's */
    MP4_DISC_WRONG_GAME,  /* not Mario Party 4 (USA), GMPE01 */
    MP4_DISC_WRONG_REV,   /* a revision other than 0 or 1 */
    MP4_DISC_BAD_FST,     /* the file system table does not parse */
};
#define MP4_DISC_HEADER_SIZE 0x440u
/* Checks a disc's first 0x440 bytes: Mario Party 4 (USA), Rev 0 or 1, as a plain image. */
int mp4_disc_check_header(const u8* header, u32 size, int* revision);
const char* mp4_disc_status_text(int status);
u32 mp4_disc_fst_offset(const u8* header);
u32 mp4_disc_fst_size(const u8* header);

/* ---- the file system table (FST) ----
 * 12-byte entries: a directory flag and a name offset, then a file's disc offset and size, or a
 * directory's parent and the index past its last entry. The names follow the entries. */
typedef struct Mp4Fst {
    const u8* data;
    u32 size;
    u32 count;
    u32 names;            /* where the name table starts */
} Mp4Fst;
int mp4_fst_parse(Mp4Fst* fst, const u8* data, u32 size);   /* 0 when it parses */
/* A file by its path from the root, '/'-separated and case-insensitive ("data/m440.bin").
 * Returns 1 and its disc offset and size when found. */
int mp4_fst_find(const Mp4Fst* fst, const char* path, u32* offset, u32* size);

/* ---- the archives in the disc's data directory (.bin) ----
 * A count, the offsets of that many files, and at each offset a file's unpacked size, its
 * decode type and its packed bytes, all big-endian. */
#define MP4_DECODE_NONE 0
#define MP4_DECODE_LZ 1
#define MP4_DECODE_SLIDE 2
#define MP4_DECODE_FSLIDE_ALT 3
#define MP4_DECODE_FSLIDE 4
#define MP4_DECODE_RLE 5
typedef struct Mp4ArchiveEntry {
    u32 offset;           /* where the file's record is in the archive */
    u32 size;             /* the record's bytes, to the next file or the archive's end */
} Mp4ArchiveEntry;
/* The file count from an archive's start, or -1 when the first `size` bytes cannot hold its
 * offset table. */
s32 mp4_archive_count(const u8* table, u32 size);
/* Where file `index` is, from the first `size` bytes of an archive `total` bytes long (at least
 * its offset table). 0 when that is inside the archive. */
int mp4_archive_entry(const u8* table, u32 size, u32 total, u32 index, Mp4ArchiveEntry* out);
/* A record's unpacked size (0xFFFFFFFF when it is too short to say), and the record unpacked into
 * dst, which holds dst_size bytes. 0 on success. */
u32 mp4_archive_raw_len(const u8* record, u32 size);
int mp4_archive_unpack(const u8* record, u32 size, void* dst, u32 dst_size);
/* Unpacks one file. 0 on success; -1 for an unknown type or packed bytes that run out. */
int mp4_decode(const void* src, u32 src_size, void* dst, u32 raw_len, u32 type);

/* ---- the player's disc (mp4_disc.c) ---- */
int mp4_disc_open(void);                       /* MP4_DISC_*; reads the header and the FST */
int mp4_disc_ready(void);                      /* opened with MP4_DISC_OK */
int mp4_disc_find(const char* path, u32* offset, u32* size);
int mp4_disc_read(u32 offset, void* dst, u32 size);   /* 1 when all of it was read */

#endif
