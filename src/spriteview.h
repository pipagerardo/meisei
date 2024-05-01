/******************************************************************************
 *                                                                            *
 *                             "spriteview.h"                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef SPRITEVIEW_H
#define SPRITEVIEW_H

int spriteview_get_open_fi(void);
int spriteview_get_save_fi(void);

void spriteview_init(void);
void spriteview_clean(void);
INT_PTR CALLBACK spriteview_window( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam );

#endif /* SPRITEVIEW_H */
