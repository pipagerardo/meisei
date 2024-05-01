/******************************************************************************
 *                                                                            *
 *                                "tool.h"                                    *
 *                                                                            *
 ******************************************************************************/

#ifndef TOOL_H
#define TOOL_H

#define TOOL_REPAINT		(WM_APP+1)
#define TOOL_POPUP			(WM_APP+2)
#define TOOL_MENUCHANGED	(WM_APP+3)

#define TOOL_DBM_EMPTY		0x2e333d	/* dark gray/blue */
#define TOOL_DBIN0			'.'			/* binary 0 */

enum {
	TOOL_WINDOW_TILEVIEW=0,
	TOOL_WINDOW_SPRITEVIEW,
	TOOL_WINDOW_PSGTOY,

	TOOL_WINDOW_MAX
};

int tool_get_pattern_fi_shared(void);
void tool_set_pattern_fi_shared(u32);
int tool_is_quitting(void);
u8* tool_get_local_vdp_ram_ptr(void);
int* tool_get_local_vdp_regs_ptr(void);
int __fastcall tool_get_local_vdp_status(void);
int* tool_get_local_psg_regs_ptr(void);
int __fastcall tool_get_local_psg_address(void);
int __fastcall tool_get_local_psg_e_vol(void);
int* tool_get_local_draw_palette_ptr(void);

void tool_copy_locals(void);
void tool_copy_palette(int*);
void tool_repaint(void);
void tool_menuchanged(void);

void tool_init(void);
void tool_clean(void);
void tool_settings_save(void);
int __fastcall tool_is_running(void);
HWND tool_getactivewindow(void);

void tool_init_bmi(BITMAPINFO*,int,int,int);
void toolwindow_setpos(HWND,UINT,UINT,UINT,int,int,int);
void toolwindow_savepos(HWND,UINT);
void toolwindow_resize(HWND,LONG,LONG);
int toolwindow_restrictresize(HWND,WPARAM,LPARAM,int,int);
void toolwindow_relative_clientpos(HWND,POINT*,int,int);
void tool_set_window(UINT,UINT);
HWND tool_get_window(UINT);
void tool_reset_window(UINT);

#endif /* TOOL_H */
