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

/* The SDK's quaternions in C (partyboard libs/dolphin/src/mtx/quat.c), for m417's raft. */
float acosf(float);
float sinf(float);
float cosf(float);
float sqrtf(float);

void C_QUATMultiply(const Quaternion* a, const Quaternion* b, Quaternion* ab)
{
    Quaternion r;
    r.x = a->w * b->x + a->x * b->w + a->y * b->z - a->z * b->y;
    r.y = a->w * b->y - a->x * b->z + a->y * b->w + a->z * b->x;
    r.z = a->w * b->z + a->x * b->y - a->y * b->x + a->z * b->w;
    r.w = a->w * b->w - a->x * b->x - a->y * b->y - a->z * b->z;
    *ab = r;
}

void C_QUATNormalize(const Quaternion* src, Quaternion* unit)
{
    float len = sqrtf(src->x * src->x + src->y * src->y + src->z * src->z + src->w * src->w);
    if (len > 0.0f) {
        float inv_len = 1.0f / len;
        unit->x = src->x * inv_len;
        unit->y = src->y * inv_len;
        unit->z = src->z * inv_len;
        unit->w = src->w * inv_len;
    } else {
        unit->x = unit->y = unit->z = 0.0f;
        unit->w = 1.0f;
    }
}

void C_QUATInverse(const Quaternion* src, Quaternion* inv)
{
    float len_squared = src->x * src->x + src->y * src->y + src->z * src->z + src->w * src->w;
    if (len_squared > 0.0f) {
        float inv_len_squared = 1.0f / len_squared;
        inv->x = -src->x * inv_len_squared;
        inv->y = -src->y * inv_len_squared;
        inv->z = -src->z * inv_len_squared;
        inv->w = src->w * inv_len_squared;
    } else {
        inv->x = inv->y = inv->z = 0.0f;
        inv->w = 1.0f;
    }
}

void C_QUATRotAxisRad(Quaternion* q, const Vec* axis, f32 rad)
{
    Vec dst;
    f32 s, c;
    VECNormalize((Vec*) axis, &dst);
    s = sinf(rad * 0.5f);
    c = cosf(rad * 0.5f);
    q->x = s * dst.x;
    q->y = s * dst.y;
    q->z = s * dst.z;
    q->w = c;
}

void C_QUATSlerp(const Quaternion* p, const Quaternion* q, Quaternion* r, f32 t)
{
    f32 ratioA, ratioB;
    f32 value = 1.0f;
    f32 cosHalfTheta = p->x * q->x + p->y * q->y + p->z * q->z + p->w * q->w;
    if (cosHalfTheta < 0.0f) {
        cosHalfTheta = -cosHalfTheta;
        value = -value;
    }
    if (cosHalfTheta <= 0.9999899864196777f) {
        f32 halfTheta = acosf(cosHalfTheta);
        f32 sinHalfTheta = sinf(halfTheta);
        ratioA = sinf((1.0f - t) * halfTheta) / sinHalfTheta;
        ratioB = sinf(t * halfTheta) / sinHalfTheta;
        value *= ratioB;
    } else {
        ratioA = 1.0f - t;
        value *= t;
    }
    r->x = (ratioA * p->x) + (value * q->x);
    r->y = (ratioA * p->y) + (value * q->y);
    r->z = (ratioA * p->z) + (value * q->z);
    r->w = (ratioA * p->w) + (value * q->w);
}
