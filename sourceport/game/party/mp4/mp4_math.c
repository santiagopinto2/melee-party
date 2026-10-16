/* Melee Party, Mario Party 4 runtime: maths MP4 calls that Melee's SDK only has in paired-single
 * assembly (psmtx.c), in C. */
#include <dolphin/mtx.h>

/* A matrix reordered by columns, and that matrix times an array of vectors (EnvelopeExec.c). */
void mp4_mtx_reorder(Mtx src, ROMtx dest)
{
    int r, c;
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 4; c++) {
            dest[c][r] = src[r][c];
        }
    }
}

void mp4_mtx_ro_mult_vec_array(ROMtx m, Vec* srcBase, Vec* dstBase, u32 count)
{
    u32 i;
    for (i = 0; i < count; i++) {
        Vec v = srcBase[i];
        dstBase[i].x = m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z + m[3][0];
        dstBase[i].y = m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z + m[3][1];
        dstBase[i].z = m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z + m[3][2];
    }
}

/* The vector helpers MP4's minigames use (ext_math.h), written in assembly in MP4. */
void HuSetVecF(Vec* v, f32 x, f32 y, f32 z)
{
    v->x = x;
    v->y = y;
    v->z = z;
}

void HuSubVecF(Vec* out, Vec* a, Vec* b)
{
    out->x = a->x - b->x;
    out->y = a->y - b->y;
    out->z = a->z - b->z;
}
