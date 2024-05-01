/******************************************************************************
 *                                                                            *
 *                               "tileview.c"                                 *
 *                                                                            *
 ******************************************************************************/

/* tool window */
#include <windows.h>
#include <commctrl.h>

#include "global.h"
#include "tool.h"
#include "tileview.h"
#include "file.h"
#include "vdp.h"
#include "main.h"
#include "resource.h"
#include "input.h"
#include "screenshot.h"
#include "movie.h"
#include "netplay.h"
#include "settings.h"

static struct {
	int busy;
	int isnt;
	HCURSOR cursor_cross;
	int win_width;
	int win_height;

	/* follow */
	int fnum_prev;
	int ftotal_prev;

	/* popup menu */
	int rclickdown;
	int rclick;
	int popup_active;
	POINT popup_p;
	UINT ext_dialog;
	HWND ext_wnd;

	/* copy/paste */
	u8 copy_p[8];
	u8 copy_c[8];
	int paste_valid;

	/* nt change dialog */
	int ntc_a;
	u8 ntc_v;

	/* tilemap info */
	int blank;
	int mode;
	int block[3];
	int nt;
	int pt;
	int ct;

	/* tilemap bmp */
	struct {
		HWND wnd;
		HDC wdc;
		HDC bdc;
		int* screen;
		HBITMAP bmp;
		BITMAPINFO info;
	} tv;

	/* highlight info */
	int hltile;
	int hlwidth;
	int hlavis;
	int nat;
	int pat;
	int cat;

	int in_tv;
	int in_tv_prev;
	POINT p_tv;
	POINT p_tv_prev;

	/* highlight bmp */
	struct {
		HWND wnd;
		HDC wdc;
		HDC bdc;
		int* screen;
		HBITMAP bmp;
		BITMAPINFO info;
	} h;

	/* details */
	HFONT sfont;
	int dclick;
	int dinfo;

	/* local state */
	u8 vdp_ram[0x4000];
	int vdp_regs[8];
	int screenmode;
	int validscreen;

} tileview;

static u8* vdp_ram;
static int* vdp_regs;
static int* draw_palette;

/* initial window size */
#define TILEVIEW_WIN_WIDTH			288
#define TILEVIEW_WIN_HEIGHT			538

/* initial btext positions */
#define TILEVIEW_BTEXT_X			8
#define TILEVIEW_BTEXT_Y			48

/* non-volatile */
static int tileview_source;
static char tileview_dir[STRING_SIZE]={0};
static int tileview_open_fi; /* 1: all supp. 2: .scX, 3: .nt, 4: .pgt, 5: .ct, 6: any */

#define TILEVIEW_OPENI_NT_DEFAULT	FALSE
#define TILEVIEW_OPENI_PT_DEFAULT	TRUE
#define TILEVIEW_OPENI_CT_DEFAULT	TRUE
static int tileview_openi_nt;
static int tileview_openi_pt;
static int tileview_openi_ct;
static int tileview_open_sim;

static int tileview_save_fi; /* 1: .png, 2: .scX, 3: .nt, 4: .pgt, 5: .ct */
static int tileview_save_mask;
static int tileview_save_header;
static int tileview_follow_enabled=FALSE;
static int tileview_follow_show=TRUE;
static int tileview_follow_num=0;


/******************************************************************************
 *                                                                            *
 *                      #include "tileedit.c"                                 *
 *                                                                            *
 ******************************************************************************/
// #include "tileedit.c"

/* tile editor (tileview.c), similar to Wolf's Polka */

enum {
	TILEEDIT_ACT_NOTHING=0,
	TILEEDIT_ACT_FG,	/* zoom foreground draw */
	TILEEDIT_ACT_BG,	/* zoom background draw */
	TILEEDIT_ACT_MOVE,	/* zoom move */
	TILEEDIT_ACT_FC,	/* foreground column draw */
	TILEEDIT_ACT_BC		/* background column draw */
};

static struct {
	/* generic */
	int busy;
	int act;
	int act_done;
	int rclick;
	int pal[0x10];
	int curpal;
	HBRUSH brush[0x10];
	int screen3;
	u8 p[8];
	u8 c[8];
	u8 p_open[8];
	u8 c_open[8];
	int copy_open;
	int colour;
	int xy;
	int allblocks_vis;

	int ukey_prev;
	int fkey_prev;
	HCURSOR cursor_cross;
	HCURSOR cursor_size;
	HANDLE tickh;
	HANDLE tickv;

	/* local state */
	u8 vdp_ram[0x4000];
	int vdp_regs[8];
	int pattern_orig;
	int block_orig;
	int row_orig;
	int mode;

	/* move */
	POINT p_move;
	POINT p_move_prev;

	/* zoom bmp */
	struct {
		HWND wnd;
		HDC wdc;
		HDC bdc;
		int* screen;
		HBITMAP bmp;
		BITMAPINFO info;
	} zoom;

	int in_zoom;
	int in_zoom_prev;
	POINT p_zoom;
	POINT p_zoom_prev;

	/* fg bmp */
	struct {
		HWND wnd;
		HDC wdc;
		HDC bdc;
		int* screen;
		HBITMAP bmp;
		BITMAPINFO info;
	} fg;

	int in_fg;
	int in_fg_prev;
	POINT p_fg;
	POINT p_fg_prev;

	/* bg bmp */
	struct {
		HWND wnd;
		HDC wdc;
		HDC bdc;
		int* screen;
		HBITMAP bmp;
		BITMAPINFO info;
	} bg;

	int in_bg;
	int in_bg_prev;
	POINT p_bg;
	POINT p_bg_prev;

} tileedit;

typedef struct _tileedit_undo {
	u8 p[8];
	u8 c[8];
	struct _tileedit_undo* next;
	struct _tileedit_undo* prev;
} _tileedit_undo;
static _tileedit_undo* _tileedit_undo_begin;
static _tileedit_undo* _tileedit_undo_cursor;

/* non-volatile */
static char tileedit_dir[STRING_SIZE]={0};
static int tileedit_fi; /* 1: .pattern, 2: any */
static int tileedit_allblocks;


/* helper functions */
static void tileedit_download(void)
{
	/* send tile data+mode from edit ram/regs to edit p/c */
	const int mode=(tileedit.vdp_regs[1]>>4&1)|(tileedit.vdp_regs[1]>>1&4)|(tileedit.vdp_regs[0]&2);
	const int ct=tileedit.vdp_regs[3]<<6;
	int pg=tileedit.vdp_regs[4]<<11&0x3fff;
	int i;

	/* fill pattern */
	if (mode&2) pg=(tileedit.block_orig<<11&pg)|(pg&0x2000);
	memcpy(tileedit.p,tileedit.vdp_ram+(pg|tileedit.pattern_orig<<3),8);
	if (mode&4) {
		/* screen 3 */
		memset(tileedit.p,0xf0,8);
		for (i=0;i<4;i++) tileedit.c[i]=tileedit.vdp_ram[pg|tileedit.pattern_orig<<3|tileedit.row_orig<<1];
		for (i=4;i<8;i++) tileedit.c[i]=tileedit.vdp_ram[pg|tileedit.pattern_orig<<3|tileedit.row_orig<<1|1];
	}
	else if (mode==2) memcpy(tileedit.c,tileedit.vdp_ram+(((ct&0x2000)|(tileedit.block_orig<<11&0x1800)|tileedit.pattern_orig<<3)&(ct|0x3f)),8); /* screen 2 */
	else if (mode&1) for (i=0;i<8;i++) tileedit.c[i]=tileedit.vdp_regs[7]; /* screen 0 */
	else for (i=0;i<8;i++) tileedit.c[i]=tileedit.vdp_ram[ct|tileedit.pattern_orig>>3]; /* screen 1 */

	tileedit.mode=mode;
}

static int tileedit_upload(int* reg_local,u8* p,u8* c,int allblocks,int s01c)
{
	int i,success=TRUE;
	int modeo=(reg_local[1]>>4&1)|(reg_local[1]>>1&4)|(reg_local[0]&2);
	int pgo=reg_local[4]<<11&0x3fff;
	int cto=reg_local[3]<<6;
	if (modeo&2) pgo=(tileedit.block_orig<<11&pgo)|(pgo&0x2000);

	if (modeo&4) {
		/* screen 3 */
		success&=vdp_upload(pgo|tileedit.pattern_orig<<3|tileedit.row_orig<<1,&c[3],2);
	}
	else {
		/* upload pattern data */
		pgo|=(tileedit.pattern_orig<<3);
		if (allblocks&&modeo&2) {
			pgo&=~0x1800;
			for (i=0;i<0x1800;i+=0x800) success&=vdp_upload(pgo|i,p,8);
		}
		else success&=vdp_upload(pgo,p,8);

		/* upload colour data */
		if (modeo==2) {
			/* screen 2 */
			cto=((cto&0x2000)|(tileedit.block_orig<<11&0x1800)|tileedit.pattern_orig<<3)&(cto|0x3f);

			if (allblocks) {
				cto&=~0x1800;
				for (i=0;i<0x1800;i+=0x800) success&=vdp_upload(cto|i,c,8);
			}
			else success&=vdp_upload(cto,c,8);
		}
		else if ((modeo&5)==0&&s01c) {
			/* screen 1 */
			success&=vdp_upload(cto|tileedit.pattern_orig>>3,c,1);
		}

		/* upload register data (screen 0) */
		else if (s01c) {
			success&=vdp_upload(7|VDP_UPLOAD_REG,c,1);
			reg_local[7]=c[0];
		}
	}

	return success;
}

static int tileedit_changepal(HWND dialog,u32 i)
{
	HWND w;
	i&=0xf;

	if (i==(u32)tileedit.curpal) return FALSE;

	/* down */
	w=GetDlgItem(dialog,i+IDC_PALEDIT_00);
	SetWindowLongPtr(w,GWL_EXSTYLE,WS_EX_CLIENTEDGE);
	SetWindowPos(w,NULL,0,0,0,0,SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOSIZE);

	/* prev up */
	w=GetDlgItem(dialog,tileedit.curpal+IDC_PALEDIT_00);
	SetWindowLongPtr(w,GWL_EXSTYLE,WS_EX_DLGMODALFRAME);
	SetWindowPos(w,NULL,0,0,0,0,SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOSIZE);

	tileedit.curpal=i;

	return TRUE;
}

static void tileedit_action_done(void)
{
	tileedit.act=TILEEDIT_ACT_NOTHING;
	tileedit.act_done=TRUE;
}

static void tileedit_clean_undo(_tileedit_undo* u)
{
	int b=(u==_tileedit_undo_begin);
	if (!u->next) return;

	u=u->next;
	for (;;) {
		if (!u->next) {
			MEM_CLEAN(u);
			break;
		}
		u=u->next;
		MEM_CLEAN(u->prev);
	}

	/* all */
	if (b) {
		_tileedit_undo_cursor=_tileedit_undo_begin;
		_tileedit_undo_cursor->next=NULL;
		memcpy(tileedit.p,_tileedit_undo_cursor->p,8);
		memcpy(tileedit.c,_tileedit_undo_cursor->c,8);
	}
}

static int tileedit_findcommonct(u8* ct)
{
	u8 t[0x100];
	int i,j=0,k=0;

	memset(t,0,0x100);
	for (i=0;i<8;i++) t[ct[i]]++;

	/* find most common colour */
	for (i=0;i<0x100;i++) {
		if ((t[i]==k&&(j&0xf)==(j>>4&0xf))||t[i]>k) { j=i; k=t[i]; }
	}

	return j;
}

static int tileedit_screen0cdiff(int mode,u8* ct,int c)
{
	int i;

	if (mode==2||(mode&5)!=1) return FALSE;
	i=tileedit_findcommonct(ct);

	memset(ct,i,8);

	return (c!=i);
}

static int tileedit_screen1cdiff(int mode,u8* ct,int cto,u8* ram)
{
	int i;

	if (mode==2||(mode&5)!=0) return FALSE;
	i=tileedit_findcommonct(ct);

	memset(ct,i,8);

	return (ram[cto|tileedit.pattern_orig>>3]!=i);
}

static int tileedit_apply(HWND dialog,const int ok)
{
	int i,s0=FALSE;
	u8 ram_local[0x4000];
	int reg_local[8];
	int modeo,cto;
	u8 p[8]; u8 c[8]; u8 c2[8];

	/* copy local state */
	memcpy(ram_local,vdp_ram,0x4000);
	for (i=0;i<8;i++) reg_local[i]=vdp_regs[i];
	memcpy(p,tileedit.p,8);
	memcpy(c,tileedit.c,8); memcpy(c2,c,8);

	/* bogus mode */
	if ((reg_local[1]&0x18)==0x18) {
		return MessageBox(dialog,"Invalid mixed mode!\nChanges won't be applied.","I am Error.",MB_ICONEXCLAMATION|(ok?(MB_OKCANCEL|MB_DEFBUTTON2):MB_OK));
	}

	/* screen 3 <-> not screen 3 */
	if ((reg_local[1]>>3^tileedit.screen3)&1) {
		return MessageBox(dialog,"Incompatible screen mode!\nChanges won't be applied.","I am Error.",MB_ICONEXCLAMATION|(ok?(MB_OKCANCEL|MB_DEFBUTTON2):MB_OK));
	}

	modeo=(reg_local[1]>>4&1)|(reg_local[1]>>1&4)|(reg_local[0]&2);
	cto=reg_local[3]<<6;

	/* mode changed */
	if (modeo!=tileedit.mode) {
		if ((s0=tileedit_screen0cdiff(modeo,c,reg_local[7]))) i=MessageBox(dialog,"Screen mode is different, with colour changes affecting\nthe entire surface. Apply anyway?","meisei",MB_ICONEXCLAMATION|(ok?(MB_YESNOCANCEL|MB_DEFBUTTON3):(MB_YESNO|MB_DEFBUTTON2)));
		else if (tileedit_screen1cdiff(modeo,c,cto,ram_local)) i=MessageBox(dialog,"Screen mode is different, with colour changes affecting\nsurrounding tiles. Apply anyway?","meisei",MB_ICONEXCLAMATION|(ok?(MB_YESNOCANCEL|MB_DEFBUTTON3):(MB_YESNO|MB_DEFBUTTON2)));
		else i=MessageBox(dialog,"Screen mode is different.\nApply changes anyway?","meisei",MB_ICONEXCLAMATION|(ok?(MB_YESNOCANCEL|MB_DEFBUTTON3):(MB_YESNO|MB_DEFBUTTON2)));
		if (i!=IDYES) return i;
	}

	/* global colour */
	else if ((s0=tileedit_screen0cdiff(modeo,c,reg_local[7]))) {
		i=MessageBox(dialog,"Colour changes will affect the entire screen.\nApply anyway?","meisei",MB_ICONEXCLAMATION|(ok?(MB_YESNOCANCEL|MB_DEFBUTTON3):(MB_YESNO|MB_DEFBUTTON2)));
		if (i!=IDYES) return i;
	}
	else if (tileedit_screen1cdiff(modeo,c,cto,ram_local)) {
		i=MessageBox(dialog,"Colour changes will affect surrounding tiles too.\nApply anyway?","meisei",MB_ICONEXCLAMATION|(ok?(MB_YESNOCANCEL|MB_DEFBUTTON3):(MB_YESNO|MB_DEFBUTTON2)));
		if (i!=IDYES) return i;
	}

	if (!tileedit_upload(reg_local,p,c,tileedit_allblocks,((modeo&5)==0)?TRUE:s0)) LOG_ERROR_WINDOW(dialog,"Couldn't upload VDP data!");

	for (i=0;i<8;i++) tileedit.vdp_regs[i]=reg_local[i];
	if (tileedit.mode!=modeo) {
		char t[0x100];
		char tr[0x10]={0};
		char tb[0x10]={0};

		if (modeo&2) sprintf(tb,"/%d",tileedit.block_orig);
		if (modeo&4) sprintf(tr,", row %d",tileedit.row_orig);
		sprintf(t,"Pattern $%02X%s%s",tileedit.pattern_orig,tb,tr);
		SetDlgItemText(dialog,IDC_TILEEDIT_ZOOMT,t);

		tileedit.mode=modeo;
	}

	/* screen degraded, undo invalidated */
	if (!ok&&memcmp(c,c2,8)) {
		memcpy(tileedit.p_open,p,8);
		memcpy(tileedit.c_open,c,8);
		tileedit.copy_open=TRUE;
	}

	return IDYES;
}


/* zoom subwindow */
static BOOL CALLBACK tileedit_sub_zoom(HWND wnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	if (!tileedit.zoom.wnd) return DefWindowProc(wnd,msg,wParam,lParam);

	switch (msg) {

		case WM_SETCURSOR:
			SetCursor(tileedit.cursor_cross);
			return 1;
        break;

		case WM_MOUSEMOVE:
			tileedit.in_zoom=input_mouse_in_client(wnd,NULL,&tileedit.p_zoom,TRUE);
			if (tileedit.act==TILEEDIT_ACT_MOVE&&!(wParam&MK_SHIFT)) {
				/* end move (let go of shift) */
				tileedit_action_done();
				if (GetCapture()==wnd) ReleaseCapture();
			}
        break;

		case WM_LBUTTONDOWN:
			/* end whatever */
			tileedit_action_done();
			if (!tileedit.in_zoom) {
				if (GetCapture()==wnd) ReleaseCapture();
				break;
			}

			if (GetCapture()!=wnd) SetCapture(wnd);
			if (wParam&MK_SHIFT) {
				/* start move */
				GetCursorPos(&tileedit.p_move);
				tileedit.p_move_prev.x=tileedit.p_move.x;
				tileedit.p_move_prev.y=tileedit.p_move.y;
				tileedit.act=TILEEDIT_ACT_MOVE;
				SetCursor(tileedit.cursor_size);
			} else {
				/* start fg draw */
				tileedit.act=TILEEDIT_ACT_FG;
				SetCursor(tileedit.cursor_cross);
			}
        break;

		case WM_RBUTTONDOWN:
			/* end whatever */
			tileedit_action_done();
			if (!tileedit.in_zoom) {
				if (GetCapture()==wnd) ReleaseCapture();
				break;
			}

			if (tileedit.screen3) {
				/* pick colour */
				tileedit.rclick=TRUE;
			} else {
				/* start bg draw */
				if (GetCapture()!=wnd) SetCapture(wnd);
				tileedit.act=TILEEDIT_ACT_BG;
				SetCursor(tileedit.cursor_cross);
			}
        break;

		case WM_LBUTTONUP:                              // Warning -Wimplicit-fallthrough
			if (tileedit.act==TILEEDIT_ACT_BG) break;
			tileedit_action_done();
			if (GetCapture()==wnd) ReleaseCapture();

		case WM_RBUTTONUP:
			if (tileedit.act==TILEEDIT_ACT_FG||tileedit.act==TILEEDIT_ACT_MOVE) break;
			tileedit_action_done();
			if (GetCapture()==wnd) ReleaseCapture();
        break;

		case WM_CAPTURECHANGED:
			tileedit_action_done();
        break;

		case WM_PAINT:
        {
			PAINTSTRUCT ps;
			HGDIOBJ obj;
			HDC dc;

			if (tileedit.busy) break;

			dc=BeginPaint(wnd,&ps);

			obj=SelectObject(tileedit.zoom.bdc,tileedit.zoom.bmp);
			StretchBlt(dc,0,0,128,128,tileedit.zoom.bdc,0,0,8,8,SRCCOPY);
			SelectObject(tileedit.zoom.bdc,obj);

			EndPaint(wnd,&ps);
		}
        break;

		default:
        break;
	}

	return DefWindowProc(wnd,msg,wParam,lParam);
}

/* fg subwindow */
static BOOL CALLBACK tileedit_sub_fg(HWND wnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	if (!tileedit.fg.wnd||tileedit.screen3) return DefWindowProc(wnd,msg,wParam,lParam);

	switch (msg) {

		case WM_SETCURSOR:
			SetCursor(tileedit.cursor_cross);
			return 1;
        break;

		case WM_MOUSEMOVE:
			tileedit.in_fg=input_mouse_in_client(wnd,NULL,&tileedit.p_fg,TRUE);
        break;

		case WM_LBUTTONDOWN:
			/* end whatever */
			tileedit_action_done();
			if (!tileedit.in_fg) {
				if (GetCapture()==wnd) ReleaseCapture();
				break;
			}
			/* start draw */
			if (GetCapture()!=wnd) SetCapture(wnd);
			tileedit.act=TILEEDIT_ACT_FC;
			SetCursor(tileedit.cursor_cross);
        break;

		case WM_RBUTTONDOWN:                            // Warning -Wimplicit-fallthrough
			tileedit.rclick=TRUE;
		case WM_LBUTTONUP: case WM_RBUTTONUP:           // Warning -Wimplicit-fallthrough
			if (GetCapture()==wnd) ReleaseCapture();
		case WM_CAPTURECHANGED:
			tileedit_action_done();
        break;

		case WM_PAINT:
        {
			PAINTSTRUCT ps;
			HGDIOBJ obj;
			HDC dc;

			if (tileedit.busy) break;

			dc=BeginPaint(wnd,&ps);

			obj=SelectObject(tileedit.fg.bdc,tileedit.fg.bmp);
			StretchBlt(dc,0,0,22,128,tileedit.fg.bdc,0,0,1,8,SRCCOPY);
			SelectObject(tileedit.fg.bdc,obj);

			EndPaint(wnd,&ps);
		}
        break;

		default:
        break;
	}

	return DefWindowProc(wnd,msg,wParam,lParam);
}

/* bg subwindow */
static BOOL CALLBACK tileedit_sub_bg(HWND wnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	if (!tileedit.bg.wnd||tileedit.screen3) return DefWindowProc(wnd,msg,wParam,lParam);

	switch (msg) {

		case WM_SETCURSOR:
			SetCursor(tileedit.cursor_cross);
			return 1;
        break;

		case WM_MOUSEMOVE:
			tileedit.in_bg=input_mouse_in_client(wnd,NULL,&tileedit.p_bg,TRUE);
        break;

		case WM_LBUTTONDOWN:
			/* end whatever */
			tileedit_action_done();
			if (!tileedit.in_bg) {
				if (GetCapture()==wnd) ReleaseCapture();
				break;
			}

			/* start draw */
			if (GetCapture()!=wnd) SetCapture(wnd);
			tileedit.act=TILEEDIT_ACT_BC;
			SetCursor(tileedit.cursor_cross);
        break;

		case WM_RBUTTONDOWN:                                // Warning -Wimplicit-fallthrough
			tileedit.rclick=TRUE;
		case WM_LBUTTONUP: case WM_RBUTTONUP:               // Warning -Wimplicit-fallthrough
			if (GetCapture()==wnd) ReleaseCapture();
		case WM_CAPTURECHANGED:
			tileedit_action_done();
        break;

		case WM_PAINT:
        {
			PAINTSTRUCT ps;
			HGDIOBJ obj;
			HDC dc;

			if (tileedit.busy) break;

			dc=BeginPaint(wnd,&ps);

			obj=SelectObject(tileedit.bg.bdc,tileedit.bg.bmp);
			StretchBlt(dc,0,0,31,128,tileedit.bg.bdc,0,0,1,8,SRCCOPY);
			SelectObject(tileedit.bg.bdc,obj);

			EndPaint(wnd,&ps);
		}
        break;

		default:
        break;
	}

	return DefWindowProc(wnd,msg,wParam,lParam);
}


/* main window */
static INT_PTR CALLBACK tileview_editor_dialog( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch (msg)
	{

		case WM_INITDIALOG:
        {
			char t[0x100]={0}; char tb[0x10]={0}; char tr[0x10]={0};
			int i;

			tileedit.busy=TRUE;

			tileedit.curpal=0;
			for (i=0;i<0x10;i++) tileedit.pal[i]=-1;

			tileedit_download();
			memcpy(_tileedit_undo_cursor->p,tileedit.p,8); memcpy(_tileedit_undo_cursor->c,tileedit.c,8);

			if (tileedit.mode&2) sprintf(tb,"/%d",tileedit.block_orig);
			if (tileedit.mode&4) sprintf(tr,", row %d",tileedit.row_orig);
			sprintf(t,"Pattern $%02X%s%s",tileedit.pattern_orig,tb,tr);
			SetDlgItemText(dialog,IDC_TILEEDIT_ZOOMT,t);

			tileedit.cursor_cross=LoadCursor(NULL,IDC_CROSS);
			tileedit.cursor_size=LoadCursor(NULL,IDC_SIZEALL);

			if (netplay_is_active()||movie_get_active_state()) {
				EnableWindow(GetDlgItem(dialog,IDOK),FALSE);
				EnableWindow(GetDlgItem(dialog,IDC_TILEEDIT_APPLY),FALSE);
			}

			tileedit.in_fg=FALSE; tileedit.in_fg_prev=2; tileedit.p_fg_prev.x=1000;
			tileedit.in_bg=FALSE; tileedit.in_bg_prev=2; tileedit.p_bg_prev.x=1000;
			tileedit.p_fg.x=tileedit.p_fg.y=tileedit.p_bg.x=tileedit.p_bg.y=0;

			if (!tileedit.screen3) {
				/* init fg */
				tileedit.fg.wnd=GetDlgItem(dialog,IDC_TILEEDIT_FG);
				toolwindow_resize(tileedit.fg.wnd,22,128);
				tileedit.fg.wdc=GetDC(tileedit.fg.wnd);
				tileedit.fg.bdc=CreateCompatibleDC(tileedit.fg.wdc);
				tool_init_bmi(&tileedit.fg.info,1,8,32);
				tileedit.fg.bmp=CreateDIBSection(tileedit.fg.wdc,&tileedit.fg.info,DIB_RGB_COLORS,(void*)&tileedit.fg.screen,NULL,0);
				SetWindowLongPtr(tileedit.fg.wnd,GWLP_WNDPROC,(LONG_PTR)tileedit_sub_fg);

				/* init bg */
				tileedit.bg.wnd=GetDlgItem(dialog,IDC_TILEEDIT_BG);
				toolwindow_resize(tileedit.bg.wnd,31,128);
				tileedit.bg.wdc=GetDC(tileedit.bg.wnd);
				tileedit.bg.bdc=CreateCompatibleDC(tileedit.bg.wdc);
				tool_init_bmi(&tileedit.bg.info,1,8,32);
				tileedit.bg.bmp=CreateDIBSection(tileedit.bg.wdc,&tileedit.bg.info,DIB_RGB_COLORS,(void*)&tileedit.bg.screen,NULL,0);
				SetWindowLongPtr(tileedit.bg.wnd,GWLP_WNDPROC,(LONG_PTR)tileedit_sub_bg);
			}

			/* init zoom */
			tileedit.in_zoom=FALSE; tileedit.in_zoom_prev=2; tileedit.p_zoom_prev.x=1000;
			tileedit.p_zoom.x=tileedit.p_zoom.y=0;
			tileedit.p_move.x=tileedit.p_move.y=0;
			if (!tileedit.screen3) ShowWindow(GetDlgItem(dialog,IDC_TILEEDIT_XY),SW_HIDE);
			tileedit.colour=-1;

			tileedit.zoom.wnd=GetDlgItem(dialog,IDC_TILEEDIT_ZOOM);
			toolwindow_resize(tileedit.zoom.wnd,128,128);
			tileedit.zoom.wdc=GetDC(tileedit.zoom.wnd);
			tileedit.zoom.bdc=CreateCompatibleDC(tileedit.zoom.wdc);
			tool_init_bmi(&tileedit.zoom.info,8,8,32);
			tileedit.zoom.bmp=CreateDIBSection(tileedit.zoom.wdc,&tileedit.zoom.info,DIB_RGB_COLORS,(void*)&tileedit.zoom.screen,NULL,0);
			SetWindowLongPtr(tileedit.zoom.wnd,GWLP_WNDPROC,(LONG_PTR)tileedit_sub_zoom);

			if (!tileedit.screen3) {
				/* init checkbox */
				CheckDlgButton(dialog,IDC_TILEEDIT_ALLBLOCKS,tileedit_allblocks?BST_CHECKED:BST_UNCHECKED);
				tileedit.allblocks_vis=vdp_regs[0]&2;
				if (!tileedit.allblocks_vis) EnableWindow(GetDlgItem(dialog,IDC_TILEEDIT_ALLBLOCKS),FALSE);

				/* position ticks */
				if (!tileedit.tickh) tileedit.tickh=LoadImage(MAIN->module,MAKEINTRESOURCE(ID_BITMAP_TICKH),IMAGE_BITMAP,0,0,0);
				if (!tileedit.tickv) tileedit.tickv=LoadImage(MAIN->module,MAKEINTRESOURCE(ID_BITMAP_TICKV),IMAGE_BITMAP,0,0,0);
				/* up */
				for (i=0;i<8;i++) {
					SetWindowPos(GetDlgItem(dialog,i+IDC_TICKS_HU00),NULL,16+0x10*i,29,2,4,SWP_NOZORDER);
					SendMessage(GetDlgItem(dialog,i+IDC_TICKS_HU00),STM_SETIMAGE,IMAGE_BITMAP,(LPARAM)tileedit.tickh);
				}
				/* down */
				for (i=0;i<8;i++) {
					SetWindowPos(GetDlgItem(dialog,i+IDC_TICKS_HD00),NULL,16+0x10*i,163,2,4,SWP_NOZORDER);
					SendMessage(GetDlgItem(dialog,i+IDC_TICKS_HD00),STM_SETIMAGE,IMAGE_BITMAP,(LPARAM)tileedit.tickh);
				}
				/* left */
				for (i=0;i<8;i++) {
					SetWindowPos(GetDlgItem(dialog,i+IDC_TICKS_VL00),NULL,11,34+0x10*i,4,2,SWP_NOZORDER);
					SendMessage(GetDlgItem(dialog,i+IDC_TICKS_VL00),STM_SETIMAGE,IMAGE_BITMAP,(LPARAM)tileedit.tickv);
				}
				/* right */
				for (i=0;i<10;i++) {
					SetWindowPos(GetDlgItem(dialog,i+IDC_TICKS_VR00),NULL,146,25+0x10*i,0,0,SWP_NOSIZE|SWP_NOZORDER);
				}
			}

			main_parent_window(dialog,MAIN_PW_OUTERL,MAIN_PW_LEFT,-8,0,0);

			tileedit.xy=-1;
			tileedit.ukey_prev=0xc000;
			tileedit.fkey_prev=0xe000;
			tileedit.copy_open=tileedit.rclick=tileedit.act_done=FALSE;
			tileedit.act=TILEEDIT_ACT_NOTHING;

			tileview.ext_wnd=dialog;
			tileedit.busy=FALSE;
		}
        break;

		case WM_DESTROY:
			if (tileview.ext_wnd) {
				int i;

				GdiFlush();
				tileedit.busy=TRUE;
				tileview.ext_wnd=NULL;

				tileedit_clean_undo(_tileedit_undo_begin);
				if (tileedit.tickh) { DeleteObject(tileedit.tickh); tileedit.tickh=NULL; }
				if (tileedit.tickv) { DeleteObject(tileedit.tickv); tileedit.tickv=NULL; }
				for (i=0;i<0x10;i++) {
					if (tileedit.brush[i]) { DeleteObject(tileedit.brush[i]); tileedit.brush[i]=NULL; }
				}

				/* clean up zoom */
				tileedit.zoom.screen=NULL;
				if (tileedit.zoom.bmp) { DeleteObject(tileedit.zoom.bmp); tileedit.zoom.bmp=NULL; }
				if (tileedit.zoom.bdc) { DeleteDC(tileedit.zoom.bdc); tileedit.zoom.bdc=NULL; }
				if (tileedit.zoom.wdc) { ReleaseDC(tileedit.zoom.wnd,tileedit.zoom.wdc); tileedit.zoom.wdc=NULL; }
				tileedit.zoom.wnd=NULL;

				/* clean up fg */
				tileedit.fg.screen=NULL;
				if (tileedit.fg.bmp) { DeleteObject(tileedit.fg.bmp); tileedit.fg.bmp=NULL; }
				if (tileedit.fg.bdc) { DeleteDC(tileedit.fg.bdc); tileedit.fg.bdc=NULL; }
				if (tileedit.fg.wdc) { ReleaseDC(tileedit.fg.wnd,tileedit.fg.wdc); tileedit.fg.wdc=NULL; }
				tileedit.fg.wnd=NULL;

				/* clean up bg */
				tileedit.bg.screen=NULL;
				if (tileedit.bg.bmp) { DeleteObject(tileedit.bg.bmp); tileedit.bg.bmp=NULL; }
				if (tileedit.bg.bdc) { DeleteDC(tileedit.bg.bdc); tileedit.bg.bdc=NULL; }
				if (tileedit.bg.wdc) { ReleaseDC(tileedit.bg.wnd,tileedit.bg.wdc); tileedit.bg.wdc=NULL; }
				tileedit.bg.wnd=NULL;

				main_menu_enable(IDM_TILEVIEW,TRUE);
			}
			break;

		case WM_COMMAND:

			switch (LOWORD(wParam))
            {

				/* load pattern */
				case IDC_TILEEDIT_OPEN:
                {
					const char* filter="Pattern Files (*.pattern)\0*.pattern\0All Files (*.*)\0*.*\0\0";
					const char* title="Open Pattern";
					char fn[STRING_SIZE]={0};
					OPENFILENAME of;

					if (!tileview.ext_wnd) break;

					tileedit.in_zoom=tileedit.in_fg=tileedit.in_bg=FALSE;

					if (tileedit.act) {
						ReleaseCapture();
						tileedit_action_done();
					}

					memset(&of,0,sizeof(OPENFILENAME));

					of.lStructSize=sizeof(OPENFILENAME);
					of.hwndOwner=dialog;
					of.hInstance=MAIN->module;
					of.lpstrFile=fn;
					of.lpstrFilter=filter;
					of.lpstrTitle=title;
					of.nMaxFile=STRING_SIZE;
					of.nFilterIndex=tileedit_fi;
					of.Flags=OFN_ENABLESIZING|OFN_HIDEREADONLY|OFN_FILEMUSTEXIST;
					of.lpstrInitialDir=strlen(tileedit_dir)?tileedit_dir:file->tooldir?file->tooldir:file->appdir;

					main_menu_enable(IDM_TILEVIEW,FALSE); /* resource leak if forced to close */
					if (GetOpenFileName(&of)) {
						int i,j,size;
						u8 data[0x20];
						FILE* fd=NULL;
						int success=FALSE;
						if (!tileview.ext_wnd) {
							/* cleaned up already (check again since time passed) */
							main_menu_enable(IDM_TILEVIEW,TRUE);
							break;
						}
						tileedit_fi=of.nFilterIndex;
						tool_set_pattern_fi_shared(tileedit_fi);
						if (strlen(fn)&&(size=file_open_custom(&fd,fn))>0) {
							if ((size==8||size==0x10||size==0x20)&&file_read_custom(fd,data,size)) {
								if (size==8||size==0x20) {
									if (!tileedit.screen3) {
										/* sprite/tile, no colour data included */
										success=TRUE;
										memcpy(tileedit.p_open,data,8);
									}
								}
								else {
									success=TRUE;
									memcpy(tileedit.c_open,data+8,8);
									if (tileedit.screen3) {
										/* 4 colours, no pattern */
										memset(tileedit.p_open,0xf0,8);
										for (i=1;i<4;i++) tileedit.c_open[i]=tileedit.c_open[0];
										for (i=5;i<8;i++) tileedit.c_open[i]=tileedit.c_open[4];
									}
									else {
										memcpy(tileedit.p_open,data,8);
										if (tileedit.mode!=2) {
											/* screen 0/1, only 2 colours */
											j=tileedit_findcommonct(tileedit.c_open);
											memset(tileedit.c_open,j,8);
										}
									}
								}
							}
						}
						file_close_custom(fd);
						if (success) {
							tileedit.copy_open=TRUE;
							if (strlen(fn+of.nFileOffset)&&(of.nFileExtension-1)>of.nFileOffset) {
								char wintitle[STRING_SIZE]={0};
								fn[of.nFileExtension-1]=0;
								sprintf(wintitle,"Tile Editor - %s",fn+of.nFileOffset);
								SetWindowText(dialog,wintitle);
							}
						}
						else LOG_ERROR_WINDOW(dialog,"Couldn't load pattern!");

						if (strlen(fn)&&of.nFileOffset) {
							fn[of.nFileOffset]=0; strcpy(tileedit_dir,fn);
						}
					}
					PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDC_TILEEDIT_APPLY),TRUE);
					main_menu_enable(IDM_TILEVIEW,TRUE);
				}
                break;

				/* save pattern */
				case IDC_TILEEDIT_SAVE:
                {
					const char* filter="Pattern File (*.pattern)\0*.pattern\0All Files (*.*)\0*.*\0\0";
					const char* defext="\0\0\0\0";
					const char* title="Save Pattern As";
					char fn[STRING_SIZE]={0};
					OPENFILENAME of;

					if (!tileview.ext_wnd) break;

					tileedit.in_zoom=tileedit.in_fg=tileedit.in_bg=FALSE;

					if (tileedit.act) {
						ReleaseCapture();
						tileedit_action_done();
					}

					memset(&of,0,sizeof(OPENFILENAME));

					of.lStructSize=sizeof(OPENFILENAME);
					of.hwndOwner=dialog;
					of.hInstance=MAIN->module;
					of.lpstrFile=fn;
					of.lpstrDefExt=defext;
					of.lpstrFilter=filter;
					of.lpstrTitle=title;
					of.nMaxFile=STRING_SIZE;
					of.nFilterIndex=tileedit_fi;
					of.Flags=OFN_ENABLESIZING|OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
					of.lpstrInitialDir=strlen(tileedit_dir)?tileedit_dir:file->tooldir?file->tooldir:file->appdir;

					main_menu_enable(IDM_TILEVIEW,FALSE); /* resource leak if forced to close */
					if (GetSaveFileName(&of)) {
						u8 data[0x10];
						FILE* fd=NULL;

						if (!tileview.ext_wnd) {
							/* cleaned up already (check again since time passed) */
							main_menu_enable(IDM_TILEVIEW,TRUE);
							break;
						}

						tileedit_fi=of.nFilterIndex;
						tool_set_pattern_fi_shared(tileedit_fi);

						memcpy(data,tileedit.p,8);
						memcpy(data+8,tileedit.c,8);

						if (!strlen(fn)||!file_save_custom(&fd,fn)||!file_write_custom(fd,data,0x10)) LOG_ERROR_WINDOW(dialog,"Couldn't save pattern!");
						else if (strlen(fn+of.nFileOffset)&&(of.nFileExtension-1)>of.nFileOffset) {
							char wintitle[STRING_SIZE]={0};
							fn[of.nFileExtension-1]=0;
							sprintf(wintitle,"Tile Editor - %s",fn+of.nFileOffset);
							SetWindowText(dialog,wintitle);
						}

						file_close_custom(fd);
						if (strlen(fn)&&of.nFileOffset) {
							fn[of.nFileOffset]=0; strcpy(tileedit_dir,fn);
						}
					}

					PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDC_TILEEDIT_APPLY),TRUE);
					main_menu_enable(IDM_TILEVIEW,TRUE);
				}
                break;

				/* allblocks checkbox */
				case IDC_TILEEDIT_ALLBLOCKS:
					if (tileedit.screen3||!tileview.ext_wnd) break;
					tileedit_allblocks=(IsDlgButtonChecked(dialog,IDC_TILEEDIT_ALLBLOCKS)==BST_CHECKED);
                break;

				/* apply */
				case IDC_TILEEDIT_APPLY:
					if (netplay_is_active()||movie_get_active_state()||!tileview.ext_wnd) break;
					tileedit_apply(dialog,FALSE);
                break;

				/* close dialog */
				case IDOK:              // Warning -Wimplicit-fallthrough
					if (netplay_is_active()||movie_get_active_state()||!tileview.ext_wnd||tileedit_apply(dialog,TRUE)==IDCANCEL) break;
				case IDCANCEL:
					if (!tileview.ext_wnd) break;
					EndDialog(dialog,0);
                break;

				default: break;
			} /* WM_COMMAND */

			break;

		case WM_LBUTTONDOWN: {
			/* choose from palette */
			POINT p={0,0};
			RECT r;
			int i;

			if (!tileview.ext_wnd) break;

			GetCursorPos(&p);

			for (i=0;i<0x10;i++) {
				GetWindowRect(GetDlgItem(dialog,i+IDC_PALEDIT_00),&r);
				if (PtInRect(&r,p)) {
					tileedit_changepal(dialog,i);
					break;
				}
			}

			break;
		}

		case WM_CTLCOLORSTATIC: {
			UINT i=GetDlgCtrlID((HWND)lParam);

			/* change palette background */
			if (tileview.ext_wnd&&i>=IDC_PALEDIT_00&&i<=IDC_PALEDIT_15) {
				int j=i-IDC_PALEDIT_00;
				if (tileedit.brush[j]!=NULL) DeleteObject(tileedit.brush[j]);
				i=tileedit.pal[j]; i=(i>>16&0xff)|(i&0xff00)|(i<<16&0xff0000);
				tileedit.brush[j]=CreateSolidBrush(i);
				return tileedit.brush[j] == NULL ? FALSE : TRUE;
			}
			break;
		}

		case TOOL_REPAINT: {
			int* screen;
			u8 p[8]; u8 c[8];
			int v[3]={FALSE,FALSE,FALSE};
			int i,j,fg,bg,xy=-2;
			int pal[0x10];

			if (!tileview.ext_wnd||tileedit.busy) break;
			tileedit.busy=TRUE;

			/* coordinates */
			if (tileedit.in_zoom) {
				tileedit.in_zoom&=input_mouse_in_client(tileedit.zoom.wnd,dialog,&tileedit.p_zoom,GetForegroundWindow()==dialog);
				tileedit.p_zoom.x=tileedit.p_zoom.x>>4&7; tileedit.p_zoom.y=tileedit.p_zoom.y>>4&7;
				if (tileedit.in_zoom) xy=tileedit.p_zoom.x|tileedit.p_zoom.y<<4;
			}
			if (tileedit.in_fg) {
				tileedit.in_fg&=input_mouse_in_client(tileedit.fg.wnd,dialog,&tileedit.p_fg,GetForegroundWindow()==dialog);
				tileedit.p_fg.x=0; tileedit.p_fg.y=tileedit.p_fg.y>>4&7;
				if (tileedit.in_fg) xy=8|tileedit.p_fg.y<<4|((tileedit.mode==2)?0:0xf0);
			}
			if (tileedit.in_bg) {
				tileedit.in_bg&=input_mouse_in_client(tileedit.bg.wnd,dialog,&tileedit.p_bg,GetForegroundWindow()==dialog);
				tileedit.p_bg.x=0; tileedit.p_bg.y=tileedit.p_bg.y>>4&7;
				if (tileedit.in_bg) xy=9|tileedit.p_bg.y<<4|((tileedit.mode==2)?0:0xf0);
			}

			i=tileedit.in_zoom|tileedit.in_fg|tileedit.in_bg;
			if (i!=(tileedit.in_zoom_prev|tileedit.in_fg_prev|tileedit.in_bg_prev)) {
				if (!tileedit.screen3) {
					EnableWindow(GetDlgItem(dialog,IDC_TILEEDIT_XYT),i);
					ShowWindow(GetDlgItem(dialog,IDC_TILEEDIT_XY),i?SW_NORMAL:SW_HIDE);
				}
				EnableWindow(GetDlgItem(dialog,IDC_TILEEDIT_CT),i);
				EnableWindow(GetDlgItem(dialog,IDC_TILEEDIT_C),i);
			}

			if (!tileedit.screen3&&tileedit.xy!=xy) {
				char t[0x100]={0};
				char tx[0x10]={0};
				char ty[0x10]={0};

				if (xy>=0) {
					/* x */
					switch (xy&0xf) {
						case 8: sprintf(tx,"FG"); break;
						case 9: sprintf(tx,"BG"); break;
						default: sprintf(tx,"%d",xy&0xf); break;
					}

					/* y */
					switch (xy>>4&0xf) {
						case 0xf: sprintf(ty,"Y"); break;
						default: sprintf(ty,"%d",xy>>4&0xf); break;
					}

					sprintf(t,"(%s,%s)",tx,ty);
				}
				else sprintf(t," ");

				SetDlgItemText(dialog,IDC_TILEEDIT_XY,t);
				tileedit.xy=xy;
			}

			/* allblocks checkbox visibility */
			if (!tileedit.screen3&&tileedit.allblocks_vis!=(vdp_regs[0]&2)) {
				tileedit.allblocks_vis=vdp_regs[0]&2;
				EnableWindow(GetDlgItem(dialog,IDC_TILEEDIT_ALLBLOCKS),tileedit.allblocks_vis!=0);
			}

			memcpy(pal,draw_palette,0x10*sizeof(int)); pal[0]=pal[vdp_regs[7]&0xf];
			memcpy(p,tileedit.p,8);
			memcpy(c,tileedit.c,8);

			/* hotkey action */
			i=(GetAsyncKeyState(0x45)&0x8000)|(GetAsyncKeyState(0x56)>>1&0x4000)|(GetAsyncKeyState(0x48)>>2&0x2000)|(GetAsyncKeyState(0x49)>>3&0x1000); /* e,v,h,i */
			if (i&&!tileedit.act_done&&tileedit.act==TILEEDIT_ACT_NOTHING&&tileedit.fkey_prev==0&&dialog==GetForegroundWindow()) {
				u8 p2[8]; u8 c2[8];

				switch (i) {

					/* erase */
					case 0x8000:
						if (tileedit.screen3) memset(c,0,8);
						else {
							if (GetAsyncKeyState(VK_SHIFT)&0x8000) memset(c,0xf0,8);
							else memset(p,0,8);
						}
						break;

					/* vertical flip */
					case 0x4000:
						j=8;
						while (j--) { p2[j^7]=p[j]; c2[j^7]=c[j]; }
						memcpy(p,p2,8); memcpy(c,c2,8);

						break;

					/* horizontal flip */
					case 0x2000:
						if (tileedit.screen3) {
							j=8;
							while (j--) c2[j]=(c[j]>>4&0xf)|(c[j]<<4&0xf0);
							memcpy(c,c2,8);
						}
						else {
							int k; j=8; memset(p2,0,8);
							while (j--) {
								k=8;
								while (k--) p2[j]|=((p[j]>>k&1)<<(k^7));
							}
							memcpy(p,p2,8);
						}

						break;

					/* invert */
					case 0x1000:
						if (!tileedit.screen3) {
							j=8;
							while (j--) p[j]^=0xff;
						}
						break;

					/* multiple keys, ignore */
					default: break;
				}

				tileedit.act_done=TRUE;
			}
			tileedit.fkey_prev=i;

			/* next */
			if (tileedit.act_done&&(memcmp(p,_tileedit_undo_cursor->p,8)|memcmp(c,_tileedit_undo_cursor->c,8))) {
				tileedit_clean_undo(_tileedit_undo_cursor);
				MEM_CREATE_T(_tileedit_undo_cursor->next,sizeof(_tileedit_undo),_tileedit_undo*);
				_tileedit_undo_cursor->next->prev=_tileedit_undo_cursor;
				_tileedit_undo_cursor=_tileedit_undo_cursor->next;
				memcpy(_tileedit_undo_cursor->p,p,8);
				memcpy(_tileedit_undo_cursor->c,c,8);
			}
			tileedit.act_done=FALSE;

			/* handle action */
			switch (tileedit.act) {
				case TILEEDIT_ACT_FG:
					if (!tileedit.in_zoom) break;
					if (tileedit.screen3) {
						for (i=tileedit.p_zoom.y&4;i<(tileedit.p_zoom.y&4)+4;i++) c[i]=tileedit.curpal<<(~tileedit.p_zoom.x&4)|(c[i]&(0xf<<(tileedit.p_zoom.x&4)));
					}
					else p[tileedit.p_zoom.y]|=(1<<(7-tileedit.p_zoom.x));
					break;

				case TILEEDIT_ACT_BG:
					if (!tileedit.in_zoom) break;
					p[tileedit.p_zoom.y]&=(1<<(7-tileedit.p_zoom.x)^0xff);
					break;

				case TILEEDIT_ACT_MOVE: {
					int d[2];
					u8 p2[8]; u8 c2[8];
					j=tileedit.screen3;

					/* compute difference */
					GetCursorPos(&tileedit.p_move);
					d[0]=tileedit.p_move.x-tileedit.p_move_prev.x;
					d[1]=tileedit.p_move.y-tileedit.p_move_prev.y;
					for (i=0;i<2;i++) {
						if (d[i]<0) { d[i]=-d[i]; if (d[i]>(0x3f+j)) d[i]=0x3f+j; d[i]=(8-(d[i]>>(4+(j<<1))))&7; }
						else { if (d[i]>(0x3f+j)) d[i]=0x3f+j; d[i]>>=(4+(j<<1)); }
					}
					if (d[0]) tileedit.p_move_prev.x=tileedit.p_move.x;
					if (d[1]) tileedit.p_move_prev.y=tileedit.p_move.y;

					if (tileedit.screen3) {
						if (d[1]) d[1]=4;

						/* rotate x */
						if (d[0]) {
							i=8;
							while (i--) c[i]=(c[i]>>4&0xf)|(c[i]<<4&0xf0);
						}
					}
					else {
						/* rotate x, screen 0/1/2 */
						while (d[0]--) {
							i=8;
							while (i--) p[i]=p[i]>>1|(p[i]<<7&0x80);
						}
					}

					/* rotate y */
					i=8;
					while (i--) {
						p2[(i+d[1])&7]=p[i];
						c2[(i+d[1])&7]=c[i];
					}
					memcpy(p,p2,8);
					memcpy(c,c2,8);

					break;
				}

				case TILEEDIT_ACT_FC:
					if (!tileedit.in_fg) break;
					if (tileedit.mode==2) c[tileedit.p_fg.y]=(c[tileedit.p_fg.y]&0xf)|tileedit.curpal<<4;
					else {
						i=8;
						while (i--) c[i]=(c[i]&0xf)|tileedit.curpal<<4;
					}
					break;

				case TILEEDIT_ACT_BC:
					if (!tileedit.in_bg) break;
					if (tileedit.mode==2) c[tileedit.p_bg.y]=(c[tileedit.p_bg.y]&0xf0)|tileedit.curpal;
					else {
						i=8;
						while (i--) c[i]=(c[i]&0xf0)|tileedit.curpal;
					}
					break;

				default: break;
			}

			/* undo/redo */
			i=(GetAsyncKeyState(0x5a)&0x8000)|(GetAsyncKeyState(0x59)>>1&0x4000); /* z/y */
			if (tileedit.act==TILEEDIT_ACT_NOTHING&&(i==0x8000||i==0x4000)&&tileedit.ukey_prev==0&&dialog==GetForegroundWindow()&&GetAsyncKeyState(VK_CONTROL)&0x8000) {
				if (i==0x8000) _tileedit_undo_cursor=_tileedit_undo_cursor->prev; /* undo */
				else if (_tileedit_undo_cursor->next) _tileedit_undo_cursor=_tileedit_undo_cursor->next; /* redo */

				memcpy(p,_tileedit_undo_cursor->p,8);
				memcpy(c,_tileedit_undo_cursor->c,8);
			}
			tileedit.ukey_prev=i;

			/* colour info */
			if (tileedit.in_zoom) {
				i=(p[tileedit.p_zoom.y]>>(7-tileedit.p_zoom.x)&1)<<2;
				i=(c[tileedit.p_zoom.y]>>i&0xf)|i<<2;
			}
			else if (tileedit.in_fg) i=c[tileedit.p_fg.y]>>4|0x10;
			else if (tileedit.in_bg) i=c[tileedit.p_bg.y]&0xf;
			else i=tileedit.curpal|0x20;

			if (i!=tileedit.colour) {
				char t[0x10];
				char ci[0x10]={0};

				if (!tileedit.screen3&&!(i&0x20)) {
					if (i&0x10) sprintf(ci," FG");
					else sprintf(ci," BG");
				}

				sprintf(t,"%d %s",i&0xf,ci);
				SetDlgItemText(dialog,IDC_TILEEDIT_C,t);
				tileedit.colour=i;
			}

			if (tileedit.in_fg|tileedit.in_bg|(tileedit.in_zoom&tileedit.screen3)&&tileedit.rclick) tileedit_changepal(dialog,i&0xf);
			tileedit.rclick=FALSE;

			/* applied with degrading/loaded */
			if (tileedit.copy_open) {
				memcpy(p,tileedit.p_open,8);
				memcpy(c,tileedit.c_open,8);

				tileedit_clean_undo(_tileedit_undo_begin);
				memcpy(_tileedit_undo_cursor->p,p,8);
				memcpy(_tileedit_undo_cursor->c,c,8);

				tileedit.copy_open=FALSE;
			}

			/* redraw needed? */
			if (memcmp(p,tileedit.p,8)) { memcpy(tileedit.p,p,8); v[0]=TRUE; }
			if (memcmp(c,tileedit.c,8)) { memcpy(tileedit.c,c,8); v[0]=v[1]=v[2]=TRUE; }
			if (memcmp(pal,tileedit.pal,0x10*sizeof(int))) {
				memcpy(tileedit.pal,pal,0x10*sizeof(int));
				v[0]=v[1]=v[2]=TRUE;

				for (i=0;i<0x10;i++) InvalidateRect(GetDlgItem(dialog,i+IDC_PALEDIT_00),NULL,FALSE);
			}

			/* update screen(s) */
			if (tileedit.screen3) v[1]=v[2]=FALSE;
			if (v[0]|v[1]|v[2]) GdiFlush();

			if (v[0]) {
				screen=tileedit.zoom.screen;
				for (i=0;i<8;i++) {
					j=p[i]; fg=pal[c[i]>>4]; bg=pal[c[i]&0xf];
					#define U() *screen++=((j<<=1)&0x100)?fg:bg;
					U(); U(); U(); U(); U(); U(); U(); U();
					#undef U
				}
			}

			if (v[1]) for (i=0;i<8;i++) tileedit.fg.screen[i]=pal[c[i]>>4];
			if (v[2]) for (i=0;i<8;i++) tileedit.bg.screen[i]=pal[c[i]&0xf];


			tileedit.in_zoom_prev=tileedit.in_zoom; tileedit.p_zoom_prev.x=tileedit.p_zoom.x; tileedit.p_zoom_prev.y=tileedit.p_zoom.y;
			tileedit.in_fg_prev=tileedit.in_fg; tileedit.p_fg_prev.x=tileedit.p_fg.x; tileedit.p_fg_prev.y=tileedit.p_fg.y;
			tileedit.in_bg_prev=tileedit.in_bg; tileedit.p_bg_prev.x=tileedit.p_bg.x; tileedit.p_bg_prev.y=tileedit.p_bg.y;

			tileedit.busy=FALSE;

			if (v[0]) InvalidateRect(tileedit.zoom.wnd,NULL,FALSE);
			if (v[1]) InvalidateRect(tileedit.fg.wnd,NULL,FALSE);
			if (v[2]) InvalidateRect(tileedit.bg.wnd,NULL,FALSE);

			break;
		} /* TOOL_REPAINT */

		case TOOL_MENUCHANGED: {
			int i=((netplay_is_active()|movie_get_active_state())==0);
			EnableWindow(GetDlgItem(dialog,IDOK),i);
			EnableWindow(GetDlgItem(dialog,IDC_TILEEDIT_APPLY),i);
			break;
		}

		default: break;
	}

	return 0;
}

/******************************************************************************
 *                                                                            *
 *                      END  "tileedit.c"                                     *
 *                                                                            *
 ******************************************************************************/

static const char* tileview_source_name[TILEVIEW_SOURCE_MAX]={
	"Pattern Table",	"Name Table",		"Name Table + Overlay"
};

const char* tileview_get_source_name(u32 i) { if (i>=TILEVIEW_SOURCE_MAX) return NULL; else return tileview_source_name[i]; }
int tileview_get_source(void) { return tileview_source; }
int tileview_get_open_fi(void) { return tileview_open_fi; }
int tileview_get_save_fi(void) { return tileview_save_fi; }
int tileview_get_openi_nt(void) { return tileview_openi_nt; }
int tileview_get_openi_pt(void) { return tileview_openi_pt; }
int tileview_get_openi_ct(void) { return tileview_openi_ct; }
int tileview_get_open_sim(void) { return tileview_open_sim; }
int tileview_get_save_mask(void) { return tileview_save_mask; }
int tileview_get_save_header(void) { return tileview_save_header; }
int tileedit_get_allblocks(void) { return tileedit_allblocks; }


/* tile follow */
#define L 0xff4000 /* light */
#define D 0xff0000 /* dark */
#define S 0xe01000 /* shadow */
/* 0=black, 1=trans */
static const int tileview_ftile[64]={
	1,1,1,1,1,1,1,1,
	1,L,0,1,L,D,0,1,
	1,D,0,1,D,S,0,1,
	1,L,L,L,L,D,0,1,
	1,D,D,D,D,S,0,1,
	1,L,0,1,L,D,0,1,
	1,D,0,1,D,S,0,1,
	1,1,1,1,1,1,1,1
};
#undef L
#undef D
#undef S

/* overlay font */
/* brightness table */
#define B(x) ((x)&0xff)|((x)<<8&0xff00)|((x)<<16&0xff0000)
static const int tileview_overlay_bri[64]={
	0,0,0,0,0,0,0,0,
	0,B(0xff),B(0xfc),B(0xe0),B(0xff),B(0xfc),B(0xfc),0,
	0,B(0xfe),B(0xf8),B(0xe0),B(0xff),B(0xfc),B(0xf8),0,
	0,B(0xfc),B(0xf8),B(0xc0),B(0xff),B(0xf8),B(0xf0),0,
	0,B(0xf8),B(0xf0),B(0xc0),B(0xfe),B(0xf0),B(0xe0),0,
	0,B(0xf0),B(0xe0),B(0xc0),B(0xfc),B(0xe0),B(0xe0),0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0
};
#undef B
static u8 tileview_overlay_font[0x800];
static int tileview_overlay_pal[0x100];

/* 3 by 5 font */
#define B(x) BIN8(x)
static const u8 tileview_raw_font[0x50]={
/* 0      1      2      3      4      5      6      7      8      9      A      B      C      D      E      F */
B(111),B(010),B(111),B(111),B(101),B(111),B(111),B(111),B(111),B(111),B(010),B(110),B(011),B(110),B(111),B(111),
B(101),B(010),B(001),B(001),B(101),B(100),B(100),B(001),B(101),B(101),B(101),B(101),B(100),B(101),B(100),B(100),
B(101),B(010),B(111),B(011),B(111),B(111),B(111),B(001),B(111),B(111),B(111),B(110),B(100),B(101),B(110),B(110),
B(101),B(010),B(100),B(001),B(001),B(001),B(101),B(010),B(101),B(001),B(101),B(101),B(100),B(101),B(100),B(100),
B(111),B(010),B(111),B(111),B(001),B(111),B(111),B(010),B(111),B(111),B(101),B(110),B(011),B(110),B(111),B(100)
};
#undef B

int __fastcall tileview_get_raw_font(int i) { return tileview_raw_font[i]; }

void tileview_overlay_font_generate(void)
{
	int i,j;

	/* 00 - FF overlay */
	for (i=0;i<0x100;i++) {
		tileview_overlay_font[i<<3]=0;

		/* insert font */
		for (j=0;j<5;j++) {
			tileview_overlay_font[i<<3|(j+1)]=tileview_raw_font[(i>>4&0xf)|j<<4]<<4|tileview_raw_font[(i&0xf)|j<<4]<<1;
		}

		/* bottom lines empty */
		tileview_overlay_font[i<<3|6]=0;
		tileview_overlay_font[i<<3|7]=0;
	}

	/* base rainbow palette */
	j=0x0; for (i=0;i<0x100;i+=6) tileview_overlay_pal[j++]=0xff0000|i<<8;			/* RF G+ B0 */
	i-=0x100; for (;i<0x100;i+=6) tileview_overlay_pal[j++]=0xff00|(0x100-i)<<16;	/* R- GF B0 */
	i-=0x100; for (;i<0x100;i+=6) tileview_overlay_pal[j++]=0xff00|i;				/* R0 GF B+ */
	i-=0x100; for (;i<0x100;i+=6) tileview_overlay_pal[j++]=0xff|(0x100-i)<<8;		/* R0 G- BF */
	i-=0x100; for (;i<0x100;i+=6) tileview_overlay_pal[j++]=0xff|i<<16;				/* R+ G0 BF */
	i-=0x100; for (;i<0x100;i+=6) tileview_overlay_pal[j++]=0xff0000|(0x100-i);		/* RF G0 B- */
}

/* change nametable byte dialog */
static INT_PTR CALLBACK tileview_changent_dialog( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam )
{
    lParam = lParam;
	switch (msg) {

		case WM_INITDIALOG: {
			RECT r;
			char t[0x100]={0};
			sprintf(t,"&NT: B+$%03X:",tileview.ntc_a&0xfff);
			SetDlgItemText(dialog,IDC_TILEVIEW_CHANGENT_A,t);

			SendDlgItemMessage(dialog,IDC_TILEVIEW_CHANGENT_S,UDM_SETRANGE,0,(LPARAM)MAKELONG(255,0));
			SendDlgItemMessage(dialog,IDC_TILEVIEW_CHANGENT_V,EM_LIMITTEXT,3,0);
			SetDlgItemInt(dialog,IDC_TILEVIEW_CHANGENT_V,tileview.ntc_v,FALSE);
			sprintf(t,"($%02X)",tileview.ntc_v);
			SetDlgItemText(dialog,IDC_TILEVIEW_CHANGENT_VX,t);

			/* position window on popup menu location */
			GetWindowRect(GetParent(dialog),&r);
			r.top=tileview.popup_p.y-r.top; if (r.top<0) r.top=0;
			r.left=tileview.popup_p.x-r.left; if (r.left<0) r.left=0;
			main_parent_window(dialog,MAIN_PW_LEFT,MAIN_PW_LEFT,r.left,r.top,0);

			PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDC_TILEVIEW_CHANGENT_V),TRUE);

			tileview.ext_wnd=dialog;
			break;
		}

		case WM_DESTROY:
			tileview.ext_wnd=NULL;
			break;

		case WM_COMMAND:

			switch (LOWORD(wParam)) {

				/* change value */
				case IDC_TILEVIEW_CHANGENT_V:
					if (tileview.ext_wnd&&GetDlgItem(dialog,IDC_TILEVIEW_CHANGENT_V)&&GetDlgItem(dialog,IDC_TILEVIEW_CHANGENT_VX)) {
						u32 i=GetDlgItemInt(dialog,IDC_TILEVIEW_CHANGENT_V,NULL,FALSE);
						if (i>255) { i=255; SetDlgItemInt(dialog,IDC_TILEVIEW_CHANGENT_V,i,FALSE); }
						if (i!=tileview.ntc_v) {
							char t[0x100]={0};
							tileview.ntc_v=i;
							sprintf(t,"($%02X)",tileview.ntc_v);
							SetDlgItemText(dialog,IDC_TILEVIEW_CHANGENT_VX,t);
						}
					}
					break;

				/* close dialog */
				case IDOK:
					if (netplay_is_active()||movie_get_active_state()) break;
					if (!tileview.isnt||!vdp_upload(tileview.ntc_a+(vdp_regs[2]<<10&0x3fff),&tileview.ntc_v,1)) {
						/* error */
						EndDialog(dialog,1);
					}
					else EndDialog(dialog,0);
					break;

				case IDCANCEL:
					EndDialog(dialog,0);
					break;

				default: break;
			}

			break;

		case TOOL_MENUCHANGED:
			EnableWindow(GetDlgItem(dialog,IDOK),(netplay_is_active()|movie_get_active_state())==0);
			break;

		default: break;
	}

	return 0;
}

static int tileview_nthl(int a)
{
	if (!tileview.isnt||!tileview_follow_enabled||netplay_is_active()||movie_get_active_state()) return FALSE;

	tileview.ntc_v=tileview_follow_num;
	return vdp_upload((a+(tileedit.vdp_regs[2]<<10))&0x3fff,&tileview.ntc_v,1);
}

/* tile open/save helper functions */
static void tileview_ct_screen1to2(u8* data)
{
	int i;
	u8 datab[0x800];

	for (i=0;i<0x800;i++) datab[i]=data[i>>6];
	memcpy(data,datab,0x800);
	memcpy(data+0x800,datab,0x800);
	memcpy(data+0x1000,datab,0x800);
}

static int tileview_save_nt(u8* dest,int ntsize,int ptsize,int isnt,HWND dialog)
{
	if (!isnt) {
		int i=MessageBox(dialog,"Tile Viewer currently displays pattern table.\nSave name table as complete pattern list?","meisei",MB_ICONEXCLAMATION|MB_YESNOCANCEL|MB_DEFBUTTON2);
		if (i==IDCANCEL) return TRUE;
		else isnt=i==IDNO;
	}

	if (isnt) memcpy(dest,tileview.vdp_ram+(tileview.vdp_regs[2]<<10&0x3fff),ntsize);
	else {
		int i;
		for (i=0;i<ptsize;i++) dest[i]=i&0xff;
	}

	return FALSE;
}

static void tileview_save_pt2(u8* dest)
{
	int table=tileview.vdp_regs[4]<<11&0x3fff;
	if (!tileview_save_mask) table|=0x1800;
	memcpy(dest,tileview.vdp_ram+(table&0x2000),0x800);
	memcpy(dest+0x800,tileview.vdp_ram+(table&0x2800),0x800);
	memcpy(dest+0x1000,tileview.vdp_ram+(table&0x3000),0x800);
}

static void tileview_save_ct2(u8* dest)
{
	int table=tileview.vdp_regs[3]<<6;
	if (tileview_save_mask) {
		/* mimic mask */
		int i,line,ct,ct_mask;

		for (line=0;line<192;line++) {
			ct=(table&0x2000)|(line&7)|(line<<5&0x1f00);
			ct_mask=table|0x3f;

			for (i=0;i<0x100;i+=8) dest[(ct|i)&0x1fff]=tileview.vdp_ram[(ct|i)&ct_mask];
		}
	}
	else {
		table&=0x2000;
		memcpy(dest,tileview.vdp_ram+table,0x1800);
	}
}

static int tileview_get_screenmode(int* i)
{
	if (i[0]&2) return 2;
	else if (i[1]&8) return 3;
	else if (i[1]&0x10) return 0;
	else return 1;
}

static int tileview_validscreen(int* i)
{
	const int s=(i[1]>>4&1)|(i[1]>>1&4)|(i[0]&2);
	return ((s==0)|(s==1)|(s==2)|(s==4));
}

/* open window (attached to openfilename dialog) */
static UINT_PTR CALLBACK tileview_open_hook(HWND dialog,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch (msg) {
		case WM_INITDIALOG:
			/* init checkboxes */
			CheckDlgButton(dialog,IDC_TILEVIEWOPEN_NT,tileview_openi_nt?BST_CHECKED:BST_UNCHECKED);
			CheckDlgButton(dialog,IDC_TILEVIEWOPEN_PGT,tileview_openi_pt?BST_CHECKED:BST_UNCHECKED);
			CheckDlgButton(dialog,IDC_TILEVIEWOPEN_CT,tileview_openi_ct?BST_CHECKED:BST_UNCHECKED);
			CheckDlgButton(dialog,IDC_TILEVIEWOPEN_SIM,tileview_open_sim?BST_CHECKED:BST_UNCHECKED);

			/* screen valid? */
			if (tileview.validscreen) ShowWindow(GetDlgItem(dialog,IDC_TILEVIEWOPEN_INFO),SW_HIDE);

			/* disable controls? */
			if (tileview_open_fi!=1&&tileview_open_fi!=2&&tileview_open_fi!=6) {
				EnableWindow(GetDlgItem(dialog,IDC_TILEVIEWOPEN_NT),FALSE);
				EnableWindow(GetDlgItem(dialog,IDC_TILEVIEWOPEN_PGT),FALSE);
				EnableWindow(GetDlgItem(dialog,IDC_TILEVIEWOPEN_CT),FALSE);
			}
			else if (tileview.screenmode!=1&&tileview.screenmode!=2) EnableWindow(GetDlgItem(dialog,IDC_TILEVIEWOPEN_CT),FALSE);
			if (tileview_open_fi==3||tileview.screenmode!=2) EnableWindow(GetDlgItem(dialog,IDC_TILEVIEWOPEN_SIM),FALSE);

			break;

		case WM_CTLCOLORSTATIC:
			if (GetDlgCtrlID((HWND)lParam)==IDC_TILEVIEWOPEN_INFO&&!tileview.validscreen) {
				/* warn if mixed mode */
				SetTextColor((HDC)wParam,RGB(200,0,0));
				SetBkMode((HDC)wParam,TRANSPARENT);
				return (HBRUSH)GetStockObject(NULL_BRUSH) == NULL ? FALSE : TRUE;
			}

			break;

		case WM_NOTIFY:

			switch (((LPOFNOTIFY)lParam)->hdr.code) {

				/* file type is changed */
				case CDN_TYPECHANGE: {
					int fi=((LPOFNOTIFY)lParam)->lpOFN->nFilterIndex;

					EnableWindow(GetDlgItem(dialog,IDC_TILEVIEWOPEN_NT),(fi==1)|(fi==2)|(fi==6));
					EnableWindow(GetDlgItem(dialog,IDC_TILEVIEWOPEN_PGT),(fi==1)|(fi==2)|(fi==6));
					EnableWindow(GetDlgItem(dialog,IDC_TILEVIEWOPEN_CT),((fi==1)|(fi==2)|(fi==6))&(tileview.screenmode!=0)&(tileview.screenmode!=3));
					EnableWindow(GetDlgItem(dialog,IDC_TILEVIEWOPEN_SIM),(fi!=3)&(tileview.screenmode==2));
					break;
				}

				/* exit dialog affirmatively */
				case CDN_FILEOK:
					tileview_openi_nt=(IsDlgButtonChecked(dialog,IDC_TILEVIEWOPEN_NT)==BST_CHECKED);
					tileview_openi_pt=(IsDlgButtonChecked(dialog,IDC_TILEVIEWOPEN_PGT)==BST_CHECKED);
					tileview_openi_ct=(IsDlgButtonChecked(dialog,IDC_TILEVIEWOPEN_CT)==BST_CHECKED);
					tileview_open_sim=(IsDlgButtonChecked(dialog,IDC_TILEVIEWOPEN_SIM)==BST_CHECKED);
					break;

				default: break;
			}

			break;

		default: break;
	}

	return 0;
}

/* screen info */
static INT_PTR CALLBACK tileview_screeninfo_dialog( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam )
{
    lParam = lParam;
	switch (msg) {

		case WM_INITDIALOG: {
			char t[0x1000]={0};
			char table[0x200]={0};

			switch (tileview.screenmode) {
				case 0: sprintf(table,"$0000-$03BF: NT\r\n$0800-$0FFF: PGT"); break;
				case 1: sprintf(table,"$0000-$07FF: PGT\r\n$1800-$1AFF: NT\r\n$2000-$201F: CT"); break;
				case 2: sprintf(table,"$0000-$17FF: PGT\r\n$1800-$1AFF: NT\r\n$2000-$37FF: CT"); break;
				default: sprintf(table,"$0000-$07FF: PGT\r\n$0800-$0AFF: NT"); break; /* 3 */
			}

			sprintf(t,"%s\r\n\r\nTo view it on an MSX, save with BLOAD header, and type:\r\nCOLOR %d,%d,%d:SCREEN %d:BLOAD\"foo.sc%d\",S:A$=INPUT$(1)",table,tileview.vdp_regs[7]>>4,tileview.vdp_regs[7]&0xf,tileview.vdp_regs[7]&0xf,tileview.screenmode,tileview.screenmode);

			main_parent_window(dialog,0,0,0,0,0);
			SetFocus(dialog);

			SetDlgItemText(dialog,IDC_TILEVIEW_SINFO_TEXT,t);

			break;
		}

		case WM_CTLCOLORSTATIC:
			/* prevent colour changing for read-only editbox */
			return GetSysColorBrush(COLOR_WINDOW) == NULL ? FALSE : TRUE;
			break;

		case WM_COMMAND:

			switch (LOWORD(wParam)) {

				/* close dialog */
				case IDOK: case IDCANCEL:
					EndDialog(dialog,0);
					break;

				default: break;
			}

			break;

		default: break;
	}

	return 0;
}

/* save window (attached to openfilename dialog) */
static UINT_PTR CALLBACK tileview_save_hook(HWND dialog,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch (msg) {
		case WM_INITDIALOG:
			/* init checkboxes */
			CheckDlgButton(dialog,IDC_TILEVIEWSAVE_MASK,tileview_save_mask?BST_CHECKED:BST_UNCHECKED);
			CheckDlgButton(dialog,IDC_TILEVIEWSAVE_HEADER,tileview_save_header?BST_CHECKED:BST_UNCHECKED);

			/* screen valid? */
			if (tileview.validscreen) {
				char t[0x100];

				if (tileview.screenmode==0) sprintf(t,"      (TextColour: %d, BackDrop: %d)",tileview.vdp_regs[7]>>4,tileview.vdp_regs[7]&0xf);
				else sprintf(t,"      (BackDrop: %d)",tileview.vdp_regs[7]&0xf);
				EnableWindow(GetDlgItem(dialog,IDC_TILEVIEWSAVE_INFO),FALSE);
				SetDlgItemText(dialog,IDC_TILEVIEWSAVE_INFO,t);
			}

			/* disable controls? */
			if (tileview_save_fi!=2) { EnableWindow(GetDlgItem(dialog,IDC_TILEVIEWSAVE_HEADER),FALSE); EnableWindow(GetDlgItem(dialog,IDC_TILEVIEWSAVE_HELP),FALSE); }
			if (tileview.screenmode!=2||tileview_save_fi==1||tileview_save_fi==3) EnableWindow(GetDlgItem(dialog,IDC_TILEVIEWSAVE_MASK),FALSE);
			if (tileview_save_fi==1) ShowWindow(GetDlgItem(dialog,IDC_TILEVIEWSAVE_INFO),SW_HIDE);

			break;

		case WM_SIZE:
			if (wParam!=SIZE_MINIMIZED&&GetDlgItem(dialog,IDC_TILEVIEWSAVE_HELP)) {
				RECT r;

				/* reposition info button */
				GetClientRect(GetParent(dialog),&r);
				SetWindowPos(GetDlgItem(dialog,IDC_TILEVIEWSAVE_HELP),NULL,r.right-87,r.bottom-56,0,0,SWP_NOSIZE|SWP_NOZORDER);
			}
			break;

		case WM_CTLCOLORSTATIC:
			if (GetDlgCtrlID((HWND)lParam)==IDC_TILEVIEWSAVE_INFO&&!tileview.validscreen) {
				/* warn if mixed mode */
				SetTextColor((HDC)wParam,RGB(200,0,0));
				SetBkMode((HDC)wParam,TRANSPARENT);
				return GetStockObject(NULL_BRUSH) == NULL ? FALSE : TRUE;
			}

			break;

		case WM_NOTIFY:

			switch (((LPOFNOTIFY)lParam)->hdr.code) {

				/* file type is changed */
				case CDN_TYPECHANGE: {
					int fi=((LPOFNOTIFY)lParam)->lpOFN->nFilterIndex;

					EnableWindow(GetDlgItem(dialog,IDC_TILEVIEWSAVE_MASK),(fi!=1)&(fi!=3)&(tileview.screenmode==2));
					EnableWindow(GetDlgItem(dialog,IDC_TILEVIEWSAVE_HEADER),fi==2);
					EnableWindow(GetDlgItem(dialog,IDC_TILEVIEWSAVE_HELP),fi==2);
					ShowWindow(GetDlgItem(dialog,IDC_TILEVIEWSAVE_INFO),(fi==1)?SW_HIDE:SW_NORMAL);
					break;
				}

				/* exit dialog affirmatively */
				case CDN_FILEOK:
					tileview_save_mask=(IsDlgButtonChecked(dialog,IDC_TILEVIEWSAVE_MASK)==BST_CHECKED);
					tileview_save_header=(IsDlgButtonChecked(dialog,IDC_TILEVIEWSAVE_HEADER)==BST_CHECKED);
					break;

				default: break;
			}

			break;

		case WM_COMMAND:
			/* help button */
			if (LOWORD(wParam)==IDC_TILEVIEWSAVE_HELP) {
				DialogBox(MAIN->module,MAKEINTRESOURCE(IDD_TILEVIEW_SINFO),dialog,(DLGPROC)tileview_screeninfo_dialog);
			}
			break;

		default: break;
	}

	return 0;
}

/* tilemap subwindow */
static BOOL CALLBACK tileview_sub_tilemap(HWND wnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	if (!tileview.tv.wnd) return DefWindowProc(wnd,msg,wParam,lParam);

	switch (msg) {

		case WM_SETCURSOR:
			SetCursor(tileview.cursor_cross);
			return 1;
			break;

		case WM_MOUSEMOVE: {
			int in;
			POINT p;

			in=input_mouse_in_client(wnd,NULL,&p,TRUE);
			toolwindow_relative_clientpos(wnd,&p,256,192);
			tileview.in_tv=in;
			tileview.p_tv.x=p.x; tileview.p_tv.y=p.y;

			break;
		}

		case WM_LBUTTONDOWN:
			if (wParam&MK_SHIFT) break;
			tileview.dclick=TRUE;
			break;

		case WM_RBUTTONDOWN:
			if (wParam&MK_SHIFT) break;
			tileview.rclickdown=TRUE;
			break;

		case WM_RBUTTONUP:
			tileview.rclick=tileview.rclickdown;
			tileview.rclickdown=FALSE;
			break;

		case WM_PAINT: {
			PAINTSTRUCT ps;
			HGDIOBJ obj;
			HDC dc;
			RECT r;

			if (tileview.busy) break;

			GetClientRect(wnd,&r);
			dc=BeginPaint(wnd,&ps);

			obj=SelectObject(tileview.tv.bdc,tileview.tv.bmp);
			if (r.right==256&&r.bottom==192) BitBlt(dc,0,0,256,192,tileview.tv.bdc,0,0,SRCCOPY);
			else StretchBlt(dc,0,0,r.right,r.bottom,tileview.tv.bdc,0,0,256,192,SRCCOPY);
			SelectObject(tileview.tv.bdc,obj);

			EndPaint(wnd,&ps);

			break;
		}

		default: break;
	}

	return DefWindowProc(wnd,msg,wParam,lParam);
}

/* highlight subwindow */
static BOOL CALLBACK tileview_sub_highlight(HWND wnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch (msg) {

		case WM_PAINT: {
			PAINTSTRUCT ps;
			HGDIOBJ obj;
			HDC dc;

			if (!tileview.h.wnd||tileview.busy) break;

			dc=BeginPaint(wnd,&ps);

			obj=SelectObject(tileview.h.bdc,tileview.h.bmp);
			StretchBlt(dc,0,0,tileview.hlwidth<<2,32,tileview.h.bdc,0,0,tileview.hlwidth,8,SRCCOPY);
			SelectObject(tileview.h.bdc,obj);

			EndPaint(wnd,&ps);

			break;
		}

		default: break;
	}

	return DefWindowProc(wnd,msg,wParam,lParam);
}


static void tileview_update_block_info(HWND dialog,UINT id,int n,int b)
{
	char t[0x100];

	if (b==tileview.block[n]) return;

	if (b==3) sprintf(t,"--");
	else sprintf(t,"p%d",b);

	SetDlgItemText(dialog,id,t);
	tileview.block[n]=b;
}

/* main window */
INT_PTR CALLBACK tileview_window( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch (msg) {

		case WM_INITDIALOG: {
			int i,j;
			HDC d_dc;
			HWND d_wnd;
			char t[0x100];

			if (tileview.tv.wnd) break; /* shouldn't happen :) */

			tileview.busy=TRUE;

			/* tilemap source combobox */
			for (i=0;i<TILEVIEW_SOURCE_MAX;i++) SendDlgItemMessage(dialog,IDC_TILEVIEW_SOURCE,CB_ADDSTRING,0,(LPARAM)((LPCTSTR)tileview_source_name[i]));
			SendDlgItemMessage(dialog,IDC_TILEVIEW_SOURCE,CB_SETCURSEL,tileview_source,0);
			tileview.isnt=(tileview_source!=TILEVIEW_SOURCE_PT);

			/* init tile highlight */
			tileview.in_tv=FALSE; tileview.in_tv_prev=-1; tileview.p_tv_prev.x=1000;
			tileview.p_tv.x=tileview.p_tv.y=0;
			ShowWindow(GetDlgItem(dialog,IDC_TILEVIEW_XY),SW_HIDE);
			tileview.hltile=tileview.nat=tileview.pat=tileview.cat=-2;
			tileview.hlavis=FALSE; ShowWindow(GetDlgItem(dialog,IDC_TILEVIEW_HTEXTA),SW_HIDE); ShowWindow(GetDlgItem(dialog,IDC_TILEVIEW_HTEXTNA),SW_HIDE);
			tileview.hlwidth=8;
			tileview.h.wnd=GetDlgItem(dialog,IDC_TILEVIEW_HTILE);
			toolwindow_resize(tileview.h.wnd,32,32);
			tileview.h.wdc=GetDC(tileview.h.wnd);
			tileview.h.bdc=CreateCompatibleDC(tileview.h.wdc);
			tool_init_bmi(&tileview.h.info,8,8,32);
			tileview.h.bmp=CreateDIBSection(tileview.h.wdc,&tileview.h.info,DIB_RGB_COLORS,(void*)&tileview.h.screen,NULL,0);
			SetWindowLongPtr(tileview.h.wnd,GWLP_WNDPROC,(LONG_PTR)tileview_sub_highlight);

			/* init tile follow */
			tileview.fnum_prev=tileview.ftotal_prev=-1;
			SendDlgItemMessage(dialog,IDC_TILEVIEW_FSPIN,UDM_SETRANGE,0,(LPARAM)MAKELONG(255,0));
			SendDlgItemMessage(dialog,IDC_TILEVIEW_FEDIT,EM_LIMITTEXT,3,0);
			SetDlgItemInt(dialog,IDC_TILEVIEW_FEDIT,tileview_follow_num,FALSE);
			CheckDlgButton(dialog,IDC_TILEVIEW_FCHECK,tileview_follow_enabled?BST_CHECKED:BST_UNCHECKED);

			/* pattern details box, fixed width font */
			d_wnd=GetDlgItem(dialog,IDC_TILEVIEW_SDETAILS);
			d_dc=GetDC(d_wnd);
			if ((tileview.sfont=CreateFont(-MulDiv(10,GetDeviceCaps(d_dc,LOGPIXELSY),72),0,0,0,FW_BOLD,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,FIXED_PITCH|FF_MODERN,"Courier New\0"))!=NULL) {
				SendDlgItemMessage(dialog,IDC_TILEVIEW_SDETAILS,WM_SETFONT,(WPARAM)tileview.sfont,TRUE);
			}
			ReleaseDC(d_wnd,d_dc);

			ShowWindow(GetDlgItem(dialog,IDC_TILEVIEW_DETAILSC),SW_HIDE);
			tileview.dinfo=tileview.dclick=FALSE;
			sprintf(t,"Click on the tilemap to show.");
			SetDlgItemText(dialog,IDC_TILEVIEW_SDETAILS,t);

			/* init tilemap */
			ShowWindow(GetDlgItem(dialog,IDC_TILEVIEW_BLANK),SW_HIDE);
			tileview.blank=tileview.mode=tileview.block[0]=tileview.block[1]=tileview.block[2]=-1;
			tileview.nt=tileview.pt=tileview.ct=-1;
			tileview.tv.wnd=GetDlgItem(dialog,IDC_TILEVIEW_TILES);
			toolwindow_resize(tileview.tv.wnd,256,192);
			tileview.tv.wdc=GetDC(tileview.tv.wnd);
			tileview.tv.bdc=CreateCompatibleDC(tileview.tv.wdc);
			tool_init_bmi(&tileview.tv.info,256,192,32);
			tileview.tv.bmp=CreateDIBSection(tileview.tv.wdc,&tileview.tv.info,DIB_RGB_COLORS,(void*)&tileview.tv.screen,NULL,0);
			SetWindowLongPtr(tileview.tv.wnd,GWLP_WNDPROC,(LONG_PTR)tileview_sub_tilemap);

			/* position btext */
			SetWindowPos(GetDlgItem(dialog,IDC_TILEVIEW_B0),NULL,TILEVIEW_BTEXT_X,TILEVIEW_BTEXT_Y,0,0,SWP_NOSIZE|SWP_NOZORDER);
			SetWindowPos(GetDlgItem(dialog,IDC_TILEVIEW_B1),NULL,TILEVIEW_BTEXT_X,TILEVIEW_BTEXT_Y+64,0,0,SWP_NOSIZE|SWP_NOZORDER);
			SetWindowPos(GetDlgItem(dialog,IDC_TILEVIEW_B2),NULL,TILEVIEW_BTEXT_X,TILEVIEW_BTEXT_Y+128,0,0,SWP_NOSIZE|SWP_NOZORDER);

			tileview.cursor_cross=LoadCursor(NULL,IDC_CROSS);
			EnableWindow(GetDlgItem(dialog,IDC_TILEVIEW_OPEN),(netplay_is_active()|movie_get_active_state())==0);
			tileview.popup_active=tileview.rclick=tileview.rclickdown=FALSE;
			tileview.ext_dialog=0;
			tileview.ext_wnd=NULL;
			toolwindow_setpos(dialog,TOOL_WINDOW_TILEVIEW,MAIN_PW_OUTERR,MAIN_PW_CENTER,8,0,0);

			i=tileview.win_width; j=tileview.win_height;
			tileview.win_width=TILEVIEW_WIN_WIDTH; tileview.win_height=TILEVIEW_WIN_HEIGHT;
			if (i!=0&&j!=0) toolwindow_resize(dialog,i,j);

			tileview.busy=FALSE;

			return 1;
			break;
		}

		case WM_CLOSE:
			if (tool_get_window(TOOL_WINDOW_TILEVIEW)) {

				GdiFlush();
				tileview.busy=TRUE;

				toolwindow_savepos(dialog,TOOL_WINDOW_TILEVIEW);
				main_menu_check(IDM_TILEVIEW,FALSE);
				if (tileview.sfont) { DeleteObject(tileview.sfont); tileview.sfont=NULL; }

				/* clean up tilemap */
				tileview.tv.screen=NULL;
				if (tileview.tv.bmp) { DeleteObject(tileview.tv.bmp); tileview.tv.bmp=NULL; }
				if (tileview.tv.bdc) { DeleteDC(tileview.tv.bdc); tileview.tv.bdc=NULL; }
				if (tileview.tv.wdc) { ReleaseDC(tileview.tv.wnd,tileview.tv.wdc); tileview.tv.wdc=NULL; }
				tileview.tv.wnd=NULL;

				/* clean up highlight */
				tileview.h.screen=NULL;
				if (tileview.h.bmp) { DeleteObject(tileview.h.bmp); tileview.h.bmp=NULL; }
				if (tileview.h.bdc) { DeleteDC(tileview.h.bdc); tileview.h.bdc=NULL; }
				if (tileview.h.wdc) { ReleaseDC(tileview.h.wnd,tileview.h.wdc); tileview.h.wdc=NULL; }
				tileview.h.wnd=NULL;

				main_menu_enable(IDM_TILEVIEW,TRUE);
				DestroyWindow(dialog);
				tool_reset_window(TOOL_WINDOW_TILEVIEW);
			}

			break;

		case WM_SIZING:
			if (tileview.tv.wnd) {
				InvalidateRect(dialog,NULL,FALSE);
				return toolwindow_restrictresize(dialog,wParam,lParam,TILEVIEW_WIN_WIDTH,TILEVIEW_WIN_HEIGHT);
			}

			break;

		case WM_SIZE:
			if (wParam!=SIZE_MINIMIZED&&tileview.tv.wnd) {
				RECT rc,r;
				POINT p;

				GetClientRect(dialog,&rc);

				/* relocate items on Y */
				#define LOCATE(i)															\
					GetWindowRect(GetDlgItem(dialog,i),&r);									\
					p.x=r.left; p.y=r.top; ScreenToClient(dialog,&p);						\
					SetWindowPos(GetDlgItem(dialog,i),NULL,p.x,p.y+(rc.bottom-tileview.win_height),0,0,SWP_NOSIZE|SWP_NOZORDER)

				LOCATE(IDC_TILEVIEW_HTILE);
				LOCATE(IDC_TILEVIEW_HPATT);
				LOCATE(IDC_TILEVIEW_HTEXTT);
				LOCATE(IDC_TILEVIEW_HTEXTAT);
				LOCATE(IDC_TILEVIEW_HTEXTA);
				LOCATE(IDC_TILEVIEW_HTEXTNA);
				LOCATE(IDC_TILEVIEW_FCHECK);
				LOCATE(IDC_TILEVIEW_FSPIN);
				LOCATE(IDC_TILEVIEW_FEDIT);
				LOCATE(IDC_TILEVIEW_FTEXT);
				LOCATE(IDC_TILEVIEW_DETAILST);
				LOCATE(IDC_TILEVIEW_DETAILSC);
				LOCATE(IDC_TILEVIEW_OPEN);
				LOCATE(IDC_TILEVIEW_SAVE);

				#undef LOCATE

				/* relocate items on X and Y */
				#define LOCATE(i)															\
					GetWindowRect(GetDlgItem(dialog,i),&r);									\
					p.x=r.left; p.y=r.top; ScreenToClient(dialog,&p);						\
					SetWindowPos(GetDlgItem(dialog,i),NULL,p.x+(rc.right-tileview.win_width),p.y+(rc.bottom-tileview.win_height),0,0,SWP_NOSIZE|SWP_NOZORDER)

				LOCATE(IDC_TILEVIEW_XY);
				LOCATE(IDOK);

				#undef LOCATE

				/* relocate items on Y and width */
				#define LOCATE(i)															\
					GetWindowRect(GetDlgItem(dialog,i),&r);									\
					p.x=r.left; p.y=r.top; ScreenToClient(dialog,&p);						\
					GetWindowRect(GetDlgItem(dialog,i),&r); OffsetRect(&r,-r.left,-r.top);	\
					SetWindowPos(GetDlgItem(dialog,i),NULL,p.x,p.y+(rc.bottom-tileview.win_height),r.right+(rc.right-tileview.win_width),r.bottom,SWP_NOZORDER)

				LOCATE(IDC_TILEVIEW_SEPARATOR);
				LOCATE(IDC_TILEVIEW_SDETAILS);

				#undef LOCATE

				/* resize tilemap */
				GetWindowRect(GetDlgItem(dialog,IDC_TILEVIEW_TILES),&r); OffsetRect(&r,-r.left,-r.top);
				SetWindowPos(GetDlgItem(dialog,IDC_TILEVIEW_TILES),NULL,0,0,r.right+(rc.right-tileview.win_width),r.bottom+(rc.bottom-tileview.win_height),SWP_NOMOVE|SWP_NOZORDER);

				/* position btext */
				GetClientRect(GetDlgItem(dialog,IDC_TILEVIEW_TILES),&r);
				SetWindowPos(GetDlgItem(dialog,IDC_TILEVIEW_B1),NULL,TILEVIEW_BTEXT_X,TILEVIEW_BTEXT_Y+r.bottom/3.0,0,0,SWP_NOSIZE|SWP_NOZORDER);
				SetWindowPos(GetDlgItem(dialog,IDC_TILEVIEW_B2),NULL,TILEVIEW_BTEXT_X,TILEVIEW_BTEXT_Y+r.bottom/1.5,0,0,SWP_NOSIZE|SWP_NOZORDER);

				tileview.win_width=rc.right; tileview.win_height=rc.bottom;

				InvalidateRect(dialog,NULL,FALSE);
			}
			break;

		case WM_COMMAND:

			switch (LOWORD(wParam)) {

				/* tilemap source */
				case IDC_TILEVIEW_SOURCE: {
					int s;

					if (!tileview.tv.wnd) break; /* cleaned up already */

					if ((HIWORD(wParam))==CBN_SELCHANGE&&(s=SendDlgItemMessage(dialog,LOWORD(wParam),CB_GETCURSEL,0,0))!=CB_ERR) {
						tileview_source=s;
						tileview.isnt=(tileview_source!=TILEVIEW_SOURCE_PT);
					}

					break;
				}

				/* tile follow checkbox */
				case IDC_TILEVIEW_FCHECK:

					if (!tileview.tv.wnd) break; /* cleaned up already */
					tileview_follow_enabled=IsDlgButtonChecked(dialog,IDC_TILEVIEW_FCHECK)==BST_CHECKED;
					break;

				/* tile follow edit/spin */
				case IDC_TILEVIEW_FEDIT:
					if (tileview.tv.wnd&&GetDlgItem(dialog,LOWORD(wParam))!=NULL) {
						int i=GetDlgItemInt(dialog,LOWORD(wParam),NULL,FALSE);
						if (i>255||i<0) { i=255; SetDlgItemInt(dialog,LOWORD(wParam),i,FALSE); }
						if (i!=tileview_follow_num) {
							tileview_follow_num=i;
						}
					}

					break;

				/* load tilemap */
				case IDC_TILEVIEW_OPEN: {
					int i,j,screenmode,shift;
					const char* title="Open Tile Data";
					char filter[STRING_SIZE]={0};
					char fn[STRING_SIZE]={0};
					OPENFILENAME of;

					if (!tileview.tv.wnd||tileview.popup_active||tileview.ext_dialog||netplay_is_active()||movie_get_active_state()) break;

					tileview.ext_dialog=1;
					tileview.in_tv=tileview.rclick=tileview.rclickdown=FALSE;

					/* create local-local copy of vdp regs */
					for (i=0;i<8;i++) tileview.vdp_regs[i]=vdp_regs[i];

					memset(&of,0,sizeof(OPENFILENAME));

					screenmode=tileview_get_screenmode(tileview.vdp_regs);
					tileview.screenmode=screenmode;
					tileview.validscreen=tileview_validscreen(tileview.vdp_regs);

					memcpy(filter,"All Supported Files\0*.sc0;*.sc1;*.sc2;*.sc3;*.sc4;*.nt;*.pgt;*.ct\0Screen X Files (*.scX)\0*.scX\0NT Dumps (*.nt)\0*.nt\0PGT Dumps (*.pgt)\0*.pgt\0CT Dumps (*.ct)\0*.ct\0All Files (*.*)\0*.*\0\0                                                                             ",196);
					filter[73]=filter[86]=filter[93]='0'+screenmode;

					of.lStructSize=sizeof(OPENFILENAME);
					of.hwndOwner=dialog;
					of.hInstance=MAIN->module;
					of.lpstrFile=fn;
					of.lpstrFilter=filter;
					of.lpstrTitle=title;
					of.nMaxFile=STRING_SIZE;
					of.nFilterIndex=tileview_open_fi;
					of.Flags=OFN_ENABLESIZING|OFN_EXPLORER|OFN_HIDEREADONLY|OFN_FILEMUSTEXIST|OFN_ENABLETEMPLATE|OFN_ENABLEHOOK;
					of.lpstrInitialDir=strlen(tileview_dir)?tileview_dir:file->tooldir?file->tooldir:file->appdir;
					of.lpTemplateName=MAKEINTRESOURCE(IDD_TILEVIEW_OPEN);
					of.lpfnHook=tileview_open_hook;

					shift=GetAsyncKeyState(VK_SHIFT);
					main_menu_enable(IDM_TILEVIEW,FALSE); /* resource leak if forced to close */
					if (GetOpenFileName(&of)) {
						int success=FALSE;
						int pno,pgo,cto;
						int prevmode=(tileview.vdp_regs[0]&2)|(tileview.vdp_regs[1]&0x18);
						int size=0;
						u32 fi=of.nFilterIndex;
						FILE* fd=NULL;

						if (!tileview.tv.wnd) {
							/* cleaned up already (check again since time passed) */
							main_menu_enable(IDM_TILEVIEW,TRUE);
							break;
						}

						tileview_open_fi=fi;
						fi--; if (fi<1) fi=5; /* all supported == all files */

						/* mode may have changed */
						for (i=0;i<8;i++) tileview.vdp_regs[i]=vdp_regs[i];
						screenmode=tileview_get_screenmode(tileview.vdp_regs);
						tileview.validscreen=tileview_validscreen(tileview.vdp_regs);

						pno=tileview.vdp_regs[2]<<10&0x3fff;
						pgo=tileview.vdp_regs[4]<<11&0x3fff;
						cto=tileview.vdp_regs[3]<<6;

						if (!netplay_is_active()&&!movie_get_active_state()&&strlen(fn)&&(size=file_open_custom(&fd,fn))>10) {
							u8 data[0x4010];
							u8 datab[0x4000];
							memset(data,0,0x4010);

							if (size<0x4010&&file_read_custom(fd,data,size)) {
								const int screen_source_lut[4][4]={
									{0x0000,0x1800,0x1800,0x0800},	/* nt */
									{0x0800,0x0000,0x0000,0x0000},	/* pgt */
									{0x2000,0x2000,0x2000,0x2000},	/* ct */
									{0x1000,0x2020,0x3800,0x0b00}	/* min size */
								};

								int screenmode_source=screenmode;
								int realsize=size,offset=0;

								file_close_custom(fd);
								fd=NULL;

								/* detect BLOAD header (skip with SHIFT) */
								if (!((shift|GetAsyncKeyState(VK_SHIFT))&0x8000)) {
									int sa=data[2]<<8|data[1];
									int ea=data[4]<<8|data[3];
									if (data[0]==0xfe&&ea>=sa&&size==(8+ea-sa)) {
										offset=7;
										realsize=1+(ea-sa);
										if (realsize>0x4000) realsize=0x4000;
									}
								}

								if (((tileview.vdp_regs[0]&2)|(tileview.vdp_regs[1]&0x18))!=prevmode) {
									if (MessageBox(dialog,"Mode was changed, load anyway?","meisei",MB_ICONEXCLAMATION|MB_YESNO|MB_DEFBUTTON2)==IDYES) {
										if (fi==1) fi=5;
									}
									else fi=0;
								}

								if (fi>4) {
									i=of.nFileExtension;

									switch (realsize) {
										case 0x20: fi=4; break; /* screen 1 ct */
										case 0x300: case 960: fi=2; break; /* nt */
										case 0x800: fi=3; break; /* screen 0/1/3 pt */
										case 0x1800: fi=3+(fn[i]=='c'); break; /* screen 2 pt/ct */

										default:
											/* detect source screenmode */
											if ((fn[i]=='s'||fn[i]=='S')&&fn[i+1]!='\0') {
												int inv=FALSE;

												char t[0x10];
												memset(t,0,0x10);
												t[0]=fn[i+1+(fn[i+2]!='\0')];

												i=0; sscanf(t,"%d",&i); inv=i==0;
												i=1; sscanf(t,"%d",&i); inv&=(i==1);
												if (!inv) {
													if (i==4) i=2; /* screen 4 -> 2 */
													if (i>=0&&i<4) {
														if (tileview_open_sim&&screenmode==2&&i!=2) {
															/* make copies of pt */
															memcpy(datab,data+offset+screen_source_lut[1][i],0x800);
															memcpy(data+offset+screen_source_lut[1][i]+0x800,datab,0x800);
															memcpy(data+offset+screen_source_lut[1][i]+0x1000,datab,0x800);

															if (i==1) tileview_ct_screen1to2(data+offset+0x2000); /* mimic screen 1 ct */
															else if (i==0) {
																/* mimic screen 0 "ct" */
																u8 s0c=tileview.vdp_regs[7]>>4|tileview.vdp_regs[7]<<4;
																if ((((s0c&0xf0)==0||(s0c&0xf0)==0x10)&&((s0c&0xf)==0||(s0c&0xf)==1))||(s0c>>4&0xf)==(s0c&0xf)) s0c=0xf4; /* if not visible, use standard white on blue */
																for (j=0x2000;j<0x3800;j++) data[offset+j]=s0c;
															}
														}
														screenmode_source=i;
													}
												}

												fi=1;
											}
											else {
												/* error */
												LOG_ERROR_WINDOW(dialog,"Couldn't detect file type!");
												fi=0;
											}

											break;
									}
								}

								switch (fi) {
									/* scX */
									case 1:
										if (realsize>=screen_source_lut[3][screenmode_source]) {
											success=3;
											switch (screenmode) {
												/* screen 0 */
												case 0:
													if (tileview_openi_nt) success&=vdp_upload(pno,data+offset+screen_source_lut[0][screenmode_source],960);
													if (tileview_openi_pt) success&=vdp_upload(pgo,data+offset+screen_source_lut[1][screenmode_source],0x800);
													break;

												/* screen 1 */
												case 1:
													if (tileview_openi_nt) success&=vdp_upload(pno,data+offset+screen_source_lut[0][screenmode_source],0x300);
													if (tileview_openi_pt) success&=vdp_upload(pgo,data+offset+screen_source_lut[1][screenmode_source],0x800);
													if (tileview_openi_ct) success&=vdp_upload(cto,data+offset+screen_source_lut[2][screenmode_source],0x20);
													break;

												/* screen 2 */
												case 2:
													if (tileview_openi_nt) success&=vdp_upload(pno,data+offset+screen_source_lut[0][screenmode_source],0x300);
													if (tileview_openi_pt) success&=vdp_upload(pgo&0x2000,data+offset+screen_source_lut[1][screenmode_source],0x1800);
													if (tileview_openi_ct) success&=vdp_upload(cto&0x2000,data+offset+screen_source_lut[2][screenmode_source],0x1800);
													break;

												/* screen 3 */
												case 3:
													if (tileview_openi_nt) success&=vdp_upload(pno,data+offset+screen_source_lut[0][screenmode_source],0x300);
													if (tileview_openi_pt) success&=vdp_upload(pgo,data+offset+screen_source_lut[1][screenmode_source],0x800);
													break;

												default: break;
											}

											if (success&2) {
												LOG_ERROR_WINDOW(dialog,"No data imported!");
												if ((tileview_openi_nt|tileview_openi_pt|tileview_openi_ct)==0) {
													/* set to default if all were unchecked */
													tileview_openi_nt=TILEVIEW_OPENI_NT_DEFAULT;
													tileview_openi_pt=TILEVIEW_OPENI_PT_DEFAULT;
													tileview_openi_ct=TILEVIEW_OPENI_CT_DEFAULT;
												}
											}
										}
										if (!success) LOG_ERROR_WINDOW(dialog,"Couldn't load tilemap!");
										success&=1;

										break;

									/* nt */
									case 2:
										if (!vdp_upload(pno,data+offset,(screenmode==0)?960:0x300)) LOG_ERROR_WINDOW(dialog,"Couldn't load name table!");
										else success=TRUE;
										break;

									/* pgt */
									case 3:
										if (screenmode==2) {
											if (tileview_open_sim&&realsize==0x800) {
												/* 1/3rd, make copies */
												memcpy(datab,data+offset,0x800);
												memcpy(data+offset+0x800,datab,0x800);
												memcpy(data+offset+0x1000,datab,0x800);
											}
											success=vdp_upload(pgo&0x2000,data+offset,0x1800);
										}
										else success=vdp_upload(pgo,data+offset,0x800);
										if (!success) LOG_ERROR_WINDOW(dialog,"Couldn't load pattern table!");
										break;

									/* ct */
									case 4:
										if (screenmode==2) {
											if (tileview.validscreen) {
												if (tileview_open_sim&&realsize==0x20) tileview_ct_screen1to2(data+offset); /* mimic screen 1 */
												success=vdp_upload(cto&0x2000,data+offset,0x1800);
											}
											else success=FALSE;
										}
										else if (screenmode==1) success=vdp_upload(cto,data+offset,0x20);
										if (!success) LOG_ERROR_WINDOW(dialog,"Couldn't load colour table!");
										break;

									default: break;
								}
							}
							else LOG_ERROR_WINDOW(dialog,"Couldn't load tilemap!");
						}
						else LOG_ERROR_WINDOW(dialog,"Couldn't load tilemap!");

						file_close_custom(fd);
						if (success&&strlen(fn+of.nFileOffset)) {
							char wintitle[STRING_SIZE]={0};
							sprintf(wintitle,"Tile Viewer - %s",fn+of.nFileOffset);
							SetWindowText(dialog,wintitle);
						}
						if (strlen(fn)&&of.nFileOffset) {
							fn[of.nFileOffset]=0; strcpy(tileview_dir,fn);
						}
					}

					tileview.ext_dialog=0;
					PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDOK),TRUE);
					main_menu_enable(IDM_TILEVIEW,TRUE);

					break;
				}

				/* save tilemap */
				case IDC_TILEVIEW_SAVE: {
					int i;

					int* dibdata;
					const int dibsize=sizeof(int)*256*192;
					int isnt=tileview.isnt;
					int screenmode;
					const char* defext="png";
					const char* title="Save Tile Data As";
					char filter[STRING_SIZE]={0};
					char fn[STRING_SIZE]={0};
					OPENFILENAME of;

					if (!tileview.tv.wnd||tileview.ext_dialog||tileview.popup_active) break;

					tileview.ext_dialog=1;
					tileview.in_tv=tileview.rclick=tileview.rclickdown=FALSE;

					/* create local-local copy of vdp state */
					memcpy(tileview.vdp_ram,vdp_ram,0x4000);
					for (i=0;i<8;i++) tileview.vdp_regs[i]=vdp_regs[i];
					MEM_CREATE_N(dibdata,dibsize);
					GdiFlush();
					memcpy(dibdata,tileview.tv.screen,dibsize);

					memset(&of,0,sizeof(OPENFILENAME));

					screenmode=tileview_get_screenmode(tileview.vdp_regs);
					tileview.screenmode=screenmode;
					tileview.validscreen=tileview_validscreen(tileview.vdp_regs);

					memcpy(filter,"PNG Image (*.png)\0*.png\0Screen X file (*.scX)\0*.scX\0NT Dump (*.nt)\0*.nt\0PGT Dump (*.pgt)\0*.pgt\0CT Dump (*.ct)\0*.ct\0\0                                                                                               ",130);
					filter[31]=filter[43]=filter[50]='0'+screenmode;

					of.lStructSize=sizeof(OPENFILENAME);
					of.hwndOwner=dialog;
					of.hInstance=MAIN->module;
					of.lpstrFile=fn;
					of.lpstrDefExt=defext;
					of.lpstrFilter=filter;
					of.lpstrTitle=title;
					of.nMaxFile=STRING_SIZE;
					of.nFilterIndex=tileview_save_fi;
					of.Flags=OFN_ENABLESIZING|OFN_EXPLORER|OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_ENABLETEMPLATE|OFN_ENABLEHOOK;
					of.lpstrInitialDir=strlen(tileview_dir)?tileview_dir:file->tooldir?file->tooldir:file->appdir;
					of.lpTemplateName=MAKEINTRESOURCE(IDD_TILEVIEW_SAVE);
					of.lpfnHook=tileview_save_hook;

					main_menu_enable(IDM_TILEVIEW,FALSE); /* resource leak if forced to close */
					if (GetSaveFileName(&of)) {
						int success=FALSE;
						int cancel=FALSE;
						FILE* fd=NULL;
						u8 data[0x4000];

						if (!tileview.tv.wnd) {
							/* cleaned up already (check again since time passed) */
							main_menu_enable(IDM_TILEVIEW,TRUE);
							break;
						}

						memset(data,0,0x4000);

						tileview_save_fi=of.nFilterIndex;

						switch (tileview_save_fi) {

							/* scX */
							case 2: {
								int offset=0;

								switch (screenmode) {
									/* screen 0 */
									case 0:
										/* bload 7 byte header */
										if (tileview_save_header) {
											const u8 header[7]={0xfe,0,0,0xff,0x0f,0,0}; /* $0000-$0FFF */
											memcpy(data,header,7);
											offset+=7;
										}

										/* nametable at $0000 */
										cancel=tileview_save_nt(data+offset,960,0x100,isnt,dialog);
										offset+=0x800;

										/* pattern generator table at $0800 */
										memcpy(data+offset,tileview.vdp_ram+(tileview.vdp_regs[4]<<11&0x3fff),0x800);
										offset+=0x800;

										break;

									/* screen 1 */
									case 1:
										/* bload 7 byte header */
										if (tileview_save_header) {
											const u8 header[7]={0xfe,0,0,0x1f,0x20,0,0}; /* $0000-$201F */
											memcpy(data,header,7);
											offset+=7;
										}

										/* pattern generator table at $0000 */
										memcpy(data+offset,tileview.vdp_ram+(tileview.vdp_regs[4]<<11&0x3fff),0x800);
										offset+=0x1800;

										/* nametable at $1800 */
										cancel=tileview_save_nt(data+offset,0x300,0x100,isnt,dialog);
										offset+=0x800;

										/* colour table at $2000 */
										memcpy(data+offset,tileview.vdp_ram+(tileview.vdp_regs[3]<<6),0x20);
										offset+=0x20;

										break;

									/* screen 2 */
									case 2:
										/* bload 7 byte header */
										if (tileview_save_header) {
											const u8 header[7]={0xfe,0,0,0xff,0x37,0,0}; /* $0000-$37FF */
											memcpy(data,header,7);
											offset+=7;
										}

										/* pattern generator table at $0000 */
										tileview_save_pt2(data+offset);
										offset+=0x1800;

										/* nametable at $1800 */
										cancel=tileview_save_nt(data+offset,0x300,0x300,isnt,dialog);
										offset+=0x800;

										/* colour table at $2000 */
										tileview_save_ct2(data+offset);
										offset+=0x1800;

										break;

									/* screen 3 */
									default:
										/* bload 7 byte header */
										if (tileview_save_header) {
											const u8 header[7]={0xfe,0,0,0xff,0x0a,0,0}; /* $0000-$0AFF */
											memcpy(data,header,7);
											offset+=7;
										}

										/* pattern generator table at $0000 */
										memcpy(data+offset,tileview.vdp_ram+(tileview.vdp_regs[4]<<11&0x3fff),0x800);
										offset+=0x800;

										/* nametable at $0800 */
										cancel=tileview_save_nt(data+offset,0x300,0x100,isnt,dialog);
										offset+=0x300;

										break;
								}

								if (cancel) break;

								/* save */
								if (!offset||!strlen(fn)||!file_save_custom(&fd,fn)||!file_write_custom(fd,data,offset)) LOG_ERROR_WINDOW(dialog,"Couldn't save tilemap!");
								else success=TRUE;

								break;
							}

							/* nt */
							case 3:
								i=(screenmode==0)?960:0x300;
								tileview_save_nt(data,i,i,TRUE,dialog);
								if (!strlen(fn)||!file_save_custom(&fd,fn)||!file_write_custom(fd,data,i)) LOG_ERROR_WINDOW(dialog,"Couldn't save name table!");
								else success=TRUE;

								break;

							/* pgt */
							case 4:
								if (screenmode==2) { i=0x1800; tileview_save_pt2(data); }
								else { i=0x800; memcpy(data,tileview.vdp_ram+(tileview.vdp_regs[4]<<11&0x3fff),i); }
								if (!strlen(fn)||!file_save_custom(&fd,fn)||!file_write_custom(fd,data,i)) LOG_ERROR_WINDOW(dialog,"Couldn't save pattern table!");
								else success=TRUE;

								break;

							/* ct */
							case 5:
								if (screenmode==2) { i=0x1800; tileview_save_ct2(data); }
								else { i=0x20; memcpy(data,tileview.vdp_ram+(tileview.vdp_regs[3]<<6),i); }
								if (screenmode==0||screenmode==3||!strlen(fn)||!file_save_custom(&fd,fn)||!file_write_custom(fd,data,i)) LOG_ERROR_WINDOW(dialog,"Couldn't save colour table!");
								else success=TRUE;

								break;

							/* png */
							default:
								if (!screenshot_save(256,192,SCREENSHOT_TYPE_32BPP,(void*)dibdata,NULL,fn)) LOG_ERROR_WINDOW(dialog,"Couldn't save screenshot!");
								/* no "success" */
								break;
						}

						file_close_custom(fd);
						if (success&&strlen(fn+of.nFileOffset)) {
							char wintitle[STRING_SIZE]={0};
							sprintf(wintitle,"Tile Viewer - %s",fn+of.nFileOffset);
							SetWindowText(dialog,wintitle);
						}
						if (!cancel&&strlen(fn)&&of.nFileOffset) {
							fn[of.nFileOffset]=0; strcpy(tileview_dir,fn);
						}
					}

					tileview.ext_dialog=0;
					PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDOK),TRUE);
					main_menu_enable(IDM_TILEVIEW,TRUE);
					MEM_CLEAN(dibdata);

					break;
				}

				/* close dialog manually */
				case IDOK: case IDCANCEL:
					PostMessage(dialog,WM_CLOSE,0,0);
					break;

				default: break;
			} /* WM_COMMAND */

			break;

		case TOOL_REPAINT: {

			/* background emulation from vdp.c */
			const int toshiba=(vdp_get_chiptype()&~1)==VDP_CHIPTYPE_T7937A;
			const int modeo=(vdp_regs[1]>>4&1)|(vdp_regs[1]>>1&4)|(vdp_regs[0]&2);
			const int pno=vdp_regs[2]<<10&0x3fff;
			const int pgo=vdp_regs[4]<<11&0x3fff;
			const int cto=vdp_regs[3]<<6;
			const int bdo=vdp_regs[7]&0xf;
			const int tco=vdp_regs[7]>>4;
			const int bd=draw_palette[bdo];
			const int tc0=draw_palette[tco?tco:bdo];
			const int follow=(tileview_follow_enabled&tileview_follow_show)?tileview_follow_num:-1;
			int hltile,hlwidth,line,ftotal=0,pn=0,pg=0,ct=0,mode=modeo;
			int* screen;
			int block[3]={3,3,3};

			if (!tileview.tv.wnd||tileview.busy) break;
			tileview.busy=TRUE;

			/* coordinates */
			/* in case mouse moved too fast/to another window */
			if (tileview.in_tv) {
				int in;
				POINT p;

				in=input_mouse_in_client(tileview.tv.wnd,dialog,&p,GetForegroundWindow()==dialog);
				toolwindow_relative_clientpos(tileview.tv.wnd,&p,256,192);
				tileview.p_tv.x=p.x; tileview.p_tv.y=p.y;
				if (!in) tileview.in_tv=tileview.rclickdown=tileview.rclick=0;
			}
			if (tileview.p_tv_prev.x!=tileview.p_tv.x||tileview.p_tv_prev.y!=tileview.p_tv.y) {
				char t[0x100];
				sprintf(t,"(%d,%d)",(int)tileview.p_tv.x,(int)tileview.p_tv.y);
				SetDlgItemText(dialog,IDC_TILEVIEW_XY,t);
			}
			if (tileview.in_tv!=tileview.in_tv_prev) ShowWindow(GetDlgItem(dialog,IDC_TILEVIEW_XY),tileview.in_tv?SW_NORMAL:SW_HIDE);

			GdiFlush();

			screen=tileview.tv.screen;

			if (modeo&2&&modeo&5) mode^=2;

			/* bogus mode */
			if (mode!=0&&mode!=1&&mode!=2&&mode!=4) {
				int i=0xc000;
				while (i--) *screen++=TOOL_DBM_EMPTY;
			}

			else {

			int tileline;

			/* write tiles */
			for (line=0;line<192;line++) {

			pg=pgo; ct=cto; tileline=line&7;

			if (modeo&2) {
				if (toshiba) { pg|=0x1800; ct|=0x1fc0; }
				pg=(line<<5&pg)|(pg&0x2000);

				/* mirror of block 0 (PT mode only) */
				if (!tileview.isnt&&line>63&&mode!=2&&(pg&0x1800)==0) {
					int i=0x100;
					while (i--) *screen++=TOOL_DBM_EMPTY;
					continue;
				}
			}

			/* overlay darkness */
			#define OSHIFT 1
			#define OMASK 0x7f7f7f

			switch (mode) {

				/* screen 1 */
				case 0: {
					int b,c,t,to,x,pd,p,cd;

					if (tileview.isnt) {
						pn=pno|(line<<2&0xfe0);
					}
					else {
						pn=line<<2&0xe0;

						/* only upper part */
						if (line==64&&!(modeo&2)) {
							int i=0x8000;
							while (i--) *screen++=TOOL_DBM_EMPTY;
							line=191; break;
						}
					}

					pg|=tileline;

					/* 32 tiles, 8 pixels per tile */
					for (x=0;x<32;x++) {
						to=tileview.isnt?vdp_ram[pn|x]:pn|x;
						pd=vdp_ram[to<<3|pg];
						cd=vdp_ram[to>>3|ct];

						/* overlay */
						if (tileview_source==TILEVIEW_SOURCE_NTO) {
							#define UPD1o() p=((pd<<=1)&0x100)?cd>>4:cd&0xf; *screen++=((t<<=1)&0x100)?c|tileview_overlay_bri[b]:p?draw_palette[p]>>OSHIFT&OMASK:bd>>OSHIFT&OMASK; b++
							c=tileview_overlay_pal[to];
							t=to<<3|tileline; b=t<<3&0x38;
							t=tileview_overlay_font[t];
							UPD1o(); UPD1o(); UPD1o(); UPD1o(); UPD1o(); UPD1o(); UPD1o(); UPD1o();
						}
						/* normal */
						else {
							#define UPD1() *screen++=(p=((pd<<=1)&0x100)?cd>>4:cd&0xf)?draw_palette[p]:bd
							UPD1(); UPD1(); UPD1(); UPD1(); UPD1(); UPD1(); UPD1(); UPD1();
						}
						/* + follow? */
						if (to==follow) {
							#define UPDf() if (tileview_ftile[t]!=1) *screen=tileview_ftile[t]; screen++; t++
							screen-=8; t=tileline<<3;
							UPDf(); UPDf(); UPDf(); UPDf(); UPDf(); UPDf(); UPDf(); UPDf();
						}
					}

					break;
				}

				/* screen 0 */
				case 1: {
					int b,c,t,to,x,max,pd;

					if (tileview.isnt) {
						pn=pno|((line>>3)*40);
						max=40;
					}
					else {
						pn=(line>>3&7)*40;
						max=(pn==240)?16:40;

						if (pn==280) break;

						/* only upper part */
						if (line==64&&!(modeo&2)) {
							int i=0x8000;
							while (i--) *screen++=TOOL_DBM_EMPTY;
							line=191; break;
						}
					}

					/* on the actual screen it's left:6 pixels, right:10 pixels, in the tileviewer it looks better when centered */
					/* left border */
					*screen++=TOOL_DBM_EMPTY; *screen++=TOOL_DBM_EMPTY; *screen++=TOOL_DBM_EMPTY; *screen++=TOOL_DBM_EMPTY;
					*screen++=TOOL_DBM_EMPTY; *screen++=TOOL_DBM_EMPTY; *screen++=TOOL_DBM_EMPTY; *screen++=TOOL_DBM_EMPTY;

					pg|=tileline;

					/* 40 tiles, 6 pixels per tile */
					for (x=0;x<max;x++) {
						to=tileview.isnt?vdp_ram[pn+x]:pn+x;
						pd=vdp_ram[to<<3|pg];

						/* overlay */
						if (tileview_source==TILEVIEW_SOURCE_NTO) {
							#define UPD0o() pd<<=1; b++; *screen++=((t<<=1)&0x100)?c|tileview_overlay_bri[b]:(pd&0x100)?tc0>>OSHIFT&OMASK:bd>>OSHIFT&OMASK
							c=tileview_overlay_pal[to];
							t=to<<3|tileline; b=t<<3&0x38;
							t=tileview_overlay_font[t]<<1;
							UPD0o(); UPD0o(); UPD0o(); UPD0o(); UPD0o(); UPD0o();
							#undef UPD0o
						}
						/* normal */
						else {
							#define UPD0() *screen++=((pd<<=1)&0x100)?tc0:bd;
							UPD0(); UPD0(); UPD0(); UPD0(); UPD0(); UPD0();
						}
						/* + follow? */
						if (to==follow) {
							screen-=6; t=tileline<<3|1;
							UPDf(); UPDf(); UPDf(); UPDf(); UPDf(); UPDf();
						}
					}

					/* fill remaining part on bottom tile row in PN mode */
					if (max==16) {
						x=144;
						while (x--) *screen++=TOOL_DBM_EMPTY;

						if ((line&63)==55) {
							x=0x800;
							while (x--) *screen++=TOOL_DBM_EMPTY;
						}
					}

					/* right border */
					*screen++=TOOL_DBM_EMPTY; *screen++=TOOL_DBM_EMPTY; *screen++=TOOL_DBM_EMPTY; *screen++=TOOL_DBM_EMPTY;
					*screen++=TOOL_DBM_EMPTY; *screen++=TOOL_DBM_EMPTY; *screen++=TOOL_DBM_EMPTY; *screen++=TOOL_DBM_EMPTY;

					break;
				}

				/* screen 2 */
				case 2: {
					int b,c,t,to,x,pd,p,cd,ct_mask=ct|0x3f;
					pn=tileview.isnt?pno|(line<<2&0xfe0):line<<2&0xe0;

					pg|=tileline;
					ct=(ct&0x2000)|tileline|(line<<5&0x1800);

					/* 32 tiles, 8 pixels per tile */
					for (x=0;x<32;x++) {
						to=tileview.isnt?vdp_ram[pn|x]:pn|x; cd=to<<3;
						pd=vdp_ram[cd|pg];
						cd=vdp_ram[(cd|ct)&ct_mask];

						/* overlay */
						if (tileview_source==TILEVIEW_SOURCE_NTO) {
							c=tileview_overlay_pal[to];
							t=to<<3|tileline; b=t<<3&0x38;
							t=tileview_overlay_font[t];
							UPD1o(); UPD1o(); UPD1o(); UPD1o(); UPD1o(); UPD1o(); UPD1o(); UPD1o();
							#undef UPD1o
						}
						/* normal */
						else { UPD1(); UPD1(); UPD1(); UPD1(); UPD1(); UPD1(); UPD1(); UPD1(); }
						/* + follow? */
						if (to==follow) {
							screen-=8; t=tileline<<3;
							UPDf(); UPDf(); UPDf(); UPDf(); UPDf(); UPDf(); UPDf(); UPDf();
						}
					}

					break;
				}

				/* screen 3 (squashed in PT mode) */
				case 4: {
					int b,c,t,to,x,pd,p;

					if (tileview.isnt) {
						pn=pno|(line<<2&0xfe0);
						pg|=(line>>2&7);
					}
					else {
						pn=line<<2&0xe0;
						pg|=tileline;

						/* only upper part */
						if (line==64&&!(modeo&2)) {
							int i=0x8000;
							while (i--) *screen++=TOOL_DBM_EMPTY;
							line=191; break;
						}
					}

					/* 32 tiles, 8 pixels per 'tile' */
					for (x=0;x<32;x++) {
						to=tileview.isnt?vdp_ram[pn|x]:pn|x;
						pd=vdp_ram[to<<3|pg];

						#define UPD3()										\
						/* left 4 pixels */									\
						p=pd>>4; p=p?draw_palette[p]:bd;					\
						*screen++=p; *screen++=p; *screen++=p; *screen++=p;	\
																			\
						/* right 4 pixels */								\
						p=pd&0xf; p=p?draw_palette[p]:bd;					\
						*screen++=p; *screen++=p; *screen++=p; *screen++=p

						/* overlay */
						if (tileview_source==TILEVIEW_SOURCE_NTO) {
							#define UPD3o() *screen++=((t<<=1)&0x100)?c|tileview_overlay_bri[b]:p; b++
							c=tileview_overlay_pal[to];
							t=to<<3|tileline; b=t<<3&0x38;
							t=tileview_overlay_font[t];

							/* left 4 pixels */
							p=pd>>4; p=p?draw_palette[p]>>OSHIFT&OMASK:bd>>OSHIFT&OMASK;
							UPD3o(); UPD3o(); UPD3o(); UPD3o();

							/* right 4 pixels */
							p=pd&0xf; p=p?draw_palette[p]>>OSHIFT&OMASK:bd>>OSHIFT&OMASK;
							UPD3o(); UPD3o(); UPD3o(); UPD3o();

							#undef UPD3o
						}
						/* normal */
						else { UPD3(); }
						/* + follow? */
						if (to==follow) {
							screen-=8; t=tileline<<3;
							UPDf(); UPDf(); UPDf(); UPDf(); UPDf(); UPDf(); UPDf(); UPDf();
							#undef UPDf
						}
					}

					break;
				}

				default: break;
			}

			} /* big loop */

			#undef OSHIFT
			#undef OMASK

			/* pattern block info */
			block[0]=0;											/* p0 -- -- */
			if (modeo&2) {
				/* screen 2 (or mixed) */
				block[1]=1; block[2]=2;

				if (!toshiba)
				switch (pgo&0x1800) {
					case 0: block[1]=block[2]=0; break;			/* p0 p0 p0 */
					case 0x800: block[1]=1; block[2]=0; break;	/* p0 p1 p0 */
					case 0x1000: block[1]=0; block[2]=2; break;	/* p0 p0 p2 */
					case 0x1800: block[1]=1; block[2]=2; break;	/* p0 p1 p2 */

					default: break;
				}
			}

			} /* big else */

			tileview_update_block_info(dialog,IDC_TILEVIEW_B0,0,block[0]);
			tileview_update_block_info(dialog,IDC_TILEVIEW_B1,1,block[1]);
			tileview_update_block_info(dialog,IDC_TILEVIEW_B2,2,block[2]);

			/* blank info */
			if ((vdp_regs[1]&0x40)!=tileview.blank) {
				tileview.blank=vdp_regs[1]&0x40;
				ShowWindow(GetDlgItem(dialog,IDC_TILEVIEW_BLANK),tileview.blank?SW_HIDE:SW_NORMAL);
			}

			/* mode info */
			if (modeo!=tileview.mode) {
				EnableWindow(GetDlgItem(dialog,IDC_TILEVIEW_M1),(modeo&1)!=0);
				EnableWindow(GetDlgItem(dialog,IDC_TILEVIEW_M2),(modeo&4)!=0);
				EnableWindow(GetDlgItem(dialog,IDC_TILEVIEW_M3),(modeo&2)!=0);
				tileview.mode=modeo;
			}

			/* name table info */
			if (pno!=tileview.nt) {
				char t[0x100];
				sprintf(t,"(NT: $%04X,",pno);

				SetDlgItemText(dialog,IDC_TILEVIEW_NT,t);
				tileview.nt=pno;
			}

			/* pattern generator table info */
			if (pgo!=tileview.pt) {
				char t[0x100];
				sprintf(t,"PGT: $%04X,",pgo);

				SetDlgItemText(dialog,IDC_TILEVIEW_PT,t);
				tileview.pt=pgo;
			}

			/* colour table info */
			if (cto!=tileview.ct) {
				char t[0x100];
				sprintf(t,"CT: $%04X)",cto);

				SetDlgItemText(dialog,IDC_TILEVIEW_CT,t);
				tileview.ct=cto;
			}


			/* highlighted/selected tile */
			pn=-2; pg=pgo; ct=cto;
			if (toshiba&&modeo&2) { pg|=0x1800; ct|=0x1fc0; }
			screen=tileview.h.screen;

			if (tileview.in_tv&&tileview.tv.screen[tileview.p_tv.y*256+tileview.p_tv.x]!=TOOL_DBM_EMPTY) {
				int a,cd,pd,p,i,j,k,but=0,tile=0,row=0,block=tileview.p_tv.y>>6&3;
				pn=-1;

				/* get mouse button state for nametable drawing */
				if (GetAsyncKeyState(VK_SHIFT)&0x8000) {
					but=(GetAsyncKeyState(VK_LBUTTON)&0x8000)|(GetAsyncKeyState(VK_RBUTTON)>>1&0x4000);
					if (but&0xc000&&GetSystemMetrics(SM_SWAPBUTTON)) but^=0xc000;
				}

				switch (mode) {
					/* screen 0 */
					case 1:
						tile=((tileview.p_tv.x-8)/6)+(40*(tileview.p_tv.y>>3&7));

						/* correct for NT mode where tile is NT index */
						if (tileview.isnt) {
							pn=pno+(tile+(block*320));
							tile=vdp_ram[pn];
						}

						/* get address */
						if (modeo&2) pg=(block<<11&pg)|(pg&0x2000);
						pg|=(tile<<3);
						ct=-1; /* TC/BD */

						/* fill */
						for (i=0;i<8;i++) {
							pd=vdp_ram[pg|i];
							UPD0(); UPD0(); UPD0(); UPD0(); UPD0(); UPD0();
							screen+=2;
						}

						break;

					/* screen 3 */
					case 4:
						tile=(tileview.p_tv.x>>3&0x1f)|(tileview.p_tv.y<<2&0xe0);
						row=tileview.p_tv.y>>1&3;

						/* correct for NT mode where tile is NT index */
						if (tileview.isnt) {
							pn=pno|tile|block<<8;
							tile=vdp_ram[pn];

							row=tileview.p_tv.y>>3&3;
						}

						/* get address */
						if (modeo&2) pg=(block<<11&pg)|(pg&0x2000);
						pg|=(tile<<3|row<<1);
						ct=-1; /* unused */

						/* fill */
						pd=vdp_ram[pg]; UPD3(); UPD3(); UPD3(); UPD3();
						pd=vdp_ram[pg|1]; UPD3(); UPD3(); UPD3(); UPD3();

						break;

					/* screen 1/2 */
					default:
						tile=(tileview.p_tv.x>>3&0x1f)|(tileview.p_tv.y<<2&0xe0);

						/* correct for NT mode where tile is NT index */
						if (tileview.isnt) {
							pn=pno|tile|block<<8;
							tile=vdp_ram[pn];
						}

						/* get address */
						if (modeo&2) pg=(block<<11&pg)|(pg&0x2000);
						pg|=(tile<<3);
						if (mode==2) {
							/* screen 2 colour */
							j=7; /* ormask */
							ct=((ct&0x2000)|(block<<11&0x1800)|tile<<3)&(ct|0x3f);
						}
						else {
							/* screen 1 colour */
							j=0;
							ct=tile>>3|ct;
						}

						/* fill */
						for (i=0;i<8;i++) {
							pd=vdp_ram[pg|i];
							cd=vdp_ram[ct|(i&j)];
							UPD1(); UPD1(); UPD1(); UPD1(); UPD1(); UPD1(); UPD1(); UPD1();
						}

						break;
				}

				hltile=tile|block<<8|row<<12|(pg<<5&0x30000)|modeo<<20;

				/* left click */
				if (tileview.dclick) {
					char dinfo[0x1000]={0};
					char t[0x1000];

					/* show table info */
					if (!tileview.dinfo) {
						ShowWindow(GetDlgItem(dialog,IDC_TILEVIEW_DETAILSC),SW_NORMAL);

						tileview.dinfo=TRUE;
					}

					if (modeo&2) sprintf(dinfo,"/%d",hltile>>16&3); /* block */
					sprintf(t,"&Pattern $%02X%s Generator Data",tile,dinfo);
					SetDlgItemText(dialog,IDC_TILEVIEW_DETAILST,t);
					dinfo[0]=0;

					/* detailed info */
					k=mode==4?2:8;

					for (j=0;j<k;j++) {
						/* pattern generator address */
						sprintf(t,"%04X: ",pg|j);
						strcat(dinfo,t);

						/* binary data */
						i=8;
						while (i--) {
							sprintf(t,"%c",(vdp_ram[pg|j]>>i&1)?'1':TOOL_DBIN0);
							strcat(dinfo,t);
						}

						/* hex data */
						sprintf(t," %02X",vdp_ram[pg|j]);
						strcat(dinfo,t);

						/* colour data */
						switch (mode) {
							/* screen 1 */
							case 0:
								if (j==0) {
									/* 1 byte per tile */
									sprintf(t," - %04X: %02X",ct,vdp_ram[ct]);
									strcat(dinfo,t);
								}
								break;

							/* screen 0 */
							case 1:
								if (j==0) {
									/* tc/bd */
									sprintf(t," - TC:%X BD:%X",tco,bdo);
									strcat(dinfo,t);
								}
								break;

							/* screen 2 */
							case 2:
								sprintf(t," - %04X: %02X",ct|j,vdp_ram[ct|j]);
								strcat(dinfo,t);
								break;

							/* screen 3 */
							case 4:
								if (j==0) strcat(dinfo," - unused");
								break;

							default: break;
						}

						/* next line */
						if (j<(k-1)) strcat(dinfo,"\r\n");
					}

					t[0]=0; SetDlgItemText(dialog,IDC_TILEVIEW_SDETAILS,t);
					SendMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDC_TILEVIEW_SDETAILS),TRUE);
					SetDlgItemText(dialog,IDC_TILEVIEW_SDETAILS,dinfo);
				}

				a=((pn-pno)&0x3fff)|block<<16|row<<20;

				/* right click, popup menu */
				if (tileview.rclick) PostMessage(dialog,TOOL_POPUP,tile,a);

				/* shift + right click, highlight */
				if (but&0x4000) {
					if (!tileview_follow_enabled) {
						tileview_follow_enabled=TRUE;
						CheckDlgButton(dialog,IDC_TILEVIEW_FCHECK,BST_CHECKED);
					}

					if (tile!=tileview_follow_num) SetDlgItemInt(dialog,IDC_TILEVIEW_FEDIT,tile,FALSE);
				}

				/* shift + left click, nt -> hl */
				if (but&0x8000) tileview_nthl(a); /* don't care about error */
			}
			else {
				/* invalid highlight */
				if (tileview.popup_active) {
					/* don't redraw highlight/info */
					hltile=tileview.hltile;
					pn=tileview.nat;
					pg=tileview.pat;
					ct=tileview.cat;
				}
				else {
					int i=64;
					while (i--) *screen++=TOOL_DBM_EMPTY;

					hltile=-1;
					pg=-1;
				}
			}

			tileview.busy=FALSE;

			if (hltile!=tileview.hltile) {
				char t[0x100];

				if (hltile<0) {
					sprintf(t,"none targeted");

					/* address invalid */
					if (tileview.hlavis) {
						ShowWindow(GetDlgItem(dialog,IDC_TILEVIEW_HTEXTA),SW_HIDE);
						ShowWindow(GetDlgItem(dialog,IDC_TILEVIEW_HTEXTNA),SW_HIDE);
						EnableWindow(GetDlgItem(dialog,IDC_TILEVIEW_HTEXTAT),FALSE);
						tileview.hlavis=FALSE;
					}
				}
				else {
					char row[0x100]={0};
					char blc[0x100]={0};
					if (mode==4) { sprintf(blc,", row %d",hltile>>12&3); sprintf(row,"r%d",hltile>>12&3); }
					if (modeo&2) sprintf(blc,", block %d/%d%s",hltile>>8&3,hltile>>16&3,row); /* colour/physical block (always 0/1/2), pattern block + row if screen 3 mixed */
					sprintf(t,"$%02X (%d)%s",hltile&0xff,hltile&0xff,blc);

					/* address valid */
					if (!tileview.hlavis) {
						ShowWindow(GetDlgItem(dialog,IDC_TILEVIEW_HTEXTA),SW_NORMAL);
						ShowWindow(GetDlgItem(dialog,IDC_TILEVIEW_HTEXTNA),SW_NORMAL);
						EnableWindow(GetDlgItem(dialog,IDC_TILEVIEW_HTEXTAT),TRUE);
						tileview.hlavis=TRUE;
					}
				}

				SetDlgItemText(dialog,IDC_TILEVIEW_HTEXTT,t);
				tileview.hltile=hltile;
			}

			/* address info */
			/* nametable */
			if (pn!=tileview.nat) {
				char t[0x100];

				if (pn<0||!tileview.isnt) sprintf(t," ");
				else sprintf(t,"NT: $%04X (base + $%03X)",pn,pn-pno);
				SetDlgItemText(dialog,IDC_TILEVIEW_HTEXTNA,t);

				tileview.nat=pn;
			}
			/* pattern generator table/colour table */
			if (pg!=tileview.pat||ct!=tileview.cat) {
				char t[0x100];
				char c[0x100];

				/* colour */
				switch (mode) {
					case 1: sprintf(c,", TC: $%X, BD: $%X",tco,bdo); break;
					case 4: c[0]=0; break; /* screen 3, none */
					default: sprintf(c,", CT: $%04X",ct);
				}

				sprintf(t,"PGT: $%04X%s",pg,c);
				SetDlgItemText(dialog,IDC_TILEVIEW_HTEXTA,t);

				tileview.pat=pg; tileview.cat=ct;
			}

			/* follow info */
			for (pn=pno+((mode==1)?959:767);pn>=pno;pn--) {
				ftotal+=(vdp_ram[pn]==tileview_follow_num);
			}
			if (ftotal!=tileview.ftotal_prev||tileview_follow_num!=tileview.fnum_prev) {
				char t[0x100];

				if (ftotal) sprintf(t,"($%02X, %d in NT)",tileview_follow_num,ftotal);
				else sprintf(t,"($%02X, not in NT)",tileview_follow_num);
				SetDlgItemText(dialog,IDC_TILEVIEW_FTEXT,t);

				tileview.ftotal_prev=ftotal;
				tileview.fnum_prev=tileview_follow_num;
			}

			tileview.in_tv_prev=tileview.in_tv;
			tileview.p_tv_prev.x=tileview.p_tv.x; tileview.p_tv_prev.y=tileview.p_tv.y;
			tileview.rclick=tileview.dclick=FALSE;

			/* resize highlight? */
			hlwidth=tileview.popup_active?tileview.hlwidth:(mode==1)?6:8; /* screen 0, tiles have width 6 */
			if (hlwidth!=tileview.hlwidth) {
				toolwindow_resize(tileview.h.wnd,hlwidth<<2,32);
				tileview.hlwidth=hlwidth;
			}

			InvalidateRect(tileview.tv.wnd,NULL,FALSE);
			InvalidateRect(tileview.h.wnd,NULL,FALSE);

			if (tileview.ext_dialog&&tileview.ext_wnd) PostMessage(tileview.ext_wnd,TOOL_REPAINT,0,0);

			#undef UPD0
			#undef UPD1
			#undef UPD3

			break;
		} /* TOOL_REPAINT */

		case TOOL_POPUP: {
			int i,gray,gray_nthl,gray_paste;
			HMENU menu,popup;
			UINT id;
			int in_tv_prev=tileview.in_tv;
			int pno;
			pno = pno;

			if (!tileview.tv.wnd||tileview.popup_active||tileview.ext_dialog) break;
			if ((menu=LoadMenu(MAIN->module,tileview.isnt?"tileviewntpopup":"tileviewpatternpopup"))==NULL) break;

			tileview.in_tv=FALSE;
			tileview.popup_active=TRUE;

			/* create local-local copy of vdp state */
			memcpy(tileedit.vdp_ram,vdp_ram,0x4000);
			for (i=0;i<8;i++) tileedit.vdp_regs[i]=vdp_regs[i];
			pno=tileedit.vdp_regs[2]<<10&0x3fff;
			tileedit.pattern_orig=wParam;
			tileedit.block_orig=lParam>>16&3;
			tileedit.row_orig=lParam>>20&3;

			if ((int)wParam==tileview_follow_num&&tileview_follow_enabled) CheckMenuItem(menu,IDM_TILEVIEW_HL,MF_CHECKED);
			if (tileview_follow_show) CheckMenuItem(menu,IDM_TILEVIEW_SHOWHL,MF_CHECKED);

			gray=netplay_is_active()|movie_get_active_state();
			gray_nthl=gray|(tileview_follow_enabled==0);
			gray_paste=gray|(tileview.paste_valid==0);
			if (gray) EnableMenuItem(menu,IDM_TILEVIEW_EDIT,MF_GRAYED);
			if (gray&&tileview.isnt) EnableMenuItem(menu,IDM_TILEVIEW_NTCHANGE,MF_GRAYED);
			if (gray_nthl&&tileview.isnt) EnableMenuItem(menu,IDM_TILEVIEW_NTHL,MF_GRAYED);
			if (gray&&!tileview.isnt) EnableMenuItem(menu,IDM_TILEVIEW_CUT,MF_GRAYED);
			if (gray_paste&&!tileview.isnt) EnableMenuItem(menu,IDM_TILEVIEW_PASTE,MF_GRAYED);

			popup=GetSubMenu(menu,0);
			tileview.popup_p.x=tileview.popup_p.y=0;
			GetCursorPos(&tileview.popup_p);

			#define CLEAN_MENU(x)				\
			if (menu) {							\
				DestroyMenu(menu); menu=NULL;	\
				tileview.in_tv=x;				\
			}									\
			tileview.popup_active=FALSE


			id=TrackPopupMenuEx(popup,TPM_LEFTALIGN|TPM_TOPALIGN|TPM_NONOTIFY|TPM_RETURNCMD|TPM_RIGHTBUTTON,(int)tileview.popup_p.x,(int)tileview.popup_p.y,dialog,NULL);
			switch (id) {
				/* cut/copy/paste pattern */
				case IDM_TILEVIEW_CUT: {
					u8 p_e[8];
					u8 c_e[8];

					if (gray||tileview.isnt) break;
					tileedit_download();
					memcpy(tileview.copy_p,tileedit.p,8);
					memcpy(tileview.copy_c,tileedit.c,8);
					tileview.paste_valid=TRUE;

					memset(p_e,0,8); memset(c_e,0,8);
					if (!tileedit_upload(tileedit.vdp_regs,p_e,c_e,0,0)) {
						CLEAN_MENU(FALSE);
						LOG_ERROR_WINDOW(dialog,"Couldn't cut tile!");
					}

					break;
				}

				case IDM_TILEVIEW_COPY:
					if (tileview.isnt) break;
					tileedit_download();
					memcpy(tileview.copy_p,tileedit.p,8);
					memcpy(tileview.copy_c,tileedit.c,8);
					tileview.paste_valid=TRUE;
					break;

				case IDM_TILEVIEW_PASTE:
					if (gray_paste||tileview.isnt) break;
					if (!tileedit_upload(tileedit.vdp_regs,tileview.copy_p,tileview.copy_c,0,0)) {
						CLEAN_MENU(FALSE);
						LOG_ERROR_WINDOW(dialog,"Couldn't paste tile!");
					}
					break;

				case IDM_TILEVIEW_HL:
					if (!tileview_follow_enabled) {
						tileview_follow_enabled=TRUE;
						CheckDlgButton(dialog,IDC_TILEVIEW_FCHECK,BST_CHECKED);
					}
					else if ((int)wParam==tileview_follow_num) {
						tileview_follow_enabled=FALSE;
						CheckDlgButton(dialog,IDC_TILEVIEW_FCHECK,BST_UNCHECKED);
					}

					if ((int)wParam!=tileview_follow_num) SetDlgItemInt(dialog,IDC_TILEVIEW_FEDIT,wParam,FALSE);
					PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDC_TILEVIEW_FCHECK),TRUE);
					break;

				case IDM_TILEVIEW_SHOWHL:
					tileview_follow_show^=1;
					break;

				/* paste NT byte */
				case IDM_TILEVIEW_NTHL:
					if (gray_nthl||!tileview.isnt) break;
					if (!tileview_nthl(lParam)) {
						CLEAN_MENU(FALSE);
						LOG_ERROR_WINDOW(dialog,"Couldn't set value!");
					}
					break;

				/* manual NT change */
				case IDM_TILEVIEW_NTCHANGE:
					if (gray||!tileview.isnt) break;
					CLEAN_MENU(FALSE);

					tileview.ntc_v=wParam;
					tileview.ntc_a=lParam&0xfff;
					tileview.ext_dialog=IDD_TILEVIEW_CHANGENT;
					if (DialogBox(MAIN->module,MAKEINTRESOURCE(IDD_TILEVIEW_CHANGENT),dialog,(DLGPROC)tileview_changent_dialog)==1) LOG_ERROR_WINDOW(dialog,"Couldn't set value!");
					tileview.ext_dialog=0;

					break;

				/* tile editor */
				case IDM_TILEVIEW_EDIT:
					if (gray) break;
					CLEAN_MENU(FALSE);

					/* bogus mode */
					if ((tileedit.vdp_regs[1]&0x18)==0x18) {
						LOG_ERROR_WINDOW(dialog,"Invalid mixed mode!");
						break;
					}

					if (tileedit.vdp_regs[1]&8) {
						tileedit.screen3=TRUE;
						tileview.ext_dialog=IDD_TILE3EDIT;
						DialogBox(MAIN->module,MAKEINTRESOURCE(IDD_TILE3EDIT),dialog,(DLGPROC)tileview_editor_dialog);
					}
					else {
						tileedit.screen3=FALSE;
						tileview.ext_dialog=IDD_TILE0EDIT;
						DialogBox(MAIN->module,MAKEINTRESOURCE(IDD_TILE0EDIT),dialog,(DLGPROC)tileview_editor_dialog);
					}
					tileview.ext_dialog=0;

					PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDOK),TRUE);
					break;

				/* cancel */
				default: break;
			}

			CLEAN_MENU(in_tv_prev);
			#undef CLEAN_MENU

			break;
		}

		case TOOL_MENUCHANGED:
			if (!tileview.tv.wnd) break;
			EnableWindow(GetDlgItem(dialog,IDC_TILEVIEW_OPEN),(netplay_is_active()|movie_get_active_state())==0);
			if (tileview.ext_dialog&&tileview.ext_wnd) PostMessage(tileview.ext_wnd,TOOL_MENUCHANGED,0,0);
			break;

		default: break;
	}

	return 0;
}


/* init/clean (only once) */
void tileview_init(void)
{
	int i;
	char* c=NULL;

	vdp_ram=tool_get_local_vdp_ram_ptr();
	vdp_regs=tool_get_local_vdp_regs_ptr();
	draw_palette=tool_get_local_draw_palette_ptr();

	tileview_overlay_font_generate();
	memset(&tileview,0,sizeof(tileview));
	memset(&tileedit,0,sizeof(tileedit));

	MEM_CREATE_T(_tileedit_undo_begin,sizeof(_tileedit_undo),_tileedit_undo*);
	_tileedit_undo_begin->prev=_tileedit_undo_begin;
	_tileedit_undo_cursor=_tileedit_undo_begin;

	/* settings */
	i=TILEVIEW_SOURCE_PT;
	SETTINGS_GET_STRING(settings_info(SETTINGS_TILEMAPSOURCE),&c);
	if (c&&strlen(c)) {
		for (i=0;i<TILEVIEW_SOURCE_MAX;i++) if (stricmp(c,tileview_get_source_name(i))==0) break;
		if (i==TILEVIEW_SOURCE_MAX) i=TILEVIEW_SOURCE_PT;
	}
	MEM_CLEAN(c);
	tileview_source=i;

	tileedit_fi=tool_get_pattern_fi_shared();
	tileview_open_fi=1;	SETTINGS_GET_INT(settings_info(SETTINGS_FILTERINDEX_OPENTILES),&tileview_open_fi);	CLAMP(tileview_open_fi,1,6);
	tileview_save_fi=2;	SETTINGS_GET_INT(settings_info(SETTINGS_FILTERINDEX_SAVETILES),&tileview_save_fi);	CLAMP(tileview_save_fi,1,5);

	tileview_openi_nt=TILEVIEW_OPENI_NT_DEFAULT;	i=settings_get_yesnoauto(SETTINGS_TIMPORT_NT); if (i==FALSE||i==TRUE) tileview_openi_nt=i;
	tileview_openi_pt=TILEVIEW_OPENI_PT_DEFAULT;	i=settings_get_yesnoauto(SETTINGS_TIMPORT_PGT); if (i==FALSE||i==TRUE) tileview_openi_pt=i;
	tileview_openi_ct=TILEVIEW_OPENI_CT_DEFAULT;	i=settings_get_yesnoauto(SETTINGS_TIMPORT_CT); if (i==FALSE||i==TRUE) tileview_openi_ct=i;
	tileview_open_sim=TRUE;							i=settings_get_yesnoauto(SETTINGS_TSIMULATEMODE); if (i==FALSE||i==TRUE) tileview_open_sim=i;
	tileview_save_mask=TRUE;						i=settings_get_yesnoauto(SETTINGS_TILESAVEMIMICMASKS); if (i==FALSE||i==TRUE) tileview_save_mask=i;
	tileview_save_header=TRUE;						i=settings_get_yesnoauto(SETTINGS_TILEBLOAD); if (i==FALSE||i==TRUE) tileview_save_header=i;
	tileedit_allblocks=TRUE;						i=settings_get_yesnoauto(SETTINGS_TAPPLYTOALLBLOCKS); if (i==FALSE||i==TRUE) tileedit_allblocks=i;
}

void tileview_clean(void)
{
	MEM_CLEAN(_tileedit_undo_begin);
	_tileedit_undo_cursor=NULL;
}
