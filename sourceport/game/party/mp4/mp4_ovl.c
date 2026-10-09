/* Melee Party, Mario Party 4 runtime: MP4's minigames are overlays (REL files) that its object
 * manager loads by number (objdll.c). Here each one is linked in, with its symbols prefixed by its
 * name (tools/mp4_rel_names.py), and starting it calls its ObjectSetup. The boot overlay is the one
 * a minigame returns to: it does nothing, and the match ends.
 *
 * On the console a REL is loaded fresh every time, with its initialised variables at their
 * initial values and the rest zero; a minigame counts on that (m440 keeps its "game over" flag and
 * its remaining-rounds count in them). Linked in, those variables persist, so the second game of a
 * party would be over as it started. Each overlay's sources are built with -fdata-sections, which
 * on COFF puts every variable, initialised or not, in a .data$<symbol> section of its own, and the
 * linker sorts those by name: two marker variables, .data$m440_ and .data$m440z, bracket all of
 * m440's (its symbols are m440_...). The bytes between them are saved the first time the overlay
 * starts and put back before every later start. */
#include <dolphin/os.h>
#include <string.h>


void* malloc(size_t size);

#include "game/gamework_data.h"
#include "game/object.h"
#include "mp4.h"

void m440_ObjectSetup(void);
void m438_ObjectSetup(void);
void m412_ObjectSetup(void);
void m403_ObjectSetup(void);
void m441_ObjectSetup(void);
void m404_ObjectSetup(void);
void m416_ObjectSetup(void);
void m422_ObjectSetup(void);
void m421_ObjectSetup(void);
void m434_ObjectSetup(void);
void m429_ObjectSetup(void);
void m453_ObjectSetup(void);

const int mp4_overlay_m440 = DLL_m440Dll;
const int mp4_overlay_m438 = DLL_m438Dll;
const int mp4_overlay_m412 = DLL_m412Dll;
const int mp4_overlay_m403 = DLL_m403Dll;
const int mp4_overlay_m441 = DLL_m441Dll;
const int mp4_overlay_m404 = DLL_m404Dll;
const int mp4_overlay_m416 = DLL_m416Dll;
const int mp4_overlay_m422 = DLL_m422Dll;
const int mp4_overlay_m421 = DLL_m421Dll;
const int mp4_overlay_m434 = DLL_m434Dll;
const int mp4_overlay_m429 = DLL_m429Dll;
const int mp4_overlay_m453 = DLL_m453Dll;

/* The markers: a byte before and a byte after the overlay's variables (see above). */
#define OVERLAY_MARKERS(name)                                                       \
    __attribute__((section(".data$" #name "_"))) static char name##_data_begin = 1; \
    __attribute__((section(".data$" #name "z"))) static char name##_data_end = 1;
OVERLAY_MARKERS(m440)
OVERLAY_MARKERS(m438)
OVERLAY_MARKERS(m412)
OVERLAY_MARKERS(m403)
OVERLAY_MARKERS(m441)
OVERLAY_MARKERS(m404)
OVERLAY_MARKERS(m416)
OVERLAY_MARKERS(m422)
OVERLAY_MARKERS(m421)
OVERLAY_MARKERS(m434)
OVERLAY_MARKERS(m429)
OVERLAY_MARKERS(m453)

static int boot_reached;

static void boot_ObjectSetup(void)
{
    boot_reached = 1;
}

static const struct {
    OMOVL overlay;
    void (*object_setup)(void);
    char* data_begin;
    char* data_end;
} overlays[] = {
    { DLL_bootDll, boot_ObjectSetup, NULL, NULL },
    { DLL_m440Dll, m440_ObjectSetup, &m440_data_begin, &m440_data_end },
    { DLL_m438Dll, m438_ObjectSetup, &m438_data_begin, &m438_data_end },
    { DLL_m412Dll, m412_ObjectSetup, &m412_data_begin, &m412_data_end },
    { DLL_m403Dll, m403_ObjectSetup, &m403_data_begin, &m403_data_end },
    { DLL_m441Dll, m441_ObjectSetup, &m441_data_begin, &m441_data_end },
    { DLL_m404Dll, m404_ObjectSetup, &m404_data_begin, &m404_data_end },
    { DLL_m416Dll, m416_ObjectSetup, &m416_data_begin, &m416_data_end },
    { DLL_m422Dll, m422_ObjectSetup, &m422_data_begin, &m422_data_end },
    { DLL_m421Dll, m421_ObjectSetup, &m421_data_begin, &m421_data_end },
    { DLL_m434Dll, m434_ObjectSetup, &m434_data_begin, &m434_data_end },
    { DLL_m429Dll, m429_ObjectSetup, &m429_data_begin, &m429_data_end },
    { DLL_m453Dll, m453_ObjectSetup, &m453_data_begin, &m453_data_end },
};
#define OVERLAYS (sizeof overlays / sizeof overlays[0])

/* each overlay's initial variables, saved at its first start */
static char* initial_data[OVERLAYS];

int mp4_boot_reached(void)
{
    return boot_reached;
}

void mp4_boot_reset(void)
{
    boot_reached = 0;
}

void omDLLInit(FileListEntry *ovl_list)
{
    (void) ovl_list;
}

/* The overlay's variables as a fresh load would have them. */
static void overlay_data_reset(u32 i)
{
    char* begin = overlays[i].data_begin;
    size_t size;
    if (begin == NULL) {
        return;
    }
    size = (size_t) (overlays[i].data_end - begin);
    if (initial_data[i] == NULL) {
        initial_data[i] = malloc(size);   /* the C runtime's heap: kept for the session */
        if (initial_data[i] == NULL) {
            OSReport("[party] mp4: no memory to keep overlay %d's initial data (%u bytes)\n",
                     (int) overlays[i].overlay, (u32) size);
            return;
        }
        memcpy(initial_data[i], begin, size);
        OSReport("[party] mp4: overlay %d keeps %u bytes of variables\n", (int) overlays[i].overlay,
                 (u32) size);
    } else {
        memcpy(begin, initial_data[i], size);
    }
}

s32 omDLLStart(s16 overlay, s16 flag)
{
    u32 i;
    (void) flag;
    for (i = 0; i < OVERLAYS; i++) {
        if (overlays[i].overlay == overlay) {
            overlay_data_reset(i);
            overlays[i].object_setup();
            return (s32) i;
        }
    }
    OSReport("[party] mp4: overlay %d is not linked in\n", overlay);
    return -1;
}

void omDLLNumEnd(s16 overlay, s16 flag)
{
    (void) overlay;
    (void) flag;
}

void omDLLDBGOut(void) {}
