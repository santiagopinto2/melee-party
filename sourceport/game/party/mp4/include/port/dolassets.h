/* The textures MP4 keeps in its executable (reflection, toon and highlight maps), which partyboard
 * reads out of the disc's main.dol (include/port/dolassets.h there). Here mp4_dolassets.c reads
 * them from the disc's main.dol the first time each is asked for; NULL if it cannot. */
#ifndef PORT_DOLASSETS_H_
#define PORT_DOLASSETS_H_

enum {
    MP4_DOL_HILITE,
    MP4_DOL_HILITE2,
    MP4_DOL_HILITE3,
    MP4_DOL_HILITE4,
    MP4_DOL_REFMAP0,
    MP4_DOL_REFMAP1,
    MP4_DOL_REFMAP2,
    MP4_DOL_REFMAP3,
    MP4_DOL_REFMAP4,
    MP4_DOL_TOONMAP,
    MP4_DOL_TOONMAP2,
    MP4_DOL_ASSET_MAX
};

void* mp4_dol_asset(int id);

#define hiliteData mp4_dol_asset(MP4_DOL_HILITE)
#define hiliteData2 mp4_dol_asset(MP4_DOL_HILITE2)
#define hiliteData3 mp4_dol_asset(MP4_DOL_HILITE3)
#define hiliteData4 mp4_dol_asset(MP4_DOL_HILITE4)
#define refMapData0 mp4_dol_asset(MP4_DOL_REFMAP0)
#define refMapData1 mp4_dol_asset(MP4_DOL_REFMAP1)
#define refMapData2 mp4_dol_asset(MP4_DOL_REFMAP2)
#define refMapData3 mp4_dol_asset(MP4_DOL_REFMAP3)
#define refMapData4 mp4_dol_asset(MP4_DOL_REFMAP4)
#define toonMapData mp4_dol_asset(MP4_DOL_TOONMAP)
#define toonMapData2 mp4_dol_asset(MP4_DOL_TOONMAP2)

#endif
