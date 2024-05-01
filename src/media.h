/******************************************************************************
 *                                                                            *
 *                                "media.h"                                   *
 *                                                                            *
 ******************************************************************************/

#ifndef MEDIA_H
#define MEDIA_H

int media_get_filterindex_bios(void);
int media_get_filterindex_tape(void);
int media_get_filterindex_cart(void);
void media_init(void);
INT_PTR CALLBACK media_dialog( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam );
int media_open_single(const char*);
void media_drop(WPARAM);


#endif /* MEDIA_H */
