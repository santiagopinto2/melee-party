/* Melee Party, Mario Party 4 runtime: HSF model structures from their file form (big-endian, 32-bit
 * offsets) into the loader's 64-bit structures. A C port of the HSF half of partyboard's
 * src/port/byteswap.cpp (github.com/mariopartyrd/partyboard, CC0): each function swaps the file
 * structure in place, as the original does (the loader reads some of them again afterwards), and
 * copies it into the native one, offsets left as numbers for the loader to resolve. */
#include <string.h>

#include "port/byteswap.h"

static void sw16(void* p)
{
    u8* b = p;
    u8 t = b[0];
    b[0] = b[1];
    b[1] = t;
}

static void sw32(void* p)
{
    u8* b = p;
    u8 t = b[0];
    b[0] = b[3];
    b[3] = t;
    t = b[1];
    b[1] = b[2];
    b[2] = t;
}

static void sw32n(void* p, s32 count)
{
    s32 i;
    for (i = 0; i < count; i++) {
        sw32((u8*) p + i * 4);
    }
}

static void sw16n(void* p, s32 count)
{
    s32 i;
    for (i = 0; i < count; i++) {
        sw16((u8*) p + i * 2);
    }
}

#define PTR(T, v) ((T) (uintptr_t) (v))

void byteswap_clear_visited_ptrs(void) {}

void byteswap_u16(u16* src) { sw16(src); }
void byteswap_s16(s16* src) { sw16(src); }
void byteswap_u32(u32* src) { sw32(src); }
void byteswap_s32(s32* src) { sw32(src); }
void byteswap_float(float* src) { sw32(src); }
void byteswap_vec(Vec* src) { sw32n(src, 3); }
void byteswap_hsfvec3f(HuVecF* src) { sw32n(src, 3); }
void byteswap_hsfvec2f(HuVec2f* src) { sw32n(src, 2); }

static void swap_transform(HSFTRANSFORM* t) { sw32n(t, 9); }

void byteswap_hsfheader(HSFHEADER* src)
{
    /* everything after the magic: 21 sections of an offset and a count */
    sw32n(&src->scene, (s32) ((sizeof(HSFHEADER) - offsetof(HSFHEADER, scene)) / 4));
}

void byteswap_hsfscene(HSFSCENE* src)
{
    sw32(&src->fogType);
    sw32(&src->fogStart);
    sw32(&src->fogEnd);
}

void byteswap_hsfcluster(HsfCluster32b* obj, HSFCLUSTER* dest)
{
    sw32(&obj->name[0]);
    sw32(&obj->name[1]);
    sw32(&obj->targetName);
    sw32(&obj->part);
    sw32(&obj->index);
    sw32n(obj->weight, 32);
    sw16(&obj->type);
    sw32(&obj->vertexCnt);
    sw32(&obj->vertex);

    dest->name[0] = PTR(char*, obj->name[0]);
    dest->name[1] = PTR(char*, obj->name[1]);
    dest->targetName = PTR(char*, obj->targetName);
    dest->part = PTR(HSFPART*, obj->part);
    dest->index = obj->index;
    memcpy(dest->weight, obj->weight, sizeof dest->weight);
    dest->adjusted = obj->adjusted;
    dest->unk95 = obj->unk95;
    dest->type = obj->type;
    dest->vertexNum = obj->vertexCnt;
    dest->vertex = PTR(HSFBUFFER**, obj->vertex);
}

void byteswap_hsfattribute(HsfAttribute32b* obj, HSFATTRIBUTE* dest)
{
    sw32(&obj->name);
    sw32(&obj->unk04);
    sw32(&obj->kColor);
    sw32(&obj->nbtTpLvl);
    sw32(&obj->unk20);
    sw32(&obj->unk28);
    sw32(&obj->unk2C);
    sw32(&obj->unk30);
    sw32(&obj->unk34);
    sw32(&obj->wrapS);
    sw32(&obj->wrapT);
    sw32(&obj->maxLod);
    sw32(&obj->flag);
    sw32(&obj->bitmap);

    dest->name = PTR(char*, obj->name);
    dest->animWorkP = PTR(void*, obj->unk04);
    memcpy(dest->unk8, obj->unk8, sizeof dest->unk8);
    dest->kColor = obj->kColor;
    memcpy(dest->unk10, obj->unk10, sizeof dest->unk10);
    dest->nbtTpLvl = obj->nbtTpLvl;
    memcpy(dest->unk18, obj->unk18, sizeof dest->unk18);
    dest->unk20 = obj->unk20;
    memcpy(dest->unk24, obj->unk24, sizeof dest->unk24);
    dest->scale.x = obj->unk28;
    dest->scale.y = obj->unk2C;
    dest->trans.x = obj->unk30;
    dest->trans.y = obj->unk34;
    memcpy(dest->unk38, obj->unk38, sizeof dest->unk38);
    dest->wrapS = obj->wrapS;
    dest->wrapT = obj->wrapT;
    memcpy(dest->unk6C, obj->unk6C, sizeof dest->unk6C);
    dest->maxLod = obj->maxLod;
    dest->flag = obj->flag;
    dest->bitmap = PTR(HSFBITMAP*, obj->bitmap);
}

void byteswap_hsfmaterial(HsfMaterial32b* obj, HSFMATERIAL* dest)
{
    sw32(&obj->name);
    sw16(&obj->pass);
    sw32(&obj->hiliteScale);
    sw32(&obj->unk18);
    sw32(&obj->invAlpha);
    sw32n(obj->unk20, 2);
    sw32(&obj->refAlpha);
    sw32(&obj->unk2C);
    sw32(&obj->flags);
    sw32(&obj->attrNum);
    sw32(&obj->attr);

    dest->name = PTR(char*, obj->name);
    memcpy(dest->unk4, obj->unk4, sizeof dest->unk4);
    dest->pass = obj->pass;
    dest->vtxMode = obj->vtxMode;
    memcpy(dest->litColor, obj->litColor, 3);
    memcpy(dest->color, obj->color, 3);
    memcpy(dest->shadowColor, obj->shadowColor, 3);
    dest->hiliteScale = obj->hiliteScale;
    dest->unk18 = obj->unk18;
    dest->invAlpha = obj->invAlpha;
    dest->unk20[0] = obj->unk20[0];
    dest->unk20[1] = obj->unk20[1];
    dest->refAlpha = obj->refAlpha;
    dest->unk2C = obj->unk2C;
    dest->flags = obj->flags;
    dest->attrNum = obj->attrNum;
    dest->attr = PTR(intptr_t*, obj->attr);
}

void byteswap_hsfbuffer(HsfBuffer32b* obj, HSFBUFFER* dest)
{
    sw32(&obj->name);
    sw32(&obj->count);
    sw32(&obj->data);

    dest->name = PTR(char*, obj->name);
    dest->count = obj->count;
    dest->data = PTR(void*, obj->data);
}

void byteswap_hsfmatrix(HsfMatrix32b* obj, HSFMATRIX* dest)
{
    u32 count, i;
    sw32(&obj->base_idx);
    sw32(&obj->count);

    dest->base_idx = obj->base_idx;
    dest->count = obj->count;
    dest->data = (Mtx*) (obj + 1);
    count = dest->base_idx + dest->count + dest->base_idx * dest->count;
    for (i = 0; i < count; i++) {
        sw32n(dest->data[i], 12);
    }
}

void byteswap_hsfpalette(HsfPalette32b* obj, HSFPALETTE* dest)
{
    sw32(&obj->name);
    sw32(&obj->unk);
    sw32(&obj->palSize);
    sw32(&obj->data);

    dest->name = PTR(char*, obj->name);
    dest->unk = obj->unk;
    dest->palSize = obj->palSize;
    dest->data = PTR(u16*, obj->data);
}

void byteswap_hsfpart(HsfPart32b* obj, HSFPART* dest)
{
    sw32(&obj->name);
    sw32(&obj->num);
    sw32(&obj->vertex);

    dest->name = PTR(char*, obj->name);
    dest->num = obj->num;
    dest->vertex = PTR(u16*, obj->vertex);
}

void byteswap_hsfbitmap(HsfBitmap32b* obj, HSFBITMAP* dest)
{
    sw32(&obj->name);
    sw32(&obj->maxLod);
    sw16(&obj->sizeX);
    sw16(&obj->sizeY);
    sw16(&obj->palSize);
    sw32(&obj->palData);
    sw32(&obj->unk);
    sw32(&obj->data);

    dest->name = PTR(char*, obj->name);
    dest->maxLod = obj->maxLod;
    dest->dataFmt = obj->dataFmt;
    dest->pixSize = obj->pixSize;
    dest->sizeX = obj->sizeX;
    dest->sizeY = obj->sizeY;
    dest->palSize = obj->palSize;
    dest->tint = obj->tint;
    dest->palData = PTR(u16*, obj->palData);
    dest->unk = obj->unk;
    dest->data = PTR(void*, obj->data);
}

void byteswap_hsfmapattr(HsfMapAttr32b* obj, HSFMAPATTR* dest)
{
    sw32(&obj->minX);
    sw32(&obj->minZ);
    sw32(&obj->maxX);
    sw32(&obj->maxZ);
    sw32(&obj->data);
    sw32(&obj->dataLen);

    dest->minX = obj->minX;
    dest->minZ = obj->minZ;
    dest->maxX = obj->maxX;
    dest->maxZ = obj->maxZ;
    dest->data = PTR(u16*, obj->data);
    dest->dataLen = obj->dataLen;
}

void byteswap_hsfskeleton(HsfSkeleton32b* obj, HSFSKELETON* dest)
{
    sw32(&obj->name);
    swap_transform(&obj->transform);

    dest->name = PTR(char*, obj->name);
    dest->transform = obj->transform;
}

void byteswap_hsfshape(HsfShape32b* obj, HSFSHAPE* dest)
{
    sw32(&obj->name);
    sw16(&obj->num16[0]);
    sw16(&obj->num16[1]);
    sw32(&obj->vertex);

    dest->name = PTR(char*, obj->name);
    dest->num16[0] = obj->num16[0];
    dest->num16[1] = obj->num16[1];
    dest->vertex = PTR(HSFBUFFER**, obj->vertex);
}

void byteswap_hsfcenv_single(HSFCENVSINGLE* obj)
{
    sw32(&obj->target);
    sw16(&obj->pos);
    sw16(&obj->posNum);
    sw16(&obj->normal);
    sw16(&obj->normalNum);
}

void byteswap_hsfcenv_dual_weight(HSFCENVDUALWEIGHT* obj)
{
    sw32(&obj->weight);
    sw16(&obj->pos);
    sw16(&obj->posNum);
    sw16(&obj->normal);
    sw16(&obj->normalNum);
}

void byteswap_hsfcenv_dual(HsfCenvDual32b* obj, HSFCENVDUAL* dest)
{
    sw32(&obj->target1);
    sw32(&obj->target2);
    sw32(&obj->weightNum);
    sw32(&obj->weight);

    dest->target1 = obj->target1;
    dest->target2 = obj->target2;
    dest->weightNum = obj->weightNum;
    dest->weight = PTR(HSFCENVDUALWEIGHT*, obj->weight);
}

void byteswap_hsfcenv_multi_weight(HSFCENVMULTIWEIGHT* obj)
{
    sw32(&obj->target);
    sw32(&obj->value);
}

void byteswap_hsfcenv_multi(HsfCenvMulti32b* obj, HSFCENVMULTI* dest)
{
    sw32(&obj->weightNum);
    sw16(&obj->pos);
    sw16(&obj->posNum);
    sw16(&obj->normal);
    sw16(&obj->normalNum);
    sw32(&obj->weight);

    dest->weightNum = obj->weightNum;
    dest->pos = obj->pos;
    dest->posNum = obj->posNum;
    dest->normal = obj->normal;
    dest->normalNum = obj->normalNum;
    dest->weight = PTR(HSFCENVMULTIWEIGHT*, obj->weight);
}

void byteswap_hsfcenv(HsfCenv32b* obj, HSFCENV* dest)
{
    sw32n(obj, 9);   /* name, three offsets and five counts, all 32 bits */

    dest->name = PTR(char*, obj->name);
    dest->singleData = PTR(HSFCENVSINGLE*, obj->singleData);
    dest->dualData = PTR(HSFCENVDUAL*, obj->dualData);
    dest->multiData = PTR(HSFCENVMULTI*, obj->multiData);
    dest->singleCount = obj->singleCount;
    dest->dualCount = obj->dualCount;
    dest->multiCount = obj->multiCount;
    dest->vtxCount = obj->vtxCount;
    dest->copyCount = obj->copyCount;
}

static void swap_camera(HSFCAMERA* obj)
{
    sw32n(&obj->pos, 3);
    sw32n(&obj->target, 3);
    sw32(&obj->upRot);
    sw32(&obj->fov);
    sw32(&obj->nnear);
    sw32(&obj->ffar);
}

static void swap_light(HSFLIGHT* obj)
{
    sw32n(&obj->pos, 3);
    sw32n(&obj->target, 3);
    sw32(&obj->unk2C);
    sw32(&obj->ref_distance);
    sw32(&obj->ref_brightness);
    sw32(&obj->cutoff);
}

static void swap_object_data(HsfObjectData32b* obj, HSFMESH* dest, u32 type)
{
    sw32(&obj->parent);
    sw32(&obj->childrenCount);
    sw32(&obj->children);
    swap_transform(&obj->base);
    swap_transform(&obj->curr);
    sw32(&obj->face);
    sw32(&obj->vertex);
    sw32(&obj->normal);
    sw32(&obj->color);
    sw32(&obj->st);
    sw32(&obj->material);
    sw32(&obj->attribute);
    sw32(&obj->shapeNum);
    sw32(&obj->shape);
    sw32(&obj->clusterNum);
    sw32(&obj->cluster);
    sw32(&obj->cenvNum);
    sw32(&obj->cenv);
    sw32(&obj->vtxtop);
    sw32(&obj->normtop);

    dest->parent = PTR(HSFOBJECT*, obj->parent);
    dest->childrenCount = obj->childrenCount;
    dest->children = PTR(HSFOBJECT**, obj->children);
    dest->base = obj->base;
    dest->curr = obj->curr;
    dest->face = PTR(HSFBUFFER*, obj->face);
    dest->vertex = PTR(HSFBUFFER*, obj->vertex);
    dest->normal = PTR(HSFBUFFER*, obj->normal);
    dest->color = PTR(HSFBUFFER*, obj->color);
    dest->st = PTR(HSFBUFFER*, obj->st);
    dest->material = PTR(HSFMATERIAL*, obj->material);
    dest->attribute = PTR(HSFATTRIBUTE*, obj->attribute);
    dest->writeNum = obj->writeNum;
    dest->shapeType = obj->shapeType;
    dest->matPass = obj->matPass;
    dest->shapeNum = obj->shapeNum;
    dest->shape = PTR(HSFBUFFER**, obj->shape);
    dest->clusterNum = obj->clusterNum;
    dest->cluster = PTR(HSFCLUSTER**, obj->cluster);
    dest->cenvNum = obj->cenvNum;
    dest->cenv = PTR(HSFCENV*, obj->cenv);
    dest->vtxtop = PTR(HuVecF*, obj->vtxtop);
    dest->normtop = PTR(HuVecF*, obj->normtop);

    switch (type) {
    case HSF_OBJ_MESH:
        sw32n(&obj->mesh.min, 3);
        sw32n(&obj->mesh.max, 3);
        sw32(&obj->mesh.baseMorph);
        sw32n(obj->mesh.morphWeight, 32);
        sw32(&obj->mesh.unkF0);

        dest->mesh.min = obj->mesh.min;
        dest->mesh.max = obj->mesh.max;
        dest->mesh.baseMorph = obj->mesh.baseMorph;
        memset(dest->mesh.morphWeight, 0, sizeof dest->mesh.morphWeight);
        memcpy(dest->mesh.morphWeight, obj->mesh.morphWeight, sizeof obj->mesh.morphWeight);
        break;
    case HSF_OBJ_REPLICA:
        sw32(&obj->replica);
        dest->replica = PTR(HSFOBJECT*, obj->replica);
        break;
    default:
        break;
    }
}

void byteswap_hsfobject(HsfObject32b* obj, HSFOBJECT* dest)
{
    sw32(&obj->name);
    sw32(&obj->type);
    sw32(&obj->constData);
    sw32(&obj->flags);

    dest->name = PTR(char*, obj->name);
    dest->type = obj->type;
    dest->constData = PTR(void*, obj->constData);
    dest->flags = obj->flags;

    switch (obj->type) {
    case HSF_OBJ_CAMERA:
        swap_camera(&obj->camera);
        memcpy(&dest->camera, &obj->camera, sizeof dest->camera);
        break;
    case HSF_OBJ_LIGHT:
        swap_light(&obj->light);
        memcpy(&dest->light, &obj->light, sizeof dest->light);
        break;
    default:
        swap_object_data(&obj->data, &dest->mesh, obj->type);
        break;
    }
}

void byteswap_hsfbitmapkey(HsfBitmapKey32b* obj, HSFBITMAPKEY* dest)
{
    sw32(&obj->time);
    sw32(&obj->data);

    dest->time = obj->time;
    dest->data = PTR(HSFBITMAP*, obj->data);
}

void byteswap_hsftrack(HsfTrack32b* obj, HSFTRACK* dest)
{
    sw16(&obj->target);
    sw16(&obj->curveType);
    sw16(&obj->numKeyframes);

    dest->type = obj->type;
    dest->start = obj->start;
    dest->target = obj->target;
    dest->curveType = obj->curveType;
    dest->numKeyframes = obj->numKeyframes;

    if (obj->curveType == HSF_CURVE_CONST) {
        sw32(&obj->value);
        dest->value = obj->value;
    } else {
        sw32(&obj->data);
        dest->data = PTR(void*, obj->data);
    }

    if (obj->type == HSF_TRACK_CLUSTER_WEIGHT) {
        sw32(&obj->clusterWeight);
        dest->clusterWeight = obj->clusterWeight;
    } else {
        sw16(&obj->attrIdx);
        sw16(&obj->channel);
        dest->attrIdx = obj->attrIdx;
        dest->channel = obj->channel;
    }
}

void byteswap_hsfmotion(HsfMotion32b* obj, HSFMOTION* dest)
{
    sw32(&obj->name);
    sw32(&obj->numTracks);
    sw32(&obj->track);
    sw32(&obj->maxTime);

    dest->name = PTR(char*, obj->name);
    dest->numTracks = obj->numTracks;
    dest->track = PTR(HSFTRACK*, obj->track);
    dest->maxTime = obj->maxTime;
}

void byteswap_hsfface(HsfFace32b* obj, HSFFACE* dest)
{
    sw16(&obj->type);
    sw16(&obj->mat);
    sw32n(&obj->nbt, 3);

    dest->type = obj->type;
    dest->mat = obj->mat;
    dest->nbt = obj->nbt;

    if ((obj->type & 7) == 4) {
        sw32(&obj->strip.count);
        sw32(&obj->strip.data);
        sw16n(obj->strip.indices[0], 3 * 4);

        dest->strip.count = obj->strip.count;
        dest->strip.data = PTR(s16*, obj->strip.data);
        memcpy(dest->strip.indices, obj->strip.indices, sizeof obj->strip.indices);
    } else {
        sw16n(obj->indices[0], 4 * 4);
        memcpy(dest->indices, obj->indices, sizeof obj->indices);
    }
}
