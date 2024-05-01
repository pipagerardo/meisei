/******************************************************************************
 *                                                                            *
 *                               "version.c"                                  *
 *                                                                            *
 ******************************************************************************/

#include "global.h"
#include "version.h"

const char* version_info[VERSION_MAX]={
	VERSION_AUTHOR_S,
	VERSION_URL_S,
	VERSION_EMAIL_S,
	VERSION_DESCRIPTION_S,
	VERSION_FILENAME_S,
	VERSION_NAME_S,
	VERSION_NUMBER_S,
	VERSION_STAGE_S,
	VERSION_DATE_S,
	VERSION_TIME_S
};

const char* version_get(u32 type)
{
	if (type<VERSION_MAX) return version_info[type];
	else return NULL;
}
