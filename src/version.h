/******************************************************************************
 *                                                                            *
 *                               "version.h"                                  *
 *                                                                            *
 ******************************************************************************/

#ifndef VERSION_H
#define VERSION_H

#ifndef FROM_RESOURCE_RC
enum {
    VERSION_AUTHOR=0,
    VERSION_URL,
    VERSION_EMAIL,
    VERSION_DESCRIPTION,
    VERSION_FILENAME,
    VERSION_NAME,
    VERSION_NUMBER,
    VERSION_STAGE,
    VERSION_DATE,
    VERSION_TIME,
    VERSION_MAX
};

const char* version_get(u32);
#endif

#define VERSION_NUMBER_D		13          /* increase by 1 every new version */
#define VERSION_NUMBER_R		1,3,3,0
#define VERSION_NUMBER_S		"1.3.3"

/* (some copyright info is also in resource.rc) */
#define VERSION_AUTHOR_S		"hap"
#define VERSION_URL_S			"https://github.com/pipagerardo/meisei" // "http://tsk-tsk.net/"
#define VERSION_EMAIL_S			"mkoelew@yahoo.com"

#ifdef MEISEI_ESP
#define VERSION_DESCRIPTION_S	"Emulador de MSX"
#define VERSION_FILENAME_S		"meisei_esp.exe"
#else
#define VERSION_DESCRIPTION_S	"MSX emulator"
#define VERSION_FILENAME_S		"meisei.exe"
#endif

#define VERSION_NAME_S			"meisei"

#ifdef MEISEI_ESP
#define VERSION_STAGE_S			"Lanzamiento"
#else
#define VERSION_STAGE_S			"Release"
#endif

#define VERSION_DATE_S			__DATE__
#define VERSION_TIME_S			__TIME__

#endif /* VERSION_H */
