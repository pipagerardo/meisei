/******************************************************************************
 *                                                                            *
 *                                 "tool.c"                                   *
 *                                                                            *
 ******************************************************************************/

#include "global.h"
#include "tool.h"
#include "tileview.h"
#include "spriteview.h"
#include "psgtoy.h"
#include "psg.h"
#include "vdp.h"
#include "draw.h"
#include "main.h"
#include "resource.h"
#include "settings.h"
#include "psglogger.h"

static HWND tool_window[TOOL_WINDOW_MAX];
static int tool_windowpos[TOOL_WINDOW_MAX][2];

static int pattern_fi_shared;	/* pattern filetype (shared between tileview and spriteview) */
static int tool_quitting=FALSE;

/* generic debug locals */
static u8 vdp_ram[0x4000];		/* 16KB vdp ram */
static int vdp_regs[8];			/* vdp registers */
static int vdp_status;			/* vdp status register */
static int psg_regs[0x10];		/* psg registers */
static int psg_address;			/* psg address */
static int psg_e_vol;			/* psg envelope volume */

static int draw_palette[16];	/* 32 bit colour palette */

int tool_get_pattern_fi_shared(void) { return pattern_fi_shared; }
void tool_set_pattern_fi_shared(u32 i) { if (i>2) i=1; pattern_fi_shared=i; }
int tool_is_quitting(void) { return tool_quitting; }
u8* tool_get_local_vdp_ram_ptr(void) { return vdp_ram; }
int* tool_get_local_vdp_regs_ptr(void) { return vdp_regs; }
int __fastcall tool_get_local_vdp_status(void) { return vdp_status; }
int* tool_get_local_psg_regs_ptr(void) { return psg_regs; }
int __fastcall tool_get_local_psg_address(void) { return psg_address; }
int __fastcall tool_get_local_psg_e_vol(void) { return psg_e_vol; }
int* tool_get_local_draw_palette_ptr(void) { return draw_palette; }

void tool_copy_locals(void)
{
	static int semaphore=FALSE;

	int i;

	if (semaphore||!tool_is_running()) return;
	semaphore=TRUE;

	memcpy(vdp_ram,vdp_get_ram(),0x4000);
	for (i=0;i<8;i++) vdp_regs[i]=vdp_get_reg(i);
	vdp_status=vdp_get_status();
	for (i=0;i<0x10;i++) psg_regs[i]=psg_get_reg(i);
	psg_address=psg_get_address();
	psg_e_vol=psg_get_e_vol();

	semaphore=FALSE;
}

void tool_copy_palette(int* p)
{
	int i=16;

	while (i--) draw_palette[i]=p[i];
}

void tool_repaint(void)
{
	int i=TOOL_WINDOW_MAX;

	if (!tool_is_running()) return;

	while (i--) {
		if (tool_window[i]) PostMessage(tool_window[i],TOOL_REPAINT,0,0);
	}
}

void tool_menuchanged(void)
{
	int i=TOOL_WINDOW_MAX;

	if (!tool_is_running()) return;

	while (i--) {
		if (tool_window[i]) PostMessage(tool_window[i],TOOL_MENUCHANGED,0,0);
	}
}


int __fastcall tool_is_running(void)
{
	int i=TOOL_WINDOW_MAX;

	while (i--) {
		if (tool_window[i]) return TRUE;
	}

	return FALSE;
}

HWND tool_getactivewindow(void)
{
	int i=TOOL_WINDOW_MAX;
	HWND w=GetActiveWindow();

	if (!w) return NULL;

	while (i--) {
		if (tool_window[i]==w) return w;
	}

	return NULL;
}


void tool_init_bmi(BITMAPINFO* info,int width,int height,int bits)
{
	memset(info,0,sizeof(BITMAPINFO));
	info->bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
	info->bmiHeader.biWidth=width;
	info->bmiHeader.biHeight=-height;
	info->bmiHeader.biPlanes=1;
	info->bmiHeader.biBitCount=bits;
	info->bmiHeader.biCompression=BI_RGB;

	/* 1bpp b/w */
	if (bits==1) {
		info->bmiHeader.biClrUsed=2;

		info->bmiColors[0].rgbRed=TOOL_DBM_EMPTY>>16&0xff;
		info->bmiColors[0].rgbGreen=TOOL_DBM_EMPTY>>8&0xff;
		info->bmiColors[0].rgbBlue=TOOL_DBM_EMPTY&0xff;
		info->bmiColors[1].rgbRed=info->bmiColors[1].rgbGreen=info->bmiColors[1].rgbBlue=0xff;
	}

	/* 8bpp */
	else if (bits==8) {
		info->bmiHeader.biClrUsed=0x100;

		/* black */
		memset(info->bmiColors,0,0x100*sizeof(u32));
	}
}

void toolwindow_setpos(HWND wnd,UINT id,UINT xloc,UINT yloc,int xoff,int yoff,int desk)
{
	if (tool_windowpos[id][0]==-1) {
		/* initial */
		main_parent_window(wnd,xloc,yloc,xoff,yoff,desk);
	}
	else {
		/* previous position */
		RECT r;

		/* check out of bounds */
		if (!SystemParametersInfo(SPI_GETWORKAREA,0,&r,0)) {
			r.right=GetSystemMetrics(SM_CXFULLSCREEN);
			r.bottom=GetSystemMetrics(SM_CYFULLSCREEN);
		}
		if (tool_windowpos[id][0]>r.right) tool_windowpos[id][0]=r.right-32;
		if (tool_windowpos[id][1]>r.bottom) tool_windowpos[id][1]=r.bottom-GetSystemMetrics(SM_CYSIZE);

		SetWindowPos(wnd,NULL,tool_windowpos[id][0],tool_windowpos[id][1],0,0,SWP_NOSIZE|SWP_NOZORDER);
	}
}

void toolwindow_savepos(HWND wnd,UINT id)
{
	RECT r;

	GetWindowRect(wnd,&r);
	tool_windowpos[id][0]=r.left;
	tool_windowpos[id][1]=r.top;
}

void toolwindow_resize(HWND wnd,LONG width,LONG height)
{
	RECT rc,rw;

	GetWindowRect(wnd,&rw);
	GetClientRect(wnd,&rc);

	SetWindowPos(wnd,NULL,0,0,width+((rw.right-rw.left)-(rc.right-rc.left)),height+((rw.bottom-rw.top)-(rc.bottom-rc.top)),SWP_NOMOVE|SWP_NOZORDER);
}

int toolwindow_restrictresize(HWND wnd,WPARAM wParam,LPARAM lParam,int x,int y)
{
	RECT rc,rw;
	LPRECT rl=(LPRECT)lParam;
	int ret=0;

	GetWindowRect(wnd,&rw);
	GetClientRect(wnd,&rc);
	x+=((rw.right-rw.left)-(rc.right-rc.left));
	y+=((rw.bottom-rw.top)-(rc.bottom-rc.top));

	/* disallow user resizing if minimum size */
	if ((rl->right-rl->left)<x) {
		if (wParam==WMSZ_TOPRIGHT||wParam==WMSZ_RIGHT||wParam==WMSZ_BOTTOMRIGHT) {
			rl->left=rw.left;
			rl->right=rw.left+x;
		}
		else {
			rl->left=rw.right-x;
			rl->right=rw.right;
		}

		ret|=1;
	}
	if ((rl->bottom-rl->top)<y) {
		if (wParam==WMSZ_BOTTOMLEFT||wParam==WMSZ_BOTTOM||wParam==WMSZ_BOTTOMRIGHT) {
			rl->top=rw.top;
			rl->bottom=rw.top+y;
		}
		else {
			rl->top=rw.bottom-y;
			rl->bottom=rw.bottom;
		}
		ret|=1;
	}

	return ret;
}

void toolwindow_relative_clientpos(HWND wnd,POINT* p,int x,int y)
{
	RECT r;
	double d;

	GetClientRect(wnd,&r);
	r.right--; r.bottom--; x--; y--;

	if (r.right!=x) { d=p->x/(double)r.right; p->x=d*x+0.5; CLAMP(p->x,0,x); }
	if (r.bottom!=y) { d=p->y/(double)r.bottom; p->y=d*y+0.5; CLAMP(p->y,0,y); }
}

void tool_set_window(UINT window_id,UINT menu_id)
{
	if (window_id>=TOOL_WINDOW_MAX) return;

	/* close */
	if (tool_window[window_id]) SendMessage(tool_window[window_id],WM_CLOSE,0,0);

	/* open */
	else {
		int run=tool_is_running();

		switch (window_id) {
			case TOOL_WINDOW_TILEVIEW:
			    tool_window[window_id]=CreateDialog(MAIN->module,MAKEINTRESOURCE(IDD_TILEVIEW),MAIN->window,(DLGPROC)tileview_window);
            break;
			case TOOL_WINDOW_SPRITEVIEW:
			    tool_window[window_id]=CreateDialog(MAIN->module,MAKEINTRESOURCE(IDD_SPRITEVIEW),MAIN->window,(DLGPROC)spriteview_window);
            break;
			case TOOL_WINDOW_PSGTOY:
			    tool_window[window_id]=CreateDialog(MAIN->module,MAKEINTRESOURCE(IDD_PSGTOY),MAIN->window,(DLGPROC)psgtoy_window);
            break;
			default:
            break;
		}

		if (tool_window[window_id]) {
			if (!run) tool_copy_locals();
			ShowWindow(tool_window[window_id],SW_SHOW);
			UpdateWindow(tool_window[window_id]);
			SetFocus(MAIN->window);
			main_menu_check(menu_id,TRUE);
		}
	}
}

HWND tool_get_window(UINT window_id)
{
	if (window_id>=TOOL_WINDOW_MAX) return NULL;
	else return tool_window[window_id];
}

void tool_reset_window(UINT window_id)
{
	if (window_id>=TOOL_WINDOW_MAX) return;

	tool_window[window_id]=NULL;
}


/* init/clean */
void tool_init(void)
{
	int i=TOOL_WINDOW_MAX;

	while (i--) {
		tool_window[i]=NULL;
		tool_windowpos[i][0]=-1;
	}

	/* init locals */
	memset(vdp_ram,0,0x4000);
	for (i=0;i<8;i++) vdp_regs[i]=0;
	vdp_status=0;
	for (i=0;i<0x10;i++) psg_regs[i]=0;
	psg_address=psg_e_vol=0;
	for (i=0;i<16;i++) draw_palette[i]=0;

	/* settings */
	pattern_fi_shared=1;
	SETTINGS_GET_INT(settings_info(SETTINGS_FILTERINDEX_BIOS),&pattern_fi_shared);
	CLAMP(pattern_fi_shared,1,2);

	/* kids */
	tileview_init();
	spriteview_init();
	psgtoy_init();
}

void tool_settings_save(void)
{
	char c[STRING_SIZE];

	SETTINGS_PUT_STRING(NULL,"; *** Tool settings ***\n\n");

	/* tile viewer */
	SETTINGS_PUT_STRING(NULL,"; Tile Viewer:\n\n");
	SETTINGS_PUT_STRING(NULL,"; Valid "); SETTINGS_PUT_STRING(NULL,settings_info(SETTINGS_TILEMAPSOURCE)); SETTINGS_PUT_STRING(NULL," options: ");
	SETTINGS_PUT_STRING(NULL,tileview_get_source_name(TILEVIEW_SOURCE_PT)); SETTINGS_PUT_STRING(NULL,", ");
	SETTINGS_PUT_STRING(NULL,tileview_get_source_name(TILEVIEW_SOURCE_NT)); SETTINGS_PUT_STRING(NULL,",\n; -- or ");
	SETTINGS_PUT_STRING(NULL,tileview_get_source_name(TILEVIEW_SOURCE_NTO)); SETTINGS_PUT_STRING(NULL,".\n\n");
	SETTINGS_PUT_STRING(settings_info(SETTINGS_TILEMAPSOURCE),tileview_get_source_name(tileview_get_source()));

	settings_put_yesnoauto(SETTINGS_TAPPLYTOALLBLOCKS,tileedit_get_allblocks());
	settings_put_yesnoauto(SETTINGS_TIMPORT_NT,tileview_get_openi_nt());
	settings_put_yesnoauto(SETTINGS_TIMPORT_PGT,tileview_get_openi_pt());
	settings_put_yesnoauto(SETTINGS_TIMPORT_CT,tileview_get_openi_ct());
	settings_put_yesnoauto(SETTINGS_TSIMULATEMODE,tileview_get_open_sim());
	settings_put_yesnoauto(SETTINGS_TILEBLOAD,tileview_get_save_header());
	settings_put_yesnoauto(SETTINGS_TILESAVEMIMICMASKS,tileview_get_save_mask());

	/* psg toy */
	SETTINGS_PUT_STRING(NULL,"\n; PSG Toy:\n\n");
	SETTINGS_PUT_INT(settings_info(SETTINGS_CORRECTCENTS),psgtoy_get_centcorrection());
	settings_put_yesnoauto(SETTINGS_YMLOGTRIMSILENCE,psglogger_get_trimsilence());
	settings_put_yesnoauto(SETTINGS_YMLOGINTERLEAVED,psglogger_get_interleaved());
	SETTINGS_PUT_STRING(NULL,"\n");

	settings_put_yesnoauto(SETTINGS_CUSTOMWAVE,psgtoy_get_custom_enabled());
	psgtoy_get_custom_wave_string(0,c); SETTINGS_PUT_STRING(settings_info(SETTINGS_CUSTOMWAVEA),c);
	psgtoy_get_custom_wave_string(1,c); SETTINGS_PUT_STRING(settings_info(SETTINGS_CUSTOMWAVEB),c);
	psgtoy_get_custom_wave_string(2,c); SETTINGS_PUT_STRING(settings_info(SETTINGS_CUSTOMWAVEC),c);
	sprintf(c,"%02X%02X%02X",psgtoy_get_custom_amplitude(0),psgtoy_get_custom_amplitude(1),psgtoy_get_custom_amplitude(2)); SETTINGS_PUT_STRING(settings_info(SETTINGS_CUSTOMWAVEAMP),c);
}

void tool_clean(void)
{
	int i=TOOL_WINDOW_MAX;

	tool_quitting=TRUE;

	while (i--) {
		if (tool_window[i]) SendMessage(tool_window[i],WM_CLOSE,0,0);
	}

	tileview_clean();
	spriteview_clean();
	psgtoy_clean();
}
