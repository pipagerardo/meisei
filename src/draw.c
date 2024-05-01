/******************************************************************************
 *                                                                            *
 *                                 "draw.c"                                   *
 *                                                                            *
 ******************************************************************************/

#define COBJMACROS
#include <io.h>
#define DIRECTDRAW_VERSION 0x700
#include <ddraw.h>
#define DIRECT3D_VERSION 0x900
#include <d3d9types.h>
#include <d3d9caps.h>
#include <d3d9.h>
#include <math.h>
#include <commctrl.h>
#include <float.h>
#include <windows.h>

#include "global.h"
#include "main.h"
#include "resource.h"
#include "msx.h"
#include "settings.h"
#include "ti_ntsc.h"
#include "crystal.h"
#include "draw.h"
#include "sound.h"
#include "file.h"
#include "input.h"
#include "vdp.h"
#include "movie.h"
#include "tool.h"
#include "screenshot.h"
#include "mapper.h"

#define ZOOM_MAX				6
#define FRAMESKIP_MAX			10
#define NTSC_CLIP_L				4
#define NTSC_CLIP_R				3
#define TEXT_MAX_TICKS_DEFAULT	4000

#define D_MUTEX_TIME			INFINITE
#define SC_MUTEX_TIME			INFINITE
#define TEXT_MUTEX_TIME			INFINITE

/* defaults */
#define DRAW_PALETTE_DEFAULT	DRAW_PALETTE_TMS9129
#define DRAW_DECODER_DEFAULT	DRAW_DECODER_INDUSTRIAL

/* d3d */
static struct {
	HINSTANCE dll;
	LPDIRECT3D9 d3d;
	D3DCAPS9 caps;
	IDirect3DDevice9* device;
	D3DPRESENT_PARAMETERS pp;
	D3DDISPLAYMODE displaymode;
	IDirect3DVertexBuffer9* vb;
	IDirect3DTexture9* texture;
	LPDIRECT3DSURFACE9 surface;

	int border_refresh_needed;
	int dynamic;
	int flush_needed;
	int vb_recalc_needed;

	int vsync_recalc_needed;
	int vsync_recalc_count;
	int qpc_enabled;
	LARGE_INTEGER qpf;
	LONGLONG vsync_qpc;
} d3d;

/* ddraw */
static struct {
	LPDIRECTDRAW ddraw;
	DDCAPS caps;

	/* primary surface */
	LPDIRECTDRAWSURFACE surface_primary;
	DDSURFACEDESC desc_primary;
	LPDIRECTDRAWCLIPPER clipper;

	/* back surface */
	LPDIRECTDRAWSURFACE surface_back;
	DDSURFACEDESC desc_back;
} ddraw;

/* std */
static struct {
	/* surface clipping */
	struct {
		int u;
		int d;
		int l;
		int r;
	} clip;

	/* visible area */
	struct {
		int u;
		int d;
		int l;
		int r;

		int u_set;
		int d_set;
		int l_set;
		int r_set;
	} vis;

	u8* screen;
	int semaphore;
	int repaint;
	DWORD repaint_ticks;
	int text_scroll;
	int fullscreen;
	int fullscreen_set;
	RECT winrect;
	RECT rect_dest;
	RECT rect_dest_orig;
	RECT rect_dest_prev;
	RECT rect_dest_orig_prev;
	int ch;
	int cv;
	int menuvis;
	HANDLE text_mutex;
	HANDLE sc_mutex;
	HANDLE d_mutex;
	int resetting;
	int recreate_needed;
	int reset_needed;
	int reset_done;
	HFONT font;
	HDC text_dc;
	HBRUSH paledit_brush[0x10];

	int vsync;
	int force5060;
	int force5060_auto;
	int softrender;
	int text_max_ticks;
	int surface_width;
	int surface_height;
	int source_width;
	int source_width_winrel;
	int source_height;
	int source_height_winrel;
	int win_width;
	int win_height;
	int zoom;
	int maxzoom;
	float fzoom;
	int flip_x;
	int flip_x_prev;
	int bfilter;
	int bfilter_prev;
	int correct_aspect;
	int stretch;
	int rendertype;
	int vidformat;
	int palette;
	int palette_fi;
	int savepalette;
	char* loadpalette;
	int bc;
	int bc_prev;
	int frameskip_set;
	int frameskip_count;
	int skippedframe;
	int screenshot;
	int overlay;
	int init_done;
	int device_ready;
	u32 surface_change;

} draw;

static struct {
	int is_used;
	ti_ntsc_t* ntsc;
	ti_ntsc_setup_t setup;
	float matrix[6];
	int vidsignal;
	int decoder_type;
	float decoder[6];
	float hue;

	float saturation;
	float contrast;
	float brightness;
	float sharpness;
	float gamma;
	float resolution;
	float artifacts;
	float fringing;
	float bleed;

} draw_ntsc;

/* LIFO */
typedef struct draw_text {
	DWORD ticks;
	int cont;
	int colour;
	int type;
	int div;
	char text[0x100];
	struct draw_text* next;
} draw_text;

static draw_text* draw_text_cursor;
static draw_text* draw_text_begin;

static COLORREF draw_text_colour[0x20];

static int draw_palette_32[16];
static int draw_palette_16[16];
static u8 draw_palette_load[16*3];
static u8 draw_palette_ntsc[16*3];

#define PAL_NTSC32(c) (draw_palette_ntsc[(c)*3+2]|(draw_palette_ntsc[(c)*3+1]<<8)|(draw_palette_ntsc[(c)*3]<<16))
#define PAL_NTSC16(c) ((draw_palette_ntsc[(c)*3+2]>>3)|(draw_palette_ntsc[(c)*3+1]>>2<<5)|(draw_palette_ntsc[(c)*3]>>3<<11))

/*                                            hue sat con bri gam */
static const float palette_tuning_default[]={ 0.0,0.0,0.0,0.0,0.0 };

static const float draw_decoder_preset[][6]={
	/* R-Y, G-Y, B-Y colour demodulation angle, R-Y, G-Y, B-Y gain */
	{90.0, 236.0, 0.0, 0.57, 0.351,1.016},	/* industrial */
	{112.0,252.0, 0.0, 0.83, 0.3,  1.0},	/* Sony CXA2025AS/U */
	{95.0, 240.0, 0.0, 0.78, 0.3,  1.0},	/* Sony CXA2025AS/J */
	{105.0,236.0, 0.0, 0.78, 0.33, 1.0},	/* Sony CXA2095S/U */
	{95.0, 236.0, 0.0, 0.6,  0.33, 1.0}		/* Sony CXA2095S/J */
};

static const char* draw_rendertype_name[DRAW_RENDERTYPE_MAX]={
	"Direct3D",
	"DirectDraw"
};
static const char* draw_vidsignal_name[DRAW_VIDSIGNAL_MAX]={
	"RGB",
	"Monochrome",
	"Composite",
	"S-Video",
	"custom"
};
static const char* draw_decoder_name[DRAW_DECODER_MAX]={
	"Industrial",
	"Sony CXA2025AS/U",
	"Sony CXA2025AS/J",
	"Sony CXA2095S/U",
	"Sony CXA2095S/J",
	"custom"
};
static const char* draw_palette_name[DRAW_PALETTE_MAX]={
	"TI TMS9129",
	"Yamaha V9938",
	"Sega 315-5124",
	"custom - Konami Antiques MSX",
	"custom - Vampier Konami",
	"custom - Wolf's happy little colours",

	"File",
	"Internal"
};

const char* draw_get_rendertype_name(u32 i) { if (i>=DRAW_RENDERTYPE_MAX) return NULL; else return draw_rendertype_name[i]; }
const char* draw_get_vidsignal_name(u32 i) { if (i>=DRAW_VIDSIGNAL_MAX) return NULL; else return draw_vidsignal_name[i]; }
const char* draw_get_decoder_name(u32 i) { if (i>=DRAW_DECODER_MAX) return NULL; else return draw_decoder_name[i]; }
const char* draw_get_palette_name(u32 i) { if (i>=DRAW_PALETTE_MAX) return NULL; else return draw_palette_name[i]; }

static void draw_recreate_font(void);
static void draw_text_out(void);
static void draw_blit(int);

int (*drawdev_init)(void);
void (*drawdev_clean)(void);
void (*drawdev_reset)(void);
int (*drawdev_recreate_screen)(void);
void (*drawdev_flush)(void);
void (*drawdev_render)(void);
int (*drawdev_text_begin)(void);
void (*drawdev_text_end)(void);
void (*drawdev_vsync)(void);
void (*drawdev_blit)(void);
int (*drawdev_get_refreshrate)(void);

static int d3d_init(void);
static void d3d_clean(void);
static void d3d_reset(void);
static int d3d_recreate_screen(void);
static void d3d_flush_needed(void);
static void d3d_render(void);
static int d3d_text_begin(void);
static void d3d_text_end(void);
static void d3d_vsync(void);
static void d3d_blit(void);
static int d3d_get_refreshrate(void);

static int d3d_check_vsync_on(void);

static int ddraw_init(void);
static void ddraw_clean(void);
static void ddraw_reset(void);
static int ddraw_recreate_screen(void);
static void ddraw_flush(void);
static void ddraw_render(void);
static int ddraw_text_begin(void);
static void ddraw_text_end(void);
static void ddraw_blit(void);
static void ddraw_vsync(void);
static int ddraw_get_refreshrate(void);
static int ddraw_lock(LPDIRECTDRAWSURFACE*,DDSURFACEDESC*);
static void ddraw_unlock(LPDIRECTDRAWSURFACE*);
static HRESULT ddraw_create_surface(LPDIRECTDRAWSURFACE*,DDSURFACEDESC*,int,int,int);

static void drawdev_void_void_null(void) { ; }
static int drawdev_int_void_null(void) { return 0; }

int __fastcall draw_get_overlay(void) { return draw.overlay; }
int draw_get_correct_aspect(void) { return draw.correct_aspect; }
int draw_get_bfilter(void) { return draw.bfilter; }
int draw_get_stretch(void) { return draw.stretch; }
int draw_get_fullscreen(void) { return draw.fullscreen; }
int draw_get_fullscreen_set(void) { return draw.fullscreen_set; }
u8* draw_get_screen_ptr(void) { return draw.screen; }
int draw_get_5060(void) { return draw.force5060; }
int draw_get_5060_auto(void) { return draw.force5060_auto; }
int draw_get_source_width(void) { return draw.source_width; }
int draw_get_source_height(void) { return draw.source_height; }
int draw_get_refreshrate(void) { return (*drawdev_get_refreshrate)(); }
int draw_get_vidformat(void) { return draw.vidformat; }
int draw_get_rendertype(void) { return draw.rendertype; }
int draw_get_softrender(void) { return draw.softrender; }
int draw_get_vsync(void) { return draw.vsync; }
int draw_get_filterindex_palette(void) { return draw.palette_fi; }
int draw_get_flip_x(void) { return draw.flip_x; }
int __fastcall draw_is_flip_x(void) { return draw.flip_x&(d3d.d3d!=NULL); }
int draw_get_text(void) { return draw.text_max_ticks!=0; }

void draw_set_vidformat(int v) { draw.vidformat=v; }
void draw_set_surface_change(u32 s) { WaitForSingleObject(draw.sc_mutex,SC_MUTEX_TIME); draw.surface_change=s; ReleaseMutex(draw.sc_mutex); }
void draw_set_recreate(void) { draw.recreate_needed=TRUE; }
void draw_set_repaint(int r) { draw.repaint=r; }
void draw_set_softrender(int s) { draw.softrender=s; }
void draw_set_rendertype(int r) { draw.rendertype=r; }
void draw_set_screenshot(void) { draw.screenshot=TRUE; }
void draw_set_text_max_ticks_default(void) { draw.text_max_ticks=TEXT_MAX_TICKS_DEFAULT; }
void draw_reset_bc_prev(void) { draw.bc_prev=-1; }

void draw_create_text_mutex(void)
{
    if ((draw.text_mutex=(CreateMutex(NULL,FALSE,NULL)))==NULL)
    {
        LOG(
            LOG_MISC|LOG_ERROR,
            "Can't create draw text mutex!\n"
        );
        exit(1);
    }
}
static void draw_update_vidsignal_radio(void) { main_menu_radio(IDM_VSRGB+draw_ntsc.vidsignal,IDM_VSRGB,IDM_VSCUSTOM); }


/* init/clean */
void draw_setzero(void)
{
	memset(&draw,0,sizeof(draw));
	memset(&d3d,0,sizeof(d3d));
	memset(&ddraw,0,sizeof(ddraw));
	memset(&draw_ntsc,0,sizeof(draw_ntsc));
	draw_text_cursor=NULL;
	draw_text_begin=NULL;

	drawdev_clean=drawdev_reset=drawdev_flush=drawdev_render=drawdev_text_end=drawdev_vsync=drawdev_blit=&drawdev_void_void_null;
	drawdev_recreate_screen=drawdev_init=drawdev_text_begin=drawdev_get_refreshrate=&drawdev_int_void_null;
}

static int draw_init_renderer(void)
{
	switch (draw.rendertype) {

		case DRAW_RENDERTYPE_D3D:
			drawdev_init=&d3d_init;
			drawdev_clean=&d3d_clean;
			drawdev_reset=&d3d_reset;
			drawdev_recreate_screen=&d3d_recreate_screen;
			drawdev_flush=&d3d_flush_needed;
			drawdev_render=&d3d_render;
			drawdev_text_begin=&d3d_text_begin;
			drawdev_text_end=&d3d_text_end;
			drawdev_vsync=&d3d_vsync;
			drawdev_blit=&d3d_blit;
			drawdev_get_refreshrate=&d3d_get_refreshrate;

			break;

		case DRAW_RENDERTYPE_DD:
			drawdev_init=&ddraw_init;
			drawdev_clean=&ddraw_clean;
			drawdev_reset=&ddraw_reset;
			drawdev_recreate_screen=&ddraw_recreate_screen;
			drawdev_flush=&ddraw_flush;
			drawdev_render=&ddraw_render;
			drawdev_text_begin=&ddraw_text_begin;
			drawdev_text_end=&ddraw_text_end;
			drawdev_vsync=&ddraw_vsync;
			drawdev_blit=&ddraw_blit;
			drawdev_get_refreshrate=&ddraw_get_refreshrate;

			break;

		default: break;
	}

	return (*drawdev_init)();
}

static void draw_switch_renderer(void)
{
	int err=0;

	err=draw_init_renderer();
	if (draw.rendertype==DRAW_RENDERTYPE_D3D&&err!=0) {
		/* d3d error, try ddraw */
		(*drawdev_clean)();
		LOG(
            LOG_MISC|LOG_WARNING,
            "D3D error %d, switching to DirectDraw\n",
            err
        );

		draw.rendertype=DRAW_RENDERTYPE_DD;
		draw_init_renderer();
	}

	main_menu_radio(IDM_RENDERD3D+draw.rendertype,IDM_RENDERD3D,IDM_RENDERDD);
	main_menu_check(IDM_SOFTRENDER,draw.softrender);
}

int draw_init(void)
{
	int i=0;

	if (MAIN->window==NULL) return FALSE;

	/* mutexes (text one has been created already) */
	if ((draw.sc_mutex=(CreateMutex(NULL,FALSE,NULL)))==NULL)
    {
        LOG(
            LOG_MISC|LOG_ERROR,
            "Can't create draw sc mutex!\n"
        );
        exit(1);
    }
	if ((draw.d_mutex=(CreateMutex(NULL,FALSE,NULL)))==NULL)
    {
        LOG(
            LOG_MISC|LOG_ERROR,
            "Can't create draw d mutex!\n"
        );
        exit(1);
    }

	draw_init_settings();
	draw_recreate_font();
	draw_init_palette(NULL);

	draw_switch_renderer();

	/* text colours, 00bbggrr */
	for (i=0;i<0x20;i++) draw_text_colour[i]=0x004040d0; /* warning: red */
	draw_text_colour[0]=0x00ffb060; /* cyan */
	draw_text_colour[1]=0x00c070f0; /* pink */
	draw_text_colour[2]=0x0040e8e8; /* yellow */
	draw_text_colour[3]=0x0030d858; /* green */
	draw_text_colour[4]=0x0050a0f0; /* orange */
	draw_text_colour[5]=0x00ffffff; /* white */
	draw_text_colour[6]=0x00f04040; /* blue */
	draw_text_colour[7]=0x00808080; /* gray */
	draw_text_colour[8]=0x00000000; /* black */


	MEM_CREATE(draw.screen,sizeof(u8)*draw.source_width*draw.source_height);

	draw_set_window_size();
	draw.init_done=TRUE;

	LOG(
        LOG_VERBOSE,
        "draw initialised\n"
    );

	return TRUE;
}

static void draw_recreate_font(void)
{
	if (draw.font) { DeleteObject(draw.font); draw.font=NULL; } /* free font */

	if (draw_ntsc.is_used) draw.font=CreateFont(
		14,							/* height */
		15,							/* width */
		0,0,						/* escapement, orientation */
		FW_BOLD,					/* weight */
		FALSE,FALSE,FALSE,			/* italic?, underlined?, striked out? */
		ANSI_CHARSET,				/* charset */
		OUT_DEFAULT_PRECIS,			/* output precision */
		CLIP_DEFAULT_PRECIS,		/* clipping precision */
		ANTIALIASED_QUALITY,		/* quality */
		VARIABLE_PITCH|FF_SWISS,	/* pitch and family */
		"Arial\0"					/* typeface name */
	);

	else draw.font=CreateFont(12,6,0,0,FW_BOLD,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,VARIABLE_PITCH|FF_SWISS,"MS Sans Serif\0");
}

void __fastcall draw_check_reset(void)
{
	if (draw.init_done) {
		/* check if renderer needs to be recreated */
		if (draw.recreate_needed) {
			draw.recreate_needed=FALSE;

			WaitForSingleObject(draw.d_mutex,D_MUTEX_TIME);
			(*drawdev_clean)();
			draw_switch_renderer();
			ReleaseMutex(draw.d_mutex);
		}

		/* reset needed? */
		if (draw.reset_needed) {
			draw.resetting=TRUE;
			draw.reset_needed=FALSE;

			WaitForSingleObject(draw.d_mutex,D_MUTEX_TIME);
			(*drawdev_reset)();
			ReleaseMutex(draw.d_mutex);

			draw.resetting=FALSE;
		}
	}
}

void draw_clean(void)
{
	int i;

	WaitForSingleObject(draw.d_mutex,D_MUTEX_TIME); (*drawdev_clean)(); ReleaseMutex(draw.d_mutex);

	if (draw.savepalette) draw_save_palette();

	draw_text_clean();
	if (draw.font) { DeleteObject(draw.font); draw.font=NULL; }
	MEM_CLEAN(draw.screen);

	MEM_CLEAN(draw_ntsc.ntsc);
	MEM_CLEAN(draw.loadpalette);

	CloseHandle(draw.text_mutex);
	CloseHandle(draw.sc_mutex);
	CloseHandle(draw.d_mutex);

	for (i=0;i<0x10;i++) {
		if (draw.paledit_brush[i]) { DeleteObject(draw.paledit_brush[i]); draw.paledit_brush[i]=NULL; }
	}

	LOG(LOG_VERBOSE,"draw cleaned\n");
}


/* settings */
static void draw_correct_dimensions(void)
{
	draw.vis.u=draw.vis.u_set;
	draw.vis.d=draw.vis.d_set;
	draw.vis.l=draw.vis.l_set;
	draw.vis.r=draw.vis.r_set;

	draw.source_width=256;
	draw.source_height=192;
	if (draw.vis.d<0) { draw.source_height-=draw.vis.d; draw.vis.d=0; }

	draw.source_width_winrel=draw.source_width-(draw.vis.l+draw.vis.r);
	draw.source_height_winrel=draw.source_height-(draw.vis.u+draw.vis.d);
	draw_set_correct_aspect(draw.correct_aspect);

	if (draw_ntsc.vidsignal==DRAW_VIDSIGNAL_RGB) {
		draw_ntsc.is_used=FALSE;
		draw.clip.l=draw.clip.r=0;
		draw.surface_width=draw.source_width-(draw.vis.l+draw.vis.r);
	}
	else {
		draw_ntsc.is_used=TRUE;
		draw.clip.l=NTSC_CLIP_L+6; draw.clip.r=NTSC_CLIP_R+6;
		draw.vis.l-=3; if (draw.vis.l<0) { draw.clip.l+=2*(draw.vis.l); draw.vis.l=0; }
		draw.vis.r-=3; if (draw.vis.r<0) { draw.clip.r+=2*(draw.vis.r); draw.vis.r=0; }

		draw.surface_width=TI_NTSC_OUT_WIDTH((draw.source_width-(draw.vis.l+draw.vis.r)));
	}

	draw.surface_height=draw.source_height-(draw.vis.u+draw.vis.d);
}

void draw_init_settings(void)
{
    int i,t;
	char* c;
	float f;
	t = FALSE; t = t;

	draw.text_max_ticks=TRUE;	i=settings_get_yesnoauto(SETTINGS_MESSAGES); if (i==FALSE||i==TRUE) { draw.text_max_ticks=i; } draw_set_text(draw.text_max_ticks);
	draw.softrender=FALSE;		i=settings_get_yesnoauto(SETTINGS_SOFTRENDER); if (i==FALSE||i==TRUE) { draw.softrender=i; }
	draw.bfilter=TRUE;			i=settings_get_yesnoauto(SETTINGS_BFILTER); if (i==FALSE||i==TRUE) { draw.bfilter=i; } draw.bfilter_prev=draw.bfilter;
	draw.flip_x=FALSE;			i=settings_get_yesnoauto(SETTINGS_FLIPX); if (i==FALSE||i==TRUE) { draw.flip_x=i; } draw.flip_x_prev=draw.flip_x;
	draw.vsync=TRUE;			i=settings_get_yesnoauto(SETTINGS_VSYNC); if (i==FALSE||i==TRUE) { draw.vsync=i; } draw_set_vsync(draw.vsync);
	draw.correct_aspect=TRUE;	i=settings_get_yesnoauto(SETTINGS_ASPECTRATIO); if (i==FALSE||i==TRUE) { draw.correct_aspect=i; }
	draw.stretch=FALSE;			i=settings_get_yesnoauto(SETTINGS_STRETCH); if (i==FALSE||i==TRUE) { draw.stretch=i; } draw_set_stretch(draw.stretch);
	draw.savepalette=FALSE;		i=settings_get_yesnoauto(SETTINGS_SAVEPALETTE); if (i==FALSE||i==TRUE) { draw.savepalette=i; }
	draw.fullscreen_set=FALSE;	i=settings_get_yesnoauto(SETTINGS_FULLSCREEN); if (i==FALSE||i==TRUE) { draw.fullscreen_set=i; }

	draw.palette_fi=1;			if (SETTINGS_GET_INT(settings_info(SETTINGS_FILTERINDEX_PALETTE),&i)) { CLAMP(i,1,2); draw.palette_fi=i; }
	draw.overlay=0;				if (SETTINGS_GET_INT(settings_info(SETTINGS_BORDEROVERLAY),&i)) { CLAMP(i,0,15); draw.overlay=i; }
	draw.vis.l_set=0;			if (SETTINGS_GET_INT(settings_info(SETTINGS_CLIPLEFT),&i)) { CLAMP(i,0,100); draw.vis.l_set=i; }
	draw.vis.r_set=0;			if (SETTINGS_GET_INT(settings_info(SETTINGS_CLIPRIGHT),&i)) { CLAMP(i,0,100); draw.vis.r_set=i; }
	draw.vis.u_set=0;			if (SETTINGS_GET_INT(settings_info(SETTINGS_CLIPUP),&i)) { CLAMP(i,0,100); draw.vis.u_set=i; }
	draw.vis.d_set=0;			if (SETTINGS_GET_INT(settings_info(SETTINGS_CLIPDOWN),&i)) { CLAMP(i,0,100); draw.vis.d_set=i; }
	draw.zoom=0;				if (SETTINGS_GET_INT(settings_info(SETTINGS_WINDOWZOOM),&i)) { CLAMP(i,1,ZOOM_MAX); draw.zoom=i; }

	draw.force5060_auto=TRUE; draw.force5060=FALSE; i=settings_get_yesnoauto(SETTINGS_FORCE5060); if (i==FALSE||i==TRUE) { draw.force5060=i; draw.force5060_auto=FALSE; } else if (i==2) { draw.force5060_auto=TRUE; }
	draw.vidformat=vdp_get_chiptype_vf(vdp_get_chiptype());

	#define DRAW_GET_ANGLE_SETTING(x,y,d)				\
	f=d;												\
	if (SETTINGS_GET_FLOAT(settings_info(x),&f)) {		\
		if (f<0.0) f+=(360*(int)((f-360.0)/-360));		\
		i=f; f-=i; i%=360; f+=i; CLAMP(f,0.0,360.0);	\
	}													\
	y=f

	DRAW_GET_ANGLE_SETTING(SETTINGS_HUE,draw_ntsc.hue,palette_tuning_default[0]);
	DRAW_GET_ANGLE_SETTING(SETTINGS_RYANGLE,draw_ntsc.decoder[0],draw_decoder_preset[0][0]);
	DRAW_GET_ANGLE_SETTING(SETTINGS_GYANGLE,draw_ntsc.decoder[1],draw_decoder_preset[0][1]);
	DRAW_GET_ANGLE_SETTING(SETTINGS_BYANGLE,draw_ntsc.decoder[2],draw_decoder_preset[0][2]);

	f=draw_decoder_preset[0][3]; if (SETTINGS_GET_FLOAT(settings_info(SETTINGS_RYGAIN),&f)) { CLAMP(f,0.0,2.0); } draw_ntsc.decoder[3]=f;
	f=draw_decoder_preset[0][4]; if (SETTINGS_GET_FLOAT(settings_info(SETTINGS_GYGAIN),&f)) { CLAMP(f,0.0,2.0); } draw_ntsc.decoder[4]=f;
	f=draw_decoder_preset[0][5]; if (SETTINGS_GET_FLOAT(settings_info(SETTINGS_BYGAIN),&f)) { CLAMP(f,0.0,2.0); } draw_ntsc.decoder[5]=f;

	f=palette_tuning_default[1]; if (SETTINGS_GET_FLOAT(settings_info(SETTINGS_SATURATION),&f)) { CLAMP(f,-1.0,1.0); } draw_ntsc.saturation=f;
	f=palette_tuning_default[2]; if (SETTINGS_GET_FLOAT(settings_info(SETTINGS_CONTRAST),&f)) { CLAMP(f,-1.0,1.0); } draw_ntsc.contrast=f;
	f=palette_tuning_default[3]; if (SETTINGS_GET_FLOAT(settings_info(SETTINGS_BRIGHTNESS),&f)) { CLAMP(f,-1.0,1.0); } draw_ntsc.brightness=f;
	f=palette_tuning_default[4]; if (SETTINGS_GET_FLOAT(settings_info(SETTINGS_GAMMA),&f)) { CLAMP(f,-1.0,1.0); } draw_ntsc.gamma=f;
	f=0.0; if (SETTINGS_GET_FLOAT(settings_info(SETTINGS_SHARPNESS),&f)) { CLAMP(f,-1.0,1.0); } draw_ntsc.sharpness=f;
	f=0.0; if (SETTINGS_GET_FLOAT(settings_info(SETTINGS_RESOLUTION),&f)) { CLAMP(f,-1.0,1.0); } draw_ntsc.resolution=f;
	f=0.0; if (SETTINGS_GET_FLOAT(settings_info(SETTINGS_ARTIFACTS),&f)) { CLAMP(f,-1.0,1.0); } draw_ntsc.artifacts=f;
	f=0.0; if (SETTINGS_GET_FLOAT(settings_info(SETTINGS_FRINGING),&f)) { CLAMP(f,-1.0,1.0); } draw_ntsc.fringing=f;
	f=0.0; if (SETTINGS_GET_FLOAT(settings_info(SETTINGS_BLEED),&f)) { CLAMP(f,-1.0,1.0); } draw_ntsc.bleed=f;

	/* string */
	#define DRAW_GET_STRING_SETTING(xdes,xfun,xset,xdef,xmax)	\
	i=xdef; c=NULL; t=FALSE;									\
	SETTINGS_GET_STRING(settings_info(xset),&c);				\
	if (c&&strlen(c)) {											\
		t=TRUE;													\
		for (i=0;i<xmax;i++) if (stricmp(c,draw_get_##xfun##_name(i))==0) break; \
		if (i==xmax) i=xdef;									\
	}															\
	MEM_CLEAN(c); xdes=i

	DRAW_GET_STRING_SETTING(draw_ntsc.vidsignal,	vidsignal,	SETTINGS_VIDSIGNAL,		DRAW_VIDSIGNAL_RGB,			DRAW_VIDSIGNAL_MAX	); draw_update_vidsignal_radio();
	DRAW_GET_STRING_SETTING(draw.rendertype,		rendertype,	SETTINGS_RENDERTYPE,	DRAW_RENDERTYPE_D3D,		DRAW_RENDERTYPE_MAX	);
	DRAW_GET_STRING_SETTING(draw_ntsc.decoder_type,	decoder,	SETTINGS_DECODER,		DRAW_DECODER_DEFAULT,		DRAW_DECODER_MAX	);

	/* auto/value */
	#define DRAW_GET_AUTOVAL_SETTING(set,x,max)					\
	i=-1; c=NULL;												\
	SETTINGS_GET_STRING(settings_info(set),&c);					\
	if (c&&strlen(c)) {											\
		if (stricmp(c,"auto")==0) i=-1;							\
		else {													\
			int inv;											\
			i=0; sscanf(c,"%d",&i); inv=i==0;					\
			i=1; sscanf(c,"%d",&i); inv&=(i==1);				\
			if (inv) i=-1;										\
			else { CLAMP(i,-1,max); }							\
		}														\
	}															\
	MEM_CLEAN(c); x=i

	DRAW_GET_AUTOVAL_SETTING(SETTINGS_FRAMESKIP,	draw.frameskip_set,		FRAMESKIP_MAX	);

	/* palette */
	i=DRAW_PALETTE_DEFAULT; c=NULL;
	SETTINGS_GET_STRING(settings_info(SETTINGS_PALETTE),&c);
	if (c&&strlen(c)) {
		for (i=0;i<DRAW_PALETTE_FILE;i++) if (stricmp(c,draw_get_palette_name(i))==0) break;
		if (i==DRAW_PALETTE_FILE&&draw_load_palette(c,NULL)!=0) {
			LOG(LOG_MISC|LOG_WARNING,"couldn't load palette\n");
			i=DRAW_PALETTE_DEFAULT;
		}
	}
	MEM_CLEAN(c); draw.palette=i;

	draw_init_ntsc();
	draw_correct_dimensions();
}

void draw_settings_save(void)
{
	SETTINGS_PUT_STRING(NULL,"; *** Graphics settings ***\n\n");

	/* std */
	SETTINGS_PUT_INT(settings_info(SETTINGS_WINDOWZOOM),draw.zoom);
	settings_put_yesnoauto(SETTINGS_FULLSCREEN,draw.fullscreen);
	if (draw.frameskip_set<0) SETTINGS_PUT_STRING(settings_info(SETTINGS_FRAMESKIP),"auto");
	else SETTINGS_PUT_INT(settings_info(SETTINGS_FRAMESKIP),draw.frameskip_set);
	settings_put_yesnoauto(SETTINGS_ASPECTRATIO,draw.correct_aspect);
	settings_put_yesnoauto(SETTINGS_STRETCH,draw.stretch);
	settings_put_yesnoauto(SETTINGS_TIMECODE,movie_get_timecode());
	settings_put_yesnoauto(SETTINGS_MESSAGES,draw.text_max_ticks!=0);

	SETTINGS_PUT_STRING(NULL,"\n");
	settings_put_yesnoauto(SETTINGS_LAYERB,vdp_get_bg_enabled());
	settings_put_yesnoauto(SETTINGS_LAYERS,vdp_get_spr_enabled());
	settings_put_yesnoauto(SETTINGS_LAYERUNL,vdp_get_spr_unlim());

	/* window border */
	SETTINGS_PUT_STRING(NULL,"\n; Screen border pixels clipping:\n\n");
	SETTINGS_PUT_INT(settings_info(SETTINGS_CLIPUP),draw.vis.u_set);
	SETTINGS_PUT_INT(settings_info(SETTINGS_CLIPDOWN),draw.vis.d_set);
	SETTINGS_PUT_INT(settings_info(SETTINGS_CLIPLEFT),draw.vis.l_set);
	SETTINGS_PUT_INT(settings_info(SETTINGS_CLIPRIGHT),draw.vis.r_set);

	SETTINGS_PUT_STRING(NULL,"\n");
	SETTINGS_PUT_INT(settings_info(SETTINGS_BORDEROVERLAY),draw.overlay);

	/* renderer */
	SETTINGS_PUT_STRING(NULL,"\n; Valid "); SETTINGS_PUT_STRING(NULL,settings_info(SETTINGS_RENDERTYPE)); SETTINGS_PUT_STRING(NULL," options: ");
	SETTINGS_PUT_STRING(NULL,draw_get_rendertype_name(DRAW_RENDERTYPE_D3D)); SETTINGS_PUT_STRING(NULL,", or ");
	SETTINGS_PUT_STRING(NULL,draw_get_rendertype_name(DRAW_RENDERTYPE_DD)); SETTINGS_PUT_STRING(NULL,".\n\n");

	SETTINGS_PUT_STRING(settings_info(SETTINGS_RENDERTYPE),draw_get_rendertype_name(draw.rendertype));
	settings_put_yesnoauto(SETTINGS_SOFTRENDER,draw.softrender);
	settings_put_yesnoauto(SETTINGS_VSYNC,draw.vsync);
	settings_put_yesnoauto(SETTINGS_BFILTER,draw.bfilter);
	settings_put_yesnoauto(SETTINGS_FLIPX,draw.flip_x);

	/* vdp chip */
	SETTINGS_PUT_STRING(NULL,"\n; Valid "); SETTINGS_PUT_STRING(NULL,settings_info(SETTINGS_VDPCHIP)); SETTINGS_PUT_STRING(NULL," options: ");
	SETTINGS_PUT_STRING(NULL,vdp_get_chiptype_name(VDP_CHIPTYPE_TMS9118)); SETTINGS_PUT_STRING(NULL,", ");
	SETTINGS_PUT_STRING(NULL,vdp_get_chiptype_name(VDP_CHIPTYPE_TMS9129)); SETTINGS_PUT_STRING(NULL,", ");
	SETTINGS_PUT_STRING(NULL,vdp_get_chiptype_name(VDP_CHIPTYPE_TMS9918)); SETTINGS_PUT_STRING(NULL,",\n; -- ");
	SETTINGS_PUT_STRING(NULL,vdp_get_chiptype_name(VDP_CHIPTYPE_TMS9929)); SETTINGS_PUT_STRING(NULL,", ");
	SETTINGS_PUT_STRING(NULL,vdp_get_chiptype_name(VDP_CHIPTYPE_T6950));  SETTINGS_PUT_STRING(NULL,", or ");
	SETTINGS_PUT_STRING(NULL,vdp_get_chiptype_name(VDP_CHIPTYPE_T7937A)); SETTINGS_PUT_STRING(NULL,".\n\n");

	SETTINGS_PUT_STRING(settings_info(SETTINGS_VDPCHIP),vdp_get_chiptype_name(vdp_get_chiptype()));
	settings_put_yesnoauto(SETTINGS_FORCE5060,draw.force5060_auto?2:draw.force5060);

	/* decoder */
	SETTINGS_PUT_STRING(NULL,"\n; Valid "); SETTINGS_PUT_STRING(NULL,settings_info(SETTINGS_DECODER)); SETTINGS_PUT_STRING(NULL," options: ");
	SETTINGS_PUT_STRING(NULL,draw_get_decoder_name(DRAW_DECODER_INDUSTRIAL)); SETTINGS_PUT_STRING(NULL,", ");
	SETTINGS_PUT_STRING(NULL,draw_get_decoder_name(DRAW_DECODER_SONY_CXA2025AS_U)); SETTINGS_PUT_STRING(NULL,", ");
	SETTINGS_PUT_STRING(NULL,draw_get_decoder_name(DRAW_DECODER_SONY_CXA2025AS_J)); SETTINGS_PUT_STRING(NULL,",\n; -- ");
	SETTINGS_PUT_STRING(NULL,draw_get_decoder_name(DRAW_DECODER_SONY_CXA2095S_U)); SETTINGS_PUT_STRING(NULL,", ");
	SETTINGS_PUT_STRING(NULL,draw_get_decoder_name(DRAW_DECODER_SONY_CXA2095S_J)); SETTINGS_PUT_STRING(NULL,", or ");
	SETTINGS_PUT_STRING(NULL,draw_get_decoder_name(DRAW_DECODER_CUSTOM)); SETTINGS_PUT_STRING(NULL," (with settings below).\n\n");

	SETTINGS_PUT_STRING(settings_info(SETTINGS_DECODER),draw_get_decoder_name(draw_ntsc.decoder_type));
	SETTINGS_PUT_FLOAT(settings_info(SETTINGS_RYANGLE),draw_ntsc.decoder[0]);
	SETTINGS_PUT_FLOAT(settings_info(SETTINGS_GYANGLE),draw_ntsc.decoder[1]);
	SETTINGS_PUT_FLOAT(settings_info(SETTINGS_BYANGLE),draw_ntsc.decoder[2]);
	SETTINGS_PUT_FLOAT(settings_info(SETTINGS_RYGAIN),draw_ntsc.decoder[3]);
	SETTINGS_PUT_FLOAT(settings_info(SETTINGS_GYGAIN),draw_ntsc.decoder[4]);
	SETTINGS_PUT_FLOAT(settings_info(SETTINGS_BYGAIN),draw_ntsc.decoder[5]);

	/* decoder std */
	SETTINGS_PUT_STRING(NULL,"\n");
	SETTINGS_PUT_FLOAT(settings_info(SETTINGS_HUE),draw_ntsc.hue);
	SETTINGS_PUT_FLOAT(settings_info(SETTINGS_SATURATION),draw_ntsc.saturation);
	SETTINGS_PUT_FLOAT(settings_info(SETTINGS_CONTRAST),draw_ntsc.contrast);
	SETTINGS_PUT_FLOAT(settings_info(SETTINGS_BRIGHTNESS),draw_ntsc.brightness);
	SETTINGS_PUT_FLOAT(settings_info(SETTINGS_GAMMA),draw_ntsc.gamma);

	/* video signal */
	SETTINGS_PUT_STRING(NULL,"\n; Valid "); SETTINGS_PUT_STRING(NULL,settings_info(SETTINGS_VIDSIGNAL)); SETTINGS_PUT_STRING(NULL," options: ");
	SETTINGS_PUT_STRING(NULL,draw_get_vidsignal_name(DRAW_VIDSIGNAL_RGB)); SETTINGS_PUT_STRING(NULL,", ");
	SETTINGS_PUT_STRING(NULL,draw_get_vidsignal_name(DRAW_VIDSIGNAL_MONOCHROME)); SETTINGS_PUT_STRING(NULL,", ");
	SETTINGS_PUT_STRING(NULL,draw_get_vidsignal_name(DRAW_VIDSIGNAL_COMPOSITE)); SETTINGS_PUT_STRING(NULL,", ");
	SETTINGS_PUT_STRING(NULL,draw_get_vidsignal_name(DRAW_VIDSIGNAL_SVIDEO)); SETTINGS_PUT_STRING(NULL,",\n; -- or ");
	SETTINGS_PUT_STRING(NULL,draw_get_vidsignal_name(DRAW_VIDSIGNAL_CUSTOM)); SETTINGS_PUT_STRING(NULL," (with settings below).\n\n");

	SETTINGS_PUT_STRING(settings_info(SETTINGS_VIDSIGNAL),draw_get_vidsignal_name(draw_ntsc.vidsignal));
	SETTINGS_PUT_FLOAT(settings_info(SETTINGS_SHARPNESS),draw_ntsc.sharpness);
	SETTINGS_PUT_FLOAT(settings_info(SETTINGS_RESOLUTION),draw_ntsc.resolution);
	SETTINGS_PUT_FLOAT(settings_info(SETTINGS_FRINGING),draw_ntsc.fringing);
	SETTINGS_PUT_FLOAT(settings_info(SETTINGS_ARTIFACTS),draw_ntsc.artifacts);
	SETTINGS_PUT_FLOAT(settings_info(SETTINGS_BLEED),draw_ntsc.bleed);

	/* palette */
	SETTINGS_PUT_STRING(NULL,"\n; Valid palette options: ");
	SETTINGS_PUT_STRING(NULL,draw_get_palette_name(DRAW_PALETTE_TMS9129)); SETTINGS_PUT_STRING(NULL,", ");
	SETTINGS_PUT_STRING(NULL,draw_get_palette_name(DRAW_PALETTE_SMS)); SETTINGS_PUT_STRING(NULL,", ");
	SETTINGS_PUT_STRING(NULL,draw_get_palette_name(DRAW_PALETTE_V9938)); SETTINGS_PUT_STRING(NULL,",\n; -- ");
	SETTINGS_PUT_STRING(NULL,draw_get_palette_name(DRAW_PALETTE_KONAMI)); SETTINGS_PUT_STRING(NULL,", ");
	SETTINGS_PUT_STRING(NULL,draw_get_palette_name(DRAW_PALETTE_VAMPIER)); SETTINGS_PUT_STRING(NULL,",\n; -- ");
	SETTINGS_PUT_STRING(NULL,draw_get_palette_name(DRAW_PALETTE_WOLF)); SETTINGS_PUT_STRING(NULL,", or a file.\n\n");

	SETTINGS_PUT_STRING(settings_info(SETTINGS_PALETTE),draw.loadpalette?draw.loadpalette:draw_get_palette_name(draw.palette));
	settings_put_yesnoauto(SETTINGS_SAVEPALETTE,draw.savepalette);
}


/* NTSC */
static void draw_reinit_ntsc(void)
{
	if (draw_ntsc.ntsc) ti_ntsc_init(draw_ntsc.ntsc,&draw_ntsc.setup);
}

static void draw_init_ntsc_vidsignal(void)
{
	ti_ntsc_setup_t setup;

	draw_ntsc.setup.sharpness=draw_ntsc.sharpness;
	draw_ntsc.setup.resolution=draw_ntsc.resolution;
	draw_ntsc.setup.artifacts=draw_ntsc.artifacts;
	draw_ntsc.setup.fringing=draw_ntsc.fringing;
	draw_ntsc.setup.bleed=draw_ntsc.bleed;

	switch (draw_ntsc.vidsignal) {
		case DRAW_VIDSIGNAL_RGB: setup=ti_ntsc_rgb; break;
		case DRAW_VIDSIGNAL_MONOCHROME: setup=ti_ntsc_monochrome; break;
		case DRAW_VIDSIGNAL_COMPOSITE: setup=ti_ntsc_composite; break;
		case DRAW_VIDSIGNAL_SVIDEO: setup=ti_ntsc_svideo; break;

		default: setup=draw_ntsc.setup; break;
	}

	if (draw_ntsc.vidsignal!=DRAW_VIDSIGNAL_CUSTOM) {
		draw_ntsc.sharpness=draw_ntsc.setup.sharpness=setup.sharpness;
		draw_ntsc.resolution=draw_ntsc.setup.resolution=setup.resolution;
		draw_ntsc.artifacts=draw_ntsc.setup.artifacts=setup.artifacts;
		draw_ntsc.fringing=draw_ntsc.setup.fringing=setup.fringing;
		draw_ntsc.bleed=draw_ntsc.setup.bleed=setup.bleed;
	}

	/* update saturation in case of mono<->colour */
	if (draw_ntsc.vidsignal==DRAW_VIDSIGNAL_MONOCHROME) draw_ntsc.setup.saturation=setup.saturation;
	else draw_ntsc.setup.saturation=draw_ntsc.saturation;
}

static void draw_init_ntsc_colour(void)
{
	int i;
	float* decoder=draw_ntsc.decoder;

	if (draw_ntsc.decoder_type!=DRAW_DECODER_CUSTOM) {
		memcpy(draw_ntsc.decoder,draw_decoder_preset[draw_ntsc.decoder_type],sizeof(draw_ntsc.decoder));
	}

	for (i=0;i<3;i++) {
		draw_ntsc.matrix[i<<1]=2*sin(RAD(decoder[i]))*draw_ntsc.decoder[i+3];
		draw_ntsc.matrix[i<<1|1]=2*cos(RAD(decoder[i]))*draw_ntsc.decoder[i+3];
	}

	draw_ntsc.setup.decoder_matrix=draw_ntsc.matrix;

	draw_ntsc.setup.hue=draw_ntsc.hue/180.0;
	draw_ntsc.setup.hue+=(33.0/180.0); /* yuv -> yiq: hue shifts 33 degrees */
	if (draw_ntsc.setup.hue>1.0) draw_ntsc.setup.hue-=2.0;

	if (draw_ntsc.vidsignal!=DRAW_VIDSIGNAL_MONOCHROME) draw_ntsc.setup.saturation=draw_ntsc.saturation;

	draw_ntsc.setup.contrast=draw_ntsc.contrast;
	draw_ntsc.setup.brightness=draw_ntsc.brightness;
	draw_ntsc.setup.gamma=draw_ntsc.gamma;
}

void draw_init_ntsc(void)
{
	MEM_CREATE_T_N(draw_ntsc.ntsc,sizeof(ti_ntsc_t),ti_ntsc_t*)

	draw_init_ntsc_vidsignal();
	draw_init_ntsc_colour();

	draw_ntsc.setup.palette_out=draw_palette_ntsc;
	draw_reinit_ntsc();
}


/* palette */
void draw_init_palette(const char* load)
{
    load = load;
	u8* p=draw_palette_ntsc;
	u8 palette[16*3];
	int h=FALSE,i,k,s=draw.palette;

	draw_ntsc.setup.saturation=draw_ntsc.saturation;

p_retry:
	switch (s) {
#if 0
		/* auto (not used in meisei now) */
		case DRAW_PALETTE_AUTO: {
			int g=TRUE,e=0,t=1;

			if (load) {
				if (t&&(t=draw_load_palette(load,"pal"))!=0) e|=t; /* try [gamename].pal */
				if (t&&(t=draw_load_palette(load,"zip"))!=0) e|=t; /* else [gamename].zip */
			}

			if (t) g=FALSE;
			if (t&&(t=draw_load_palette("default","pal"))!=0) e|=t; /* else default.pal */
			if (t&&(t=draw_load_palette("default","zip"))!=0) e|=t; /* else default.zip */

			if (t) {
				if (e>=8) LOG(LOG_MISC|LOG_WARNING,"couldn't load palette\n");
				s=DRAW_PALETTE_INTERNAL; goto p_retry; /* else use internal */
			}

			if (g) LOG(LOG_MISC,"loaded game-specific palette\n");
			else LOG(LOG_VERBOSE,"loaded default palette\n");

			memcpy(palette,draw_palette_load,48);
			h=TRUE; p=palette;
			break;
		}
#endif
		/* internal (will only happen on error) */
		case DRAW_PALETTE_INTERNAL:
			h=FALSE; p=draw_palette_ntsc;
			break;

		/* from custom file */
		case DRAW_PALETTE_FILE:
			memcpy(palette,draw_palette_load,48);
			h=TRUE; p=palette;
			break;

		/* internal hardcoded */
		default: {
			int hid,size=0;
			u8* pt;

			if (s>=DRAW_PALETTE_HARDCODED_MAX) { s=DRAW_PALETTE_INTERNAL; goto p_retry; }

			switch (draw.palette) {
				case DRAW_PALETTE_TMS9129:	hid=ID_PAL_TMS9129; break;
				case DRAW_PALETTE_V9938:	hid=ID_PAL_V9938; break;
				case DRAW_PALETTE_SMS:		hid=ID_PAL_SMS; break;
				case DRAW_PALETTE_KONAMI:	hid=ID_PAL_KONAMI; break;
				case DRAW_PALETTE_VAMPIER:	hid=ID_PAL_VAMPIER; break;
				case DRAW_PALETTE_WOLF:		hid=ID_PAL_WOLF; break;

				default: s=DRAW_PALETTE_INTERNAL; goto p_retry;
			}

			pt=file_from_resource(hid,&size);
			if (pt==NULL||size!=48) { file_from_resource_close(); s=DRAW_PALETTE_INTERNAL; goto p_retry; }

			memcpy(palette,pt,48); memcpy(draw_palette_load,pt,48);
			file_from_resource_close();
			h=TRUE; p=palette;
			break;
		}
	}

	draw_ntsc.setup.palette=h?draw_palette_load:NULL;
	draw_reinit_ntsc();

	p=draw_palette_ntsc; /* use NTSC palette anyway */

	for (i=0;i<16;i++) {
		k=i*3;
		draw_palette_32[i]=p[k+2]|(p[k+1]<<8)|(p[k]<<16); /* 888 */
		draw_palette_16[i]=(p[k+2]>>3)|(p[k+1]>>2<<5)|(p[k]>>3<<11); /* 565 */
	}
	tool_copy_palette(draw_palette_32);

	if (draw_ntsc.vidsignal==DRAW_VIDSIGNAL_MONOCHROME) {
		draw_ntsc.setup.saturation=ti_ntsc_monochrome.saturation;
		draw_reinit_ntsc();
	}
}

void draw_save_palette(void)
{
	int i;
	u8 p[48];

	for (i=0;i<16;i++) {
		p[i*3]=draw_palette_32[i]>>16&0xff;
		p[i*3+1]=draw_palette_32[i]>>8&0xff;
		p[i*3+2]=draw_palette_32[i]&0xff;
	}

	file_setfile(&file->palettedir,file->appname,"pal",NULL);
	if (file_save()) {
		if (file_write(p,16*3)) LOG(LOG_VERBOSE,"saved palette\n");
		else LOG(LOG_MISC|LOG_WARNING,"palette write error\n");
	}
	else LOG(LOG_MISC|LOG_WARNING,"couldn't save palette\n");
	file_close();
}

int draw_load_palette(const char* c,const char* e)
{
	u8 p[48];

	int len,paldir,ext=0;

	if (c==NULL) return 1;
	len=strlen(c); if (len==0||len>=STRING_SIZE) return 1;

	memset(p,0,48);

	/* filename includes path? */
	for (paldir=0;paldir<len;paldir++) if (c[paldir]=='\\'||c[paldir]=='/'||c[paldir]==':') break;
	paldir=paldir==len;

	file_setfile(paldir?&file->palettedir:NULL,c,paldir?e:NULL,"pal");
	if (!file_open()) { file_close(); return 2; }

	if (file->is_zip) {
		if (!file->filename_in_zip_post||strlen(file->filename_in_zip_post)<4) { file_close(); return 4; }
		if (stricmp(file->filename_in_zip_post+(strlen(file->filename_in_zip_post)-4),".pal")!=0) { file_close(); return 4; }
	}

	if (file->size!=48) { file_close(); return 8; }
	if (!file_read(p,48)) { file_close(); return 16; }

	file_close();
	memcpy(draw_palette_load,p,48);

	/* set filename */
	len=strlen(file->name)+1;
	if (file->ext&&strlen(file->ext)) {
		ext=TRUE;
		len+=strlen(file->ext)+1;
	}

	MEM_CLEAN(draw.loadpalette);
	MEM_CREATE(draw.loadpalette,len);
	strcpy(draw.loadpalette,file->name);
	if (ext) {
		strcat(draw.loadpalette,".");
		strcat(draw.loadpalette,file->ext);
	}

	file_setdirectory(&file->palettedir);

	return 0;
}


/* change runtime settings */
void draw_set_window_size(void)
{
	int width,height;
	RECT workarea;
	float f,m;
	int i;

	/* correct zoom */
	if (!SystemParametersInfo(SPI_GETWORKAREA,0,&workarea,0)) {
		workarea.left=workarea.top=0;
		workarea.right=GetSystemMetrics(SM_CXFULLSCREEN);
		workarea.bottom=GetSystemMetrics(SM_CYFULLSCREEN);
	}
	f=(workarea.right-workarea.left)/(float)((float)draw.win_width/(draw.correct_aspect?2.0:1.0));
	m=(workarea.bottom-workarea.top)/(float)((float)draw.win_height/(draw.correct_aspect?2.0:1.0));
	if (f<m) m=f;
	i=m;
	if (i<1) i=1;
	if (draw.zoom==0) draw.zoom=i-(i>1);
	if (draw.zoom>i) {
		LOG(LOG_MISC|LOG_WARNING,"%dx size is out of bounds, set to %dx size\n",draw.zoom,i);
		draw.zoom=i;
	}
	draw.fzoom=draw.zoom/(draw.correct_aspect?2.0:1.0);
	draw.maxzoom=i;

	width=0.999+draw.win_width*draw.fzoom;
	height=0.999+draw.win_height*draw.fzoom;

	if (draw.fullscreen) draw_switch_screenmode();
	else {
		if (draw.init_done) ShowWindow(MAIN->window,SW_NORMAL);
		input_mouse_cursor_reset_ticks();
	}

	main_menu_radio((IDM_ZOOM1-1)+draw.zoom,IDM_ZOOM1,(IDM_ZOOM1-1)+ZOOM_MAX);

	SetWindowPos(
		MAIN->window,	/* this window handle */
		NULL,			/* new position in the z-order: ignored */
		0,0,			/* new x,y position: ignored */
		MAIN->x_plus+width,MAIN->y_plus+height, 	/* new width, height */
		SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER	/* flags: ignore repositioning */
	);

	GetWindowRect(MAIN->window,&draw.winrect);
}

void draw_set_zoom(int z)
{
	msx_wait();

	draw.zoom=z;
	draw_set_window_size();

	msx_wait_done();
}

void draw_switch_screenmode(void)
{
	/* odd:
	- switching screenmode while a dialog is in the foreground and inputs are enabled in the background
	  causes the menu to become enabled (normally it's automatically disabled if a dialog pops up)
	- switching screenmode with doubleclick sometimes causes a window behind this window to be selected
	*/

	draw.fullscreen^=1;

	/* change to fullscreen */
	if (draw.fullscreen) {
		/* keep previous window size */
		if (IsZoomed(MAIN->window)||!GetWindowRect(MAIN->window,&draw.winrect)) {
			if (draw.winrect.right<=draw.winrect.left) draw.winrect.right=draw.winrect.left+MAIN->x_plus+(0.999+draw.win_width*draw.fzoom);
			if (draw.winrect.bottom<=draw.winrect.top) draw.winrect.bottom=draw.winrect.top+MAIN->y_plus+(0.999+draw.win_height*draw.fzoom);
		}

		SetWindowLong(MAIN->window,GWL_STYLE,WS_POPUP|WS_CLIPCHILDREN);
		SetWindowLong(MAIN->window,GWL_EXSTYLE,0);
		draw.menuvis=FALSE; SetMenu(MAIN->window,NULL);
		ShowWindow(MAIN->window,SW_MAXIMIZE);

		input_mouse_cursor_reset_ticks();
		main_menu_check(IDM_FULLSCREEN,TRUE);
	}

	/* change to windowed */
	else {
		SetWindowLong(MAIN->window,GWL_STYLE,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN);
		SetWindowLong(MAIN->window,GWL_EXSTYLE,WS_EX_CLIENTEDGE|WS_EX_ACCEPTFILES);
		if (!draw.menuvis) SetMenu(MAIN->window,MAIN->menu);
		ShowWindow(MAIN->window,SW_NORMAL);

		SetWindowPos(MAIN->window,NULL,draw.winrect.left,draw.winrect.top,draw.winrect.right-draw.winrect.left,draw.winrect.bottom-draw.winrect.top,SWP_NOOWNERZORDER|SWP_NOZORDER);
		input_mouse_cursor_reset_ticks();
		main_menu_check(IDM_FULLSCREEN,FALSE);

		InvalidateRect(NULL,NULL,FALSE);
	}
}

void draw_switch_vidformat(void)
{
	msx_wait();

	draw.vidformat^=1;
	crystal_set_mode(TRUE);

	msx_wait_done();
}

void draw_set_correct_aspect(int aspect)
{
	draw.correct_aspect=aspect;

	draw.win_width=draw.correct_aspect?TI_NTSC_OUT_WIDTH(draw.source_width_winrel)-(NTSC_CLIP_L+NTSC_CLIP_R):draw.source_width_winrel;
	draw.win_height=draw.source_height_winrel<<draw.correct_aspect;

	/* (NTSC library width is off by a few pixels) */
	/* if (draw_ntsc.vidsignal==DRAW_VIDSIGNAL_RGB&&draw.correct_aspect) draw.win_width*=0.97; */

	main_menu_check(IDM_CORRECTASPECT,draw.correct_aspect);
}

void draw_set_stretch(int stretch)
{
	draw.stretch=stretch;

	main_menu_check(IDM_STRETCHFIT,draw.stretch);
}

void draw_set_vsync(int v)
{
	draw.vsync=v;

	main_menu_check(IDM_VSYNC,draw.vsync);
}

void draw_set_bfilter(int bf)
{
	/* odd: bfilter disabled sometimes causes strange artifacts. I've noticed it happening in other applications too,
	so it's probably a hardware/DirectX quirk (tested on more than 1 computer) */

	if (d3d.d3d==NULL) return; /* d3d only */

	draw.bfilter_prev=draw.bfilter;
	draw.bfilter=bf;

	main_menu_check(IDM_BFILTER,draw.bfilter);
}

void draw_set_flip_x(int f)
{
	if (d3d.d3d==NULL) return; /* d3d only */

	draw.flip_x_prev=draw.flip_x;
	draw.flip_x=f;

	main_menu_check(IDM_FLIP_X,draw.flip_x);
}

static void draw_copy_msx_screen(int width_old,int height_old,u8* screen_old)
{
	int height=height_old<draw.source_height?height_old:draw.source_height;

	/* black */
	if (width_old<draw.source_width||height_old<draw.source_height) {
		u8* screen=draw.screen;
		int i=draw.source_width*draw.source_height;
		while (i--) *screen++=1;
	}

	/* same width */
	if (width_old==draw.source_width) memcpy(draw.screen,screen_old,sizeof(u8)*draw.source_width*height);

	/* (overscan otherwise) */
}

static void draw_change_vidsignal(void)
{
	int vidsignal_old=draw_ntsc.vidsignal;

	draw_ntsc.vidsignal=draw.surface_change&0xffff;

	draw_init_ntsc_vidsignal();
	draw_reinit_ntsc();

	if (draw_ntsc.vidsignal==vidsignal_old) return;

	if (draw_ntsc.vidsignal==DRAW_VIDSIGNAL_MONOCHROME||vidsignal_old==DRAW_VIDSIGNAL_MONOCHROME) draw_set_repaint(2); /* borders change colour */

	if (draw_ntsc.vidsignal==DRAW_VIDSIGNAL_RGB||vidsignal_old==DRAW_VIDSIGNAL_RGB) {
		int source_width_old=draw.source_width;
		int source_height_old=draw.source_height;

		draw_correct_dimensions();

		if (source_width_old!=draw.source_width||source_height_old!=draw.source_height) {
			u8* screen_old=draw.screen;

			/* recreate MSX screen */
			MEM_CREATE(draw.screen,sizeof(u8)*draw.source_width*draw.source_height);
			draw_copy_msx_screen(source_width_old,source_height_old,screen_old);
			MEM_CLEAN(screen_old);
		}

		WaitForSingleObject(draw.d_mutex,D_MUTEX_TIME); (*drawdev_recreate_screen)(); ReleaseMutex(draw.d_mutex);
	}

	draw_update_vidsignal_radio();
}

/* dialogs */
/* etc */
static void draw_setslider_float(HWND dialog,u32 slider,u32 edit,float v,int init,u32* pos)
{
	HWND item=GetDlgItem(dialog,slider);

	/* init slider/limit */
	if (init&1) {
		int i;
		SendMessage(item,TBM_SETRANGE,FALSE,(LPARAM)MAKELONG(0,100));
		for (i=10;i<100;i+=10) SendMessage(item,TBM_SETTIC,0,(LPARAM)(LONG)i);

		SendDlgItemMessage(dialog,edit,EM_LIMITTEXT,9,0); /* -1.123456 */
	}

	/* fill editbox */
	if (init&2) {
		char f[0x10];

		sprintf(f,"%f",v);
		SetDlgItemText(dialog,edit,f);
	}

	*pos=(v+1.0)*50.0;
	SendMessage(item,TBM_SETPOS,TRUE,(LPARAM)(LONG)*pos);
}

static u32 draw_changeslider_float(HWND dialog,u32 slider,u32 edit,u32 pos,float* v,u32 sc)
{
	HWND item=GetDlgItem(dialog,slider);
	char f[0x10];
	u32 i;

	i=SendMessage(item,TBM_GETPOS,0,0);
	if (i==pos) return i;

	*v=(i/50.0)-1.0;
	sprintf(f,"%f",*v);
	SetDlgItemText(dialog,edit,f);

	draw_set_surface_change(sc);

	return i;
}

static float draw_changeedit_float(HWND dialog,u32 edit,float v,float min,float max)
{
	char f[0x10];
	float in; int inv=1;

	if (GetDlgItemText(dialog,edit,(LPTSTR)f,0x10)) {
		in=0.0; sscanf(f,"%f",&in);
		// inv=in==0.0;
		inv = ( fabs(in - 0.0f) < FLT_EPSILON ) ? TRUE : FALSE;
		in=1.0; sscanf(f,"%f",&in);
		// inv&=(in==1.0);
		inv &= ( ( fabs(in - 1.0f) < FLT_EPSILON ) ? TRUE : FALSE );
	}

	if (!inv) v=in;

	CLAMP(v,min,max);
	sprintf(f,"%f",v);
	SetDlgItemText(dialog,edit,f);

	return v;
}

/* custom video signal */
INT_PTR CALLBACK draw_change_vidsignal_custom( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam )
{
    lParam = lParam;
	static int vidsignal_old=DRAW_VIDSIGNAL_CUSTOM;
	static float sharpness_old=0.0;		static u32 sharpness_pos=50;
	static float resolution_old=0.0;	static u32 resolution_pos=50;
	static float artifacts_old=0.0;		static u32 artifacts_pos=50;
	static float fringing_old=0.0;		static u32 fringing_pos=50;
	static float bleed_old=0.0;			static u32 bleed_pos=50;

	switch (msg) {

		case WM_INITDIALOG:
			draw_set_surface_change(DRAW_SC_VIDSIGNAL|DRAW_VIDSIGNAL_CUSTOM);

			vidsignal_old=draw_ntsc.vidsignal;
			sharpness_old=draw_ntsc.sharpness;
			resolution_old=draw_ntsc.resolution;
			artifacts_old=draw_ntsc.artifacts;
			fringing_old=draw_ntsc.fringing;
			bleed_old=draw_ntsc.bleed;

			draw_setslider_float(dialog,IDC_VIDSIGNAL_SHARPNESS_SLIDER,IDC_VIDSIGNAL_SHARPNESS_EDIT,draw_ntsc.sharpness,3,&sharpness_pos);
			draw_setslider_float(dialog,IDC_VIDSIGNAL_RESOLUTION_SLIDER,IDC_VIDSIGNAL_RESOLUTION_EDIT,draw_ntsc.resolution,3,&resolution_pos);
			draw_setslider_float(dialog,IDC_VIDSIGNAL_FRINGING_SLIDER,IDC_VIDSIGNAL_FRINGING_EDIT,draw_ntsc.fringing,3,&fringing_pos);
			draw_setslider_float(dialog,IDC_VIDSIGNAL_ARTIFACTS_SLIDER,IDC_VIDSIGNAL_ARTIFACTS_EDIT,draw_ntsc.artifacts,3,&artifacts_pos);
			draw_setslider_float(dialog,IDC_VIDSIGNAL_BLEED_SLIDER,IDC_VIDSIGNAL_BLEED_EDIT,draw_ntsc.bleed,3,&bleed_pos);

			main_parent_window(dialog,0,0,0,0,0);

			return 1;
			break;

		case WM_NOTIFY:

			switch (wParam) {

				/* change sliders */
				case IDC_VIDSIGNAL_SHARPNESS_SLIDER: sharpness_pos=draw_changeslider_float(dialog,wParam,IDC_VIDSIGNAL_SHARPNESS_EDIT,sharpness_pos,&draw_ntsc.sharpness,DRAW_SC_VIDSIGNAL|DRAW_VIDSIGNAL_CUSTOM); break;
				case IDC_VIDSIGNAL_RESOLUTION_SLIDER: resolution_pos=draw_changeslider_float(dialog,wParam,IDC_VIDSIGNAL_RESOLUTION_EDIT,resolution_pos,&draw_ntsc.resolution,DRAW_SC_VIDSIGNAL|DRAW_VIDSIGNAL_CUSTOM); break;
				case IDC_VIDSIGNAL_FRINGING_SLIDER: fringing_pos=draw_changeslider_float(dialog,wParam,IDC_VIDSIGNAL_FRINGING_EDIT,fringing_pos,&draw_ntsc.fringing,DRAW_SC_VIDSIGNAL|DRAW_VIDSIGNAL_CUSTOM); break;
				case IDC_VIDSIGNAL_ARTIFACTS_SLIDER: artifacts_pos=draw_changeslider_float(dialog,wParam,IDC_VIDSIGNAL_ARTIFACTS_EDIT,artifacts_pos,&draw_ntsc.artifacts,DRAW_SC_VIDSIGNAL|DRAW_VIDSIGNAL_CUSTOM); break;
				case IDC_VIDSIGNAL_BLEED_SLIDER: bleed_pos=draw_changeslider_float(dialog,wParam,IDC_VIDSIGNAL_BLEED_EDIT,bleed_pos,&draw_ntsc.bleed,DRAW_SC_VIDSIGNAL|DRAW_VIDSIGNAL_CUSTOM); break;

				default: break;
			}
			break;

		case WM_COMMAND:

			switch (LOWORD(wParam)) {

				/* editboxes */
				#define CV_CHANGEEDIT(v,s,p)											\
					 if (HIWORD(wParam)==EN_KILLFOCUS) {								\
					 	v=draw_changeedit_float(dialog,LOWORD(wParam),v,-1.0,1.0);		\
					 	draw_setslider_float(dialog,s,LOWORD(wParam),v,0,p);			\
					 draw_set_surface_change(DRAW_SC_VIDSIGNAL|DRAW_VIDSIGNAL_CUSTOM);	\
					 }

				case IDC_VIDSIGNAL_SHARPNESS_EDIT: CV_CHANGEEDIT(draw_ntsc.sharpness,IDC_VIDSIGNAL_SHARPNESS_SLIDER,&sharpness_pos) break;
				case IDC_VIDSIGNAL_RESOLUTION_EDIT: CV_CHANGEEDIT(draw_ntsc.resolution,IDC_VIDSIGNAL_RESOLUTION_SLIDER,&resolution_pos) break;
				case IDC_VIDSIGNAL_FRINGING_EDIT: CV_CHANGEEDIT(draw_ntsc.fringing,IDC_VIDSIGNAL_FRINGING_SLIDER,&fringing_pos) break;
				case IDC_VIDSIGNAL_ARTIFACTS_EDIT: CV_CHANGEEDIT(draw_ntsc.artifacts,IDC_VIDSIGNAL_ARTIFACTS_SLIDER,&artifacts_pos) break;
				case IDC_VIDSIGNAL_BLEED_EDIT: CV_CHANGEEDIT(draw_ntsc.bleed,IDC_VIDSIGNAL_BLEED_SLIDER,&bleed_pos) break;

				/* close dialog */
				case IDCANCEL:
					if (vidsignal_old==DRAW_VIDSIGNAL_CUSTOM) {
						draw_ntsc.sharpness=sharpness_old;
						draw_ntsc.resolution=resolution_old;
						draw_ntsc.artifacts=artifacts_old;
						draw_ntsc.fringing=fringing_old;
						draw_ntsc.bleed=bleed_old;
					}

					draw_set_surface_change(DRAW_SC_VIDSIGNAL|vidsignal_old);
					EndDialog(dialog,0);
					break;

				case IDOK: EndDialog(dialog,0); break;

				default: break;
			}
			break;

		default: break;
	}

	return 0;
}

/* palette */
/* palette edit */
static INT_PTR CALLBACK draw_palette_edit( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam )
{
	static UINT current=IDC_PALEDIT_00;
	static u8 palette[48];
	static int initdone=FALSE;
	static int same_was_saved=FALSE;

	switch (msg) {

		case WM_INITDIALOG: {
			int i,j;

			memcpy(palette,draw_palette_load,48);
			current=IDC_PALEDIT_00;

			/* init controls */
			for (i=0;i<3;i++) {
				SendDlgItemMessage(dialog,IDC_PALEDIT_RSLIDER+i,TBM_SETRANGE,FALSE,(LPARAM)MAKELONG(0,255));
				for (j=0x20;j<0x100;j+=0x20) SendDlgItemMessage(dialog,IDC_PALEDIT_RSLIDER+i,TBM_SETTIC,0,(LPARAM)(LONG)j);
				SendDlgItemMessage(dialog,IDC_PALEDIT_REDIT+i,EM_LIMITTEXT,3,0);
				SetDlgItemInt(dialog,IDC_PALEDIT_REDIT+i,palette[i],FALSE);
				SendDlgItemMessage(dialog,IDC_PALEDIT_RSLIDER+i,TBM_SETPOS,TRUE,(LPARAM)(LONG)palette[i]);
			}

			main_parent_window(dialog,MAIN_PW_RIGHT,MAIN_PW_LEFT,50,-30,0);
			same_was_saved=FALSE;
			initdone=TRUE;

			return 1;
			break;
		}

		case WM_CTLCOLORSTATIC: {
			UINT i=GetDlgCtrlID((HWND)lParam);

			/* change palette background */
			if (i>=IDC_PALEDIT_00&&i<=IDC_PALEDIT_15) {
				i-=IDC_PALEDIT_00;
				if (draw.paledit_brush[i]) DeleteObject(draw.paledit_brush[i]);
				draw.paledit_brush[i]=CreateSolidBrush(RGB(palette[i*3],palette[i*3+1],palette[i*3+2]));
				return draw.paledit_brush[i] == NULL ? FALSE : TRUE; /* (BOOL) */
			}
			break;
		}

		case WM_LBUTTONDOWN: {
			int i;
			HWND item;
			RECT rect={0,0,0,0};
			POINT p={0,0};

			GetCursorPos(&p);

			/* simulate palette as radiobutton */
			for (i=IDC_PALEDIT_00;i<=IDC_PALEDIT_15;i++) {
				item=GetDlgItem(dialog,i);
				GetWindowRect(item,&rect);

				if (PtInRect(&rect,p)) {
					if ((UINT)i!=current) {
						/* down */
						SetWindowLongPtr(item,GWL_EXSTYLE,WS_EX_CLIENTEDGE);
						SetWindowPos(item,NULL,0,0,0,0,SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOSIZE);

						/* prev up */
						item=GetDlgItem(dialog,current);
						SetWindowLongPtr(item,GWL_EXSTYLE,WS_EX_DLGMODALFRAME);
						SetWindowPos(item,NULL,0,0,0,0,SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOSIZE);

						current=i;

						SetDlgItemInt(dialog,IDC_PALEDIT_COLOUR,i-IDC_PALEDIT_00,FALSE);

						i=(i-IDC_PALEDIT_00)*3;
						SetDlgItemInt(dialog,IDC_PALEDIT_REDIT,palette[i],FALSE);
						SetDlgItemInt(dialog,IDC_PALEDIT_GEDIT,palette[i+1],FALSE);
						SetDlgItemInt(dialog,IDC_PALEDIT_BEDIT,palette[i+2],FALSE);
					}
					break;
				}
			}

			break;
		}

		case WM_NOTIFY:
			switch (wParam) {
				/* change sliders */
				case IDC_PALEDIT_RSLIDER: case IDC_PALEDIT_GSLIDER: case IDC_PALEDIT_BSLIDER:
					if (initdone&&GetDlgItem(dialog,current)!=NULL&&GetDlgItem(dialog,wParam)!=NULL) {
						int m=wParam-IDC_PALEDIT_RSLIDER;
						u32 i=GetDlgItemInt(dialog,IDC_PALEDIT_REDIT+m,NULL,FALSE);
						int j=SendDlgItemMessage(dialog,wParam,TBM_GETPOS,0,0);

						/* set editbox (handle colour change there) */
						if (i!=(u32)j) SetDlgItemInt(dialog,IDC_PALEDIT_REDIT+m,j,FALSE);
					}
					break;

				default: break;
			}
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {

				/* editboxes */
				case IDC_PALEDIT_REDIT: case IDC_PALEDIT_GEDIT: case IDC_PALEDIT_BEDIT:
					if (initdone&&GetDlgItem(dialog,current)!=NULL&&GetDlgItem(dialog,LOWORD(wParam))!=NULL) {
						int m=(LOWORD(wParam))-IDC_PALEDIT_REDIT;
						int j; u32 i=GetDlgItemInt(dialog,LOWORD(wParam),NULL,FALSE);
						if (i>255) { i=255; SetDlgItemInt(dialog,LOWORD(wParam),i,FALSE); }
						palette[(current-IDC_PALEDIT_00)*3+m]=i;
						InvalidateRect(GetDlgItem(dialog,current),NULL,FALSE);

						/* set slider */
						j=SendDlgItemMessage(dialog,IDC_PALEDIT_RSLIDER+m,TBM_GETPOS,0,0);
						if ((u32)j!=i) SendDlgItemMessage(dialog,IDC_PALEDIT_RSLIDER+m,TBM_SETPOS,TRUE,(LPARAM)(LONG)i);
					}
					break;

				/* save */
				case IDC_PALEDIT_SAVE: {
					const char* filter="Palette File (*.pal)\0*.pal\0\0";
					const char* defext="pal";
					const char* title="Save Palette As";
					char fn[STRING_SIZE]={0};
					OPENFILENAME of;
					int same=FALSE;

					if (draw.palette==DRAW_PALETTE_FILE&&draw.loadpalette&&strlen(draw.loadpalette)) {
						same=TRUE;

						file_setfile(&file->palettedir,draw.loadpalette,NULL,NULL);
						if (file_accessible()) strcpy(fn,file->filename);
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
					of.nFilterIndex=1;
					of.Flags=OFN_ENABLESIZING|OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
					of.lpstrInitialDir=strlen(fn)?NULL:file->palettedir?file->palettedir:file->appdir;

					if (GetSaveFileName(&of)) {
						/* same as loaded palette? */
						if (same) {
							file_setfile(&file->palettedir,draw.loadpalette,NULL,NULL);
							same=(stricmp(fn,file->filename)==0);
						}

						/* save file */
						file_setfile(NULL,fn,NULL,NULL);
						if (file_save()&&file_write(palette,48)) {
							file_close();
							same_was_saved|=same;
							break;
						}

						/* error */
						else {
							file_close();
							LOG_ERROR_WINDOW(dialog,"Couldn't save palette!");
						}
					}

					PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDOK),TRUE);

					break;
				}

				/* close */
				case IDOK: case IDCANCEL: {
					int i;
					for (i=0;i<0x10;i++) {
						if (draw.paledit_brush[i]) { DeleteObject(draw.paledit_brush[i]); draw.paledit_brush[i]=NULL; }
					}
					initdone=FALSE;
					EndDialog(dialog,same_was_saved);
					break;
				}


				default: break;
			}
			break;

		default: break;
	}

	return 0;
}

/* palette etc */
static void draw_setslider_hue(HWND dialog,int init,u32* pos)
{
	HWND item=GetDlgItem(dialog,IDC_PALGEN_HUE_SLIDER);

	/* init slider/limit */
	if (init&1) {
		int i;

		SendMessage(item,TBM_SETRANGE,FALSE,(LPARAM)MAKELONG(0,120));
		for (i=12;i<120;i+=12) SendMessage(item,TBM_SETTIC,0,(LPARAM)(LONG)i);

		SendDlgItemMessage(dialog,IDC_PALGEN_HUE_EDIT,EM_LIMITTEXT,11,0); /* -123.123456 */
	}

	/* fill editbox */
	if (init&2) {
		char f[0x10];
		if (draw_ntsc.hue<180.0) sprintf(f,"%f",draw_ntsc.hue);
		else sprintf(f,"%f",draw_ntsc.hue-360.0);
		SetDlgItemText(dialog,IDC_PALGEN_HUE_EDIT,f);
	}

	*pos=(u32)(draw_ntsc.hue/3.0+60.1)%121;
	SendMessage(item,TBM_SETPOS,TRUE,(LPARAM)(LONG)*pos);
}

static void draw_setslider_decoder(HWND dialog,u32 slider,u32 edit,int v,int init)
{
	HWND item=GetDlgItem(dialog,slider);

	/* init slider/limit */
	if (init&1) {
		int i;
		SendMessage(item,TBM_SETRANGE,FALSE,(LPARAM)MAKELONG(0,359));
		for (i=36;i<359;i+=36) SendMessage(item,TBM_SETTIC,0,(LPARAM)(LONG)i);

		/* TBM_SETBUDDY doesn't seem to work? */
		/* SendMessage(item,TBM_SETBUDDY,0,(LPARAM)GetDlgItem(dialog,edit)); */

		SendDlgItemMessage(dialog,edit,EM_LIMITTEXT,3,0);
	}

	/* fill editbox */
	if (init&2) SetDlgItemInt(dialog,edit,v,FALSE);

	SendMessage(item,TBM_SETPOS,TRUE,(LPARAM)(LONG)v);
}

static void draw_changeslider_decoder(HWND dialog,u32 slider,u32 edit,int index)
{
	int pos;

	if (draw_ntsc.decoder_type!=DRAW_DECODER_CUSTOM) return;

	pos=SendDlgItemMessage(dialog,slider,TBM_GETPOS,0,0);
	SetDlgItemInt(dialog,edit,pos,FALSE);
	draw_ntsc.decoder[index]=(float)pos;

	draw_set_surface_change(DRAW_SC_PALETTE);
}

static void draw_setgain_decoder(HWND dialog,u32 edit,float v,int init)
{
	char f[0x10];

	if (init) SendDlgItemMessage(dialog,edit,EM_LIMITTEXT,8,0); /* 1.123456 */

	sprintf(f,"%f",v);
	SetDlgItemText(dialog,edit,f);
}

static void draw_decoder_enable(HWND dialog,int enable)
{
	int i;

	for (i=IDC_PALDEC_PROP;i<=IDC_PALDEC_PROPBY;i++) EnableWindow(GetDlgItem(dialog,i),enable);
}

/* palette main */
INT_PTR CALLBACK draw_palette_settings( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam )
{
    lParam = lParam;
	static int palette_old=DRAW_PALETTE_DEFAULT;
	static float hue_old=0.0;			static u32 hue_pos=50;
	static float saturation_old=0.0;	static u32 saturation_pos=50;
	static float contrast_old=0.0;		static u32 contrast_pos=50;
	static float brightness_old=0.0;	static u32 brightness_pos=50;
	static float gamma_old=0.0;			static u32 gamma_pos=50;

	static int decoder_type_old=DRAW_DECODER_DEFAULT;
	static float decoder_old[6]={0,0,0,0,0,0}; /* rya,gya,bya,ryg,gyg,byg */

	static char* draw_loadpalette_old=NULL;
	static char* draw_file_palettedir_old=NULL;
	static u8 draw_palette_load_old[48];

	switch (msg) {

		case WM_INITDIALOG: {
			int i;
			HWND c;
			char t[0x100];

			/* palette preset */
			palette_old=draw.palette;
			c=GetDlgItem(dialog,IDC_PALGEN_PRESET);
			for (i=0;i<DRAW_PALETTE_FILE;i++) {
				if (i==DRAW_PALETTE_DEFAULT) {
					sprintf(
                        t,
                    #ifdef MEISEI_ESP
                        "%s (Por Defecto)",
                    #else
                        "%s (Default)",
                    #endif // MEISEI_ESP
                        draw_get_palette_name(i)
                    );
					SendMessage(c,CB_ADDSTRING,0,(LPARAM)((LPCTSTR)t));
				}
				else SendMessage(c,CB_ADDSTRING,0,(LPARAM)((LPCTSTR)draw_get_palette_name(i)));
			}
			SendMessage(
                c,
                CB_ADDSTRING,
                0,
            #ifdef MEISEI_ESP
                (LPARAM)((LPCTSTR)"Archivo...")
            #else
                (LPARAM)((LPCTSTR)"File...")
            #endif // MEISEI_ESP
            );

			if (draw.palette>DRAW_PALETTE_FILE) draw.palette=DRAW_PALETTE_DEFAULT;
			SendMessage(c,CB_SETCURSEL,draw.palette,0);

			/* decoder preset */
			decoder_type_old=draw_ntsc.decoder_type;
			c=GetDlgItem(dialog,IDC_PALDEC_PRESET);
			for (i=0;i<DRAW_DECODER_MAX;i++) {
				if (i==DRAW_DECODER_DEFAULT) {
					sprintf(
                        t,
                    #ifdef MEISEI_ESP
                        "%s (Por Defecto)",
                    #else
                        "%s (Default)",
                    #endif // MEISEI_ESP
                        draw_get_decoder_name(i)
                    );
					SendMessage(c,CB_ADDSTRING,0,(LPARAM)((LPCTSTR)t));
				}
				else SendMessage(c,CB_ADDSTRING,0,(LPARAM)((LPCTSTR)draw_get_decoder_name(i)));
			}
			SendMessage(c,CB_SETCURSEL,draw_ntsc.decoder_type,0);
			if (draw_ntsc.decoder_type!=DRAW_DECODER_CUSTOM) draw_decoder_enable(dialog,FALSE);

			/* loaded palette */
			MEM_CLEAN(draw_loadpalette_old);
			if (draw.loadpalette) {
				MEM_CREATE(draw_loadpalette_old,strlen(draw.loadpalette)+1);
				memcpy(draw_loadpalette_old,draw.loadpalette,strlen(draw.loadpalette)+1);
			}
			MEM_CLEAN(draw_file_palettedir_old);
			if (file->palettedir) {
				MEM_CREATE(draw_file_palettedir_old,strlen(file->palettedir)+1);
				memcpy(draw_file_palettedir_old,file->palettedir,strlen(file->palettedir)+1);
			}
			memcpy(draw_palette_load_old,draw_palette_load,48);

			/* palette tuning sliders */
			hue_old=draw_ntsc.hue;
			saturation_old=draw_ntsc.saturation;
			contrast_old=draw_ntsc.contrast;
			brightness_old=draw_ntsc.brightness;
			gamma_old=draw_ntsc.gamma;

			#define SETSLIDERS_PALETTE(x)					\
				draw_setslider_hue(dialog,x,&hue_pos);		\
				draw_setslider_float(dialog,IDC_PALGEN_SAT_SLIDER,IDC_PALGEN_SAT_EDIT,draw_ntsc.saturation,x,&saturation_pos);	\
				draw_setslider_float(dialog,IDC_PALGEN_CON_SLIDER,IDC_PALGEN_CON_EDIT,draw_ntsc.contrast,x,&contrast_pos);		\
				draw_setslider_float(dialog,IDC_PALGEN_BRI_SLIDER,IDC_PALGEN_BRI_EDIT,draw_ntsc.brightness,x,&brightness_pos);	\
				draw_setslider_float(dialog,IDC_PALGEN_GAM_SLIDER,IDC_PALGEN_GAM_EDIT,draw_ntsc.gamma,x,&gamma_pos)

			SETSLIDERS_PALETTE(3);

			/* decoder etc */
			memcpy(decoder_old,draw_ntsc.decoder,sizeof(draw_ntsc.decoder));
			draw_setslider_decoder(dialog,IDC_PALDEC_RY_SLIDER,IDC_PALDEC_RY_EDIT,(int)draw_ntsc.decoder[0],3);
			draw_setslider_decoder(dialog,IDC_PALDEC_GY_SLIDER,IDC_PALDEC_GY_EDIT,(int)draw_ntsc.decoder[1],3);
			draw_setslider_decoder(dialog,IDC_PALDEC_BY_SLIDER,IDC_PALDEC_BY_EDIT,(int)draw_ntsc.decoder[2],3);
			draw_setgain_decoder(dialog,IDC_PALDEC_RY_GAIN,draw_ntsc.decoder[3],TRUE);
			draw_setgain_decoder(dialog,IDC_PALDEC_GY_GAIN,draw_ntsc.decoder[4],TRUE);
			draw_setgain_decoder(dialog,IDC_PALDEC_BY_GAIN,draw_ntsc.decoder[5],TRUE);

			draw_set_surface_change(DRAW_SC_PALETTE);

			main_parent_window(dialog,0,0,0,0,0);
			return 1;
			break;
		}

		case WM_NOTIFY:

			switch (wParam) {

				/* change palette sliders */
				case IDC_PALGEN_SAT_SLIDER: saturation_pos=draw_changeslider_float(dialog,wParam,IDC_PALGEN_SAT_EDIT,saturation_pos,&draw_ntsc.saturation,DRAW_SC_PALETTE); break;
				case IDC_PALGEN_CON_SLIDER: contrast_pos=draw_changeslider_float(dialog,wParam,IDC_PALGEN_CON_EDIT,contrast_pos,&draw_ntsc.contrast,DRAW_SC_PALETTE); break;
				case IDC_PALGEN_BRI_SLIDER: brightness_pos=draw_changeslider_float(dialog,wParam,IDC_PALGEN_BRI_EDIT,brightness_pos,&draw_ntsc.brightness,DRAW_SC_PALETTE); break;
				case IDC_PALGEN_GAM_SLIDER: gamma_pos=draw_changeslider_float(dialog,wParam,IDC_PALGEN_GAM_EDIT,gamma_pos,&draw_ntsc.gamma,DRAW_SC_PALETTE); break;
				case IDC_PALGEN_HUE_SLIDER: {
					HWND item=GetDlgItem(dialog,wParam);
					int i=SendMessage(item,TBM_GETPOS,0,0);

					if ((u32)i!=hue_pos) {
						char f[0x10];

						hue_pos=i; i-=60;
						if (i<0) i+=120;
						if (hue_pos==120) draw_ntsc.hue=179.999;
						else draw_ntsc.hue=i*3.0;

						if (draw_ntsc.hue<180.0) sprintf(f,"%f",draw_ntsc.hue);
						else sprintf(f,"%f",draw_ntsc.hue-360.0);
						SetDlgItemText(dialog,IDC_PALGEN_HUE_EDIT,f);
						draw_set_surface_change(DRAW_SC_PALETTE);
					}

					break;
				}

				/* change decoder sliders */
				case IDC_PALDEC_RY_SLIDER: draw_changeslider_decoder(dialog,wParam,IDC_PALDEC_RY_EDIT,0); break;
				case IDC_PALDEC_GY_SLIDER: draw_changeslider_decoder(dialog,wParam,IDC_PALDEC_GY_EDIT,1); break;
				case IDC_PALDEC_BY_SLIDER: draw_changeslider_decoder(dialog,wParam,IDC_PALDEC_BY_EDIT,2); break;

				default: break;
			}
			break;

		case WM_COMMAND:

			switch (LOWORD(wParam)) {

				/* palette tuning editboxes */
				#define PT_CHANGEEDIT(v,s,p)										\
					 if (HIWORD(wParam)==EN_KILLFOCUS) {							\
					 	v=draw_changeedit_float(dialog,LOWORD(wParam),v,-1.0,1.0);	\
					 	draw_setslider_float(dialog,s,LOWORD(wParam),v,0,p);		\
					 	draw_set_surface_change(DRAW_SC_PALETTE);					\
					 }

				case IDC_PALGEN_SAT_EDIT: PT_CHANGEEDIT(draw_ntsc.saturation,IDC_PALGEN_SAT_SLIDER,&saturation_pos) break;
				case IDC_PALGEN_CON_EDIT: PT_CHANGEEDIT(draw_ntsc.contrast,IDC_PALGEN_CON_SLIDER,&contrast_pos) break;
				case IDC_PALGEN_BRI_EDIT: PT_CHANGEEDIT(draw_ntsc.brightness,IDC_PALGEN_BRI_SLIDER,&brightness_pos) break;
				case IDC_PALGEN_GAM_EDIT: PT_CHANGEEDIT(draw_ntsc.gamma,IDC_PALGEN_GAM_SLIDER,&gamma_pos) break;
				case IDC_PALGEN_HUE_EDIT:
					if (HIWORD(wParam)==EN_KILLFOCUS) {
						draw_ntsc.hue=draw_changeedit_float(dialog,LOWORD(wParam),draw_ntsc.hue,-180.0,179.999);
						draw_setslider_hue(dialog,0,&hue_pos);
						draw_set_surface_change(DRAW_SC_PALETTE);
					}
					break;

				/* decoder angle editboxes */
				#define DA_CHANGEEDIT(n,s)											\
					if (HIWORD(wParam)==EN_KILLFOCUS) {								\
						u32 i=GetDlgItemInt(dialog,LOWORD(wParam),NULL,FALSE);		\
						if (i>359) i=359;                                           \
						draw_ntsc.decoder[n]=(float)i;			                    \
						draw_setslider_decoder(dialog,s,HIWORD(wParam),i,2);		\
					}

				case IDC_PALDEC_RY_EDIT: DA_CHANGEEDIT(0,IDC_PALDEC_RY_SLIDER) break;
				case IDC_PALDEC_GY_EDIT: DA_CHANGEEDIT(1,IDC_PALDEC_GY_SLIDER) break;
				case IDC_PALDEC_BY_EDIT: DA_CHANGEEDIT(2,IDC_PALDEC_BY_SLIDER) break;

				/* decoder gain editboxes */
				#define DG_CHANGEEDIT(v)											\
					if (HIWORD(wParam)==EN_KILLFOCUS) {								\
						v=draw_changeedit_float(dialog,LOWORD(wParam),v,0.0,2.0);	\
						draw_set_surface_change(DRAW_SC_PALETTE);					\
					}

				case IDC_PALDEC_RY_GAIN: DG_CHANGEEDIT(draw_ntsc.decoder[3]) break;
				case IDC_PALDEC_GY_GAIN: DG_CHANGEEDIT(draw_ntsc.decoder[4]) break;
				case IDC_PALDEC_BY_GAIN: DG_CHANGEEDIT(draw_ntsc.decoder[5]) break;

				/* palette edit */
				case IDC_PALGEN_EDIT:
					if (
                        // HINSTANCE, LPCSTR,HWND, DLGPROC, LPARAM = 0
                        DialogBox(MAIN->module,MAKEINTRESOURCE(IDD_PALETTE_EDIT),dialog,(DLGPROC)draw_palette_edit)==1
                    ) {
						/* was saved as same file */
						char fn[STRING_SIZE]={0};
						file_setfile(&file->palettedir,draw.loadpalette,NULL,NULL);
						strcpy(fn,file->filename);
						if (draw_load_palette(fn,NULL)!=0) LOG_ERROR_WINDOW(dialog,"Couldn't reload palette!");
						else draw_set_surface_change(DRAW_SC_PALETTE);
					}

					PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDOK),TRUE);
					break;

				/* palette preset */
				case IDC_PALGEN_PRESET: {
					int s;
					if ((HIWORD(wParam))==CBN_SELCHANGE&&(s=SendDlgItemMessage(dialog,LOWORD(wParam),CB_GETCURSEL,0,0))!=CB_ERR) {
						if (s<DRAW_PALETTE_HARDCODED_MAX) {
							MEM_CLEAN(draw.loadpalette);
							draw.palette=s;
							draw_set_surface_change(DRAW_SC_PALETTE);
						}
						else {
							/* file */
							const char* filter="Palette Files (*.pal, *.zip)\0*.pal;*.zip\0All Files\0*.*\0\0";
							const char* title="Open Palette";
							char fn[STRING_SIZE]={0};
							OPENFILENAME of;
							int o=FALSE;

							memset(&of,0,sizeof(OPENFILENAME));

							/* check+set initial dir/filename */
							if (draw.loadpalette&&strlen(draw.loadpalette)) {
								file_setfile(&file->palettedir,draw.loadpalette,NULL,"pal");
								if (file->filename&&file_open()) strcpy(fn,file->filename);
								else memset(fn,0,STRING_SIZE);
								file_close();
							}

							of.lStructSize=sizeof(OPENFILENAME);
							of.hwndOwner=dialog;
							of.hInstance=MAIN->module;
							of.lpstrFile=fn;
							of.lpstrFilter=filter;
							of.lpstrTitle=title;
							of.nMaxFile=STRING_SIZE;
							of.nFilterIndex=draw.palette_fi;
							of.Flags=OFN_ENABLESIZING|OFN_HIDEREADONLY|OFN_FILEMUSTEXIST;
							of.lpstrInitialDir=strlen(fn)?NULL:file->palettedir?file->palettedir:file->appdir;

							if (GetOpenFileName(&of)) {
								o=(draw_load_palette(fn,NULL)==0);
								if (!o) LOG_ERROR_WINDOW(dialog,"Couldn't load palette!");
							}
							draw.palette_fi=of.nFilterIndex;

							if (o) { draw.palette=s; draw_set_surface_change(DRAW_SC_PALETTE); }
							else SendDlgItemMessage(dialog,IDC_PALGEN_PRESET,CB_SETCURSEL,draw.palette,0);
						}
					}
					break;
				}

				/* decoder preset */
				case IDC_PALDEC_PRESET: {
					int s;
					if ((HIWORD(wParam))==CBN_SELCHANGE&&(s=SendDlgItemMessage(dialog,LOWORD(wParam),CB_GETCURSEL,0,0))!=CB_ERR) {
						draw_ntsc.decoder_type=s;
						if (s!=DRAW_DECODER_CUSTOM) {
							draw_setslider_decoder(dialog,IDC_PALDEC_RY_SLIDER,IDC_PALDEC_RY_EDIT,(int)draw_decoder_preset[s][0],2);
							draw_setslider_decoder(dialog,IDC_PALDEC_GY_SLIDER,IDC_PALDEC_GY_EDIT,(int)draw_decoder_preset[s][1],2);
							draw_setslider_decoder(dialog,IDC_PALDEC_BY_SLIDER,IDC_PALDEC_BY_EDIT,(int)draw_decoder_preset[s][2],2);
							draw_setgain_decoder(dialog,IDC_PALDEC_RY_GAIN,draw_decoder_preset[s][3],TRUE);
							draw_setgain_decoder(dialog,IDC_PALDEC_GY_GAIN,draw_decoder_preset[s][4],TRUE);
							draw_setgain_decoder(dialog,IDC_PALDEC_BY_GAIN,draw_decoder_preset[s][5],TRUE);

							draw_decoder_enable(dialog,FALSE);
							draw_set_surface_change(DRAW_SC_PALETTE);
						}
						else draw_decoder_enable(dialog,TRUE);
					}
					break;
				}

				/* default palette tuning */
				case IDC_PALGEN_DEFAULT:
					draw_ntsc.hue=palette_tuning_default[0];
					draw_ntsc.saturation=palette_tuning_default[1];
					draw_ntsc.contrast=palette_tuning_default[2];
					draw_ntsc.brightness=palette_tuning_default[3];
					draw_ntsc.gamma=palette_tuning_default[4];

					SETSLIDERS_PALETTE(2);
					draw_set_surface_change(DRAW_SC_PALETTE);
					PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDOK),TRUE);

					break;

				/* close dialog */
				case IDCANCEL:
					draw.palette=palette_old;
					draw_ntsc.hue=hue_old;
					draw_ntsc.saturation=saturation_old;
					draw_ntsc.contrast=contrast_old;
					draw_ntsc.brightness=brightness_old;
					draw_ntsc.gamma=gamma_old;

					draw_ntsc.decoder_type=decoder_type_old;
					memcpy(draw_ntsc.decoder,decoder_old,sizeof(draw_ntsc.decoder));

					MEM_CLEAN(draw.loadpalette); draw.loadpalette=draw_loadpalette_old; draw_loadpalette_old=NULL;
					MEM_CLEAN(file->palettedir); file->palettedir=draw_file_palettedir_old; draw_file_palettedir_old=NULL;

					memcpy(draw_palette_load,draw_palette_load_old,48);

					/* reload palette in case it was changed */
					if (draw.palette==DRAW_PALETTE_FILE&&draw.loadpalette&&strlen(draw.loadpalette)&&file->palettedir&&strlen(file->palettedir)) {
						char fn[STRING_SIZE];
						file_setfile(&file->palettedir,draw.loadpalette,NULL,NULL);
						strcpy(fn,file->filename);
						if (draw_load_palette(fn,NULL)!=0) LOG(LOG_MISC|LOG_WARNING,"couldn't reload palette\n");
					}

					draw_set_surface_change(DRAW_SC_PALETTE);

					EndDialog(dialog,0);
					break;

				case IDOK:
					MEM_CLEAN(draw_loadpalette_old);
					MEM_CLEAN(draw_file_palettedir_old);
					EndDialog(dialog,0);
					break;

				default: break;
			}
			break;

		default: break;
	}

	return 0;
}


/* screenshot */
static void draw_screenshot(void)
{
	char fn[STRING_SIZE]={0};
	int cut,snum=0;

	mapper_get_current_name(fn);
	file_setfile(&file->shotdir,fn,NULL,NULL);
	strcpy(fn,file->filename); strcat(fn,".");
	cut=strlen(fn);

	/* try name.png */
	strcat(fn,"png");
	if (!access(fn,F_OK)) {
		char c[0x10];

		/* try name.2.png (etc) */
		for (snum=2;snum<10000;snum++) {
			fn[cut]=0;
			sprintf(c,"%d",snum);
			strcat(fn,c); strcat(fn,".png");
			if (access(fn,F_OK)) break;
		}
	}

	/* save */
	if (snum<10000&&screenshot_save(draw.source_width,draw.source_height,SCREENSHOT_TYPE_8BPP_INDEXED,(void*)draw.screen,(void*)draw_palette_32,fn)) {
		if (snum>0) LOG(LOG_MISC|LOG_COLOUR(LC_GREEN)|LOG_TYPE(LT_SCREENSHOT),"saved screenshot #%d     ",snum);
		else LOG(LOG_MISC|LOG_COLOUR(LC_GREEN)|LOG_TYPE(LT_SCREENSHOT),"saved screenshot         ");
	}
	else LOG(LOG_MISC|LOG_WARNING,"couldn't save screenshot\n");
}


/* draw */
void draw_draw(void)
{
	WINDOWPLACEMENT wp={0};
	wp.length=sizeof(WINDOWPLACEMENT);

	if (!draw.device_ready) return; /* not (yet) */

	if (draw.frameskip_set<0) {
		int i=sound_get_lostframes()>0;
		draw.frameskip_count=(i*(draw.frameskip_count+i))%(FRAMESKIP_MAX+1);
		draw.skippedframe=draw.frameskip_count!=0;
	}
	else {
		draw.skippedframe=draw.frameskip_count!=0;
		draw.frameskip_count=(draw.frameskip_count+1)%(draw.frameskip_set+1);
	}

	if (draw.skippedframe|draw.semaphore) return;

	draw.semaphore=TRUE;

	WaitForSingleObject(draw.sc_mutex,SC_MUTEX_TIME);
	switch (draw.surface_change&0xffff0000) {
		case DRAW_SC_VIDSIGNAL: draw_change_vidsignal(); break;
		case DRAW_SC_PALETTE: draw_init_ntsc_colour(); draw_init_palette(NULL); break;

		default: break;
	}
	draw.surface_change=0;
	ReleaseMutex(draw.sc_mutex);


	WaitForSingleObject(draw.d_mutex,D_MUTEX_TIME);

	/* external */
	if (draw.screenshot) {
		draw_screenshot();
		draw.screenshot=FALSE;
	}
	tool_repaint();

	/* render */
	if (GetWindowPlacement(MAIN->window,&wp)&&wp.showCmd==SW_SHOWMINIMIZED) { ; }
	else { (*drawdev_render)(); } /* not when minimized */
	draw_text_out(); draw_blit(TRUE);

	ReleaseMutex(draw.d_mutex);

	draw.semaphore=FALSE;
}

void draw_repaint(void)
{
	DWORD ticks=GetTickCount();

	ValidateRect(MAIN->window,NULL);

	if (!draw.device_ready) return;
	draw_set_repaint(2);

	if (((DWORD)(ticks-draw.repaint_ticks))<100||draw.skippedframe||sound_get_lostframes()>0||draw.semaphore) return;

	draw.semaphore=TRUE;
	WaitForSingleObject(draw.d_mutex,D_MUTEX_TIME); draw_blit(FALSE); ReleaseMutex(draw.d_mutex);
	draw.semaphore=FALSE;
	draw.repaint_ticks=ticks;
}

static void draw_blit(int vsync)
{
	float aspect_source=0.0,aspect_dest=0.0;

	/* rect sizes/position */
	GetClientRect(MAIN->window,&draw.rect_dest);

	if ((draw.rect_dest.bottom-draw.rect_dest.top)<=0||(draw.rect_dest.right-draw.rect_dest.left)<=0) return;
	memcpy(&draw.rect_dest_orig,&draw.rect_dest,sizeof(RECT));

	if (!draw.stretch) {
		aspect_source=(draw.win_width/(float)(draw.win_height));
		aspect_dest=((draw.rect_dest.right-draw.rect_dest.left)/(float)(draw.rect_dest.bottom-draw.rect_dest.top));
	}

	draw.ch=draw.cv=0;

	if (aspect_source<aspect_dest) {
		draw.ch=((draw.rect_dest.right-draw.rect_dest.left)-((float)(draw.rect_dest.bottom-draw.rect_dest.top)*aspect_source))/2.0;
		draw.rect_dest.left+=draw.ch;
		draw.rect_dest.right-=draw.ch;
	}
	else if (aspect_source>aspect_dest) {
		draw.cv=((draw.rect_dest.bottom-draw.rect_dest.top)-((float)(draw.rect_dest.right-draw.rect_dest.left)/aspect_source))/2.0;
		draw.rect_dest.top+=draw.cv;
		draw.rect_dest.bottom-=draw.cv;
	}

	if (vsync) (*drawdev_vsync)();
	(*drawdev_blit)();

	memcpy(&draw.rect_dest_orig_prev,&draw.rect_dest_orig,sizeof(RECT));
	memcpy(&draw.rect_dest_prev,&draw.rect_dest,sizeof(RECT));
}


/* text */
static void draw_text_out(void)
{
	/* can't LOG here */

	DWORD ticks=GetTickCount();
	draw_text* dt;

	int yd;
	int del=FALSE;
	int start=TRUE;

	WaitForSingleObject(draw.text_mutex,TEXT_MUTEX_TIME);

	dt=draw_text_begin;

	if (draw_text_cursor==NULL) { ReleaseMutex(draw.text_mutex); return; }
	if (!((*drawdev_text_begin)())) { ReleaseMutex(draw.text_mutex); return; }

	SetBkMode(draw.text_dc,TRANSPARENT);
	SelectObject(draw.text_dc,draw.font);

	draw.text_scroll-=3;
	if (draw.text_scroll<0) draw.text_scroll=0;

	for (yd=draw.text_scroll;;yd+=12) {

		if (dt->type&LT_IGNORE) yd-=12;
		else {
			int x=draw.clip.l+(draw_ntsc.is_used?4:1)+(d3d.d3d!=NULL); /* d3d texture 1 border pixel */
			int y=draw.clip.u+yd+(d3d.d3d!=NULL);
			int len=strlen(dt->text);

			/* SetTextColor(draw.text_dc,draw_text_colour[dt->colour]>>3&0x1f1f1f); */ /* darken */
			SetTextColor(draw.text_dc,(draw_text_colour[dt->colour]==0)?0x00ffffff:0);
			TextOut(draw.text_dc,x,y,dt->text,len);
			TextOut(draw.text_dc,x,y+1,dt->text,len);
			TextOut(draw.text_dc,x,y+2,dt->text,len);

			TextOut(draw.text_dc,x+1,y,dt->text,len);
			/* TextOut(draw.text_dc,x+1,y+1,dt->text,len); */
			TextOut(draw.text_dc,x+1,y+2,dt->text,len);

			TextOut(draw.text_dc,x+2,y,dt->text,len);
			TextOut(draw.text_dc,x+2,y+1,dt->text,len);
			TextOut(draw.text_dc,x+2,y+2,dt->text,len);

			SetTextColor(draw.text_dc,draw_text_colour[dt->colour]);
			TextOut(draw.text_dc,x+1,y+1,dt->text,len);
		}

		if (start) {
			start=FALSE;

			if ( (DWORD)(ticks - dt->ticks) >= (DWORD)(draw.text_max_ticks||dt->type&LT_IGNORE) ) {
				int res,divl=dt->div+3;
				del=(dt->type&LT_IGNORE)^LT_IGNORE;
				draw_text_begin=dt->next; MEM_CLEAN(dt);
				if ((dt=draw_text_begin)==NULL) { draw_text_cursor=NULL; del=TRUE; break; }

				res=draw.text_max_ticks-(draw.text_max_ticks/divl);
				if (res>(draw.text_max_ticks-50)) res=(draw.text_max_ticks-50);
				if ( (DWORD)(ticks-dt->ticks) >= (DWORD)res ) {
					dt->ticks=ticks-res;
					dt->div=divl;
				}

				continue;
			}
		}

		if (dt->next) dt=dt->next;
		else break;
	}

	if (del) draw.text_scroll=12;

	(*drawdev_text_end)();
	ReleaseMutex(draw.text_mutex);
}

static void draw_text_pop(int type)
{
	draw_text* dt=draw_text_begin;
	draw_text* dtprev=dt;

	if (type==0||(draw_text_cursor->type&0xfff)==type) return;

	for (;;) {
		if ((dt->type&0xfff)==type) {
			if (dt==draw_text_begin) draw_text_begin=dt->next;
			dtprev->next=dt->next;
			dt->next=NULL;
			draw_text_cursor->next=dt;
			break;
		}
		dtprev=dt;
		dt=dt->next;
		if (dt==NULL) break;
	}
}

void draw_text_add(int type,int colour,const char* text)
{
	/* can't LOG here */

	int len_src=strlen(text);
	int index;
	int offset=0;
	int cont;
	int copy;
	int len_dest;
	char buffer[STRING_SIZE];
	DWORD ticks=GetTickCount();
	int ignore=type&LT_IGNORE;
	type&=0xfff;
	memset(buffer,0,STRING_SIZE);

	if (len_src==0) return;

	if (draw.text_max_ticks==0) return;

	WaitForSingleObject(draw.text_mutex,TEXT_MUTEX_TIME);

	if (draw_text_cursor==NULL) {
		MEM_CREATE_T(draw_text_cursor,sizeof(draw_text),draw_text*);
		draw_text_begin=draw_text_cursor;
		draw_text_cursor->cont=TRUE;
		draw_text_cursor->type=type;
	}

	strcpy(buffer,text);

	for (;;) {
		cont=TRUE;
		for (index=offset;index<len_src;index++) {
			if (buffer[index]=='\n') { buffer[index++]=' '; cont=FALSE; break; }
		}

		draw_text_pop(type);

		if (type!=(draw_text_cursor->type&0xfff)||!draw_text_cursor->cont) {
			if (draw_text_cursor!=draw_text_cursor->next) { MEM_CLEAN(draw_text_cursor->next); }
			MEM_CREATE_T(draw_text_cursor->next,sizeof(draw_text),draw_text*);
			draw_text_cursor=draw_text_cursor->next;
		}

		len_dest=strlen(draw_text_cursor->text); copy=index-offset;
		if (type!=0&&type==(draw_text_cursor->type&0xfff)) len_dest=0;
		if ( (copy>0)&( (len_dest+copy) < ((int)sizeof(draw_text_cursor->text)-1) ) ) memcpy(&draw_text_cursor->text[len_dest],&buffer[offset],copy);
		draw_text_cursor->div=0;
		draw_text_cursor->colour=colour;
		draw_text_cursor->ticks=ticks;
		draw_text_cursor->cont=cont;
		draw_text_cursor->type=type|ignore;

		if (index==len_src) break;

		offset=index;
	}

	ReleaseMutex(draw.text_mutex);
}

void draw_set_text(int t)
{
	t=(t!=0);
	draw.text_max_ticks=t*TEXT_MAX_TICKS_DEFAULT;
	if (draw.text_max_ticks==0) draw_text_clean();
	main_menu_check(IDM_SHELLMSG,draw.text_max_ticks!=0);
}

void draw_text_clean(void)
{
	draw_text* dt=draw_text_begin;
	draw_text* dt2;

	if (dt==NULL) return;

	for (;;) {
		dt2=dt->next;
		MEM_CLEAN(dt);
		if (dt2==NULL) break;
		dt=dt2;
	}
	draw_text_begin=draw_text_cursor=NULL;
}


/* misc */
void draw_show_menu(int m)
{
	if (m&&!draw.menuvis) { SetMenu(MAIN->window,MAIN->menu); draw.menuvis^=1; }
	else if (!m&&draw.menuvis) { SetMenu(MAIN->window,NULL); draw.menuvis^=1; }
}

int draw_get_pixel(int x,int y)
{
	int pos=draw.source_width*y+x;
	if (pos>=(draw.source_width*draw.source_height)) return 0;

	else return draw_palette_32[draw.screen[pos]];
}


/* d3d */
#define TEXTURE_SIZE	1024

/* vertex type */
#define D3D_FVF			D3DFVF_XYZRHW|D3DFVF_TEX1
typedef struct {
	float x,y,z,rhw;
	float tu,tv;
} c_vertex;

#define NUM_POLYGONS	2
#define VB_SIZE			(NUM_POLYGONS*3*sizeof(c_vertex))

#define BORDER_RFSH_ALL	0xffff
#define BORDER_RFSH_D	1

#define SETSTAGE_ALL	0xffff
#define SETSTAGE_FILTER	1
#define SETSTAGE_RE		2

static void d3d_setstage(u32 stage)
{
	/* filtering */
	if (stage&SETSTAGE_FILTER) {
		DWORD filter=(draw.bfilter&((d3d.caps.TextureFilterCaps&(D3DPTFILTERCAPS_MINFLINEAR|D3DPTFILTERCAPS_MAGFLINEAR))!=0))?D3DTEXF_LINEAR:D3DTEXF_POINT;
		IDirect3DDevice9_SetSamplerState(d3d.device,0,D3DSAMP_MAGFILTER,filter);
		IDirect3DDevice9_SetSamplerState(d3d.device,0,D3DSAMP_MINFILTER,filter);
	}

	/* other */
	if (stage&SETSTAGE_RE) {
		DWORD clamp;

		if (d3d.caps.TextureAddressCaps&D3DPTADDRESSCAPS_CLAMP) clamp=D3DTADDRESS_CLAMP;
		else if (d3d.caps.TextureAddressCaps&D3DPTADDRESSCAPS_MIRROR) clamp=D3DTADDRESS_MIRROR;
		else clamp=D3DTADDRESS_WRAP;

		IDirect3DDevice9_SetSamplerState(d3d.device,0,D3DSAMP_ADDRESSU,clamp);
		IDirect3DDevice9_SetSamplerState(d3d.device,0,D3DSAMP_ADDRESSV,clamp);

		IDirect3DDevice9_SetTextureStageState(d3d.device,0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
		IDirect3DDevice9_SetTextureStageState(d3d.device,0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(d3d.device,0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);

		IDirect3DDevice9_SetTexture(d3d.device,0,(IDirect3DBaseTexture9*)d3d.texture);
	}
}

static int d3d_get_displaymode(void)
{
	return (IDirect3D9_GetAdapterDisplayMode(d3d.d3d,D3DADAPTER_DEFAULT,&d3d.displaymode)==D3D_OK);
}

// typedef IDirect3D9* (WINAPI* Direct3DCreate9_t)(UINT);  // <- FUNCIONA 64BITS!!!
// typedef INT_PTR (WINAPI* Direct3DCreate9_t) ();         // <- FUNCIONA 64BITS!!!
// typedef int (WINAPI* Direct3DCreate9_t)(UINT);          // <- FUNCIONA 32BITS!!!
typedef FARPROC Direct3DCreate9_t;

static int d3d_init(void)
{
	/*
		error 1: d3d9.dll load
		error 2: Direct3DCreate9 not in dll
		error 3: Direct3DCreate9
		error 4: IDirect3D9_GetAdapterDisplayMode
		error 5: IDirect3D9_GetDeviceCaps
		error 6: d3d.caps.PresentationIntervals D3DPRESENT_INTERVAL_IMMEDIATE|D3DPRESENT_INTERVAL_ONE
		error 7: IDirect3D9_CreateDevice
		error 8: IDirect3DDevice9_CreateVertexBuffer
		error 9: IDirect3DDevice9_CreateTexture
	*/
	int dynamic_err=0;

	/* dll/create */
	if ((d3d.dll=LoadLibrary( "d3d9.dll" ))==NULL) return 1;

    Direct3DCreate9_t pDirect3DCreate9 = (Direct3DCreate9_t)GetProcAddress(d3d.dll, "Direct3DCreate9");
    if (pDirect3DCreate9 == NULL) return 2;

	if ((d3d.d3d=(LPDIRECT3D9)(*pDirect3DCreate9)(D3D_SDK_VERSION))==NULL) return 3;

	if (!d3d_get_displaymode()) return 4;

	/* device */
	d3d.dynamic=draw.softrender^1;
	if ((IDirect3D9_GetDeviceCaps(d3d.d3d,D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,&d3d.caps))!=D3D_OK) return 5;

	if ((d3d.caps.PresentationIntervals&(D3DPRESENT_INTERVAL_IMMEDIATE|D3DPRESENT_INTERVAL_ONE))==0) return 6;
	if (!(d3d.caps.Caps2&D3DCAPS2_DYNAMICTEXTURES)) { dynamic_err|=d3d.dynamic; d3d.dynamic=0; }

	d3d.pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
	d3d.pp.hDeviceWindow=MAIN->window;
	d3d.pp.BackBufferCount=1;
	d3d.pp.Windowed=TRUE;
	d3d.pp.BackBufferWidth=GetSystemMetrics(SM_CXSCREEN);
	d3d.pp.BackBufferHeight=GetSystemMetrics(SM_CYSCREEN);
	d3d.pp.BackBufferFormat=d3d.displaymode.Format;
	d3d.pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
	d3d.pp.AutoDepthStencilFormat=D3DFMT_UNKNOWN;
	d3d.pp.MultiSampleType=D3DMULTISAMPLE_NONE;
	d3d.pp.FullScreen_RefreshRateInHz=D3DPRESENT_RATE_DEFAULT;
	if (draw.vsync) d3d.pp.Flags=D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;

	if ((IDirect3D9_CreateDevice(d3d.d3d,D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,MAIN->window,D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_MULTITHREADED,&d3d.pp,&d3d.device))!=D3D_OK) return 7;

	/* vertex buffer */
	if ((IDirect3DDevice9_CreateVertexBuffer(d3d.device,VB_SIZE,D3DUSAGE_WRITEONLY,D3D_FVF,D3DPOOL_MANAGED,&d3d.vb,NULL))!=D3D_OK) return 8;

	/* texture */
	if ((IDirect3DDevice9_CreateTexture(d3d.device,TEXTURE_SIZE,TEXTURE_SIZE,1,d3d.dynamic?D3DUSAGE_DYNAMIC:0,D3DFMT_X8R8G8B8,d3d.dynamic?D3DPOOL_DEFAULT:D3DPOOL_MANAGED,&d3d.texture,NULL))!=D3D_OK) return 9;
	if (dynamic_err) {
        draw.softrender=TRUE;
        LOG( LOG_MISC | LOG_WARNING,
            "D3D dynamic textures unsupported\n"
        );
    }
	d3d_flush_needed(); draw.reset_needed=TRUE;
	d3d_setstage(SETSTAGE_ALL);
	main_menu_enable(IDM_BFILTER,TRUE);	draw_set_bfilter(draw.bfilter);
	main_menu_enable(IDM_FLIP_X,TRUE);	draw_set_flip_x(draw.flip_x);

	if (draw.vsync&&QueryPerformanceFrequency(&d3d.qpf)) { d3d.qpc_enabled=TRUE; } /* only used for vsync */
	if (d3d_check_vsync_on()) {
		d3d.vsync_recalc_needed=TRUE;
		d3d.vsync_recalc_count=11;
	}

	draw.device_ready=TRUE;

	return 0;
}

static void d3d_clean(void)
{
	draw.semaphore=TRUE;

	if (d3d.device) IDirect3DDevice9_SetStreamSource(d3d.device,0,NULL,0,0);
	if (d3d.texture) { IDirect3DTexture9_Release(d3d.texture); d3d.texture=NULL; }
	if (d3d.vb) { IDirect3DVertexBuffer9_Release(d3d.vb); d3d.vb=NULL; }
	if (d3d.device) { IDirect3DDevice9_Release(d3d.device); d3d.device=NULL; }
	if (d3d.d3d) { IDirect3D9_Release(d3d.d3d); d3d.d3d=NULL; }
	if (d3d.dll) { FreeLibrary(d3d.dll); d3d.dll=NULL; }

	draw.device_ready=draw.semaphore=FALSE;
}

static void d3d_reset(void)
{
	HRESULT result;
	D3DPRESENT_PARAMETERS pp;
	int width,height;

	memset(&pp,0,sizeof(D3DPRESENT_PARAMETERS));

	result=IDirect3DDevice9_TestCooperativeLevel(d3d.device);
	if (result!=D3DERR_DEVICENOTRESET&&result!=D3D_OK) { draw.reset_needed=TRUE; return; }

	width=draw.rect_dest_orig.right-draw.rect_dest_orig.left; if (width<(draw.surface_width-(draw.clip.l+draw.clip.r))) width=draw.surface_width-(draw.clip.l+draw.clip.r);
	height=draw.rect_dest_orig.bottom-draw.rect_dest_orig.top; if (height<(draw.surface_height-(draw.clip.u+draw.clip.d))) height=draw.surface_height-(draw.clip.u+draw.clip.d);

	d3d_get_displaymode();
	memcpy(&pp,&d3d.pp,sizeof(D3DPRESENT_PARAMETERS));
	pp.BackBufferFormat=d3d.displaymode.Format;
	pp.BackBufferWidth=width;
	pp.BackBufferHeight=height;

	if (d3d.dynamic&&d3d.texture) { IDirect3DTexture9_Release(d3d.texture); d3d.texture=NULL; }

	result=IDirect3DDevice9_Reset(d3d.device,&pp);

	if (result==D3D_OK) {
		/* re-init resources */
		if (d3d.dynamic) result=IDirect3DDevice9_CreateTexture(d3d.device,TEXTURE_SIZE,TEXTURE_SIZE,1,D3DUSAGE_DYNAMIC,D3DFMT_X8R8G8B8,D3DPOOL_DEFAULT,&d3d.texture,NULL);

		d3d_setstage(SETSTAGE_ALL);

		d3d.pp.BackBufferFormat=d3d.displaymode.Format;
		d3d.pp.BackBufferWidth=width;
		d3d.pp.BackBufferHeight=height;

		d3d.vb_recalc_needed=TRUE;

		if (d3d.dynamic) {
			d3d_flush_needed();
			d3d_render();
		}
	}
	else if (result==D3DERR_DEVICELOST) {
		/* retry next time */
		draw.reset_needed=TRUE;
		return;
	}

	if (result!=D3D_OK) {

		LOG(
            LOG_MISC | LOG_ERROR,
        #ifdef MEISE_ESP
            "¡No se puede restablecer el dispositivo Direct3D!\n"
        #else
            "Can't reset Direct3D device!\n"
        #endif
        );
		draw_clean(); exit(1);
	}
}

static int d3d_recreate_screen(void)
{
	d3d.vb_recalc_needed=TRUE;
	draw_recreate_font();
	d3d_flush_needed();

	return 0;
}

static void d3d_flush_needed(void)
{
	d3d.flush_needed=TRUE;
}

static void d3d_render(void)
{
	int bc=draw.overlay?draw.overlay:vdp_get_bgc();
	u8* screen=draw.screen;
	D3DLOCKED_RECT lr;

	if (d3d.texture==NULL) return;

	/* lock */
	if (IDirect3DTexture9_LockRect(d3d.texture,0,&lr,NULL,D3DLOCK_DISCARD)!=D3D_OK) {
		IDirect3DTexture9_UnlockRect(d3d.texture,0);
		draw.reset_needed=TRUE;
		return;
	}

	/* flush */
	if (d3d.flush_needed) {
		d3d.flush_needed=FALSE;
		memset(lr.pBits,0,TEXTURE_SIZE*lr.Pitch);
		d3d.border_refresh_needed=BORDER_RFSH_ALL;
	}

	/* render */
	if (draw_ntsc.is_used) {
		/* NTSC */
		ti_ntsc_blit_32(draw_ntsc.ntsc,&screen[draw.vis.l+draw.vis.u*draw.source_width],draw.source_width,draw.source_width-(draw.vis.l+draw.vis.r),draw.source_height-(draw.vis.u+draw.vis.d),(void*)((u8*)lr.pBits+sizeof(u32)+lr.Pitch),lr.Pitch,bc);
	}
	else {
		/* standard (is faster) */
		int i,j,k=0;
		u8* s; u32* o;
		int max=(draw.surface_height*lr.Pitch)+sizeof(u32)+lr.Pitch;
		for (i=sizeof(u32)+lr.Pitch;i<max;i+=lr.Pitch,k++) {
			o=(u32*)&((u8*)(lr.pBits))[i];
			s=&screen[draw.vis.l+(k+draw.vis.u)*draw.source_width];
			j=draw.source_width-(draw.vis.l+draw.vis.r);
			while (j--) *o++=draw_palette_32[(*s++)];
		}
	}

	/* border */
	if (bc!=draw.bc_prev||draw.repaint) {
		d3d.border_refresh_needed=BORDER_RFSH_ALL;
		draw.repaint=FALSE;
	}

	if (d3d.border_refresh_needed) {
		u32* o; int i,max;
		bc=PAL_NTSC32(bc);

		/* down */
		if (d3d.border_refresh_needed&BORDER_RFSH_D) {
			i=2+draw.surface_width;
			o=(u32*)&((u8*)(lr.pBits))[draw.surface_height*lr.Pitch+lr.Pitch];
			while (i--) *o++=bc;
		}

		/* rest */
		if (d3d.border_refresh_needed==BORDER_RFSH_ALL) {
			/* up */
			i=2+draw.surface_width;
			o=(u32*)lr.pBits; while (i--) *o++=bc;

			/* left/right */
			if (!draw_ntsc.is_used) {
				max=draw.surface_height*lr.Pitch+lr.Pitch;
				for (i=lr.Pitch;i<max;i+=lr.Pitch) {
					o=(u32*)&((u8*)(lr.pBits))[i];
					o[0]=o[1+draw.surface_width]=bc;
				}
			}
		}

		d3d.border_refresh_needed=0;
	}
	draw.bc_prev=bc;

	/* unlock */
	IDirect3DTexture9_UnlockRect(d3d.texture,0);
}

static int d3d_text_begin(void)
{
	int ret=TRUE;

	if (d3d.texture==NULL) return FALSE;

	if ((IDirect3DTexture9_GetSurfaceLevel(d3d.texture,0,&d3d.surface))!=D3D_OK) ret=FALSE;
	else if ((IDirect3DSurface9_GetDC(d3d.surface,&draw.text_dc))!=D3D_OK) ret=FALSE;

	if (!ret) {
		if (draw.text_dc) { IDirect3DSurface9_ReleaseDC(d3d.surface,draw.text_dc); draw.text_dc=NULL; }
		if (d3d.surface) { IUnknown_Release(d3d.surface); d3d.surface=NULL; }
	}

	return ret;
}

static void d3d_text_end(void)
{
	if (draw.text_dc) { IDirect3DSurface9_ReleaseDC(d3d.surface,draw.text_dc); draw.text_dc=NULL; }
	if (d3d.surface) { IUnknown_Release(d3d.surface); d3d.surface=NULL; }
	d3d.border_refresh_needed=BORDER_RFSH_D;
}

static void d3d_vsync(void)
{
	if (msx_is_running()&&((draw.frameskip_set>0)|((draw.frameskip_set<1)&(sound_get_lostframes()==0)))&&draw.vsync&&(d3d.caps.Caps&D3DCAPS_READ_SCANLINE)&&MAIN->foreground) {

		#define DANGER_ZONE 96

		HRESULT result;
		POINT p={0,0};
		UINT sc,sc_old=0;
		int sc_hit,sc_hitc,height=d3d.displaymode.Height,fc=0;
		int rr=draw_get_refreshrate();
		float rc=0;
		D3DRASTER_STATUS status;
		memset(&status,0,sizeof(D3DRASTER_STATUS));
		if ((IDirect3DDevice9_GetRasterStatus(d3d.device,0,&status))==D3D_OK) sc_old=status.ScanLine;

		if (d3d.qpf.QuadPart==0) return; /* shouldn't happen, divide by 0 */

		if (rr==0) {
			rr=10000;
			rc=0.05;
		}
		else rc=rr;

		if (height<=0) height=1024;
		ClientToScreen(MAIN->window,&p);
		sc_hit=draw.rect_dest.bottom+p.y; if (sc_hit>height) sc_hit=height; if (((height-sc_hit)%height)<DANGER_ZONE&&p.y<DANGER_ZONE) sc_hit-=DANGER_ZONE;
		sc_hit=(int)((height+sc_hit)-((d3d.vsync_qpc/(float)(d3d.qpf.QuadPart/rc))*height))%height; if (sc_hit<=0) sc_hit=height-1;

		for (;;) {
			result=IDirect3DDevice9_GetRasterStatus(d3d.device,0,&status); sc=status.ScanLine; sc_hitc=sc_hit;
			if (sc<sc_old) { sc+=height; sc_hitc+=height; fc++; }
			if (result!=D3D_OK||fc>2) break; /* shouldn't normally happen */
			if (sc_old<(UINT)sc_hitc&&sc>=(UINT)sc_hitc) break; /* ok */
			if ((((sc_hitc-sc)/(float)height)*(1000.0/rr))>sound_get_sleeptime_raw()) sound_sleep(1); /* auto or yes: same */
			sc_old=sc%height;
		}
	}
}

static void d3d_lock_backbuffer(void)
{
	IDirect3DSurface9* bb=NULL;
	D3DLOCKED_RECT lr;

	if (IDirect3DDevice9_GetBackBuffer(d3d.device,0,0,D3DBACKBUFFER_TYPE_MONO,&bb)==D3D_OK) {
		IDirect3DSurface9_LockRect(bb,&lr,NULL,0);
		IDirect3DSurface9_UnlockRect(bb);
	}

	if (bb) IUnknown_Release(bb);
}

static void d3d_blit(void)
{
	/* weird: sometimes blits a wrong section of videomemory, and then only on a hint popup (eg. "Close"
	when hovering the mouse cursor over the top right window X button) */
	int bc=draw.overlay?draw.overlay:vdp_get_bgc();
	int rectchanged=(memcmp(&draw.rect_dest,&draw.rect_dest_prev,sizeof(RECT))!=0);
	LARGE_INTEGER qpc1,qpc2;
	int v_next=FALSE;

	d3d.vb_recalc_needed|=(draw.flip_x^draw.flip_x_prev); /* flip x changed */
	draw.flip_x_prev=draw.flip_x;

	if (draw.bfilter^draw.bfilter_prev) {
		/* bfilter changed */
		draw.bfilter_prev=draw.bfilter;
		d3d_setstage(SETSTAGE_FILTER);
	}

	if (d3d.vsync_recalc_needed) {
		QueryPerformanceFrequency(&d3d.qpf); /* may change on eg. a notebook changing its CPU speed */
		d3d_lock_backbuffer(); /* just to be sure it wasn't in use */
		QueryPerformanceCounter(&qpc1);
	}

	if (rectchanged|d3d.vb_recalc_needed) {
		void* vb;
		float ch=(float)d3d.pp.BackBufferWidth/draw.rect_dest_orig.right;
		float cv=(float)d3d.pp.BackBufferHeight/draw.rect_dest_orig.bottom;
		c_vertex v[6];

		/* quad x/y */
		v[0].x=v[1].x=v[3].x=ch*draw.rect_dest.left-0.5;
		v[2].x=v[4].x=v[5].x=ch*draw.rect_dest.right-0.5;
		v[0].y=v[3].y=v[5].y=cv*draw.rect_dest.bottom-0.5;
		v[1].y=v[2].y=v[4].y=cv*draw.rect_dest.top-0.5;

		/* texture u/v */
		v[0].tu=v[1].tu=v[3].tu=(float)(1+draw.clip.l)/TEXTURE_SIZE;
		v[2].tu=v[4].tu=v[5].tu=(float)(1+draw.surface_width-draw.clip.r)/TEXTURE_SIZE;
		v[0].tv=v[3].tv=v[5].tv=(float)(1+draw.surface_height-draw.clip.d)/TEXTURE_SIZE;
		v[1].tv=v[2].tv=v[4].tv=(float)(1+draw.clip.u)/TEXTURE_SIZE;

		if (draw.flip_x) { float f=v[0].tu; v[0].tu=v[1].tu=v[3].tu=v[2].tu; v[2].tu=v[4].tu=v[5].tu=f; }

		/* quad z/rhw unused */
		v[0].z=v[1].z=v[2].z=v[3].z=v[4].z=v[5].z=1.0;
		v[0].rhw=v[1].rhw=v[2].rhw=v[3].rhw=v[4].rhw=v[5].rhw=1.0;

		if (IDirect3DVertexBuffer9_Lock(d3d.vb,0,0,&vb,0)==D3D_OK) {
			memcpy(vb,v,VB_SIZE);
			IDirect3DVertexBuffer9_Unlock(d3d.vb);
		}
		else {
			LOG(
                LOG_MISC|LOG_ERROR,
            #ifdef MEISEI_ESP
                "¡No se puede bloquear el búfer de vértices!\n"
            #else
                "Can't lock vertex buffer!\n"
            #endif // MEISEI_ESP
            );
			draw_clean(); exit(1);
		}

		draw.reset_needed=rectchanged;
		d3d.vb_recalc_needed=FALSE;

		if (d3d_check_vsync_on()) {
			d3d.vsync_recalc_count=11;
			v_next=TRUE;
		}
	}

	/* clear backbuffer */
	IDirect3DDevice9_Clear( d3d.device, 0, NULL, D3DCLEAR_TARGET, PAL_NTSC32(bc) | 0xff000000, 1.0f, 0 );

	/* render */
	if (IDirect3DDevice9_BeginScene(d3d.device)==D3D_OK) {
		IDirect3DDevice9_SetFVF(d3d.device,D3D_FVF);
		IDirect3DDevice9_SetStreamSource(d3d.device,0,d3d.vb,0,sizeof(c_vertex));
		IDirect3DDevice9_DrawPrimitive(d3d.device,D3DPT_TRIANGLELIST,0,2);

		IDirect3DDevice9_EndScene(d3d.device);
	}

	if (d3d.vsync_recalc_needed) d3d_lock_backbuffer();

	/* present */
	if (IDirect3DDevice9_Present(d3d.device,NULL,NULL,NULL,NULL)==D3DERR_DEVICELOST) draw.reset_needed=TRUE;

	if (d3d.vsync_recalc_needed) {
		QueryPerformanceCounter(&qpc2);
		if (qpc2.QuadPart>qpc1.QuadPart) {
			LONGLONG result=qpc2.QuadPart-qpc1.QuadPart;

			/* check if result is consistent */
			if (qpc1.QuadPart!=0&&qpc2.QuadPart!=0) {
				float ratio=0;

				if (result<d3d.vsync_qpc) ratio=(float)result/d3d.vsync_qpc;
				else ratio=(float)d3d.vsync_qpc/result;

				if (ratio>0.9) d3d.vsync_recalc_count=0;
			}

			d3d.vsync_qpc=result;
		}
	}

	d3d.vsync_recalc_needed=v_next|(d3d.vsync_recalc_count&1);
	if (d3d.vsync_recalc_count) d3d.vsync_recalc_count--;
}

static int d3d_get_refreshrate(void)
{
	return d3d.displaymode.RefreshRate;
}

static int d3d_check_vsync_on(void)
{
	return ((d3d.caps.Caps&D3DCAPS_READ_SCANLINE)!=0)&draw.vsync&d3d.qpc_enabled;
}


/* ddraw */
// #define DD_CHECK_RESULT(x) if ((result=x)==DDERR_SURFACELOST) { if ((result=IDirectDrawSurface_Restore(ddraw.surface_primary))==DDERR_WRONGMODE||(result=IDirectDrawSurface_Restore(ddraw.surface_back))==DDERR_WRONGMODE) { draw.reset_needed=TRUE; } result=x; }
#define DD_CHECK_RESULT( x )                                                                        \
    if ( ( result = x ) == DDERR_SURFACELOST ) {                                                    \
        if ( (result = IDirectDrawSurface_Restore(ddraw.surface_primary)) == DDERR_WRONGMODE ||     \
             (result = IDirectDrawSurface_Restore(ddraw.surface_back))    == DDERR_WRONGMODE )      \
        {                                                                                           \
            draw.reset_needed = TRUE;                                                               \
        }                                                                                           \
        result = x;                                                                                 \
    }


static int ddraw_lock(LPDIRECTDRAWSURFACE* surface,DDSURFACEDESC* desc)
{
	HRESULT result;

	DD_CHECK_RESULT(IDirectDrawSurface_Lock(
		*surface,		/* surface to lock */
		NULL,			/* rect: NULL=whole surface */
		desc,			/* surface description */
		DDLOCK_WAIT|DDLOCK_SURFACEMEMORYPTR|DDLOCK_WRITEONLY|DDLOCK_NOSYSLOCK, /* flags */
		NULL			/* event: unused */
	))
	if (result!=DD_OK) {
        LOG(
            LOG_MISC|LOG_WARNING|LOG_TYPE(LT_DDRAWLOCKFAIL),
        #ifdef MEISEI_ESP
            "Error en el bloqueo de DirectDraw"
        #else
            "DirectDraw lock failed"
        #endif // MEISEI_ESP

        );
        return FALSE;
    }
	else return TRUE;
}

static void ddraw_unlock(LPDIRECTDRAWSURFACE* surface)
{
	IDirectDrawSurface_Unlock(*surface,NULL);
}

static void ddraw_flush_backsurface(void)
{
	DDBLTFX fx;
	memset(&fx,0,sizeof(DDBLTFX));
	fx.dwSize=sizeof(DDBLTFX);

	fx.dwFillColor=0;
	IDirectDrawSurface_Blt(ddraw.surface_back,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fx);
}

static void ddraw_flush(void)
{
	int bc=draw.overlay?draw.overlay:vdp_get_bgc();
	DDBLTFX fx;
	memset(&fx,0,sizeof(DDBLTFX));
	fx.dwSize=sizeof(DDBLTFX);

	if (draw.semaphore) return;

	draw.semaphore=TRUE;

	ddraw_flush_backsurface();

	fx.dwFillColor=ddraw.desc_back.ddpfPixelFormat.dwRGBBitCount==16?PAL_NTSC16(bc):PAL_NTSC32(bc);
	IDirectDrawSurface_Blt(ddraw.surface_primary,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fx);

	draw.semaphore=FALSE;
}

#define DDS_PRIMARY		0
#define DDS_BACK		1
#define DDS_TRANSPARENT	2

#define CK_TRANSPARENT	1

static HRESULT ddraw_create_surface(LPDIRECTDRAWSURFACE* surface,DDSURFACEDESC* desc,int flags,int width,int height)
{
	HRESULT result;

	(*desc).dwSize=sizeof(DDSURFACEDESC);
	(*desc).dwFlags=DDSD_CAPS;

	/* back surface */
	if (flags&DDS_BACK) {
		(*desc).dwFlags|=(DDSD_HEIGHT|DDSD_WIDTH);
		if (flags&DDS_TRANSPARENT) (*desc).dwFlags|=DDSD_CKSRCBLT;
		(*desc).ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|(((draw.softrender==FALSE)&&(ddraw.caps.dwCaps&DDCAPS_BLTSTRETCH))?DDSCAPS_VIDEOMEMORY:DDSCAPS_SYSTEMMEMORY);
		(*desc).dwWidth=width; (*desc).dwHeight=height;
	}
	/* primary surface */
	else {
		(*desc).ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE|DDSCAPS_VIDEOMEMORY;
	}

	/* create it */
	if ((result=IDirectDraw_CreateSurface(ddraw.ddraw,desc,surface,NULL))!=DD_OK) {
		/* try system memory instead */
		draw.softrender=TRUE;
		LOG(
            LOG_MISC|LOG_WARNING,
        #ifdef MEISEI_ESP
            "DirectDraw no pudo iniciar el procesamiento HW\n"
        #else
            "DirectDraw failed to init HW rendering\n"
        #endif // MEISEI_ESP
        );
		(*desc).ddsCaps.dwCaps=((*desc).ddsCaps.dwCaps&~DDSCAPS_VIDEOMEMORY)|DDSCAPS_SYSTEMMEMORY;
		result=IDirectDraw_CreateSurface(ddraw.ddraw,desc,surface,NULL);
	}

	if (flags&DDS_TRANSPARENT) {
		DDCOLORKEY ck;
		ck.dwColorSpaceLowValue=ck.dwColorSpaceHighValue=CK_TRANSPARENT;
		IDirectDrawSurface_SetColorKey(*surface,DDCKEY_SRCBLT,&ck);
	}

	if ((result==DD_OK)&&((IDirectDrawSurface_GetSurfaceDesc(*surface,desc))!=DD_OK)) {
		LOG(
            LOG_MISC | LOG_ERROR,
        #ifdef MEISEI_ESP
            "¡No se puede obtener la descripción de la superficie de DirectDraw!\n"
        #else
            "Can't get DirectDraw surface description!\n"
        #endif // MEISEI_ESP
        );
		draw_clean();
		exit(1);
	}

	return result;
}

// #define DDI_TRY(f,text) if ((f)!=DD_OK) { if (draw.resetting) { ddraw_clean(); draw.reset_needed=TRUE; return 1; } else { LOG(LOG_MISC|LOG_ERROR,text); draw_clean(); exit(1); } }
#define DDI_TRY( f, text )                      \
    if ( (f) != DD_OK ) {                       \
        if (draw.resetting) {                   \
            ddraw_clean();                      \
            draw.reset_needed = TRUE;           \
            return 1;                           \
        } else {                                \
            LOG( LOG_MISC | LOG_ERROR, text );  \
            draw_clean();                       \
            exit(1);                            \
        }                                       \
    }

static int ddraw_recreate_screen(void)
{
	if (ddraw.surface_back) {
        ddraw_unlock(&ddraw.surface_back);
        IDirectDrawSurface_Release(ddraw.surface_back);
        ddraw.surface_back=NULL;
    } /* free back surface */

	DDI_TRY(
         ddraw_create_surface( &ddraw.surface_back, &ddraw.desc_back, DDS_BACK, draw.surface_width, draw.surface_height ),
    #ifdef MEISEI_ESP
        "¡No se puede crear la superficie posterior de DirectDraw!\n"
    #else
         "Can't create DirectDraw back surface!\n"
    #endif // MEISEI_ESP
    )
	draw_recreate_font();
	draw_set_repaint(2);

	ddraw_flush_backsurface();

	return 0;
}

static int ddraw_init(void)
{
	draw.semaphore=TRUE;

	draw.device_ready=FALSE;

	/* d3d filtering, texture flip unsupported */
	main_menu_enable(IDM_BFILTER,FALSE);    main_menu_check(IDM_BFILTER,FALSE);
	main_menu_enable(IDM_FLIP_X,FALSE);	    main_menu_check(IDM_FLIP_X,FALSE);

	DDI_TRY(
        DirectDrawCreate(NULL,&ddraw.ddraw,NULL),
    #ifdef MEISEI_ESP
        "¡No se puede crear DirectDraw!\n"
    #else
        "Can't create DirectDraw!\n"
    #endif // MEISEI_ESP
    )
	ddraw.caps.dwSize=sizeof(DDCAPS);
	DDI_TRY(
        IDirectDraw_GetCaps(ddraw.ddraw,&ddraw.caps,NULL),
    #ifdef MEISEI_ESP
        "¡No puedo obtener capacidades de DirectDraw!\n"
    #else
        "Can't get DirectDraw capabilities!\n"
    #endif // MEISEI_ESP
    )
	DDI_TRY(
        IDirectDraw_SetCooperativeLevel(ddraw.ddraw,MAIN->window,DDSCL_NORMAL),
    #ifdef MEISEI_ESP
        "¡No se puede establecer el nivel cooperativo de DirectDraw!\n"
    #else
         "Can't set DirectDraw cooperative level!\n"
    #endif // MEISEI_ESP
    )
	DDI_TRY(
        ddraw_create_surface(&ddraw.surface_primary,&ddraw.desc_primary,DDS_PRIMARY,0,0),
    #ifdef MEISEI_ESP
        "¡No se puede crear la superficie principal de DirectDraw!\n"
    #else
        "Can't create DirectDraw primary surface!\n"
    #endif // MEISEI_ESP
    )
	DDI_TRY(
        IDirectDraw_CreateClipper(ddraw.ddraw,0,&ddraw.clipper,NULL),
    #ifdef MEISEI_ESP
        "¡No se puede crear el clipper de DirectDraw!\n"
    #else
        "Can't create DirectDraw clipper!\n"
    #endif // MEISEI_ESP
    )
	DDI_TRY(
        IDirectDrawClipper_SetHWnd(ddraw.clipper,0,MAIN->window),
    #ifdef MEISEI_ESP
        "¡No se puede configurar el clipper de DirectDraw en ventana!\n"
    #else
        "Can't set DirectDraw clipper to window!\n"
    #endif
    )
	DDI_TRY(
        IDirectDrawSurface_SetClipper(ddraw.surface_primary,ddraw.clipper),
    #ifdef MEISEI_ESP
        "¡No se puede conectar el clipper DirectDraw a la superficie principal!\n"
    #else
        "Can't attach DirectDraw clipper to primary surface!\n"
    #endif // MEISEI_ESP
    )
	DDI_TRY(
        ddraw_create_surface(&ddraw.surface_back,&ddraw.desc_back,DDS_BACK,draw.surface_width,draw.surface_height),
    #ifdef MEISEI_ESP
        "¡No se puede crear la superficie posterior de DirectDraw!\n"
    #else
        "Can't create DirectDraw back surface!\n"
    #endif // MEISEI_ESP
    )

	ddraw_flush_backsurface();

	draw.device_ready=TRUE;
	draw.semaphore=FALSE;

	return 0;
}

static void ddraw_clean(void)
{
	draw.semaphore=TRUE;

	if (ddraw.clipper) { IDirectDrawClipper_Release(ddraw.clipper); ddraw.clipper=NULL; } /* free clipper */
	if (ddraw.surface_back) { ddraw_unlock(&ddraw.surface_back); IDirectDrawSurface_Release(ddraw.surface_back); ddraw.surface_back=NULL; } /* free back surface */
	if (ddraw.surface_primary) { IDirectDrawSurface_Release(ddraw.surface_primary); ddraw.surface_primary=NULL; } /* free primary surface */
	if (ddraw.ddraw) { IDirectDraw_Release(ddraw.ddraw); ddraw.ddraw=NULL; } /* free directdraw */

	draw.device_ready=draw.semaphore=FALSE;
}

static void ddraw_reset(void)
{
	ddraw_clean();
	ddraw_init();
}

static void ddraw_render(void)
{
	u8* screen=draw.screen;

	/* lock */
	if (!ddraw_lock(&ddraw.surface_back,&ddraw.desc_back)) { draw.semaphore=FALSE; return; }

	/* render */
	/* ntsc */
	#define RENDER_NTSC(x,y)												\
	ti_ntsc_blit_##x(														\
		draw_ntsc.ntsc,														\
		&screen[draw.vis.l+draw.vis.u*draw.source_width],					\
		draw.source_width,													\
		draw.source_width-(draw.vis.l+draw.vis.r),							\
		draw.source_height-(draw.vis.u+draw.vis.d),							\
		ddraw.desc_back.lpSurface,											\
		ddraw.desc_back.lPitch,							                    \
		draw.overlay?draw.overlay:vdp_get_bgc()								\
	)

	/* standard (is faster) */
	#define RENDER_STD(x,y)													\
	int i,j,k=0;															\
	u8* s; y* o;															\
	int max=draw.surface_height*ddraw.desc_back.lPitch;	                    \
	for (i=0;i<max;i+=ddraw.desc_back.lPitch,k++) {		                    \
		o=(y*)&((u8*)(ddraw.desc_back.lpSurface))[i];						\
		s=&screen[draw.vis.l+(k+draw.vis.u)*draw.source_width];				\
		j=draw.source_width-(draw.vis.l+draw.vis.r);						\
		while (j--) *o++=draw_palette_##x[(*s++)];							\
	}

	switch (ddraw.desc_back.ddpfPixelFormat.dwRGBBitCount) {
		case 32:
			if (draw_ntsc.is_used) { RENDER_NTSC(32,int); }
			else { RENDER_STD(32,int) }
			break;
		case 16:
			if (draw_ntsc.is_used) { RENDER_NTSC(16,u16); }
			else { RENDER_STD(16,u16) }
			break;
		default:
			ddraw_unlock(&ddraw.surface_back); draw.semaphore=FALSE; /* prevent it from crashing... while it's crashing */
			LOG(
                LOG_MISC|LOG_ERROR,
            #ifdef MEISEI_ESP
                "¡La profundidad de %d bits no es compatible!\ncambie a 16 o 32\n",
            #else
                "%d-bit depth is unsupported!\nswitch to 16 or 32\n",
            #endif // MEISEI_ESP
                ddraw.desc_back.ddpfPixelFormat.dwRGBBitCount
            );
            exit(1);
        break;
	}

	/* unlock */
	ddraw_unlock(&ddraw.surface_back);
}

static int ddraw_text_begin(void)
{
	HRESULT result;

	DD_CHECK_RESULT(IDirectDrawSurface_GetDC(ddraw.surface_back,&draw.text_dc))
	return result==DD_OK;
}

static void ddraw_text_end(void)
{
	if (draw.text_dc) { IDirectDrawSurface_ReleaseDC(ddraw.surface_back,draw.text_dc); draw.text_dc=NULL; }
}

static void ddraw_vsync(void)
{
	if (msx_is_running()&&((draw.frameskip_set>0)|((draw.frameskip_set<1)&(sound_get_lostframes()==0)))&&draw.vsync&&MAIN->foreground) {
		if (ddraw.caps.dwCaps&DDCAPS_READSCANLINE) {
			HRESULT result;
			DWORD sc,sc_old=0;
			int f,height=GetSystemMetrics(SM_CYSCREEN);
			height-=((height/1024.0)*64); /* small 'delay' to prevent tearing at top of screen */
			f=draw_get_refreshrate(); if (f==0) f=10000;

			for (;;) {
				result=IDirectDraw_GetScanLine(ddraw.ddraw,&sc);
				if (result!=DD_OK||sc>=(DWORD)height||(sc_old>(DWORD)(height-(height>>2))&&sc<(DWORD)(height>>2))) break;
				if ((((height-sc)/(float)height)*(1000.0/f))>sound_get_sleeptime_raw()) sound_sleep(1);
				sc_old=sc;
			}
		}

		else {
			/* WaitForVerticalBlank seems to miss sometimes, also unable to sleep */
			BOOL vblank;
			IDirectDraw_GetVerticalBlankStatus(ddraw.ddraw,&vblank);
			if ( (u32)(!vblank&&sound_get_sleeptime_raw()) == (u32)~0 ) { IDirectDraw_WaitForVerticalBlank(ddraw.ddraw,DDWAITVB_BLOCKBEGIN,0); }
		}
	}
}

static void ddraw_blit(void)
{
	POINT p={0,0};
	RECT rect_source={0,0,0,0};
	HRESULT result;
	int bc=draw.overlay?draw.overlay:vdp_get_bgc();

	DDBLTFX fx;
	memset(&fx,0,sizeof(DDBLTFX));
	fx.dwSize=sizeof(DDBLTFX);
	fx.dwFillColor=ddraw.desc_back.ddpfPixelFormat.dwRGBBitCount==16?PAL_NTSC16(bc):PAL_NTSC32(bc);

	ClientToScreen(MAIN->window,&p);
	OffsetRect(&draw.rect_dest,p.x,p.y);
	OffsetRect(&draw.rect_dest_orig,p.x,p.y);

	SetRect(&rect_source,draw.clip.l,draw.clip.u,draw.surface_width-draw.clip.r,draw.surface_height-draw.clip.d);

	/* blit */
	#define DD_BLT(ss,sr,fl,ef)									\
		DD_CHECK_RESULT(IDirectDrawSurface_Blt(					\
			ddraw.surface_primary,	/* destination surface */	\
			&draw.rect_dest,		/* destination rectangle */	\
			ss,						/* source surface */		\
			sr,						/* source rectangle */		\
			DDBLT_ASYNC|fl,			/* flags */					\
			ef						/* effects */				\
		))														\
		if (result!=DD_OK) { DD_CHECK_RESULT(IDirectDrawSurface_Blt(ddraw.surface_primary,&draw.rect_dest,ss,sr,DDBLT_WAIT|fl,ef)) }

	DD_BLT(ddraw.surface_back,&rect_source,0,NULL)

	/* clean borders */
	draw.repaint|=(draw.bc!=bc); draw.bc=bc;

	if (draw.repaint||memcmp(&draw.rect_dest_orig_prev,&draw.rect_dest_orig,sizeof(RECT))!=0) {
		if (draw.cv>0) {
			memcpy(&draw.rect_dest,&draw.rect_dest_orig,sizeof(RECT)); draw.rect_dest.bottom=draw.rect_dest.top+draw.cv; DD_BLT(NULL,NULL,DDBLT_COLORFILL,&fx)
			memcpy(&draw.rect_dest,&draw.rect_dest_orig,sizeof(RECT)); draw.rect_dest.top=draw.rect_dest.bottom-draw.cv; DD_BLT(NULL,NULL,DDBLT_COLORFILL,&fx)
		}
		else if (draw.ch>0) {
			memcpy(&draw.rect_dest,&draw.rect_dest_orig,sizeof(RECT)); draw.rect_dest.right=draw.rect_dest.left+draw.ch; DD_BLT(NULL,NULL,DDBLT_COLORFILL,&fx)
			memcpy(&draw.rect_dest,&draw.rect_dest_orig,sizeof(RECT)); draw.rect_dest.left=draw.rect_dest.right-draw.ch; DD_BLT(NULL,NULL,DDBLT_COLORFILL,&fx)
		}
	}

	draw.repaint-=(draw.repaint!=0);
}

static int ddraw_get_refreshrate(void)
{
	DWORD f=0;

	if (!draw.device_ready) return 0;

	if (IDirectDraw_GetMonitorFrequency(ddraw.ddraw,&f)!=DD_OK) f=0;
	return f;
}
