/* Melee Party, Mario Party 4 runtime: what MP4's draw code needs from GX beyond the SDK.
 *
 * The PC port keeps vertex positions, float normals and texture coordinates in host order, where
 * GX (the host's decoder, as the console's hardware) reads arrays big-endian. Each array MP4 sets
 * goes to GX as a big-endian copy in a scratch area, made once per frame and array: skinned models
 * rewrite theirs every frame, so a copy cannot outlive it. Byte arrays (colours, 8-bit normals) need
 * no copy. */
#include <dolphin/gx.h>
#include <dolphin/os.h>

#include "mp4.h"

u8* mp4_gx_scratch(u32* size);

#define COPIES_MAX 256

static struct {
    const void* src;
    u32 size;
    void* copy;
} copies[COPIES_MAX];
static int copy_count;
static u32 scratch_used;
static int scratch_full_logged;

void mp4_gx_frame_begin(void)
{
    copy_count = 0;
    scratch_used = 0;
}

static const void* big_endian_copy(const void* src, u32 size)
{
    u32 scratch_size, i;
    u8* scratch = mp4_gx_scratch(&scratch_size);
    const u32* in = src;
    u32* out;
    int n;
    for (n = 0; n < copy_count; n++) {
        if (copies[n].src == src && copies[n].size >= size) {
            return copies[n].copy;
        }
    }
    size = (size + 3) & ~3u;
    if (copy_count >= COPIES_MAX || scratch_used + size > scratch_size) {
        if (!scratch_full_logged) {
            scratch_full_logged = 1;
            OSReport("[party] mp4: too many vertex arrays in one frame\n");
        }
        return NULL;
    }
    out = (u32*) (scratch + scratch_used);
    for (i = 0; i < size / 4; i++) {
        out[i] = __builtin_bswap32(in[i]);
    }
    copies[copy_count].src = src;
    copies[copy_count].size = size;
    copies[copy_count].copy = out;
    copy_count++;
    scratch_used = (scratch_used + size + 31) & ~31u;
    return out;
}

void mp4_gx_set_array(int attr, const void* data, unsigned int size, unsigned char stride, int host_order)
{
    /* 32-bit components: positions, float normals and texture coordinates */
    int words = attr != GX_VA_CLR0 && attr != GX_VA_CLR1 && (stride & 3) == 0;
    if (host_order && words && data != NULL) {
        const void* copy = big_endian_copy(data, size);
        if (copy == NULL) {
            return;
        }
        data = copy;
    }
    GXSetArray((GXAttr) attr, data, stride);
}

/* GX's draw-done sync in Hu3DExec (between its cameras, and after the shadow pass): on the
 * GameCube it would fire Melee's draw-done callback in the middle of a frame (HSD_VIDrawDoneXFB,
 * which asserts), and the native GX draws in order without it. */
void mp4_gx_draw_done(void) {}
void mp4_gx_wait_draw_done(void) {}
