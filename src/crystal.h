/******************************************************************************
 *                                                                            *
 *                              "crystal.h"                                   *
 *                                                                            *
 * Parece  ser  algo  relacionado  con  el  sonido  del  MSX  por culpa de la *
 * frecuencia de reloj que es diferente en algunos modelos.                   *
 ******************************************************************************/

#ifndef CRYSTAL_H
#define CRYSTAL_H

#include "global.h"

typedef struct {

	int lines;

	int overclock;
	int cycle;
	int rel_cycle;
	int frame;
	int max_cycles;
	int vblank_trigger;
	int vdp_active;
	int vdp_hblank;
	int vdp_scanline;
	int vdp_cycle;

	int mode;	 /* PAL/NTSC */
	int hc[314]; /* hblank counter: start of each hblank */
	int sc[314]; /* scanline counter: start of each scanline */

	u32 fc;
	u32 fc_h;

	int dj_reverse;
	int speed_percentage;
	int slow_percentage;
	int speed;
	int fast;

	/* sound */
	int slice_orig[2];
	int slice[2];
	int base;
	int sub;
	int cspf;

} Crystal;

extern Crystal* crystal;

int  crystal_get_cspf_max(void);
int  crystal_get_slow_max(void);
void crystal_set_mode(int);
void crystal_set_cpuspeed(int);
int  crystal_check_dj(void);
void crystal_speed(void);
void crystal_new_frame(void);
void crystal_init(void);
void crystal_settings_load(void);
void crystal_clean(void);
INT_PTR CALLBACK crystal_timing( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam );

#endif /* CRYSTAL_H */
