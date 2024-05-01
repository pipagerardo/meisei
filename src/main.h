/******************************************************************************
 *                                                                            *
 *                                 "main.h"                                   *
 *                                                                            *
 ******************************************************************************/

#ifndef MAIN_H
#define MAIN_H

typedef struct {
	HWND window;
	HINSTANCE module;
	HMENU menu;
	int foreground;
	int dialog;
	int x_plus;
	int y_plus;
	HANDLE mutex;
} Main;

// Main* MAIN;
extern Main* MAIN;

enum {
	MAIN_PW_CENTER=0,
	MAIN_PW_OUTERL,
	MAIN_PW_LEFT,
	MAIN_PW_RIGHT,
	MAIN_PW_OUTERR
};
void main_parent_window(HWND,UINT,UINT,int,int,int);

void main_menu_enable(UINT,int);
void main_menu_check(UINT,int);
void main_menu_radio(UINT,UINT,UINT);
void main_menu_update_enable(void);
void main_menu_caption_get(UINT,char*);
void main_menu_caption_put(UINT,char*);
void main_titlebar(const char*);

#endif /* MAIN_H */
