/* Melee Party, Mario Party 4 runtime: the HSF model loader, from the MP4 decompilation
 * (github.com/mariopartyrd/partyboard src/game/hsfload.c, CC0), as its PC port builds it
 * (BYTESWAPPING: the file's structures are swapped and copied into 64-bit ones). Its debug dump is
 * left out. */
#include "game/hsfload.h"
#include "game/EnvelopeExec.h"
#include "ctype.h"
#include "game/hsfformat.h"
#include <string.h>


#ifdef BYTESWAPPING
#include "game/memory.h"
#include "port/byteswap.h"
#endif
#include "game/hsfformat.h"

#define AS_S16(field) (*((s16 *)&(field)))
#define AS_U16(field) (*((u16 *)&(field)))

GXColor rgba[100];
HSFHEADER head;
HSFDATA Model;

static BOOL MotionOnly;
static HSFDATA *MotionModel;
static void *VertexDataTop;
static void *NormalDataTop;
void *fileptr;
char *StringTable;
char *DicStringTable;
void **NSymIndex;
HSFOBJECT *objtop;
HSFBUFFER *vtxtop;
HSFCLUSTER *ClusterTop;
HSFATTRIBUTE *AttributeTop;
HSFMATERIAL *MaterialTop;
#ifdef BYTESWAPPING
HSFBUFFER *NormalTop;
HSFBUFFER *StTop;
HSFBUFFER *ColorTop;
HSFBUFFER *FaceTop;
HSFCENV *CenvTop;
HSFPART *PartTop;
HSFBITMAP *BitmapTop;
#endif


static void FileLoad(void *data);
static HSFDATA *SetHsfModel(void);
static void MaterialLoad(void);
static void AttributeLoad(void);
static void SceneLoad(void);
static void ColorLoad(void);
static void VertexLoad(void);
static void NormalLoad(void);
static void STLoad(void);
static void FaceLoad(void);
static void ObjectLoad(void);
static void CenvLoad(void);
static void SkeletonLoad(void);
static void PartLoad(void);
static void ClusterLoad(void);
static void ShapeLoad(void);
static void MapAttrLoad(void);
static void PaletteLoad(void);
static void BitmapLoad(void);
static void MotionLoad(void);
static void MatrixLoad(void);

static s32 SearchObjectSetName(HSFDATA *data, char *name);
static HSFBUFFER *SearchVertexPtr(s32 id);
static HSFBUFFER *SearchNormalPtr(s32 id);
static HSFBUFFER *SearchStPtr(s32 id);
static HSFBUFFER *SearchColorPtr(s32 id);
static HSFBUFFER *SearchFacePtr(s32 id);
static HSFCENV *SearchCenvPtr(s32 id);
static HSFPART *SearchPartPtr(s32 id);
static HSFPALETTE *SearchPalettePtr(s32 id);

static HSFBITMAP *SearchBitmapPtr(s32 id);
static char *GetString(u32 *str_ofs);
static char *GetMotionString(u16 *str_ofs);

#ifdef BYTESWAPPING

static void LoadEnvelopeSourceData(HSFOBJECT *object, HSFMESH *data)
{
    if (object->mesh.cenvNum == 0) {
        object->mesh.vtxtop = NULL;
        object->mesh.normtop = NULL;
        return;
    }
    if (data->vtxtop != NULL && object->mesh.vertex != NULL) {
        HuVecF *vtx = (HuVecF *)((uintptr_t)fileptr + (uintptr_t)data->vtxtop);
        for (int i = 0; i < object->mesh.vertex->count; i++) {
            byteswap_hsfvec3f(&vtx[i]);
        }
        object->mesh.vtxtop = vtx;
        // object->data.vtxtop = CopyByteSwappedVec3Data(
        //     (HuVecF *)((uintptr_t)fileptr + (uintptr_t)data->vtxtop),
        //     object->data.vertex->count);
    } else {
        object->mesh.vtxtop = NULL;
    }
    if (data->normtop != NULL && object->mesh.normal != NULL) {
        HuVecF *norm = (HuVecF *)((uintptr_t)fileptr + (uintptr_t)data->normtop);
        for (int i = 0; i < object->mesh.normal->count; i++) {
            byteswap_hsfvec3f(&norm[i]);
        }
        object->mesh.normtop = norm;
        // object->data.normtop = CopyByteSwappedVec3Data(
        //     (HuVecF *)((uintptr_t)fileptr + (uintptr_t)data->normtop),
        //     object->data.normal->count);
    } else {
        object->mesh.normtop = NULL;
    }
}
#endif

HSFDATA *LoadHSF(void *data)
{
#ifdef BYTESWAPPING
    byteswap_clear_visited_ptrs();
#endif
    HSFDATA *hsf;
    Model.root = NULL;
    objtop = NULL;
    FileLoad(data);
    SceneLoad();
    ColorLoad();
    PaletteLoad();
    BitmapLoad();
    MaterialLoad();
    AttributeLoad();
    VertexLoad();
    NormalLoad();
    STLoad();
    FaceLoad();
#ifndef BYTESWAPPING
    ObjectLoad();
#endif
    CenvLoad();
    SkeletonLoad();
    PartLoad();
    ClusterLoad();
    ShapeLoad();
    MapAttrLoad();
#ifdef BYTESWAPPING
    // to properly set pointers
    ObjectLoad();
#endif
    MotionLoad();
    MatrixLoad();
    hsf = SetHsfModel();
    InitEnvelope(hsf);
    objtop = NULL;
    return hsf;

}

void ClusterAdjustObject(HSFDATA *model, HSFDATA *src_model)
{
    HSFCLUSTER *cluster;
    s32 i;
    if(!src_model) {
        return;
    }
    if(src_model->clusterNum == 0) {
        return;
    }
    cluster = src_model->cluster;
    if(cluster->adjusted) {
        return;
    }
    cluster->adjusted = 1;
    for(i=0; i<src_model->clusterNum; i++, cluster++) {
        char *name = cluster->targetName;
        cluster->target = SearchObjectSetName(model, name);
    }
}

static void FileLoad(void *data)
{
#ifdef BYTESWAPPING
    s32 i;
#endif
    fileptr = data;
    memcpy(&head, fileptr, sizeof(HSFHEADER));
    memset(&Model, 0, sizeof(HSFDATA));
#ifdef BYTESWAPPING
    byteswap_hsfheader(&head);
    NSymIndex = HuMemDirectMallocNum(HEAP_DATA, sizeof(void*) * head.symbol.count, MEMORY_DEFAULT_NUM);
    for (i = 0; i < head.symbol.count; i++) {
        u32 *file_symbol_real = (u32 *)((uintptr_t)fileptr + head.symbol.ofs);
        byteswap_u32(&file_symbol_real[i]);
        NSymIndex[i] = (void *)(uintptr_t)file_symbol_real[i]; // TODO is this uintptr_t cast right?
    }
    StringTable = (char *)((uintptr_t)fileptr+head.string.ofs);
    ClusterTop = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFCLUSTER) * head.cluster.count, MEMORY_DEFAULT_NUM);
    for (i = 0; i < head.cluster.count; i++) {
        HsfCluster32b *file_cluster_real = (HsfCluster32b *)((uintptr_t)fileptr + head.cluster.ofs);
        byteswap_hsfcluster(&file_cluster_real[i], &ClusterTop[i]);
    }
    AttributeTop = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFATTRIBUTE) * head.attribute.count, MEMORY_DEFAULT_NUM);
    for (i = 0; i < head.attribute.count; i++) {
        HsfAttribute32b *file_attribute_real = (HsfAttribute32b *)((uintptr_t)fileptr + head.attribute.ofs);
        byteswap_hsfattribute(&file_attribute_real[i], &AttributeTop[i]);
    }
    MaterialTop = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFMATERIAL) * head.material.count, MEMORY_DEFAULT_NUM);
    for (i = 0; i < head.material.count; i++) {
        HsfMaterial32b *file_material_real = (HsfMaterial32b *)((uintptr_t)fileptr + head.material.ofs);
        byteswap_hsfmaterial(&file_material_real[i], &MaterialTop[i]);
    }
#else
    NSymIndex = (void **)((uintptr_t)fileptr+head.symbol.ofs);
    StringTable = (char *)((uintptr_t)fileptr+head.string.ofs);
    ClusterTop = (HSFCLUSTER *)((uintptr_t)fileptr+head.cluster.ofs);
    AttributeTop = (HSFATTRIBUTE *)((uintptr_t)fileptr + head.attribute.ofs);
    MaterialTop = (HSFMATERIAL *)((uintptr_t)fileptr + head.material.ofs);
#endif
}

static HSFDATA *SetHsfModel(void)
{
#ifdef BYTESWAPPING
    // TODO free
    HSFDATA *data = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFDATA), MEMORY_DEFAULT_NUM);
#else
    HSFDATA *data = fileptr;
#endif
    data->scene = Model.scene;
    data->sceneNum = Model.sceneNum;
    data->attribute = Model.attribute;
    data->attributeNum = Model.attributeNum;
    data->bitmap = Model.bitmap;
    data->bitmapNum = Model.bitmapNum;
    data->cenv = Model.cenv;
    data->cenvNum = Model.cenvNum;
    data->skeleton = Model.skeleton;
    data->skeletonNum = Model.skeletonNum;
    data->face = Model.face;
    data->faceNum = Model.faceNum;
    data->material = Model.material;
    data->materialNum = Model.materialNum;
    data->motion = Model.motion;
    data->motionNum = Model.motionNum;
    data->normal = Model.normal;
    data->normalNum = Model.normalNum;
    data->root = Model.root;
    data->objectNum = Model.objectNum;
    data->object = objtop;
    data->matrix = Model.matrix;
    data->matrixNum = Model.matrixNum;
    data->palette = Model.palette;
    data->paletteNum = Model.paletteNum;
    data->st = Model.st;
    data->stNum = Model.stNum;
    data->vertex = Model.vertex;
    data->vertexNum = Model.vertexNum;
    data->cenv = Model.cenv;
    data->cenvNum = Model.cenvNum;
    data->cluster = Model.cluster;
    data->clusterNum = Model.clusterNum;
    data->part = Model.part;
    data->partNum = Model.partNum;
    data->shape = Model.shape;
    data->shapeNum = Model.shapeNum;
    data->mapAttr = Model.mapAttr;
    data->mapAttrNum = Model.mapAttrNum;
#ifdef TARGET_PC
    // for debugging, why doesn't the original set it?...
    data->color = Model.color;
    data->colorNum = Model.colorNum;
#endif
#ifdef BYTESWAPPING
    // TODO PC was this created solely for the sake of being able to free it later?
    data->symbol = NSymIndex;
#endif
    return data;
}

char *SetName(u32 *str_ofs)
{
    char *ret = GetString(str_ofs);
    return ret;
}

static inline char *SetMotionName(u16 *str_ofs)
{
    char *ret = GetMotionString(str_ofs);
    return ret;
}

static void MaterialLoad(void)
{
    s32 i;
    s32 j;
    if(head.material.count) {
#ifdef BYTESWAPPING
        HSFMATERIAL *file_mat = MaterialTop;
#else
        HSFMATERIAL *file_mat = (HSFMATERIAL *)((uintptr_t)fileptr+head.material.ofs);
#endif
        HSFMATERIAL *curr_mat;
        HSFMATERIAL *new_mat;
        for(i=0; i<head.material.count; i++) {
            curr_mat = &file_mat[i];
        }
        new_mat = file_mat;
        Model.material = new_mat;
        Model.materialNum = head.material.count;
#ifdef BYTESWAPPING
        file_mat = MaterialTop;
#else
        file_mat = (HSFMATERIAL *)((u32)fileptr+head.material.ofs);
#endif
        for(i=0; i<head.material.count; i++, new_mat++) {
            curr_mat = &file_mat[i];
            new_mat->name = SetName((u32 *)&curr_mat->name);
            new_mat->pass = curr_mat->pass;
            new_mat->vtxMode = curr_mat->vtxMode;
            new_mat->litColor[0] = curr_mat->litColor[0];
            new_mat->litColor[1] = curr_mat->litColor[1];
            new_mat->litColor[2] = curr_mat->litColor[2];
            new_mat->color[0] = curr_mat->color[0];
            new_mat->color[1] = curr_mat->color[1];
            new_mat->color[2] = curr_mat->color[2];
            new_mat->shadowColor[0] = curr_mat->shadowColor[0];
            new_mat->shadowColor[1] = curr_mat->shadowColor[1];
            new_mat->shadowColor[2] = curr_mat->shadowColor[2];
            new_mat->hiliteScale = curr_mat->hiliteScale;
            new_mat->unk18 = curr_mat->unk18;
            new_mat->invAlpha = curr_mat->invAlpha;
            new_mat->unk20[0] = curr_mat->unk20[0];
            new_mat->unk20[1] = curr_mat->unk20[1];
            new_mat->refAlpha = curr_mat->refAlpha;
            new_mat->unk2C = curr_mat->unk2C;
            new_mat->attrNum = curr_mat->attrNum;
            // the pointers are being treated as values
            new_mat->attr = (intptr_t *)(NSymIndex+((uintptr_t)curr_mat->attr));
            rgba[i].r = new_mat->litColor[0];
            rgba[i].g = new_mat->litColor[1];
            rgba[i].b = new_mat->litColor[2];
            rgba[i].a = 255;
            for(j=0; j<new_mat->attrNum; j++) {
                new_mat->attr[j] = new_mat->attr[j];
            }
        }
    }
}

static void AttributeLoad(void)
{
    HSFATTRIBUTE *file_attr;
    HSFATTRIBUTE *new_attr;
    HSFATTRIBUTE *temp_attr;
    s32 i;
    if(head.attribute.count) {
#ifdef BYTESWAPPING
        temp_attr = file_attr = AttributeTop;
#else
        temp_attr = file_attr = (HSFATTRIBUTE *)((u32)fileptr+head.attribute.ofs);
#endif
        new_attr = temp_attr;
        Model.attribute = new_attr;
        Model.attributeNum = head.attribute.count;
        for(i=0; i<head.attribute.count; i++, new_attr++) {
            if((u32)(uintptr_t)file_attr[i].name != -1) {
                new_attr->name = SetName((u32 *)&file_attr[i].name);
            } else {
                new_attr->name = NULL;
            }
            new_attr->bitmap = SearchBitmapPtr((s32)(intptr_t)file_attr[i].bitmap);
#ifdef OPTIMIZED_TEXTURE_LOADING
            new_attr->tex_initialized = FALSE;
            new_attr->tex8000_initialized = FALSE;
            new_attr->tlut_initialized = FALSE;
            new_attr->tlut8000_initialized = FALSE;
            memset(&new_attr->tex_obj, 0, sizeof(new_attr->tex_obj));
            memset(&new_attr->tex8000_obj, 0, sizeof(new_attr->tex8000_obj));
            memset(&new_attr->tlut_obj, 0, sizeof(new_attr->tlut_obj));
            memset(&new_attr->tlut8000_obj, 0, sizeof(new_attr->tlut8000_obj));
#endif
        }
    }
}

static void SceneLoad(void)
{
#ifdef BYTESWAPPING
    s32 i;
#endif
    HSFSCENE *file_scene;
    HSFSCENE *new_scene;
    if(head.scene.count) {
        file_scene = (HSFSCENE *)((uintptr_t)fileptr+head.scene.ofs);
#ifdef BYTESWAPPING
        for (i = 0; i < head.scene.count; i++) {
            byteswap_hsfscene(&file_scene[i]);
        }
#endif
        new_scene = file_scene;
        new_scene->fogEnd = file_scene->fogEnd;
        new_scene->fogStart = file_scene->fogStart;
        Model.scene = new_scene;
        Model.sceneNum = head.scene.count;
    }
}

static void ColorLoad(void)
{
    s32 i;
    HSFBUFFER *file_color;
    HSFBUFFER *new_color;
    void *data;
    void *color_data;
    HSFBUFFER *temp_color;

    if(head.color.count) {
#ifdef BYTESWAPPING
        HsfBuffer32b * file_color_real = (HsfBuffer32b *)((uintptr_t)fileptr + head.color.ofs);
        temp_color = file_color = ColorTop = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFBUFFER) * head.color.count, MEMORY_DEFAULT_NUM);
        for (i = 0; i < head.color.count; i++) {
            byteswap_hsfbuffer(&file_color_real[i], &file_color[i]);
        }
#else
        temp_color = file_color = (HSFBUFFER *)((u32)fileptr+head.color.ofs);
        data = &file_color[head.color.count];
        for(i=0; i<head.color.count; i++, file_color++);
#endif
        new_color = temp_color;
        Model.color = new_color;
        Model.colorNum = head.color.count;
#ifdef BYTESWAPPING
        data = (u16 *)&file_color_real[head.color.count];
#else
        file_color = (HSFBUFFER *)((u32)fileptr+head.color.ofs);
        data = &file_color[head.color.count];
#endif
        for(i=0; i<head.color.count; i++, new_color++, file_color++) {
            color_data = file_color->data;
            new_color->name = SetName((u32 *)&file_color->name);
            new_color->data = (void *)((uintptr_t)data+(uintptr_t)color_data);
        }
    }
}

static void VertexLoad(void)
{
    s32 i, j;
    HSFBUFFER *file_vertex;
    HSFBUFFER *new_vertex;
    void *data;
    HuVecF *data_elem;
    void *temp_data;

    if(head.vertex.count) {
#ifdef BYTESWAPPING
        HsfBuffer32b *file_vertex_real = (HsfBuffer32b *)((uintptr_t)fileptr + head.vertex.ofs);
        vtxtop = file_vertex = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFBUFFER) * head.vertex.count, MEMORY_DEFAULT_NUM);
        for (i = 0; i < head.vertex.count; i++) {
            byteswap_hsfbuffer(&file_vertex_real[i], &file_vertex[i]);
        }
#else
        vtxtop = file_vertex = (HSFBUFFER *)((u32)fileptr+head.vertex.ofs);
        data = (void *)&file_vertex[head.vertex.count];
        for(i=0; i<head.vertex.count; i++, file_vertex++) {
            for(j=0; j<(u32)file_vertex->count; j++) {
                data_elem = (HuVecF *)(((uintptr_t)data)+((uintptr_t)file_vertex->data)+(j*sizeof(HuVecF)));
            }
        }
#endif
        new_vertex = vtxtop;
        Model.vertex = new_vertex;
        Model.vertexNum = head.vertex.count;
#ifdef BYTESWAPPING
        VertexDataTop = data = (void *)&file_vertex_real[head.vertex.count];
#else
        file_vertex = (HSFBUFFER *)((u32)fileptr+head.vertex.ofs);
        VertexDataTop = data = (void *)&file_vertex[head.vertex.count];
#endif
        for(i=0; i<head.vertex.count; i++, new_vertex++, file_vertex++) {
            temp_data = file_vertex->data;
            new_vertex->count = file_vertex->count;
            new_vertex->name = SetName((u32 *)&file_vertex->name);
            new_vertex->data = (void *)((uintptr_t)data + (uintptr_t)temp_data);
            for(j=0; j<new_vertex->count; j++) {
                data_elem = (HuVecF *)((uintptr_t)data + (uintptr_t)temp_data + (j * sizeof(HuVecF)));
#ifdef BYTESWAPPING
                // TODO we should do extra allocations for these elements and don't swap the dvd data directly to avoid double byteswaps
                byteswap_hsfvec3f(data_elem);
#endif
                ((HuVecF *)new_vertex->data)[j].x = data_elem->x;
                ((HuVecF *)new_vertex->data)[j].y = data_elem->y;
                ((HuVecF *)new_vertex->data)[j].z = data_elem->z;
            }
        }
    }
}

static void NormalLoad(void)
{
    s32 i, j;
    void *temp_data;
    HSFBUFFER *file_normal;
    HSFBUFFER *new_normal;
    HSFBUFFER *temp_normal;
    void *data;


    if(head.normal.count) {
        s32 cenv_count = head.cenv.count;
#ifdef BYTESWAPPING
        HsfBuffer32b *file_normal_real = (HsfBuffer32b *)((uintptr_t)fileptr + head.normal.ofs);
        temp_normal = file_normal = NormalTop = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFBUFFER) * head.normal.count, MEMORY_DEFAULT_NUM);
        for (i = 0; i < head.normal.count; i++) {
            byteswap_hsfbuffer(&file_normal_real[i], &file_normal[i]);
        }
#else
        temp_normal = file_normal = (HSFBUFFER *)((u32)fileptr+head.normal.ofs);
        data = (void *)&file_normal[head.normal.count];
#endif
        new_normal = temp_normal;
        Model.normal = new_normal;
        Model.normalNum = head.normal.count;
#ifdef BYTESWAPPING
        NormalDataTop = data = (void *)&file_normal_real[head.normal.count];
#else
        file_normal = (HSFBUFFER *)((u32)fileptr+head.normal.ofs);
        NormalDataTop = data = (void *)&file_normal[head.normal.count];
#endif
        for(i=0; i<head.normal.count; i++, new_normal++, file_normal++) {
            temp_data = file_normal->data;
            new_normal->count = file_normal->count;
            new_normal->name = SetName((u32 *)&file_normal->name);
            new_normal->data = (void *)((uintptr_t)data+(uintptr_t)temp_data);
#ifdef BYTESWAPPING
            if (cenv_count != 0) {
                for (j = 0; j < new_normal->count; j++) {
                    HuVecF *normalData = &((HuVecF *)new_normal->data)[j];
                    byteswap_hsfvec3f(normalData);
                }
            }
#endif
        }
    }
}

static void STLoad(void)
{
    s32 i, j;
    HSFBUFFER *file_st;
    HSFBUFFER *temp_st;
    HSFBUFFER *new_st;
    void *data;
    HuVec2f *data_elem;
    void *temp_data;

    if(head.st.count) {
#ifdef BYTESWAPPING
        HsfBuffer32b *file_st_real = (HsfBuffer32b *)((uintptr_t)fileptr + head.st.ofs);
        temp_st = file_st = StTop = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFBUFFER) * head.st.count, MEMORY_DEFAULT_NUM);
        for (i = 0; i < head.st.count; i++) {
            byteswap_hsfbuffer(&file_st_real[i], &file_st[i]);
        }
#else
        temp_st = file_st = (HSFBUFFER *)((u32)fileptr+head.st.ofs);
        data = (void *)&file_st[head.st.count];
        for(i=0; i<head.st.count; i++, file_st++) {
            for(j=0; j<(u32)file_st->count; j++) {
                data_elem = (HuVec2f *)(((u32)data)+((u32)file_st->data)+(j*sizeof(HuVec2f)));
            }
        }
#endif
        new_st = temp_st;
        Model.st = new_st;
        Model.stNum = head.st.count;
#ifdef BYTESWAPPING
        data = (void *)&file_st_real[head.st.count];
#else
        file_st = (HSFBUFFER *)((u32)fileptr+head.st.ofs);
        data = (void *)&file_st[head.st.count];
#endif
        for(i=0; i<head.st.count; i++, new_st++, file_st++) {
            temp_data = file_st->data;
            new_st->count = file_st->count;
            new_st->name = SetName((u32 *)&file_st->name);
            new_st->data = (void *)((uintptr_t)data + (uintptr_t)temp_data);
            for(j=0; j<new_st->count; j++) {
                data_elem = (HuVec2f *)((uintptr_t)data + (uintptr_t)temp_data + (j*sizeof(HuVec2f)));
#ifdef BYTESWAPPING
                byteswap_hsfvec2f(data_elem);
#endif
                ((HuVec2f *)new_st->data)[j].x = data_elem->x;
                ((HuVec2f *)new_st->data)[j].y = data_elem->y;
            }
        }
    }
}

static void FaceLoad(void)
{
    HSFBUFFER *file_face;
    HSFBUFFER *new_face;
    HSFBUFFER *temp_face;
    HSFFACE *temp_data;
    HSFFACE *data;
    HSFFACE *file_face_strip;
    HSFFACE *new_face_strip;
    u8 *strip;
    s32 i;
    s32 j;

    if(head.face.count) {
#ifdef BYTESWAPPING
        HsfBuffer32b *file_face_real = (HsfBuffer32b *)((uintptr_t)fileptr + head.face.ofs);
        HsfFace32b *file_facedata_real = (HsfFace32b *)&file_face_real[head.face.count];
        temp_face = file_face = FaceTop = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFBUFFER) * head.face.count, MEMORY_DEFAULT_NUM);
        for (i = 0; i < head.face.count; i++) {
            byteswap_hsfbuffer(&file_face_real[i], &file_face[i]);
        }
#else
        temp_face = file_face = (HSFBUFFER *)((u32)fileptr+head.face.ofs);
        data = (HSFFACE *)&file_face[head.face.count];
#endif
        new_face = temp_face;
        Model.face = new_face;
        Model.faceNum = head.face.count;
#ifndef BYTESWAPPING
        file_face = (HSFBUFFER *)((u32)fileptr+head.face.ofs);
        data = (HSFFACE *)&file_face[head.face.count];
#endif
        for(i=0; i<head.face.count; i++, new_face++, file_face++) {
            temp_data = file_face->data;
            new_face->name = SetName((u32 *)&file_face->name);
            new_face->count = file_face->count;
#ifdef BYTESWAPPING
            {
                HsfFace32b *facedata_start = (HsfFace32b *)((uintptr_t)file_facedata_real + (uintptr_t)temp_data);
                data = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFFACE) * new_face->count, MEMORY_DEFAULT_NUM);
                for (j = 0; j < new_face->count; j++) {
                    byteswap_hsfface(&facedata_start[j], &data[j]);
                }
                new_face->data = data;
                strip = (u8 *)(&facedata_start[new_face->count]);
            }
#else
            new_face->data = (void *)((uintptr_t)data+(uintptr_t)temp_data);
            strip = (u8 *)(&((HSFFACE *)new_face->data)[new_face->count]);
#endif
        }
        new_face = temp_face;
        for(i=0; i<head.face.count; i++, new_face++) {
            file_face_strip = new_face_strip = new_face->data;
            for(j=0; j<new_face->count; j++, new_face_strip++, file_face_strip++) {
                if(AS_U16(file_face_strip->type) == 4) {
                    new_face_strip->strip.data = (s16 *)(strip+(uintptr_t)file_face_strip->strip.data*(sizeof(s16)*4));
#ifdef BYTESWAPPING
                    {
                        s32 k;
                        for (k = 0; k < new_face_strip->strip.count * 4; k++) {
                            byteswap_s16(&new_face_strip->strip.data[k]);
                        }
                    }
#endif
                }
            }
        }
    }
}

static void DispObject(HSFOBJECT *parent, HSFOBJECT *object)
{
    u32 i;
    HSFOBJECT *child_obj;
    HSFOBJECT *temp_object;
    struct {
        HSFOBJECT *parent;
        HSFBUFFER *shape;
        HSFCLUSTER *cluster;
    } temp;

    temp.parent = parent;
    object->type = object->type;
    switch(object->type) {
        case HSF_OBJ_MESH:
        {
            HSFMESH *data;
            HSFOBJECT *new_object;

            data = &object->mesh;
            new_object = temp_object = object;
            new_object->mesh.childrenCount = data->childrenCount;
            new_object->mesh.children = (HSFOBJECT **)&NSymIndex[(uintptr_t)data->children];
            for(i=0; i<new_object->mesh.childrenCount; i++) {
                child_obj = &objtop[(uintptr_t)new_object->mesh.children[i]];
                new_object->mesh.children[i] = child_obj;
            }
            new_object->mesh.parent = parent;
            if(Model.root == NULL) {
                Model.root = temp_object;
            }
            new_object->type = HSF_OBJ_MESH;
            new_object->mesh.vertex = SearchVertexPtr((s32)(intptr_t)data->vertex);
            new_object->mesh.normal = SearchNormalPtr((s32)(intptr_t)data->normal);
            new_object->mesh.st = SearchStPtr((s32)(intptr_t)data->st);
            new_object->mesh.color = SearchColorPtr((s32)(intptr_t)data->color);
            new_object->mesh.face = SearchFacePtr((s32)(intptr_t)data->face);
            new_object->mesh.shape = (HSFBUFFER **)&NSymIndex[(uintptr_t)data->shape];
            for(i=0; i<new_object->mesh.shapeNum; i++) {
                temp.shape = &vtxtop[(uintptr_t)new_object->mesh.shape[i]];
                new_object->mesh.shape[i] = temp.shape;
            }
            new_object->mesh.cluster = (HSFCLUSTER **)&NSymIndex[(uintptr_t)data->cluster];
            for(i=0; i<new_object->mesh.clusterNum; i++) {
                temp.cluster = &ClusterTop[(uintptr_t)new_object->mesh.cluster[i]];
                new_object->mesh.cluster[i] = temp.cluster;
            }
            new_object->mesh.cenv = SearchCenvPtr((s32)(intptr_t)data->cenv);
            new_object->mesh.material = Model.material;
            if((intptr_t)data->attribute >= 0) {
                new_object->mesh.attribute = Model.attribute;
            } else {
                new_object->mesh.attribute = NULL;
            }
#ifdef BYTESWAPPING
            LoadEnvelopeSourceData(new_object, data);
#else
            new_object->mesh.vtxtop = (void *)((uintptr_t)fileptr + (uintptr_t)data->vtxtop);
            new_object->mesh.normtop = (void *)((uintptr_t)fileptr + (uintptr_t)data->normtop);
#endif
            new_object->mesh.base.pos.x = data->base.pos.x;
            new_object->mesh.base.pos.y = data->base.pos.y;
            new_object->mesh.base.pos.z = data->base.pos.z;
            new_object->mesh.base.rot.x = data->base.rot.x;
            new_object->mesh.base.rot.y = data->base.rot.y;
            new_object->mesh.base.rot.z = data->base.rot.z;
            new_object->mesh.base.scale.x = data->base.scale.x;
            new_object->mesh.base.scale.y = data->base.scale.y;
            new_object->mesh.base.scale.z = data->base.scale.z;
            new_object->mesh.mesh.min.x = data->mesh.min.x;
            new_object->mesh.mesh.min.y = data->mesh.min.y;
            new_object->mesh.mesh.min.z = data->mesh.min.z;
            new_object->mesh.mesh.max.x = data->mesh.max.x;
            new_object->mesh.mesh.max.y = data->mesh.max.y;
            new_object->mesh.mesh.max.z = data->mesh.max.z;
            for(i=0; i<data->childrenCount; i++) {
                DispObject(new_object, new_object->mesh.children[i]);
            }
        }
        break;

        case HSF_OBJ_NULL1:
        {
            HSFMESH *data;
            HSFOBJECT *new_object;
            data = &object->mesh;
            new_object = temp_object = object;
            new_object->mesh.parent = parent;
            new_object->mesh.childrenCount = data->childrenCount;
            new_object->mesh.children = (HSFOBJECT **)&NSymIndex[(uintptr_t)data->children];
            for(i=0; i<new_object->mesh.childrenCount; i++) {
                child_obj = &objtop[(uintptr_t)new_object->mesh.children[i]];
                new_object->mesh.children[i] = child_obj;
            }
            if(Model.root == NULL) {
                Model.root = temp_object;
            }
            for(i=0; i<data->childrenCount; i++) {
                DispObject(new_object, new_object->mesh.children[i]);
            }
        }
        break;

        case HSF_OBJ_REPLICA:
        {
            HSFMESH *data;
            HSFOBJECT *new_object;
            data = &object->mesh;
            new_object = temp_object = object;
            new_object->mesh.parent = parent;
            new_object->mesh.childrenCount = data->childrenCount;
            new_object->mesh.children = (HSFOBJECT **)&NSymIndex[(uintptr_t)data->children];
            for(i=0; i<new_object->mesh.childrenCount; i++) {
                child_obj = &objtop[(uintptr_t)new_object->mesh.children[i]];
                new_object->mesh.children[i] = child_obj;
            }
            if(Model.root == NULL) {
                Model.root = temp_object;
            }
            new_object->mesh.replica = &objtop[(uintptr_t)new_object->mesh.replica];
            for(i=0; i<data->childrenCount; i++) {
                DispObject(new_object, new_object->mesh.children[i]);
            }
        }
        break;

        case HSF_OBJ_ROOT:
        {
            HSFMESH *data;
            HSFOBJECT *new_object;
            data = &object->mesh;
            new_object = temp_object = object;
            new_object->mesh.parent = parent;
            new_object->mesh.childrenCount = data->childrenCount;
            new_object->mesh.children = (HSFOBJECT **)&NSymIndex[(uintptr_t)data->children];
            for(i=0; i<new_object->mesh.childrenCount; i++) {
                child_obj = &objtop[(uintptr_t)new_object->mesh.children[i]];
                new_object->mesh.children[i] = child_obj;
            }
            if(Model.root == NULL) {
                Model.root = temp_object;
            }
            for(i=0; i<data->childrenCount; i++) {
                DispObject(new_object, new_object->mesh.children[i]);
            }
        }
        break;

        case HSF_OBJ_JOINT:
        {
            HSFMESH *data;
            HSFOBJECT *new_object;
            data = &object->mesh;
            new_object = temp_object = object;
            new_object->mesh.parent = parent;
            new_object->mesh.childrenCount = data->childrenCount;
            new_object->mesh.children = (HSFOBJECT **)&NSymIndex[(uintptr_t)data->children];
            for(i=0; i<new_object->mesh.childrenCount; i++) {
                child_obj = &objtop[(uintptr_t)new_object->mesh.children[i]];
                new_object->mesh.children[i] = child_obj;
            }
            if(Model.root == NULL) {
                Model.root = temp_object;
            }
            for(i=0; i<data->childrenCount; i++) {
                DispObject(new_object, new_object->mesh.children[i]);
            }
        }
        break;

        case HSF_OBJ_NULL2:
        {
            HSFMESH *data;
            HSFOBJECT *new_object;
            data = &object->mesh;
            new_object = temp_object = object;
            new_object->mesh.parent = parent;
            new_object->mesh.childrenCount = data->childrenCount;
            new_object->mesh.children = (HSFOBJECT **)&NSymIndex[(uintptr_t)data->children];
            for(i=0; i<new_object->mesh.childrenCount; i++) {
                child_obj = &objtop[(uintptr_t)new_object->mesh.children[i]];
                new_object->mesh.children[i] = child_obj;
            }
            if(Model.root == NULL) {
                Model.root = temp_object;
            }
            for(i=0; i<data->childrenCount; i++) {
                DispObject(new_object, new_object->mesh.children[i]);
            }
        }
        break;

        case HSF_OBJ_MAP:
        {
            HSFMESH *data;
            HSFOBJECT *new_object;
            data = &object->mesh;
            new_object = temp_object = object;
            new_object->mesh.parent = parent;
            new_object->mesh.childrenCount = data->childrenCount;
            new_object->mesh.children = (HSFOBJECT **)&NSymIndex[(uintptr_t)data->children];
            for(i=0; i<new_object->mesh.childrenCount; i++) {
                child_obj = &objtop[(uintptr_t)new_object->mesh.children[i]];
                new_object->mesh.children[i] = child_obj;
            }
            if(Model.root == NULL) {
                Model.root = temp_object;
            }
            for(i=0; i<data->childrenCount; i++) {
                DispObject(new_object, new_object->mesh.children[i]);
            }
        }
        break;

        default:
            break;
    }
}

static inline void FixupObject(HSFOBJECT *object)
{
    HSFMESH *objdata_8;
    HSFMESH *objdata_7;

    s32 obj_type = object->type;
    switch(obj_type) {
        case 8:
        {
            objdata_8 = &object->mesh;
            object->type = HSF_OBJ_LIGHT;
        }
        break;

        case 7:
        {
            objdata_7 = &object->mesh;
            object->type = HSF_OBJ_CAMERA;
        }
        break;

        default:
            break;

    }
}

static void ObjectLoad(void)
{
    s32 i;
    HSFOBJECT *object;
    HSFOBJECT *new_object;
    s32 obj_type;

    if(head.object.count) {
#ifdef BYTESWAPPING
        HsfObject32b *file_object_real = (HsfObject32b *)((uintptr_t)fileptr + head.object.ofs);
        objtop = object = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFOBJECT) * head.object.count, MEMORY_DEFAULT_NUM);
        for (i = 0; i < head.object.count; i++) {
            byteswap_hsfobject(&file_object_real[i], &objtop[i]);
        }
#else
        objtop = object = (HSFOBJECT *)((u32)fileptr+head.object.ofs);
#endif
        for(i=0; i<head.object.count; i++, object++) {
            new_object = object;
            new_object->name = SetName((u32 *)&object->name);
        }
        object = objtop;
        for(i=0; i<head.object.count; i++, object++) {
            if((s32)(intptr_t)object->mesh.parent == -1) {
                break;
            }
        }
        DispObject(NULL, object);
        Model.objectNum = head.object.count;
        object = objtop;
        for(i=0; i<head.object.count; i++, object++) {
            FixupObject(object);
        }
    }
}

static void CenvLoad(void)
{
    HSFCENVMULTI *multi_file;
    HSFCENVMULTI *multi_new;
    HSFCENVSINGLE *single_new;
    HSFCENVSINGLE *single_file;
    HSFCENVDUAL *dual_file;
    HSFCENVDUAL *dual_new;

    HSFCENV *cenv_new;
    HSFCENV *cenv_file;
    void *data_base;
    void *weight_base;

    s32 j;
    s32 i;

    if(head.cenv.count) {
#ifdef BYTESWAPPING
        HsfCenv32b *file_cenv_real = (HsfCenv32b *)((uintptr_t)fileptr + head.cenv.ofs);
        cenv_file = CenvTop = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFCENV) * head.cenv.count, MEMORY_DEFAULT_NUM);
        for (i = 0; i < head.cenv.count; i++) {
            byteswap_hsfcenv(&file_cenv_real[i], &cenv_file[i]);
        }
        data_base = &file_cenv_real[head.cenv.count];
#else
        cenv_file = (HSFCENV *)((u32)fileptr+head.cenv.ofs);
        data_base = &cenv_file[head.cenv.count];
#endif
        weight_base = data_base;
        cenv_new = cenv_file;
        Model.cenvNum = head.cenv.count;
        Model.cenv = cenv_file;
        for(i=0; i<head.cenv.count; i++) {
            cenv_new[i].singleData = (HSFCENVSINGLE *)((uintptr_t)cenv_file[i].singleData + (uintptr_t)data_base);
#ifndef BYTESWAPPING
            cenv_new[i].dualData = (HSFCENVDUAL *)((uintptr_t)cenv_file[i].dualData + (uintptr_t)data_base);
            cenv_new[i].multiData = (HSFCENVMULTI *)((uintptr_t)cenv_file[i].multiData + (uintptr_t)data_base);
#endif
            cenv_new[i].singleCount = cenv_file[i].singleCount;
            cenv_new[i].dualCount = cenv_file[i].dualCount;
            cenv_new[i].multiCount = cenv_file[i].multiCount;
            cenv_new[i].copyCount = cenv_file[i].copyCount;
            cenv_new[i].vtxCount = cenv_file[i].vtxCount;
#if BYTESWAPPING
            weight_base = (void *)((uintptr_t)weight_base + (cenv_new[i].singleCount * sizeof(HSFCENVSINGLE)));
            weight_base = (void *)((uintptr_t)weight_base + (cenv_new[i].dualCount * sizeof(HsfCenvDual32b)));
            weight_base = (void *)((uintptr_t)weight_base + (cenv_new[i].multiCount * sizeof(HsfCenvMulti32b)));
#else
            weight_base = (void *)((uintptr_t)weight_base + (cenv_new[i].singleCount * sizeof(HSFCENVSINGLE)));
            weight_base = (void *)((uintptr_t)weight_base + (cenv_new[i].dualCount * sizeof(HSFCENVDUAL)));
            weight_base = (void *)((uintptr_t)weight_base + (cenv_new[i].multiCount * sizeof(HSFCENVMULTI)));
#endif
        }
        for(i=0; i<head.cenv.count; i++) {
#ifdef BYTESWAPPING
            HsfCenvDual32b *dual_data_real = (HsfCenvDual32b *)((uintptr_t)cenv_file[i].dualData + (uintptr_t)data_base);
            HsfCenvMulti32b *multi_data_real = (HsfCenvMulti32b *)((uintptr_t)cenv_file[i].multiData + (uintptr_t)data_base);
            cenv_new[i].dualData = HuMemDirectMallocNum(HEAP_DATA, cenv_file[i].dualCount * sizeof(HSFCENVDUAL), MEMORY_DEFAULT_NUM);
            cenv_new[i].multiData = HuMemDirectMallocNum(HEAP_DATA, cenv_file[i].multiCount * sizeof(HSFCENVMULTI), MEMORY_DEFAULT_NUM);
#endif
            single_new = single_file = cenv_new[i].singleData;
            for(j=0; j<cenv_new[i].singleCount; j++) {
#ifdef BYTESWAPPING
                byteswap_hsfcenv_single(&single_new[j]);
#endif
                single_new[j].target = single_file[j].target;
                single_new[j].posNum = single_file[j].posNum;
                single_new[j].pos = single_file[j].pos;
                single_new[j].normalNum = single_file[j].normalNum;
                single_new[j].normal = single_file[j].normal;
            }
            dual_new = dual_file = cenv_new[i].dualData;
            for(j=0; j<cenv_new[i].dualCount; j++) {
#ifdef BYTESWAPPING
                s32 k;
                byteswap_hsfcenv_dual(&dual_data_real[j], &dual_new[j]);
#endif
                dual_new[j].target1 = dual_file[j].target1;
                dual_new[j].target2 = dual_file[j].target2;
                dual_new[j].weightNum = dual_file[j].weightNum;
                dual_new[j].weight = (HSFCENVDUALWEIGHT *)((uintptr_t)weight_base + (uintptr_t)dual_file[j].weight);
#ifdef BYTESWAPPING
                for (k = 0; k < dual_new[j].weightNum; k++) {
                    byteswap_hsfcenv_dual_weight(&dual_new[j].weight[k]);
                }
#endif
            }
            multi_new = multi_file = cenv_new[i].multiData;
            for(j=0; j<cenv_new[i].multiCount; j++) {
#ifdef BYTESWAPPING
                s32 k;
                byteswap_hsfcenv_multi(&multi_data_real[j], &multi_new[j]);
#endif
                multi_new[j].weightNum = multi_file[j].weightNum;
                multi_new[j].pos = multi_file[j].pos;
                multi_new[j].posNum = multi_file[j].posNum;
                multi_new[j].normal = multi_file[j].normal;
                multi_new[j].normalNum = multi_file[j].normalNum;
                multi_new[j].weight = (HSFCENVMULTIWEIGHT *)((uintptr_t)weight_base + (uintptr_t)multi_file[j].weight);
#ifdef BYTESWAPPING
                for (k = 0; k < multi_new[j].weightNum; k++) {
                    byteswap_hsfcenv_multi_weight(&multi_new[j].weight[k]);
                }
#endif
            }
            dual_new = dual_file = cenv_new[i].dualData;
            for(j=0; j<cenv_new[i].dualCount; j++) {
                HSFCENVDUALWEIGHT *discard = dual_new[j].weight;
            }
            multi_new = multi_file = cenv_new[i].multiData;
            for(j=0; j<cenv_new[i].multiCount; j++) {
                HSFCENVMULTIWEIGHT *weight = multi_new[j].weight;
                s32 k;
                for(k=0; k<multi_new[j].weightNum; k++, weight++);
            }
        }
    }
}

static void SkeletonLoad(void)
{
    HSFSKELETON *skeleton_file;
    HSFSKELETON *skeleton_new;
    s32 i;

    if(head.skeleton.count) {
#ifdef BYTESWAPPING
        HsfSkeleton32b *file_skeleton_real = (HsfSkeleton32b *)((uintptr_t)fileptr + head.skeleton.ofs);
        skeleton_new = skeleton_file = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFSKELETON) * head.skeleton.count, MEMORY_DEFAULT_NUM);
        for (i = 0; i < head.skeleton.count; i++) {
            byteswap_hsfskeleton(&file_skeleton_real[i], &skeleton_file[i]);
        }
#else
        skeleton_new = skeleton_file = (HSFSKELETON *)((u32)fileptr+head.skeleton.ofs);
#endif
        Model.skeletonNum = head.skeleton.count;
        Model.skeleton = skeleton_file;
        for(i=0; i<head.skeleton.count; i++) {
            skeleton_new[i].name = SetName((u32 *)&skeleton_file[i].name);
            skeleton_new[i].transform.pos.x = skeleton_file[i].transform.pos.x;
            skeleton_new[i].transform.pos.y = skeleton_file[i].transform.pos.y;
            skeleton_new[i].transform.pos.z = skeleton_file[i].transform.pos.z;
            skeleton_new[i].transform.rot.x = skeleton_file[i].transform.rot.x;
            skeleton_new[i].transform.rot.y = skeleton_file[i].transform.rot.y;
            skeleton_new[i].transform.rot.z = skeleton_file[i].transform.rot.z;
            skeleton_new[i].transform.scale.x = skeleton_file[i].transform.scale.x;
            skeleton_new[i].transform.scale.y = skeleton_file[i].transform.scale.y;
            skeleton_new[i].transform.scale.z = skeleton_file[i].transform.scale.z;
        }
    }
}

static void PartLoad(void)
{
    HSFPART *part_file;
    HSFPART *part_new;

    u16 *data;
    s32 i, j;

    if(head.part.count) {
#ifdef BYTESWAPPING
        HsfPart32b *file_part_real = (HsfPart32b *)((uintptr_t)fileptr + head.part.ofs);
        part_new = part_file = PartTop = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFPART) * head.part.count, MEMORY_DEFAULT_NUM);
        for (i = 0; i < head.part.count; i++) {
            byteswap_hsfpart(&file_part_real[i], &part_file[i]);
        }
#else
        part_new = part_file = (HSFPART *)((u32)fileptr+head.part.ofs);
#endif
        Model.partNum = head.part.count;
        Model.part = part_file;
#ifdef BYTESWAPPING
        data = (u16 *)&file_part_real[head.part.count];
#else
        data = (u16 *)&part_file[head.part.count];
#endif
        for(i=0; i<head.part.count; i++, part_new++) {
            part_new->name = SetName((u32 *)&part_file[i].name);
            part_new->num = part_file[i].num;
            part_new->vertex = &data[(uintptr_t)part_file[i].vertex];
            for(j=0; j<part_new->num; j++) {
                part_new->vertex[j] = part_new->vertex[j];
#ifdef BYTESWAPPING
                byteswap_u16(&part_new->vertex[j]);
#endif
            }
        }
    }
}

static void ClusterLoad(void)
{
    HSFCLUSTER *cluster_file;
    HSFCLUSTER *cluster_new;

    s32 i, j;

    if(head.cluster.count) {
#ifdef BYTESWAPPING
        cluster_new = cluster_file = ClusterTop;
#else
        cluster_new = cluster_file = (HSFCLUSTER *)((u32)fileptr+head.cluster.ofs);
#endif
        Model.clusterNum = head.cluster.count;
        Model.cluster = cluster_file;
        for(i=0; i<head.cluster.count; i++) {
            HSFBUFFER *vertex;
            u32 vertexSym;
            cluster_new[i].name[0] = SetName((u32 *)&cluster_file[i].name[0]);
            cluster_new[i].name[1] = SetName((u32 *)&cluster_file[i].name[1]);
            cluster_new[i].targetName = SetName((u32 *)&cluster_file[i].targetName);
            cluster_new[i].part = SearchPartPtr((s32)(intptr_t)cluster_file[i].part);
            cluster_new[i].unk95 = cluster_file[i].unk95;
            cluster_new[i].type = cluster_file[i].type;
            cluster_new[i].vertexNum = cluster_file[i].vertexNum;
            vertexSym = (uintptr_t)cluster_file[i].vertex;
            cluster_new[i].vertex = (HSFBUFFER **)&NSymIndex[vertexSym];
            for(j=0; j<cluster_new[i].vertexNum; j++) {
                vertex = SearchVertexPtr((s32)(intptr_t)cluster_new[i].vertex[j]);
                cluster_new[i].vertex[j] = vertex;
            }
        }
    }
}

static void ShapeLoad(void)
{
    s32 i, j;
    HSFSHAPE *shape_new;
    HSFSHAPE *shape_file;

    if(head.shape.count) {
#ifdef BYTESWAPPING
        HsfShape32b *file_shape_real = (HsfShape32b *)((uintptr_t)fileptr + head.shape.ofs);
        shape_new = shape_file = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFSHAPE) * head.shape.count, MEMORY_DEFAULT_NUM);
        for (i = 0; i < head.shape.count; i++) {
            byteswap_hsfshape(&file_shape_real[i], &shape_file[i]);
        }
#else
        shape_new = shape_file = (HSFSHAPE *)((u32)fileptr+head.shape.ofs);
#endif
        Model.shapeNum = head.shape.count;
        Model.shape = shape_file;
        for(i=0; i<Model.shapeNum; i++) {
            u32 vertexSym;
            HSFBUFFER *vertex;

            shape_new[i].name = SetName((u32 *)&shape_file[i].name);
            shape_new[i].num16[0] = shape_file[i].num16[0];
            shape_new[i].num16[1] = shape_file[i].num16[1];
            vertexSym = (uintptr_t)shape_file[i].vertex;
            shape_new[i].vertex = (HSFBUFFER **)&NSymIndex[vertexSym];
            for(j=0; j<shape_new[i].num16[1]; j++) {
                vertex = &vtxtop[(uintptr_t)shape_new[i].vertex[j]];
                shape_new[i].vertex[j] = vertex;
            }
        }
    }
}

static void MapAttrLoad(void)
{
    s32 i;
    HSFMAPATTR *mapattr_base;
    HSFMAPATTR *mapattr_file;
    HSFMAPATTR *mapattr_new;
    u16 *data;

    if(head.mapAttr.count) {
#ifdef BYTESWAPPING
        HsfMapAttr32b *file_mapattr_real = (HsfMapAttr32b *)((uintptr_t)fileptr + head.mapAttr.ofs);
        mapattr_file = mapattr_base = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFMAPATTR) * head.mapAttr.count, MEMORY_DEFAULT_NUM);
        for (i = 0; i < head.mapAttr.count; i++) {
            byteswap_hsfmapattr(&file_mapattr_real[i], &mapattr_base[i]);
        }
#else
        mapattr_file = mapattr_base = (HSFMAPATTR *)((u32)fileptr+head.mapAttr.ofs);
#endif
        mapattr_new = mapattr_base;
        Model.mapAttrNum = head.mapAttr.count;
        Model.mapAttr = mapattr_base;
#ifdef BYTESWAPPING
        data = (u16 *)&file_mapattr_real[head.mapAttr.count];
#else
        data = (u16 *)&mapattr_base[head.mapAttr.count];
#endif
        for(i=0; i<head.mapAttr.count; i++, mapattr_file++, mapattr_new++) {
            mapattr_new->data = &data[(uintptr_t)mapattr_file->data];
#ifdef BYTESWAPPING
            for (int j = 0; j < mapattr_new->dataLen; j++) {
                byteswap_u16(&mapattr_new->data[j]);
            }
#endif
        }
    }
}

static void BitmapLoad(void)
{
    HSFBITMAP *bitmap_file;
    HSFBITMAP *bitmap_temp;
    HSFBITMAP *bitmap_new;
    HSFPALETTE *palette;
    void *data;
    s32 i;

    if(head.bitmap.count) {
#ifdef BYTESWAPPING
        HsfBitmap32b *file_bitmap_real = (HsfBitmap32b *)((uintptr_t)fileptr + head.bitmap.ofs);
        bitmap_temp = bitmap_file = BitmapTop = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFBITMAP) * head.bitmap.count, MEMORY_DEFAULT_NUM);
        for (i = 0; i < head.bitmap.count; i++) {
            byteswap_hsfbitmap(&file_bitmap_real[i], &bitmap_file[i]);
        }
#else
        bitmap_temp = bitmap_file = (HSFBITMAP *)((u32)fileptr+head.bitmap.ofs);
        data = &bitmap_file[head.bitmap.count];
        for(i=0; i<head.bitmap.count; i++, bitmap_file++);
#endif
        bitmap_new = bitmap_temp;
        Model.bitmap = bitmap_file;
        Model.bitmapNum = head.bitmap.count;
#ifdef BYTESWAPPING
        data = (void *)&file_bitmap_real[head.bitmap.count];
#else
        bitmap_file = (HSFBITMAP *)((u32)fileptr+head.bitmap.ofs);
        data = &bitmap_file[head.bitmap.count];
#endif
        for(i=0; i<head.bitmap.count; i++, bitmap_file++, bitmap_new++) {
            bitmap_new->name = SetName((u32 *)&bitmap_file->name);
            bitmap_new->dataFmt = bitmap_file->dataFmt;
            bitmap_new->pixSize = bitmap_file->pixSize;
            bitmap_new->sizeX = bitmap_file->sizeX;
            bitmap_new->sizeY = bitmap_file->sizeY;
            bitmap_new->palSize = bitmap_file->palSize;
            palette = SearchPalettePtr((s32)(intptr_t)bitmap_file->palData);
            if(palette) {
                bitmap_new->palData = palette->data;
            }
            bitmap_new->data = (void *)((uintptr_t)data+(uintptr_t)bitmap_file->data);
        }
    }
}

static void PaletteLoad(void)
{
    s32 i;
    s32 j;
    HSFPALETTE *palette_file;
    HSFPALETTE *palette_temp;
    HSFPALETTE *palette_new;

    void *data_base;
    u16 *temp_data;
    u16 *data;

    if(head.palette.count) {
#ifdef BYTESWAPPING
        HsfPalette32b *file_palette_real = (HsfPalette32b *)((uintptr_t)fileptr + head.palette.ofs);
        palette_temp = palette_file = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFPALETTE) * head.palette.count, MEMORY_DEFAULT_NUM);
        for (i = 0; i < head.palette.count; i++) {
            byteswap_hsfpalette(&file_palette_real[i], &palette_file[i]);
        }
#else
        palette_temp = palette_file = (HSFPALETTE *)((u32)fileptr+head.palette.ofs);
        data_base = (u16 *)&palette_file[head.palette.count];
        for(i=0; i<head.palette.count; i++, palette_file++) {
            temp_data = (u16 *)((uintptr_t)data_base+(uintptr_t)palette_file->data);
        }
#endif
        Model.palette = palette_temp;
        Model.paletteNum = head.palette.count;
        palette_new = palette_temp;
#ifdef BYTESWAPPING
        data_base = (u16 *)&file_palette_real[head.palette.count];
#else
        palette_file = (HSFPALETTE *)((u32)fileptr+head.palette.ofs);
        data_base = (u16 *)&palette_file[head.palette.count];
#endif
        for(i=0; i<head.palette.count; i++, palette_file++, palette_new++) {
            temp_data = (u16 *)((uintptr_t)data_base+(uintptr_t)palette_file->data);
            data = temp_data;
            palette_new->name = SetName((u32 *)&palette_file->name);
            palette_new->data = data;
            palette_new->palSize = palette_file->palSize;
            for(j=0; j<palette_file->palSize; j++) {
                data[j] = data[j];
// #ifdef BYTESWAPPING
                // byteswap_u16(&data[j]);
// #endif
            }
        }
    }
}

char *MakeObjectName(char *name)
{
    static char buf[768];
    s32 index, num_minus;
    char *temp_name;
    num_minus = 0;
    index = 0;
    temp_name = name;
    while(*temp_name) {
        if(*temp_name == '-') {
            name = temp_name+1;
            break;
        }
        temp_name++;
    }
    while(*name) {
        if(num_minus != 0) {
            break;
        }
        if(*name == '_' && !isalpha(name[1])) {
            num_minus++;
            break;
        }
        buf[index] = *name;
        name++;
        index++;
    }
    buf[index] = '\0';
    return buf;
}

s32 CmpObjectName(char *name1, char *name2)
{
    s32 temp = 0;
    return strcmp(name1, name2);
}

static inline char *MotionGetName(HSFTRACK *track)
{
    char *ret;
    if(DicStringTable) {
        ret = &DicStringTable[track->target];
    } else {
        ret = GetMotionString(&track->target);
    }
    return ret;
}

static inline s32 FindObjectName(char *name)
{
    s32 i;
    HSFOBJECT *object;

    object = objtop;
    for(i=0; i<head.object.count; i++, object++) {
        if(!CmpObjectName(object->name, name)) {
            return i;
        }
    }
    return -1;
}

static inline s32 FindClusterName(char *name)
{
    s32 i;
    HSFCLUSTER *cluster;

    cluster = ClusterTop;
    for(i=0; i<head.cluster.count; i++, cluster++) {
        if(!strcmp(cluster->name[0], name)) {
            return i;
        }
    }
    return -1;
}

static inline s32 FindMotionClusterName(char *name)
{
    s32 i;
    HSFCLUSTER *cluster;

    cluster = MotionModel->cluster;
    for(i=0; i<MotionModel->clusterNum; i++, cluster++) {
        if(!strcmp(cluster->name[0], name)) {
            return i;
        }
    }
    return -1;
}

static inline s32 FindAttributeName(char *name)
{
    s32 i;
    HSFATTRIBUTE *attribute;

    attribute = AttributeTop;
    for(i=0; i<head.attribute.count; i++, attribute++) {
        if(!attribute->name) {
            continue;
        }
        if(!strcmp(attribute->name, name)) {
            return i;
        }
    }
    return -1;
}

static inline s32 FindMotionAttributeName(char *name)
{
    s32 i;
    HSFATTRIBUTE *attribute;

    attribute = MotionModel->attribute;
    for(i=0; i<MotionModel->attributeNum; i++, attribute++) {
        if(!attribute->name) {
            continue;
        }
        if(!strcmp(attribute->name, name)) {
            return i;
        }
    }
    return -1;
}

#ifdef BYTESWAPPING
static void ByteSwapCurveStepOrLinearData(HSFTRACK *track)
{
    int i;
    float *data = track->data;
    for (i = 0; i < track->numKeyframes; i++) {
        byteswap_float(data++);
        byteswap_float(data++);
    }
}

static void ByteSwapCurveBitmapData(HSFTRACK *track)
{
    int i;
    float *data = track->data;
    for (i = 0; i < track->numKeyframes; i++) {
        byteswap_float(data++);
        byteswap_s32((s32*) data++);
    }
}

static void ByteSwapCurveBezierData(HSFTRACK *track)
{
    int i;
    float *data = track->data;
    for (i = 0; i < track->numKeyframes; i++) {
        byteswap_float(data++);
        byteswap_float(data++);
        byteswap_float(data++);
        byteswap_float(data++);
    }
}
#endif

static inline void MotionLoadTransform(HSFTRACK *track, void *data)
{
    float *step_data;
    float *linear_data;
    float *bezier_data;
    HSFTRACK *out_track;
    char *name;
    s32 numKeyframes;
    out_track = track;
    name = MotionGetName(track);
    if(objtop) {
        out_track->target = FindObjectName(name);
    }
    numKeyframes = AS_S16(track->numKeyframes);
    switch(track->curveType) {
        case HSF_CURVE_STEP:
        {
            step_data = (float *)((uintptr_t)data + (uintptr_t)track->data);
            out_track->data = step_data;
#ifdef BYTESWAPPING
            ByteSwapCurveStepOrLinearData(out_track);
#endif
        }
        break;

        case HSF_CURVE_LINEAR:
        {
            linear_data = (float *)((uintptr_t)data + (uintptr_t)track->data);
            out_track->data = linear_data;
#ifdef BYTESWAPPING
            ByteSwapCurveStepOrLinearData(out_track);
#endif
        }
        break;

        case HSF_CURVE_BEZIER:
        {
            bezier_data = (float *)((uintptr_t)data + (uintptr_t)track->data);
            out_track->data = bezier_data;
#ifdef BYTESWAPPING
            ByteSwapCurveBezierData(out_track);
#endif
        }
        break;

        case HSF_CURVE_CONST:
            break;
    }
}

static inline void MotionLoadCluster(HSFTRACK *track, void *data)
{
    s32 numKeyframes;
    float *step_data;
    float *linear_data;
    float *bezier_data;
    HSFTRACK *out_track;
    char *name;

    out_track = track;
    name = SetMotionName(&track->target);
    if(!MotionOnly) {
        AS_S16(out_track->target) = FindClusterName(name);
    } else {
        AS_S16(out_track->target) = FindMotionClusterName(name);
    }
    numKeyframes = AS_S16(track->numKeyframes);
    (void)out_track;
    switch(track->curveType) {
        case HSF_CURVE_STEP:
        {
            step_data = (float *)((uintptr_t)data + (uintptr_t)track->data);
            out_track->data = step_data;
#ifdef BYTESWAPPING
            ByteSwapCurveStepOrLinearData(out_track);
#endif
        }
        break;

        case HSF_CURVE_LINEAR:
        {
            linear_data = (float *)((uintptr_t)data + (uintptr_t)track->data);
            out_track->data = linear_data;
#ifdef BYTESWAPPING
            ByteSwapCurveStepOrLinearData(out_track);
#endif
        }
        break;

        case HSF_CURVE_BEZIER:
        {
            bezier_data = (float *)((uintptr_t)data + (uintptr_t)track->data);
            out_track->data = bezier_data;
#ifdef BYTESWAPPING
            ByteSwapCurveBezierData(out_track);
#endif
        }
        break;

        case HSF_CURVE_CONST:
            break;
    }
}

static inline void MotionLoadClusterWeight(HSFTRACK *track, void *data)
{
    s32 numKeyframes;
    float *step_data;
    float *linear_data;
    float *bezier_data;
    HSFTRACK *out_track;
    char *name;

    out_track = track;
    name = SetMotionName(&track->target);
    if(!MotionOnly) {
        AS_S16(out_track->target) = FindClusterName(name);
    } else {
        AS_S16(out_track->target) = FindMotionClusterName(name);
    }
    numKeyframes = AS_S16(track->numKeyframes);
    (void)out_track;
    switch(track->curveType) {
        case HSF_CURVE_STEP:
        {
            step_data = (float *)((uintptr_t)data + (uintptr_t)track->data);
            out_track->data = step_data;
#ifdef BYTESWAPPING
            ByteSwapCurveStepOrLinearData(out_track);
#endif
        }
        break;

        case HSF_CURVE_LINEAR:
        {
            linear_data = (float *)((uintptr_t)data + (uintptr_t)track->data);
            out_track->data = linear_data;
#ifdef BYTESWAPPING
            ByteSwapCurveStepOrLinearData(out_track);
#endif
        }
        break;

        case HSF_CURVE_BEZIER:
        {
            bezier_data = (float *)((uintptr_t)data + (uintptr_t)track->data);
            out_track->data = bezier_data;
#ifdef BYTESWAPPING
            ByteSwapCurveBezierData(out_track);
#endif
        }
        break;

        case HSF_CURVE_CONST:
            break;
    }
}

static inline void MotionLoadMaterial(HSFTRACK *track, void *data)
{
    float *step_data;
    float *linear_data;
    float *bezier_data;
    s32 numKeyframes;
    HSFTRACK *out_track;
    out_track = track;
    numKeyframes = AS_S16(track->numKeyframes);
    switch(track->curveType) {
        case HSF_CURVE_STEP:
        {
            step_data = (float *)((uintptr_t)data + (uintptr_t)track->data);
            out_track->data = step_data;
#ifdef BYTESWAPPING
            ByteSwapCurveStepOrLinearData(out_track);
#endif
        }
        break;

        case HSF_CURVE_LINEAR:
        {
            linear_data = (float *)((uintptr_t)data + (uintptr_t)track->data);
            out_track->data = linear_data;
#ifdef BYTESWAPPING
            ByteSwapCurveStepOrLinearData(out_track);
#endif
        }
        break;

        case HSF_CURVE_BEZIER:
        {
            bezier_data = (float *)((uintptr_t)data + (uintptr_t)track->data);
            out_track->data = bezier_data;
#ifdef BYTESWAPPING
            ByteSwapCurveBezierData(out_track);
#endif
        }
        break;

        case HSF_CURVE_CONST:
            break;
    }
}

static inline void MotionLoadAttribute(HSFTRACK *track, void *data)
{
    HSFBITMAPKEY *file_frame;
    HSFBITMAPKEY *new_frame;
    s32 i;
    float *step_data;
    float *linear_data;
    float *bezier_data;
    HSFTRACK *out_track;
    char *name;
    out_track = track;
    if(AS_S16(out_track->target) != -1) {
        name = SetMotionName(&track->target);
        if(!MotionOnly) {
            AS_S16(out_track->attrIdx) = FindAttributeName(name);
        } else {
            AS_S16(out_track->attrIdx) = FindMotionAttributeName(name);
        }
    }

    switch(track->curveType) {
        case HSF_CURVE_STEP:
        {
            step_data = (float *)((uintptr_t)data + (uintptr_t)track->data);
            out_track->data = step_data;
#ifdef BYTESWAPPING
            ByteSwapCurveStepOrLinearData(out_track);
#endif
        }
        break;

        case HSF_CURVE_LINEAR:
        {
            linear_data = (float *)((uintptr_t)data + (uintptr_t)track->data);
            out_track->data = linear_data;
#ifdef BYTESWAPPING
            ByteSwapCurveStepOrLinearData(out_track);
#endif
        }
        break;

        case HSF_CURVE_BEZIER:
        {
            bezier_data = (float *)((uintptr_t)data + (uintptr_t)track->data);
            out_track->data = bezier_data;
#ifdef BYTESWAPPING
            ByteSwapCurveBezierData(out_track);
#endif
        }
        break;

        case HSF_CURVE_BITMAP:
        {
#ifdef BYTESWAPPING
            HsfBitmapKey32b *file_frame_real = (HsfBitmapKey32b *)((uintptr_t)data + (uintptr_t)track->data);
            new_frame = file_frame = track->dataTop = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFBITMAPKEY) * track->numKeyframes, MEMORY_DEFAULT_NUM);
#else
            new_frame = file_frame = (HSFBITMAPKEY *)((uintptr_t)data + (uintptr_t)track->data);
#endif
            out_track->data = file_frame;
            for(i=0; i<out_track->numKeyframes; i++, file_frame++, new_frame++) {
#ifdef BYTESWAPPING
                byteswap_hsfbitmapkey(&file_frame_real[i], new_frame);
#endif
                new_frame->data = SearchBitmapPtr((s32)(intptr_t)file_frame->data);
            }
        }
        break;
        case HSF_CURVE_CONST:
            break;
    }
}

static void MotionLoad(void)
{
    HSFMOTION *file_motion;
    HSFMOTION *temp_motion;
    HSFMOTION *new_motion;
    HSFTRACK *track_base;
    void *track_data;
    s32 i;

    MotionOnly = FALSE;
    MotionModel = NULL;
    if(head.motion.count) {
#ifdef BYTESWAPPING
        HsfMotion32b *file_motion_real = (HsfMotion32b *)((uintptr_t)fileptr + head.motion.ofs);
        temp_motion = file_motion = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFMOTION) * head.motion.count, MEMORY_DEFAULT_NUM);
        for (i = 0; i < head.motion.count; i++) {
            byteswap_hsfmotion(&file_motion_real[i], &file_motion[i]);
        }
#else
        temp_motion = file_motion = (HSFMOTION *)((uintptr_t)fileptr+head.motion.ofs);
#endif
        new_motion = temp_motion;
        Model.motion = new_motion;
        Model.motionNum = file_motion->numTracks;
#ifdef BYTESWAPPING
        {
            HsfTrack32b *track_base_real = (HsfTrack32b *)&file_motion_real[head.motion.count];
            track_base = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFTRACK) * file_motion->numTracks, MEMORY_DEFAULT_NUM);
            track_data = &track_base_real[file_motion->numTracks];
            for (i = 0; i < file_motion->numTracks; i++) {
                byteswap_hsftrack(&track_base_real[i], &track_base[i]);
            }
        }
#else
        track_base = (HSFTRACK *)&file_motion[head.motion.count];
        track_data = &track_base[file_motion->numTracks];
#endif
        new_motion->track = track_base;
        for(i=0; i<(s32)file_motion->numTracks; i++) {
            switch(track_base[i].type) {
                case HSF_TRACK_TRANSFORM:
                case HSF_TRACK_MORPH:
                    MotionLoadTransform(&track_base[i], track_data);
                    break;

                case HSF_TRACK_CLUSTER:
                    MotionLoadCluster(&track_base[i], track_data);
                    break;

                case HSF_TRACK_CLUSTER_WEIGHT:
                    MotionLoadClusterWeight(&track_base[i], track_data);
                    break;

                case HSF_TRACK_MATERIAL:
                    MotionLoadMaterial(&track_base[i], track_data);
                    break;

                case HSF_TRACK_ATTRIBUTE:
                    MotionLoadAttribute(&track_base[i], track_data);
                    break;

                default:
                    break;
            }
        }
    }
    //HACK: Bump register of i to r31
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
}

static void MatrixLoad(void)
{
    HSFMATRIX *matrix_file;

    if(head.matrix.count) {
#ifdef BYTESWAPPING
        if (head.matrix.count > 1) {
            OSReport("MORE THAN ONE MATRIX, FIX PARSING!\n");
        }
        matrix_file = HuMemDirectMallocNum(HEAP_DATA, sizeof(HSFMATRIX) * head.matrix.count, MEMORY_DEFAULT_NUM);
        byteswap_hsfmatrix((HsfMatrix32b *)((uintptr_t)fileptr + head.matrix.ofs), matrix_file);
#else
        matrix_file = (HSFMATRIX *)((uintptr_t)fileptr+head.matrix.ofs);
        matrix_file->data = (Mtx *)((u32)fileptr+head.matrix.ofs+sizeof(HSFMATRIX));
#endif
        Model.matrix = matrix_file;
        Model.matrixNum = head.matrix.count;
    }
}

static s32 SearchObjectSetName(HSFDATA *data, char *name)
{
    HSFOBJECT *object = data->object;
    s32 i;
    for(i=0; i<data->objectNum; i++, object++) {
        if(!CmpObjectName(object->name, name)) {
            return i;
        }
    }
    OSReport("Search Object Error %s\n", name);
    return -1;
}

static HSFBUFFER *SearchVertexPtr(s32 id)
{
    HSFBUFFER *vertex; 
    if(id == -1) {
        return NULL;
    }
#ifdef BYTESWAPPING
    vertex = vtxtop;
#else
    vertex = (HSFBUFFER *)((uintptr_t)fileptr+head.vertex.ofs);
#endif
    vertex += id;
    return vertex;
}

static HSFBUFFER *SearchNormalPtr(s32 id)
{
    HSFBUFFER *normal; 
    if(id == -1) {
        return NULL;
    }
#ifdef BYTESWAPPING
    normal = NormalTop;
#else
    normal = (HSFBUFFER *)((uintptr_t)fileptr+head.normal.ofs);
#endif
    normal += id;
    return normal;
}

static HSFBUFFER *SearchStPtr(s32 id)
{
    HSFBUFFER *st; 
    if(id == -1) {
        return NULL;
    }
#ifdef BYTESWAPPING
    st = StTop;
#else
    st = (HSFBUFFER *)((uintptr_t)fileptr+head.st.ofs);
#endif
    st += id;
    return st;
}

static HSFBUFFER *SearchColorPtr(s32 id)
{
    HSFBUFFER *color; 
    if(id == -1) {
        return NULL;
    }
#ifdef BYTESWAPPING
    color = ColorTop;
#else
    color = (HSFBUFFER *)((uintptr_t)fileptr+head.color.ofs);
#endif
    color += id;
    return color;
}

static HSFBUFFER *SearchFacePtr(s32 id)
{
    HSFBUFFER *face; 
    if(id == -1) {
        return NULL;
    }
#ifdef BYTESWAPPING
    face = FaceTop;
#else
    face = (HSFBUFFER *)((uintptr_t)fileptr+head.face.ofs);
#endif
    face += id;
    return face;
}

static HSFCENV *SearchCenvPtr(s32 id)
{
    HSFCENV *cenv; 
    if(id == -1) {
        return NULL;
    }
#ifdef BYTESWAPPING
    cenv = CenvTop;
#else
    cenv = (HSFCENV *)((uintptr_t)fileptr + head.cenv.ofs);
#endif
    cenv += id;
    return cenv;
}

static HSFPART *SearchPartPtr(s32 id)
{
    HSFPART *part;
    if(id == -1) {
        return NULL;
    }
#ifdef BYTESWAPPING
    part = PartTop;
#else
    part = (HSFPART *)((uintptr_t)fileptr+head.part.ofs);
#endif
    part += id;
    return part;
}

static HSFPALETTE *SearchPalettePtr(s32 id)
{
    HSFPALETTE *palette; 
    if(id == -1) {
        return NULL;
    }
    palette = Model.palette;
    palette += id;
    return palette;
}

static HSFBITMAP *SearchBitmapPtr(s32 id)
{
    HSFBITMAP *bitmap; 
    if(id == -1) {
        return NULL;
    }
#ifdef BYTESWAPPING
    bitmap = BitmapTop;
#else
    bitmap = (HSFBITMAP *)((uintptr_t)fileptr+head.bitmap.ofs);
#endif
    bitmap += id;
    return bitmap;
}

static char *GetString(u32 *str_ofs)
{
    char *ret = &StringTable[*str_ofs];
    return ret;
}

static char *GetMotionString(u16 *str_ofs)
{
    char *ret = &StringTable[*str_ofs];
    return ret;
}


#ifdef TARGET_PC
void KillHSF(HSFDATA *data)
{
    // TODO PC
    s32 i, j;
    // if (data->attributeNum)
    //     HuMemDirectFree(data->attribute);
    // if (data->bitmapNum)
    //     HuMemDirectFree(data->bitmap);
    // if (data->skeletonNum)
    //     HuMemDirectFree(data->skeleton);
    // if (data->faceNum) {
    //     for (i = 0; i < data->faceNum; i++) {
    //         HuMemDirectFree(data->face[i].data);
    //     }
    //     HuMemDirectFree(data->face);
    // }
    // if (data->materialNum)
    //     HuMemDirectFree(data->material);
    // if (data->motionNum) {
    //     HSFMOTION *motion = data->motion;
    //     for (j = 0; j < motion->numTracks; j++) {
    //         HSFTRACK *track = motion->track;
    //         if (track->type == HSF_TRACK_ATTRIBUTE && track->curveType == HSF_CURVE_BITMAP) {
    //             // in this case we needed to allocate space for HSFBITMAPKEY structs
    //             HuMemDirectFree(track->dataTop);
    //         }
    //     }
    //     HuMemDirectFree(motion->track);
    //     HuMemDirectFree(data->motion);
    // }
    // if (data->normalNum)
    //     HuMemDirectFree(data->normal);
    // if (data->objectNum)
    //     HuMemDirectFree(data->object);
    // if (data->matrixNum)
    //     HuMemDirectFree(data->matrix);
    // if (data->paletteNum)
    //     HuMemDirectFree(data->palette);
    // if (data->stNum)
    //     HuMemDirectFree(data->st);
    // if (data->vertexNum)
    //     HuMemDirectFree(data->vertex);
    // if (data->cenvNum) {
    //     for (i = 0; i < data->cenvNum; i++) {
    //         HSFCENV *cenv = &data->cenv[i];
    //         HuMemDirectFree(cenv->dualData);
    //         HuMemDirectFree(cenv->multiData);
    //     }
    //     HuMemDirectFree(data->cenv);
    // }
    // if (data->clusterNum)
    //     HuMemDirectFree(data->cluster);
    // if (data->partNum)
    //     HuMemDirectFree(data->part);
    // if (data->shapeNum)
    //     HuMemDirectFree(data->shape);
    // if (data->mapAttrNum)
    //     HuMemDirectFree(data->mapAttr);
    // HuMemDirectFree(data->symbol);
#ifdef OPTIMIZED_TEXTURE_LOADING
    for (i = 0; i < data->attributeNum; i++) {
        HSFATTRIBUTE *attr = &data->attribute[i];
        if (attr->tex_initialized) {
            GXDestroyTexObj(&attr->tex_obj);
        }
        if (attr->tex8000_initialized) {
            GXDestroyTexObj(&attr->tex8000_obj);
        }
        if (attr->tlut_initialized) {
            GXDestroyTlutObj(&attr->tlut_obj);
        }
        if (attr->tlut8000_initialized) {
            GXDestroyTlutObj(&attr->tlut8000_obj);
        }
        attr->tex_initialized = FALSE;
        attr->tex8000_initialized = FALSE;
        attr->tlut_initialized = FALSE;
        attr->tlut8000_initialized = FALSE;
    }
#endif
}
#endif
