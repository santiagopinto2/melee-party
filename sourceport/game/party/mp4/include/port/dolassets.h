/* The textures MP4 keeps in its executable (reflection, toon and highlight maps), which partyboard
 * reads out of the disc's main.dol (include/port/dolassets.h there). Not read yet: without them the
 * maps are left out of the materials that use them. */
#ifndef PORT_DOLASSETS_H_
#define PORT_DOLASSETS_H_

#define GetDolIncludeData(id) ((void*) 0)
#define hiliteData GetDolIncludeData(0)
#define hiliteData2 GetDolIncludeData(0)
#define hiliteData3 GetDolIncludeData(0)
#define hiliteData4 GetDolIncludeData(0)
#define refMapData0 GetDolIncludeData(0)
#define refMapData1 GetDolIncludeData(0)
#define refMapData2 GetDolIncludeData(0)
#define refMapData3 GetDolIncludeData(0)
#define refMapData4 GetDolIncludeData(0)
#define toonMapData GetDolIncludeData(0)
#define toonMapData2 GetDolIncludeData(0)

#endif
