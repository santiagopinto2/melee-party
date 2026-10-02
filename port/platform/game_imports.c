/* SPDX-License-Identifier: GPL-3.0-or-later
 * The table behind platform::game_imports (game_imports.h). C, so every name is the C library's one
 * function (C++ overloads strchr and friends). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char* name;
    void* address;
} Import;

#define IMPORT(f) {#f, (void*) &f}
static const Import imports[] = {
    IMPORT(atoi),    IMPORT(fclose),  IMPORT(fflush),    IMPORT(fopen),    IMPORT(fprintf),
    IMPORT(fwrite),  IMPORT(getenv),  IMPORT(memcmp),    IMPORT(memcpy),   IMPORT(memmove),
    IMPORT(memset),  IMPORT(printf),  IMPORT(snprintf),  IMPORT(sprintf),  IMPORT(strchr),
    IMPORT(strcmp),  IMPORT(strcpy),  IMPORT(strlen),    IMPORT(strncmp),  IMPORT(strncpy),
    IMPORT(strrchr), IMPORT(strtoul), IMPORT(vsnprintf), IMPORT(vsprintf), IMPORT(sqrtf),
    IMPORT(fabsf),   IMPORT(fabs),
};

void* platform_game_import(const char* name)
{
    size_t i;
    for (i = 0; i < sizeof imports / sizeof imports[0]; i++) {
        if (strcmp(imports[i].name, name) == 0) {
            return imports[i].address;
        }
    }
    return NULL;
}
