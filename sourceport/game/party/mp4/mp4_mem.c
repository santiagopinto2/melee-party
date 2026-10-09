/* Melee Party, Mario Party 4 runtime: MP4's heaps (HuMem), from the MP4 decompilation
 * (github.com/mariopartyrd/partyboard src/game/memory.c and malloc.c, CC0), with its 64-bit block
 * headers.
 *
 * The heaps live in one pool in the game image, not in the console's main memory, where Melee's
 * own heaps take the whole arena. GX reads textures, vertex arrays and display lists from MP4
 * models in place, and GX can only address memory below 0x84000000: MEM1 and this image, which the
 * host loads right after it. The pool is zero-filled .bss, so it costs nothing until MP4 data is
 * loaded, which only happens with an MP4 disc. Rollback snapshots leave it out: the MP4 minigames
 * are offline only for now. */
#include <dolphin/os.h>
#include <string.h>

#include "game/memory.h"
#include "mp4.h"

/* MP4's own sizes were 2.25, 1.25, 10.5 and 5.5 MB, and what was left of 24 MB. Music is not
 * loaded here, the DVD heap only holds an archive record while it is unpacked (a bigger one goes to
 * the data heap), and the data heap holds 64-bit structures beside the files. The image (about
 * 10 MB) and the pool together must end below 0x84000000; mp4_mem_fits checks. */
/* MP4's sizes, except: the system heap is twice MP4's (its structures are bigger here, with
 * 64-bit pointers, and m438 keeps two effects per Chain Chomp lane in it, 48 lanes, which filled
 * 1 MB), and the data heap holds what MP4's 9 MB held plus the DVD, music and misc heaps' share
 * of the pool. Those three never hold anything GX reads (a compressed record while it unpacks, a
 * sound, a scrap), so they live on the C runtime's heap instead, outside the pool. Each loaded
 * model keeps its unpacked file and the 64-bit structures made from it, so a minigame that loads
 * four characters twice (m412: a player and its reflection on the ice) needs the room. The data
 * heap's entry is what it gets at most: see data_heap_size. */
static u32 HeapSizeTbl[HEAP_MAX] = { 0x200000, 0x10000, 0xAD0000, 0x100000, 0x40000 };
#define MP4_SYSTEM_SIZE 0x200000
#define MP4_DATA_SIZE 0xAD0000
#define MP4_POOL_SIZE (MP4_SYSTEM_SIZE + MP4_DATA_SIZE)   /* the system and data heaps: what GX reads */
/* After the heaps, the frame's big-endian copies of vertex arrays (mp4_gx.c): all the float arrays
 * the frame's models draw with. A frame that wants more than the scratch has says so in the log. */
#define MP4_GX_SCRATCH_SIZE 0x100000   /* a frame of m438 wants 283 KB, of m440 less */
#define GX_LIMIT 0x84000000u
/* The least data heap MP4 runs with: m412, the biggest measured, peaks at 8.6 MB. */
#define MP4_DATA_HEAP_MIN 0x900000
static u8 mp4_pool[MP4_POOL_SIZE + MP4_GX_SCRATCH_SIZE] __attribute__((aligned(64)));
void* malloc(size_t size);
static void *HeapTbl[HEAP_MAX];
static u32 data_used, data_peak;   /* the data heap's blocks in use, and the most so far */

#define MEM_ALLOC_SIZE(size) ((((size) - 1) / 32 + 1) * 32 + 64)
#define DATA_GET_BLOCK(ptr) ((struct memory_block *) (((char *) (ptr)) - 64))
#define BLOCK_GET_DATA(block) (((char *) (block)) + 64)
#define BLOCK_ALIGNMENT 64u

struct memory_block {
    s32 size;
    u8 magic;
    u8 flag;
    struct memory_block *prev;
    struct memory_block *next;
    uintptr_t num;
    uintptr_t retaddr;
};

static void *HuMemMemoryAlloc2(void *heap_ptr, size_t size, uintptr_t num, uintptr_t retaddr);
size_t HuMemUsedMemorySizeGet(void *heap_ptr);

/* The pool is placed by the linker like any other .bss, so it moves up with every byte of code
 * linked before it, and its end can pass GX_LIMIT. The data heap gives way: it is MP4_DATA_SIZE,
 * or what is left between the system heap and a full scratch below the limit. */
static u32 data_heap_size(void)
{
    uintptr_t data = (uintptr_t) mp4_pool + MP4_SYSTEM_SIZE;
    uintptr_t end = GX_LIMIT - MP4_GX_SCRATCH_SIZE;
    if (end <= data) {
        return 0;
    }
    return end - data < MP4_DATA_SIZE ? (u32) ((end - data) & ~0xFFFu) : MP4_DATA_SIZE;
}

void HuMemInitAll(void)
{
    u8 *ptr = mp4_pool;
    s32 i;
    for (i = 0; i < HEAP_MAX; i++) {
        if (i == HEAP_SYSTEM || i == HEAP_DATA) {
            if (i == HEAP_DATA) {
                HeapSizeTbl[i] = data_heap_size();
            }
            HeapTbl[i] = HuMemInit(ptr, HeapSizeTbl[i]);
            ptr += HeapSizeTbl[i];
        } else {
            void* mem = malloc(HeapSizeTbl[i]);
            HeapTbl[i] = mem != NULL ? HuMemInit(mem, HeapSizeTbl[i]) : NULL;
        }
    }
}

int mp4_mem_fits(void)
{
    u32 data = data_heap_size();
    if (data < MP4_DATA_HEAP_MIN) {
        OSReport("[party] mp4: the game image leaves the data heap %u KB under what GX can read, "
                 "%u KB short\n", data >> 10, (MP4_DATA_HEAP_MIN - data) >> 10);
        return 0;
    }
    OSReport("[party] mp4: heaps at %p: system %u KB, data %u KB of %u, GX scratch %u KB\n",
             (void*) mp4_pool, MP4_SYSTEM_SIZE >> 10, data >> 10, MP4_DATA_SIZE >> 10,
             MP4_GX_SCRATCH_SIZE >> 10);
    return 1;
}

/* The scratch, right after the data heap: below the limit whatever the pool's place. */
u8* mp4_gx_scratch(u32* size)
{
    *size = MP4_GX_SCRATCH_SIZE;
    return mp4_pool + MP4_SYSTEM_SIZE + data_heap_size();
}

int mp4_mem_ready(void)
{
    return HeapTbl[0] != NULL;
}

void *HuMemInit(void *ptr, size_t size)
{
    return HuMemHeapInit(ptr, size);
}

void HuMemDCFlushAll(void) {}
void HuMemDCFlush(HeapID heap) { (void) heap; }

void *HuMemDirectMalloc(HeapID heap, size_t size)
{
    size = (size + 31) & ~0x1F;
    return HuMemMemoryAlloc(HeapTbl[heap], size, 0);
}

void *HuMemDirectMallocNum(HeapID heap, size_t size, uintptr_t num)
{
    size = (size + 31) & ~0x1F;
    return HuMemMemoryAllocNum(HeapTbl[heap], size, num, 0);
}

void HuMemDirectFree(void *ptr)
{
    HuMemMemoryFree(ptr, 0);
}

void HuMemDirectFreeNum(HeapID heap, uintptr_t num)
{
    HuMemMemoryFreeNum(HeapTbl[heap], num, 0);
}

size_t HuMemUsedMallocSizeGet(HeapID heap) { return HuMemUsedMemorySizeGet(HeapTbl[heap]); }
size_t HuMemUsedMallocBlockGet(HeapID heap) { return HuMemUsedMemoryBlockGet(HeapTbl[heap]); }
size_t HuMemHeapSizeGet(HeapID heap) { return HeapSizeTbl[heap]; }
void *HuMemHeapPtrGet(HeapID heap) { return HeapTbl[heap]; }

void *HuMemHeapInit(void *ptr, size_t size)
{
    struct memory_block *block = ptr;
    block->size = (s32) size;
    block->magic = 205;
    block->flag = 0;
    block->prev = block;
    block->next = block;
    block->num = -256;
    block->retaddr = 0xCDCDCDCD;
    return block;
}

void *HuMemMemoryAllocNum(void *heap_ptr, size_t size, uintptr_t num, uintptr_t retaddr)
{
    return HuMemMemoryAlloc2(heap_ptr, size, num, retaddr);
}

void *HuMemMemoryAlloc(void *heap_ptr, size_t size, uintptr_t retaddr)
{
    return HuMemMemoryAlloc2(heap_ptr, size, -256, retaddr);
}

static void *HuMemMemoryAlloc2(void *heap_ptr, size_t size, uintptr_t num, uintptr_t retaddr)
{
    s32 alloc_size = (s32) MEM_ALLOC_SIZE(size);
    struct memory_block *block = heap_ptr;
    if (block == NULL) {
        return NULL;
    }
    do {
        if (!block->flag && block->size >= alloc_size) {
            if (block->size - alloc_size > (s32) BLOCK_ALIGNMENT) {
                struct memory_block *new_block = (struct memory_block *) ((char *) block + alloc_size);
                new_block->size = block->size - alloc_size;
                new_block->magic = 205;
                new_block->flag = 0;
                new_block->retaddr = retaddr;
                block->next->prev = new_block;
                new_block->next = block->next;
                block->next = new_block;
                new_block->prev = block;
                block->size = alloc_size;
            }
            block->flag = 1;
            block->magic = 165;
            block->num = num;
            block->retaddr = retaddr;
            if (heap_ptr == HeapTbl[HEAP_DATA]) {
                data_used += (u32) block->size;
                if (data_used >= data_peak + 0x80000) {
                    data_peak = data_used;
                    OSReport("[party] mp4: data heap peak %u KB of %u KB\n", data_peak >> 10,
                             HeapSizeTbl[HEAP_DATA] >> 10);
                }
            }
            return BLOCK_GET_DATA(block);
        }
        block = block->next;
    } while (block != heap_ptr);
    OSReport("[party] mp4: out of memory, %x bytes (%x) in heap %p\n", (u32) size, (u32) num, heap_ptr);
    HuMemHeapDump(heap_ptr, 0);
    return NULL;
}

void HuMemMemoryFreeNum(void *heap_ptr, uintptr_t num, uintptr_t retaddr)
{
    struct memory_block *block = heap_ptr;
    if (block == NULL) {
        return;
    }
    do {
        struct memory_block *block_next = block->next;
        if (block->flag && block->num == num) {
            HuMemMemoryFree(BLOCK_GET_DATA(block), retaddr);
        }
        block = block_next;
    } while (block != heap_ptr);
}

void HuMemMemoryFree(void *ptr, uintptr_t retaddr)
{
    struct memory_block *block;
    if (!ptr) {
        return;
    }
    block = DATA_GET_BLOCK(ptr);
    if (block->magic != 165) {
        OSReport("[party] mp4: bad free %p\n", ptr);
        return;
    }
    if ((u8*) block >= (u8*) HeapTbl[HEAP_DATA] && (u8*) block < (u8*) HeapTbl[HEAP_DATA] + HeapSizeTbl[HEAP_DATA]) {
        data_used -= (u32) block->size;
    }
    if (block->prev < block && !block->prev->flag) {
        block->flag = 0;
        block->magic = 205;
        block->next->prev = block->prev;
        block->prev->next = block->next;
        block->prev->size += block->size;
        block = block->prev;
    }
    if (block->next > block && !block->next->flag) {
        block->next->next->prev = block;
        block->size += block->next->size;
        block->next = block->next->next;
    }
    block->flag = 0;
    block->magic = 205;
    block->retaddr = retaddr;
}

size_t HuMemUsedMemorySizeGet(void *heap_ptr)
{
    struct memory_block *block = heap_ptr;
    size_t size = 0;
    do {
        if (block->flag == 1) {
            size += block->size;
        }
        block = block->next;
    } while (block != heap_ptr);
    return size;
}

s32 HuMemUsedMemoryBlockGet(void *heap_ptr)
{
    struct memory_block *block = heap_ptr;
    s32 num_blocks = 0;
    do {
        if (block->flag == 1) {
            num_blocks++;
        }
        block = block->next;
    } while (block != heap_ptr);
    return num_blocks;
}

size_t HuMemMemoryAllocSizeGet(size_t size)
{
    return MEM_ALLOC_SIZE(size);
}

void HuMemHeapDump(void *heap_ptr, s16 status)
{
    (void) status;
    OSReport("[party] mp4: heap %p, %u bytes used in %d blocks\n", heap_ptr,
             (u32) HuMemUsedMemorySizeGet(heap_ptr), (int) HuMemUsedMemoryBlockGet(heap_ptr));
}

size_t HuMemMemorySizeGet(void *ptr)
{
    struct memory_block *block;
    if (!ptr) {
        return 0;
    }
    block = DATA_GET_BLOCK(ptr);
    if (block->flag == 1 && block->magic == 165) {
        return block->size - BLOCK_ALIGNMENT;
    }
    return 0;
}

#ifdef MU_NATIVE
/* Rollback snapshot exclusions: the MP4 heaps, used offline only (see the top of this file). */
MU_EXCLUSIONS(mp4, MU_EXCLUDE(mp4_pool))
#endif
