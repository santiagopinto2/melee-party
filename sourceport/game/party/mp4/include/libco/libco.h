/* Melee Party, Mario Party 4 runtime: coroutines for MP4's processes (HuPrc), with the interface
 * of byuu's libco, which partyboard's PC port builds its process.c against. The implementation is
 * our own (mp4_coro.c): a register switch for x86-64 (Windows and System V) and AArch64. */
#ifndef MP4_LIBCO_H
#define MP4_LIBCO_H

typedef void* cothread_t;

cothread_t co_active(void);
cothread_t co_create(unsigned int size, void (*entry)(void));
void co_delete(cothread_t thread);
void co_switch(cothread_t thread);

#endif
