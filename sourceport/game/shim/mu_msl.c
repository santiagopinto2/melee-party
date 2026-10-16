/* The pieces of the console's C runtime that were the compiler's or the linker's job.
 *
 * The game carries its own C library (src/MSL), and most of it compiles natively. What does not is
 * what Metrowerks supplied by other means: a character-class table that lived in a library, a
 * float conversion helper, the stdio object table, the stack bounds the linker defined, and
 * setjmp, which saved PowerPC registers. */
#include "mu_shim.h"

/* ---- character classes ----
 * MSL's ctype.h indexes this table with the bit it defines for each class (__control_char and the
 * rest), so the values here are those bits, as in the console's table (MSL/ctype.c). The game
 * only ever asks about ASCII. A table with bits of its own here made isalpha false for every
 * lowercase letter and isdigit false for every digit. */
#define MU_CTRL 0x01     /* __control_char */
#define MU_MOTION 0x02   /* __motion_char: tab, newline, vertical tab, form feed, return */
#define MU_SPACE 0x04    /* __space_char */
#define MU_PUNCT 0x08    /* __punctuation */
#define MU_DIGIT 0x10    /* __digit */
#define MU_HEX 0x20      /* __hex_digit */
#define MU_LOWER 0x40    /* __lower_case */
#define MU_UPPER 0x80    /* __upper_case */

const unsigned char __ctype_map[256] = {
    /* 0x00 */ MU_CTRL, MU_CTRL, MU_CTRL, MU_CTRL, MU_CTRL, MU_CTRL, MU_CTRL, MU_CTRL,
    /* 0x08 */ MU_CTRL, MU_MOTION, MU_MOTION, MU_MOTION, MU_MOTION, MU_MOTION, MU_CTRL, MU_CTRL,
    /* 0x10 */ MU_CTRL, MU_CTRL, MU_CTRL, MU_CTRL, MU_CTRL, MU_CTRL, MU_CTRL, MU_CTRL,
    /* 0x18 */ MU_CTRL, MU_CTRL, MU_CTRL, MU_CTRL, MU_CTRL, MU_CTRL, MU_CTRL, MU_CTRL,
    /* 0x20 */ MU_SPACE, MU_PUNCT, MU_PUNCT, MU_PUNCT, MU_PUNCT, MU_PUNCT, MU_PUNCT, MU_PUNCT,
    /* 0x28 */ MU_PUNCT, MU_PUNCT, MU_PUNCT, MU_PUNCT, MU_PUNCT, MU_PUNCT, MU_PUNCT, MU_PUNCT,
    /* 0x30 */ MU_DIGIT | MU_HEX, MU_DIGIT | MU_HEX, MU_DIGIT | MU_HEX, MU_DIGIT | MU_HEX,
               MU_DIGIT | MU_HEX, MU_DIGIT | MU_HEX, MU_DIGIT | MU_HEX, MU_DIGIT | MU_HEX,
    /* 0x38 */ MU_DIGIT | MU_HEX, MU_DIGIT | MU_HEX, MU_PUNCT, MU_PUNCT, MU_PUNCT, MU_PUNCT,
               MU_PUNCT, MU_PUNCT,
    /* 0x40 */ MU_PUNCT, MU_UPPER | MU_HEX, MU_UPPER | MU_HEX, MU_UPPER | MU_HEX,
               MU_UPPER | MU_HEX, MU_UPPER | MU_HEX, MU_UPPER | MU_HEX, MU_UPPER,
    /* 0x48 */ MU_UPPER, MU_UPPER, MU_UPPER, MU_UPPER, MU_UPPER, MU_UPPER, MU_UPPER, MU_UPPER,
    /* 0x50 */ MU_UPPER, MU_UPPER, MU_UPPER, MU_UPPER, MU_UPPER, MU_UPPER, MU_UPPER, MU_UPPER,
    /* 0x58 */ MU_UPPER, MU_UPPER, MU_UPPER, MU_PUNCT, MU_PUNCT, MU_PUNCT, MU_PUNCT, MU_PUNCT,
    /* 0x60 */ MU_PUNCT, MU_LOWER | MU_HEX, MU_LOWER | MU_HEX, MU_LOWER | MU_HEX,
               MU_LOWER | MU_HEX, MU_LOWER | MU_HEX, MU_LOWER | MU_HEX, MU_LOWER,
    /* 0x68 */ MU_LOWER, MU_LOWER, MU_LOWER, MU_LOWER, MU_LOWER, MU_LOWER, MU_LOWER, MU_LOWER,
    /* 0x70 */ MU_LOWER, MU_LOWER, MU_LOWER, MU_LOWER, MU_LOWER, MU_LOWER, MU_LOWER, MU_LOWER,
    /* 0x78 */ MU_LOWER, MU_LOWER, MU_LOWER, MU_PUNCT, MU_PUNCT, MU_PUNCT, MU_PUNCT, MU_CTRL,
    /* 0x80 and up: the game's Japanese text is handled by its own Shift-JIS code, not by these. */
    0
};

/* ---- the stack ----
 * The console's linker placed the stack and defined these; the debug build prints how much of it
 * was ever touched. Natively the game runs on the host's stack, so this is a window of the right
 * size for that code to measure against. */
unsigned char _stack_end[64 * 1024];
unsigned char _stack_addr[1] = { 0 };

/* ---- stdio ----
 * MSL's file table. Nothing in the game opens a file; the one reference comes from code that
 * checks whether a stream is buffered before it would print. */
void* __files[3] = { 0, 0, 0 };

/* ---- float conversion ----
 * Metrowerks' helper for turning a double into an unsigned 64-bit value, used where the game
 * converts a frame count. The C cast is the same operation. */
unsigned long long __cvt_dbl_usll(double value)
{
    if (value <= 0.0)
        return 0;
    if (value >= 18446744073709551616.0)
        return ~0ull;
    return (unsigned long long) value;
}

/* ---- debugger ---- */
int DBIsDebuggerPresent(void) { return 0; }

/* A pointer that had to fit a 32-bit slot did not (see mu_disc.h). */
void mu_addr32_failed(const void* ptr, const char* file, int line)
{
    (void) ptr;
    mu_host->panic(file, line, "a pointer does not fit its 32-bit slot");
}
