/* Melee Party, Mario Party 4 runtime: the disc header, the file system table and the archive
 * tables, parsed from bytes in memory (mp4_data.h). The archive layout is the one partyboard's
 * src/game/data.c reads (github.com/mariopartyrd/partyboard, CC0). */
#include "mp4_data.h"

static u32 be32(const u8* p)
{
    return ((u32) p[0] << 24) | ((u32) p[1] << 16) | ((u32) p[2] << 8) | p[3];
}

int mp4_disc_check_header(const u8* header, u32 size, int* revision)
{
    static const char id[6] = { 'G', 'M', 'P', 'E', '0', '1' };
    int i;
    if (size < MP4_DISC_HEADER_SIZE) {
        return MP4_DISC_SHORT;
    }
    if (be32(header + 0x1C) != 0xC2339F3Du) {
        return MP4_DISC_NOT_GC;
    }
    if (be32(header + 0x200) == 0x4E4B4954u) {   /* "NKIT" */
        return MP4_DISC_NKIT;
    }
    for (i = 0; i < 6; i++) {
        if (header[i] != (u8) id[i]) {
            return MP4_DISC_WRONG_GAME;
        }
    }
    if (revision != NULL) {
        *revision = header[7];
    }
    if (header[7] > 1) {
        return MP4_DISC_WRONG_REV;
    }
    return MP4_DISC_OK;
}

const char* mp4_disc_status_text(int status)
{
    switch (status) {
    case MP4_DISC_OK:
        return "Mario Party 4 (USA)";
    case MP4_DISC_NONE:
        return "no Mario Party 4 disc";
    case MP4_DISC_SHORT:
        return "too short for a disc image";
    case MP4_DISC_NOT_GC:
        return "not a plain GameCube .iso";
    case MP4_DISC_NKIT:
        return "an NKit image, not a plain .iso";
    case MP4_DISC_WRONG_GAME:
        return "not Mario Party 4 (USA, GMPE01)";
    case MP4_DISC_WRONG_REV:
        return "a Mario Party 4 revision other than 0 or 1";
    case MP4_DISC_BAD_FST:
        return "its file system does not read";
    default:
        return "?";
    }
}

u32 mp4_disc_fst_offset(const u8* header) { return be32(header + 0x424); }
u32 mp4_disc_fst_size(const u8* header) { return be32(header + 0x428); }

int mp4_fst_parse(Mp4Fst* fst, const u8* data, u32 size)
{
    u32 count, i;
    if (size < 12 || !(data[0] & 1)) {
        return -1;
    }
    count = be32(data + 8);
    if (count == 0 || count > size / 12) {
        return -1;
    }
    for (i = 0; i < count; i++) {
        const u8* e = data + i * 12;
        if (count * 12 + (be32(e) & 0xFFFFFF) >= size && i != 0) {
            return -1;
        }
        if ((e[0] & 1) && (be32(e + 8) > count || be32(e + 8) <= i)) {
            return -1;
        }
    }
    fst->data = data;
    fst->size = size;
    fst->count = count;
    fst->names = count * 12;
    return 0;
}

static int name_is(const Mp4Fst* fst, u32 entry, const char* name, u32 len)
{
    u32 at = fst->names + (be32(fst->data + entry * 12) & 0xFFFFFF), i;
    for (i = 0; i < len; i++, at++) {
        char a, b;
        if (at >= fst->size) {
            return 0;
        }
        a = (char) fst->data[at];
        b = name[i];
        if (a >= 'A' && a <= 'Z') {
            a = (char) (a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = (char) (b - 'A' + 'a');
        }
        if (a != b) {
            return 0;
        }
    }
    return at < fst->size && fst->data[at] == 0;
}

int mp4_fst_find(const Mp4Fst* fst, const char* path, u32* offset, u32* size)
{
    u32 dir_start = 1, dir_end = fst->count, i;
    while (*path == '/') {
        path++;
    }
    while (*path != 0) {
        u32 len = 0;
        int last;
        while (path[len] != 0 && path[len] != '/') {
            len++;
        }
        last = path[len] == 0;
        for (i = dir_start; i < dir_end;) {
            const u8* e = fst->data + i * 12;
            int is_dir = e[0] & 1;
            if (name_is(fst, i, path, len) && is_dir != last) {
                break;
            }
            /* a directory's entries are skipped as a whole */
            i = is_dir ? be32(e + 8) : i + 1;
        }
        if (i >= dir_end) {
            return 0;
        }
        if (last) {
            *offset = be32(fst->data + i * 12 + 4);
            *size = be32(fst->data + i * 12 + 8);
            return 1;
        }
        dir_start = i + 1;
        dir_end = be32(fst->data + i * 12 + 8);
        path += len;
        while (*path == '/') {
            path++;
        }
    }
    return 0;
}

s32 mp4_archive_count(const u8* table, u32 size)
{
    u32 count;
    if (size < 4) {
        return -1;
    }
    count = be32(table);
    if (count > 0xFFFF || (u64) count * 4 + 4 > size) {
        return -1;
    }
    return (s32) count;
}

int mp4_archive_entry(const u8* table, u32 size, u32 total, u32 index, Mp4ArchiveEntry* out)
{
    s32 count = mp4_archive_count(table, size);
    u32 start, end;
    if (count < 0 || index >= (u32) count) {
        return -1;
    }
    start = be32(table + 4 + index * 4);
    end = index + 1 < (u32) count ? be32(table + 8 + index * 4) : total;
    if (start < (u32) count * 4 + 4 || end > total || start > end || end - start < 8) {
        return -1;
    }
    out->offset = start;
    out->size = end - start;
    return 0;
}

u32 mp4_archive_raw_len(const u8* record, u32 size)
{
    return size < 8 ? 0xFFFFFFFFu : be32(record);
}

int mp4_archive_unpack(const u8* record, u32 size, void* dst, u32 dst_size)
{
    u32 raw_len = mp4_archive_raw_len(record, size);
    if (raw_len > dst_size) {
        return -1;
    }
    return mp4_decode(record + 8, size - 8, dst, raw_len, be32(record + 4));
}
