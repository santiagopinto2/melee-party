/* Melee Party, Mario Party 4 runtime: the player's MP4 disc, read in place through the host
 * (MuHostApi.mp4_disc_read, host API 17). Its header is checked once and its file system table
 * kept in the system heap.
 *
 * Also the textures MP4 keeps in its executable (the reflection, toon and highlight maps Hu3DInit
 * reads), read out of the disc's sys/main.dol. partyboard extracts them from the DOL at build time
 * (tools/extract_includes.py there, whose addresses these are); the USA Rev 0 and Rev 1
 * executables keep them at the same addresses. Each is read once, into the system heap, and
 * HuSprAnimRead swaps it in place from there. */
#include <dolphin/os.h>

#include "game/memory.h"
#include "mp4.h"
#include "mp4_data.h"
#include "port/dolassets.h"

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

/* ---- the DOL's textures ---- */

#define DOL_SECTIONS 18

static const struct {
    u32 address;
    u32 size;
} assets[MP4_DOL_ASSET_MAX] = {
    [MP4_DOL_HILITE] = { 0x8012C360, 0x480 },
    [MP4_DOL_HILITE2] = { 0x8012C7E0, 0x480 },
    [MP4_DOL_HILITE3] = { 0x8012CC60, 0x480 },
    [MP4_DOL_HILITE4] = { 0x8012D0E0, 0x480 },
    [MP4_DOL_REFMAP0] = { 0x801225A0, 0x1240 },
    [MP4_DOL_REFMAP1] = { 0x801237E0, 0x1100 },
    [MP4_DOL_REFMAP2] = { 0x801248E0, 0x2080 },
    [MP4_DOL_REFMAP3] = { 0x80126960, 0x2080 },
    [MP4_DOL_REFMAP4] = { 0x801289E0, 0x2080 },
    [MP4_DOL_TOONMAP] = { 0x8012AA60, 0x880 },
    [MP4_DOL_TOONMAP2] = { 0x8012B2E0, 0x1080 },
};

static void* loaded[MP4_DOL_ASSET_MAX];
static int dol_status;   /* 0 unread, 1 read, -1 unreadable */
static u32 dol_offset;
static u32 section_offset[DOL_SECTIONS];
static u32 section_address[DOL_SECTIONS];
static u32 section_size[DOL_SECTIONS];

static u32 be32(const u8* p)
{
    return ((u32) p[0] << 24) | ((u32) p[1] << 16) | ((u32) p[2] << 8) | p[3];
}

/* The DOL's section table: where each section sits in the file and where it loads. */
static int dol_open(void)
{
    u8 header[0x100];
    u8 word[4];
    int i;
    if (dol_status != 0) {
        return dol_status > 0;
    }
    dol_status = -1;
    if (!mp4_disc_read(0x420, word, sizeof word)) {
        return 0;
    }
    dol_offset = be32(word);
    if (!mp4_disc_read(dol_offset, header, sizeof header)) {
        return 0;
    }
    for (i = 0; i < DOL_SECTIONS; i++) {
        section_offset[i] = be32(header + i * 4);
        section_address[i] = be32(header + 0x48 + i * 4);
        section_size[i] = be32(header + 0x90 + i * 4);
    }
    dol_status = 1;
    return 1;
}

void* mp4_dol_asset(int id)
{
    u32 address, size;
    void* buf;
    int i;
    if (id < 0 || id >= MP4_DOL_ASSET_MAX) {
        return NULL;
    }
    if (loaded[id] != NULL) {
        return loaded[id];
    }
    if (!dol_open()) {
        return NULL;
    }
    address = assets[id].address;
    size = assets[id].size;
    for (i = 0; i < DOL_SECTIONS; i++) {
        if (section_size[i] != 0 && address >= section_address[i] &&
            address + size <= section_address[i] + section_size[i])
        {
            break;
        }
    }
    if (i == DOL_SECTIONS) {
        OSReport("[party] mp4: no DOL section holds asset %d at %08X\n", id, address);
        return NULL;
    }
    buf = HuMemDirectMalloc(HEAP_SYSTEM, size);
    if (buf == NULL ||
        !mp4_disc_read(dol_offset + section_offset[i] + (address - section_address[i]), buf, size))
    {
        OSReport("[party] mp4: reading asset %d from the DOL failed\n", id);
        return NULL;
    }
    loaded[id] = buf;
    return buf;
}
