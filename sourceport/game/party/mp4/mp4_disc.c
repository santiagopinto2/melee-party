/* Melee Party, Mario Party 4 runtime: the player's MP4 disc, read in place through the host
 * (MuHostApi.mp4_disc_read, host API 17). Its header is checked once and its file system table
 * kept in the system heap. */
#include <dolphin/os.h>

#include "game/memory.h"
#include "mp4.h"
#include "mp4_data.h"

u32 mu_mp4_disc_read(u32 offset, void* dst, u32 size);   /* shim/mu_dvd.c */

static int disc_status = -1;
static Mp4Fst disc_fst;

int mp4_disc_read(u32 offset, void* dst, u32 size)
{
    return mu_mp4_disc_read(offset, dst, size) == size;
}

int mp4_disc_open(void)
{
    u8 header[MP4_DISC_HEADER_SIZE];
    u8* fst;
    u32 fst_size;
    int revision = -1;
    if (disc_status >= 0) {
        return disc_status;
    }
    if (!mp4_disc_read(0, header, sizeof header)) {
        disc_status = MP4_DISC_NONE;
        return disc_status;
    }
    disc_status = mp4_disc_check_header(header, sizeof header, &revision);
    if (disc_status != MP4_DISC_OK) {
        return disc_status;
    }
    if (!mp4_mem_fits()) {
        OSReport("[party] mp4: the heaps are past what GX can read\n");
        disc_status = MP4_DISC_NONE;
        return disc_status;
    }
    if (!mp4_mem_ready()) {
        HuMemInitAll();
    }
    fst_size = mp4_disc_fst_size(header);
    fst = fst_size != 0 && fst_size < 0x100000 ? HuMemDirectMalloc(HEAP_SYSTEM, fst_size) : NULL;
    if (fst == NULL || !mp4_disc_read(mp4_disc_fst_offset(header), fst, fst_size) ||
        mp4_fst_parse(&disc_fst, fst, fst_size) != 0)
    {
        disc_status = MP4_DISC_BAD_FST;
        return disc_status;
    }
    OSReport("[party] mp4: Mario Party 4 (USA) Rev %d, %u files\n", revision, disc_fst.count);
    return disc_status;
}

int mp4_available(void)
{
    int status = mp4_disc_open();
    static int logged;
    if (status != MP4_DISC_OK && !logged) {
        logged = 1;
        OSReport("[party] mp4: %s: the MP4 minigames are off\n", mp4_disc_status_text(status));
    }
    return status == MP4_DISC_OK;
}

int mp4_disc_ready(void)
{
    return disc_status == MP4_DISC_OK;
}

int mp4_disc_find(const char* path, u32* offset, u32* size)
{
    return mp4_disc_ready() && mp4_fst_find(&disc_fst, path, offset, size);
}
