/******************************************************************************
 *                                                                            *
 *                             "screenshot.h"                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef SCREENSHOT_H
#define SCREENSHOT_H

enum {
	SCREENSHOT_TYPE_32BPP=0,
	SCREENSHOT_TYPE_8BPP_INDEXED,
	SCREENSHOT_TYPE_1BPP_INDEXED
};

int screenshot_save(
    const int   width,          // Ancho
    const int   height,         // Alto
    int         input_type,     // SCREENSHOT_TYPE_32BPP | SCREENSHOT_TYPE_8BPP_INDEXED | SCREENSHOT_TYPE_1BPP_INDEXED
    const void* input_data,     // datos tipo: SCREENSHOT_TYPE_32BPP
    const void* input_data2,    // datos tipo: SCREENSHOT_TYPE_8BPP_INDEXED | SCREENSHOT_TYPE_1BPP_INDEXED
    const char* fn              // ".png"
);

#endif /* SCREENSHOT_H */
