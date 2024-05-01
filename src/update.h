/******************************************************************************
 *                                                                            *
 *                                "update.h"                                  *
 *                                                                            *
 ******************************************************************************/

#ifndef UPDATE_H
#define UPDATE_H

int update_download_thread_is_active(void);
INT_PTR CALLBACK update_dialog( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam );
void update_clean(void);

#endif /* UPDATE_H */
