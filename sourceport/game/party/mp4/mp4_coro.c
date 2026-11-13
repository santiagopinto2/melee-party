/* Melee Party, Mario Party 4 runtime: the coroutines MP4's processes run on (include/libco/libco.h).
 *
 * Each coroutine has its own stack, from MP4's system heap. co_switch saves the callee-saved
 * registers on the current stack, keeps that stack pointer, and loads the other coroutine's. On
 * Windows the switch also swaps the thread's stack bounds (the TIB's StackBase and StackLimit), so
 * exception unwinding and the crash handler's stack walk see the stack they run on. A coroutine
 * whose function returns ends its process (HuPrcEnd), as MP4's never do. */
#include <dolphin/types.h>
#include <stdint.h>
#include <string.h>

#include "game/memory.h"
#include "game/process.h"
#include "libco/libco.h"

/* The C runtime's heap: GX never reads a stack, and neither MP4's heaps (its system heap is for the
 * minigame's data) nor Melee's (reset between scenes, under a process that outlives a match) will do. */
void* malloc(size_t size);
void free(void* ptr);

typedef struct Coro {
    void* sp;              /* the saved stack pointer, while switched out */
    void (*entry)(void);
} Coro;

static Coro main_coro;
static Coro* current;

void mp4_co_swap(void** save_sp, void* load_sp);

#if defined(__x86_64__) && defined(_WIN32)
/* rcx = where to save this stack pointer, rdx = the one to load */
__asm__(".text\n"
        ".globl mp4_co_swap\n"
        "mp4_co_swap:\n"
        "  push %rbp\n  push %rbx\n  push %rdi\n  push %rsi\n"
        "  push %r12\n  push %r13\n  push %r14\n  push %r15\n"
        "  mov %gs:8, %rax\n  push %rax\n"
        "  mov %gs:16, %rax\n  push %rax\n"
        "  sub $168, %rsp\n"
        "  movaps %xmm6, 0(%rsp)\n  movaps %xmm7, 16(%rsp)\n  movaps %xmm8, 32(%rsp)\n"
        "  movaps %xmm9, 48(%rsp)\n  movaps %xmm10, 64(%rsp)\n  movaps %xmm11, 80(%rsp)\n"
        "  movaps %xmm12, 96(%rsp)\n  movaps %xmm13, 112(%rsp)\n  movaps %xmm14, 128(%rsp)\n"
        "  movaps %xmm15, 144(%rsp)\n"
        "  mov %rsp, (%rcx)\n"
        "  mov %rdx, %rsp\n"
        "  movaps 0(%rsp), %xmm6\n  movaps 16(%rsp), %xmm7\n  movaps 32(%rsp), %xmm8\n"
        "  movaps 48(%rsp), %xmm9\n  movaps 64(%rsp), %xmm10\n  movaps 80(%rsp), %xmm11\n"
        "  movaps 96(%rsp), %xmm12\n  movaps 112(%rsp), %xmm13\n  movaps 128(%rsp), %xmm14\n"
        "  movaps 144(%rsp), %xmm15\n"
        "  add $168, %rsp\n"
        "  pop %rax\n  mov %rax, %gs:16\n"
        "  pop %rax\n  mov %rax, %gs:8\n"
        "  pop %r15\n  pop %r14\n  pop %r13\n  pop %r12\n"
        "  pop %rsi\n  pop %rdi\n  pop %rbx\n  pop %rbp\n"
        "  ret\n");
#define SWAP_WORDS 32          /* 168 bytes of xmm, 2 stack bounds, 8 registers, the return */
#define SWAP_RET 31
#define SWAP_BASE 22
#define SWAP_LIMIT 21
/* A new coroutine "returns" into this stub, which calls coro_start as a call would: a 16-byte
 * aligned stack and Windows' 32 bytes of shadow space above the return address. */
__asm__(".text\n"
        "mp4_co_entry:\n"
        "  sub $32, %rsp\n"
        "  call coro_start\n"
        "  ud2\n");
#define ENTRY_STUB
#elif defined(__x86_64__)
/* rdi = where to save this stack pointer, rsi = the one to load */
__asm__(".text\n"
        ".globl mp4_co_swap\n"
        "mp4_co_swap:\n"
        "  push %rbp\n  push %rbx\n  push %r12\n  push %r13\n  push %r14\n  push %r15\n"
        "  mov %rsp, (%rdi)\n"
        "  mov %rsi, %rsp\n"
        "  pop %r15\n  pop %r14\n  pop %r13\n  pop %r12\n  pop %rbx\n  pop %rbp\n"
        "  ret\n");
#define SWAP_WORDS 7
#define SWAP_RET 6
__asm__(".text\n"
        "mp4_co_entry:\n"
        "  call coro_start\n"
        "  ud2\n");
#define ENTRY_STUB
#elif defined(__aarch64__)
/* x0 = where to save this stack pointer, x1 = the one to load */
__asm__(".text\n"
        ".globl mp4_co_swap\n"
        "mp4_co_swap:\n"
        "  sub sp, sp, #160\n"
        "  stp x19, x20, [sp, #0]\n  stp x21, x22, [sp, #16]\n  stp x23, x24, [sp, #32]\n"
        "  stp x25, x26, [sp, #48]\n  stp x27, x28, [sp, #64]\n  stp x29, x30, [sp, #80]\n"
        "  stp d8, d9, [sp, #96]\n  stp d10, d11, [sp, #112]\n  stp d12, d13, [sp, #128]\n"
        "  stp d14, d15, [sp, #144]\n"
        "  mov x2, sp\n  str x2, [x0]\n"
        "  mov sp, x1\n"
        "  ldp x19, x20, [sp, #0]\n  ldp x21, x22, [sp, #16]\n  ldp x23, x24, [sp, #32]\n"
        "  ldp x25, x26, [sp, #48]\n  ldp x27, x28, [sp, #64]\n  ldp x29, x30, [sp, #80]\n"
        "  ldp d8, d9, [sp, #96]\n  ldp d10, d11, [sp, #112]\n  ldp d12, d13, [sp, #128]\n"
        "  ldp d14, d15, [sp, #144]\n"
        "  add sp, sp, #160\n"
        "  ret\n");
#define SWAP_WORDS 20
#define SWAP_RET 11            /* x30 */
#else
#error "mp4_coro.c: no coroutine switch for this CPU"
#endif

#define ENTRY_GAP 16

#ifdef ENTRY_STUB
void mp4_co_entry(void);
#define CORO_ENTRY mp4_co_entry
#else
#define CORO_ENTRY coro_start
#endif

/* Called from the stub's asm, by its name. */
#ifdef _WIN32
__attribute__((used)) void coro_start(void);
#else
__attribute__((used, visibility("hidden"))) void coro_start(void);
#endif

void coro_start(void)
{
    current->entry();
    HuPrcEnd();
}

cothread_t co_active(void)
{
    if (current == NULL) {
        current = &main_coro;
    }
    return current;
}

cothread_t co_create(unsigned int size, void (*entry)(void))
{
    /* MP4 asked for a console stack of 2 to 16 KB; the PC's frames are bigger. */
    u32 stack_size = size < 0x10000 ? 0x10000 : size;
    u8* mem = malloc(sizeof(Coro) + stack_size + 16);
    Coro* co;
    uintptr_t top;
    uintptr_t* frame;
    if (mem == NULL) {
        return NULL;
    }
    co = (Coro*) mem;
    co->entry = entry;
    top = ((uintptr_t) mem + sizeof(Coro) + stack_size) & ~(uintptr_t) 15;
    frame = (uintptr_t*) (top - ENTRY_GAP) - SWAP_WORDS;
    memset(frame, 0, (SWAP_WORDS * sizeof(uintptr_t)) + ENTRY_GAP);
    frame[SWAP_RET] = (uintptr_t) CORO_ENTRY;
#ifdef SWAP_BASE
    frame[SWAP_BASE] = top;
    frame[SWAP_LIMIT] = (uintptr_t) mem;
#endif
    co->sp = frame;
    return co;
}

void co_delete(cothread_t thread)
{
    if (thread != NULL && thread != &main_coro) {
        free(thread);
    }
}

void co_switch(cothread_t thread)
{
    Coro* from = (Coro*) co_active();
    Coro* to = (Coro*) thread;
    if (to == from) {
        return;
    }
    current = to;
    mp4_co_swap(&from->sp, to->sp);
}
