/* Melee Party, Mario Party 4 runtime: MP4's minigames are overlays (REL files) that its object
 * manager loads by number (objdll.c). Here each one is linked in, with its symbols prefixed by its
 * name (tools/mp4_rel_names.py), and starting it calls its ObjectSetup. The boot overlay is the one
 * a minigame returns to: it does nothing, and the match ends. */
#include <dolphin/os.h>

#include "game/object.h"
#include "mp4.h"

void m440_ObjectSetup(void);

const int mp4_overlay_m440 = DLL_m440Dll;

static int boot_reached;

static void boot_ObjectSetup(void)
{
    boot_reached = 1;
}

static const struct {
    OMOVL overlay;
    void (*object_setup)(void);
} overlays[] = {
    { DLL_bootDll, boot_ObjectSetup },
    { DLL_m440Dll, m440_ObjectSetup },
};

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

s32 omDLLStart(s16 overlay, s16 flag)
{
    u32 i;
    (void) flag;
    for (i = 0; i < sizeof overlays / sizeof overlays[0]; i++) {
        if (overlays[i].overlay == overlay) {
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
