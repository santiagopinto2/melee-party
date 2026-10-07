/* Melee Party, Mario Party 4 runtime: included ahead of every MP4 source here (CMakeLists.txt),
 * after mu_native.h. MP4's code is built as partyboard's PC port builds it (TARGET_PC, and
 * BYTESWAPPING for its big-endian files), against Melee's Dolphin SDK and its native GX. */
#ifndef MP4_PORT_H
#define MP4_PORT_H

#define TARGET_PC 1
#define BYTESWAPPING 1
#define OPTIMIZED_TEXTURE_LOADING 1
#define SHARED_SYM

#include <stdbool.h>

/* Melee's headers take TRUE and FALSE away (Runtime/platform.h); MP4's code uses them. */
#undef TRUE
#undef FALSE
enum { FALSE = 0, TRUE = 1 };

/* The SDK maths MP4 calls by names Melee's headers map elsewhere, or not at all: the C versions. */
#include <dolphin/mtx.h>
u32 C_MTXInvXpose(Mtx src, Mtx invX);
void C_MTXMultVecArray(Mtx m, Vec* srcBase, Vec* dstBase, u32 count);
void VECHalfAngle(Vec* a, Vec* b, Vec* half);
void mp4_mtx_reorder(Mtx src, ROMtx dest);
void mp4_mtx_ro_mult_vec_array(ROMtx m, Vec* srcBase, Vec* dstBase, u32 count);
#undef MTXMultVecArray
#undef MTXReorder
#undef MTXROMultVecArray
#define MTXInvXpose C_MTXInvXpose
#define PSMTXInvXpose C_MTXInvXpose
#define MTXMultVecArray C_MTXMultVecArray
#define MTXReorder mp4_mtx_reorder
#define MTXROMultVecArray mp4_mtx_ro_mult_vec_array
#define C_MTXPerspective MTXPerspective
#define C_MTXOrtho MTXOrtho
#define C_MTXLightPerspective MTXLightPerspective
#define C_VECHalfAngle VECHalfAngle
static inline void MTXRotAxisDeg(Mtx m, Vec* axis, f32 deg) { MTXRotAxisRad(m, axis, deg * 0.017453292519943295f); }
static inline void OSf32tos16(f32* f, s16* out) { *out = (s16) *f; }

/* Melee's C library has the float trigonometry only. */
float sinf(float);
float cosf(float);
float atan2f(float y, float x);
static inline double mp4_sin(double x) { return sinf((float) x); }
static inline double mp4_cos(double x) { return cosf((float) x); }
static inline double mp4_atan2(double y, double x) { return atan2f((float) y, (float) x); }
#define sin mp4_sin
#define cos mp4_cos
#define atan2 mp4_atan2

/* GX reads vertex arrays big-endian, as the console did; the PC port keeps them in host order. Each
 * array goes to GX through a big-endian copy made for the frame (mp4_gx.c). */
void mp4_gx_set_array(int attr, const void* data, unsigned int size, unsigned char stride, int host_order);
#define GXSETARRAY(attr, data, size, stride, le) mp4_gx_set_array((attr), (data), (size), (stride), (le))

/* The data cache: nothing to store back natively (shim/mu_cache.c keeps the SDK's others). */
#include <dolphin/os.h>
#define DCStoreRangeNoSync(addr, len) ((void) (addr), (void) (len))

/* Melee's SDK passes colour-indexed texture formats as plain ones. */
#include <dolphin/gx.h>
typedef GXTexFmt GXCITexFmt;

/* The PC port's renderer frees texture objects; GX here keeps none. */
#define GXDestroyTexObj(obj) ((void) (obj))
#define GXDestroyTlutObj(obj) ((void) (obj))

#endif
