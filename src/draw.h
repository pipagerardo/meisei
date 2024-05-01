/******************************************************************************
 *                                                                            *
 *                              "draw.h"                                      *
 *                                                                            *
 * DirectDraw y D3D9, el encargado de dibujar cosas en pantalla.              *
 ******************************************************************************/

#ifndef DRAW_H
#define DRAW_H

enum {
	DRAW_RENDERTYPE_D3D=0,
	DRAW_RENDERTYPE_DD,

	DRAW_RENDERTYPE_MAX
};

enum {
	DRAW_VIDFORMAT_NTSC=0,
	DRAW_VIDFORMAT_PAL,

	DRAW_VIDFORMAT_MAX
};

enum {
	DRAW_VIDSIGNAL_RGB=0,
	DRAW_VIDSIGNAL_MONOCHROME,
	DRAW_VIDSIGNAL_COMPOSITE,
	DRAW_VIDSIGNAL_SVIDEO,

	DRAW_VIDSIGNAL_CUSTOM,

	DRAW_VIDSIGNAL_MAX
};

enum {
	DRAW_DECODER_INDUSTRIAL=0,
	DRAW_DECODER_SONY_CXA2025AS_U,
	DRAW_DECODER_SONY_CXA2025AS_J,
	DRAW_DECODER_SONY_CXA2095S_U,
	DRAW_DECODER_SONY_CXA2095S_J,

	DRAW_DECODER_CUSTOM,

	DRAW_DECODER_MAX
};

enum {
	/* real */
	DRAW_PALETTE_TMS9129=0,	/* TMS9129 / TMS9929 */
	DRAW_PALETTE_V9938,		/* V9938 / V9958 */
	DRAW_PALETTE_SMS,

	/* artificial */
	DRAW_PALETTE_KONAMI,
	DRAW_PALETTE_VAMPIER,
	DRAW_PALETTE_WOLF,

	DRAW_PALETTE_HARDCODED_MAX
};
enum {
	DRAW_PALETTE_FILE=DRAW_PALETTE_HARDCODED_MAX,

	DRAW_PALETTE_INTERNAL,
	DRAW_PALETTE_MAX
};

const char* draw_get_rendertype_name(u32);
const char* draw_get_vidformat_name(u32);
const char* draw_get_vidsignal_name(u32);
const char* draw_get_decoder_name(u32);
const char* draw_get_palette_name(u32);

#define DRAW_SC_VIDSIGNAL	(1<<16)
#define DRAW_SC_PALETTE		(2<<16)

void draw_set_surface_change(u32);

int draw_init(void);
void draw_init_ntsc(void);
void draw_init_settings(void);
void draw_init_palette(const char*);
void draw_save_palette(void);
int draw_load_palette(const char*,const char*);
void draw_set_window_size(void);
void draw_set_zoom(int);
INT_PTR CALLBACK draw_change_vidsignal_custom( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam );
INT_PTR CALLBACK draw_palette_settings( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam );
void draw_switch_screenmode(void);
void draw_show_menu(int);
void draw_setzero(void);
void draw_settings_save(void);
void draw_clean(void);
void draw_clean_ddraw(void);
void draw_draw(void);
void draw_repaint(void);
void __fastcall draw_check_reset(void);

int __fastcall draw_get_overlay(void);
void draw_set_flip_x(int);
int draw_get_flip_x(void);
int __fastcall draw_is_flip_x(void);
void draw_set_bfilter(int);
int draw_get_bfilter(void);
void draw_set_stretch(int);
int draw_get_stretch(void);
void draw_set_correct_aspect(int);
int draw_get_correct_aspect(void);
void draw_create_text_mutex(void);
void draw_set_text_max_ticks_default(void);
void draw_set_recreate(void);
void draw_set_repaint(int);
void draw_set_screenshot(void);
int draw_get_vsync(void);
void draw_set_vsync(int);
int draw_get_softrender(void);
void draw_set_softrender(int);
int draw_get_rendertype(void);
void draw_set_rendertype(int);
int draw_get_vidformat(void);
void draw_set_vidformat(int);
void draw_switch_vidformat(void);
int draw_get_fullscreen(void);
int draw_get_fullscreen_set(void);
int draw_get_refreshrate(void);
u8* draw_get_screen_ptr(void);
void draw_text_add(int,int,const char*);
void draw_text_clean(void);
void draw_set_text(int);
int draw_get_text(void);
int draw_get_pixel(int,int);
int draw_get_5060(void);
int draw_get_5060_auto(void);
int draw_get_source_width(void);
int draw_get_source_height(void);
int draw_get_filterindex_palette(void);
void draw_reset_bc_prev(void);

#endif /* DRAW_H */
