/* partyboard's build version switches (include/version.h there, and its CMake's -DVERSION_*): the
 * MP4 disc Melee Party reads is the USA one, so NTSC and English. */
#ifndef _VERSION_H
#define _VERSION_H

#define VERSION_NTSC 1
#define VERSION_ENG 1
#define VERSION_JP 0
#define VERSION_PAL 0

#define REFRESH_RATE 60
#define REFRESH_RATE_F 60.0f
#define REFRESH_FREQ (1.0f / REFRESH_RATE_F)

#endif
