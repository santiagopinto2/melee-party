/* Melee Party, Mario Party 4 runtime: MP4's data numbers (HuData), after the MP4 decompilation
 * (github.com/mariopartyrd/partyboard src/game/data.c, CC0).
 *
 * MP4 read a whole archive into memory and unpacked files out of it. Here only an archive's offset
 * table stays in memory; each file's record is read off the disc when it is asked for, unpacked
 * into the heap it was asked for, and its packed bytes freed at once. Every read is synchronous, so
 * the asynchronous calls are done as soon as they return. */
#include <dolphin/os.h>
#include <string.h>

#include "game/data.h"
#include "mp4.h"
#include "mp4_data.h"

#define DATA_EFF_SIZE(size) (((size) + 1) & ~0x1)

#define DATADIR(name, path) path,
static const char* const DataDirName[] = {
#include "datadir_table.h"
};
#undef DATADIR

typedef struct DataDir {
    u32 offset, size;     /* the archive on the disc; size 0: not looked up yet */
    u8* table;            /* its count and offsets, or NULL while it is closed */
    u32 table_size;
    BOOL used;            /* opened with a number (HuDataDirReadNumAsync) */
    s32 num;
} DataDir;
static DataDir DataDirStat[DATADIR_ID_MAX];

static DataDir* dir_open(s32 dataNum)
{
    s32 dirId = (dataNum >> 16) & 0xFFFF;
    DataDir* dir;
    u8 count[4];
    s32 n;
    if (dirId >= DATADIR_ID_MAX || !mp4_available()) {
        OSReport("[party] mp4: no data %08x\n", dataNum);
        return NULL;
    }
    dir = &DataDirStat[dirId];
    if (dir->table != NULL) {
        return dir;
    }
    if (dir->size == 0 && !mp4_disc_find(DataDirName[dirId], &dir->offset, &dir->size)) {
        OSReport("[party] mp4: no %s on the disc\n", DataDirName[dirId]);
        return NULL;
    }
    if (!mp4_disc_read(dir->offset, count, 4) || (n = mp4_archive_count(count, 4)) < 0 ||
        (u32) n * 4 + 4 > dir->size)
    {
        return NULL;
    }
    dir->table_size = (u32) n * 4 + 4;
    dir->table = HuMemDirectMalloc(HEAP_SYSTEM, dir->table_size);
    if (dir->table != NULL && !mp4_disc_read(dir->offset, dir->table, dir->table_size)) {
        HuMemDirectFree(dir->table);
        dir->table = NULL;
    }
    dir->used = FALSE;
    return dir->table != NULL ? dir : NULL;
}

/* A file's record (size and type words, then its packed bytes), in the DVD heap. */
static u8* record_read(s32 dataNum, u32* size)
{
    DataDir* dir = dir_open(dataNum);
    Mp4ArchiveEntry entry;
    u8* record;
    if (dir == NULL ||
        mp4_archive_entry(dir->table, dir->table_size, dir->size, dataNum & 0xFFFF, &entry) != 0)
    {
        OSReport("[party] mp4: data number error %08x\n", dataNum);
        return NULL;
    }
    record = HuMemDirectMalloc(HEAP_DVD, entry.size);
    if (record == NULL) {
        record = HuMemDirectMalloc(HEAP_DATA, entry.size);   /* bigger than the DVD heap */
    }
    if (record == NULL) {
        return NULL;
    }
    if (!mp4_disc_read(dir->offset + entry.offset, record, entry.size)) {
        HuMemDirectFree(record);
        return NULL;
    }
    *size = entry.size;
    return record;
}

static void* data_read(s32 dataNum, HeapID heap, BOOL use_num, s32 num)
{
    u32 size, raw_len;
    u8* record = record_read(dataNum, &size);
    void* buf;
    if (record == NULL) {
        return NULL;
    }
    raw_len = mp4_archive_raw_len(record, size);
    buf = use_num ? HuMemDirectMallocNum(heap, DATA_EFF_SIZE(raw_len), num)
                  : HuMemDirectMalloc(heap, DATA_EFF_SIZE(raw_len));
    if (buf != NULL && mp4_archive_unpack(record, size, buf, DATA_EFF_SIZE(raw_len)) != 0) {
        OSReport("[party] mp4: cannot unpack %08x\n", dataNum);
        HuMemDirectFree(buf);
        buf = NULL;
    }
    HuMemDirectFree(record);
    return buf;
}

s32 mp4_data_dir(const char* name)
{
    u32 len = (u32) strlen(name), i, j;
    if (len > 5 && strcmp(name + len - 4, ".bin") == 0) {
        len -= 4;
    }
    if (len > 5 && strncmp(name, "data/", 5) == 0) {
        name += 5;
        len -= 5;
    }
    for (i = 0; i < DATADIR_ID_MAX; i++) {
        const char* dir = DataDirName[i] + 5;   /* past "data/" */
        for (j = 0; j < len; j++) {
            char a = name[j], b = dir[j];
            if (a >= 'A' && a <= 'Z') {
                a = (char) (a - 'A' + 'a');
            }
            if (b >= 'A' && b <= 'Z') {
                b = (char) (b - 'A' + 'a');
            }
            if (a != b) {
                break;
            }
        }
        if (j == len && strcmp(dir + len, ".bin") == 0) {
            return (s32) (i << 16);
        }
    }
    return -1;
}

s32 HuDataReadChk(s32 dataNum)
{
    s32 dirId = (dataNum >> 16) & 0xFFFF;
    return dirId < DATADIR_ID_MAX && DataDirStat[dirId].table != NULL ? dirId : HU_DATA_STAT_NONE;
}

void* HuDataGetDirPtr(s32 dataNum)
{
    s32 stat = HuDataReadChk(dataNum);
    return stat < 0 ? NULL : DataDirStat[stat].table;
}

s32 HuDataDirReadAsync(s32 dataNum)
{
    if (HuDataReadChk(dataNum) >= 0) {
        return -1;
    }
    return dir_open(dataNum) != NULL ? (dataNum >> 16) & 0xFFFF : -1;
}

s32 HuDataDirReadNumAsync(s32 dataNum, s32 num)
{
    DataDir* dir;
    if (HuDataReadChk(dataNum) >= 0) {
        return -1;
    }
    dir = dir_open(dataNum);
    if (dir == NULL) {
        return -1;
    }
    dir->used = TRUE;
    dir->num = num;
    return (dataNum >> 16) & 0xFFFF;
}

BOOL HuDataGetAsyncStat(s32 statId)
{
    (void) statId;
    return TRUE;
}

void* HuDataRead(s32 dataNum) { return data_read(dataNum, HEAP_SYSTEM, FALSE, 0); }
void* HuDataReadNum(s32 dataNum, s32 num) { return data_read(dataNum, HEAP_DATA, TRUE, num); }

void* HuDataSelHeapRead(s32 dataNum, HeapID heap)
{
    return data_read(dataNum, heap == HEAP_MUSIC || heap == HEAP_DATA || heap == HEAP_DVD ? heap : HEAP_SYSTEM,
                     FALSE, 0);
}

void* HuDataSelHeapReadNum(s32 dataNum, s32 num, HeapID heap)
{
    switch (heap) {
    case HEAP_MUSIC:
        return data_read(dataNum, HEAP_MUSIC, FALSE, 0);
    case HEAP_DATA:
    case HEAP_DVD:
        return data_read(dataNum, heap, TRUE, num);
    default:
        return data_read(dataNum, HEAP_SYSTEM, TRUE, num);
    }
}

void* HuDataReadNumHeapShortForce(s32 dataNum, s32 num, HeapID heap)
{
    return HuDataSelHeapReadNum(dataNum, num, heap);
}

void** HuDataReadMulti(s32* dataNum)
{
    s32 i, count;
    void** out;
    for (count = 0; dataNum[count] != HU_DATANUM_NONE; count++) {
    }
    out = HuMemDirectMalloc(HEAP_SYSTEM, (count + 1) * sizeof(void*));
    if (out == NULL) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        out[i] = HuDataRead(dataNum[i]);
    }
    out[i] = NULL;
    return out;
}

s32 HuDataGetSize(s32 dataNum)
{
    DataDir* dir = dir_open(dataNum);
    Mp4ArchiveEntry entry;
    u8 head[8];
    if (dir == NULL ||
        mp4_archive_entry(dir->table, dir->table_size, dir->size, dataNum & 0xFFFF, &entry) != 0 ||
        !mp4_disc_read(dir->offset + entry.offset, head, 8))
    {
        return -1;
    }
    return DATA_EFF_SIZE(mp4_archive_raw_len(head, 8));
}

void HuDataClose(void* ptr)
{
    if (ptr) {
        HuMemDirectFree(ptr);
    }
}

void HuDataCloseMulti(void** ptrs)
{
    s32 i;
    if (ptrs == NULL) {
        return;
    }
    for (i = 0; ptrs[i]; i++) {
        HuMemDirectFree(ptrs[i]);
    }
    HuMemDirectFree(ptrs);
}

void HuDataDirClose(s32 dataNum)
{
    s32 stat = HuDataReadChk(dataNum);
    if (stat < 0) {
        return;
    }
    HuMemDirectFree(DataDirStat[stat].table);
    DataDirStat[stat].table = NULL;
    DataDirStat[stat].used = FALSE;
}

void HuDataDirCloseNum(s32 num)
{
    s32 i;
    for (i = 0; i < DATADIR_ID_MAX; i++) {
        if (DataDirStat[i].table != NULL && DataDirStat[i].used && DataDirStat[i].num == num) {
            HuDataDirClose(i << 16);
        }
    }
}

void* HuDvdDataRead(char* path)
{
    u32 offset, size;
    void* buf;
    if (!mp4_available() || !mp4_disc_find(path, &offset, &size)) {
        return NULL;
    }
    buf = HuMemDirectMalloc(HEAP_DATA, size);
    if (buf != NULL && !mp4_disc_read(offset, buf, size)) {
        HuMemDirectFree(buf);
        buf = NULL;
    }
    return buf;
}

void HuDvdDataClose(void* ptr)
{
    HuDataClose(ptr);
}
