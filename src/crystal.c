/******************************************************************************
 *                                                                            *
 *                               "crystal.c"                                  *
 *                                                                            *
 ******************************************************************************/

#include <windows.h>
#include <commctrl.h>

#include "crystal.h"
#include "draw.h"
#include "main.h"
#include "resource.h"
#include "settings.h"
#include "msx.h"
#include "input.h"
#include "z80.h"
#include "netplay.h"
#include "cont.h"
#include "reverse.h"
#include "movie.h"
#include "vdp.h"

/* EXTERN */
Crystal* crystal;

/*

MSX crystal and VDP run at 118125000/11=~10738636.36Hz
MSX VDP pixel/memory clock is crystal/2=~5369318.18Hz
MSX CPU runs at crystal/3=~3579545.45Hz

VDP: 256 active cycles, 342 total
192 active lines (+ 1 dummy), 262 (NTSC) or 313 (PAL) total
(not accounting border)

NTSC refresh rate: VDPspeed/(262*342)=~59.92Hz
PAL refresh rate: VDPspeed/(313*342)=~50.16Hz

timing, with sound: create 1 sample every cpuspeed/44100 cycle: 81+13/77 (~81.17)
exact 60Hz: [cycles per frame]/(44100/60)=81+67/245 (~81.27)
exact 50Hz: [cpf]/(44100/50)=80+134/147 (~80.91)
~60.01Hz (my monitor :P ): 81+7345/25591 (~81.29)

on some MSXes, including one of mine, the Z80:VDP pixel clock 3:2 ratio is slightly different,
caused by the Z80 and VDP being driven by 2 different crystals instead of 1 central crystal,
resulting in glitches with programs that rely on official timing (eg. MSX Unleashed demo
zoom/rotation part border scroller text becomes italic):
- single crystal:             ~10.738MHz or mul    3:2
- (my) Canon V-20:
      VDP (X1), marked:       10.6875              approx 2211:1481 -- 419881:281250 is closer but large
      Z80/PSG (X2), marked:   1431818
  Toshiba HX-10 is also known to have a VDP crystal of 10.6875Mhz

*/

#define CPU_CYCLE				3
#define VDP_CYCLE				1
#define VDP_SCANLINE			(VDP_CYCLE*2*342)
#define VDP_ACTIVE				(VDP_CYCLE*2*256)
#define VDP_HBLANK				(VDP_SCANLINE-VDP_ACTIVE)

#define NTSC_LINES				262
#define NTSC_FRAME				(VDP_SCANLINE*NTSC_LINES)
#define NTSC_VBLANK_TRIGGER		(NTSC_FRAME-(VDP_SCANLINE*193))

#define PAL_LINES				313
#define PAL_FRAME				(VDP_SCANLINE*PAL_LINES)
#define PAL_VBLANK_TRIGGER		(PAL_FRAME-(VDP_SCANLINE*193))

#define WIN_SOUND_NTSC_SLICE_1	81
#define WIN_SOUND_NTSC_SLICE_2	82
#define WIN_SOUND_NTSC_BASE		77
#define WIN_SOUND_NTSC_SUB		13
#define WIN_SOUND_NTSC_CSPF		736

#define WIN_SOUND_NTSCs_SLICE_1	81
#define WIN_SOUND_NTSCs_SLICE_2	82
#if 0
#define WIN_SOUND_NTSCs_BASE	25591
#define WIN_SOUND_NTSCs_SUB		7345
#else
#define WIN_SOUND_NTSCs_BASE	245
#define WIN_SOUND_NTSCs_SUB		67
#endif
#define WIN_SOUND_NTSCs_CSPF	735

#define WIN_SOUND_PAL_SLICE_1	81
#define WIN_SOUND_PAL_SLICE_2	82
#define WIN_SOUND_PAL_BASE		77
#define WIN_SOUND_PAL_SUB		13
#define WIN_SOUND_PAL_CSPF		879

#define WIN_SOUND_PALs_SLICE_1	80
#define WIN_SOUND_PALs_SLICE_2	81
#define WIN_SOUND_PALs_BASE		147
#define WIN_SOUND_PALs_SUB		134
#define WIN_SOUND_PALs_CSPF		882	/* largest */

#define OC_UPSCALE				1024
#define OC_MAX					9999
#define OC_MIN					25
#define SP_STEPS				30	/* should be dividable by 3 */
#define SPEED_MAX				999
#define SLOW_MAX				10

int crystal_get_slow_max(void) { return SLOW_MAX; }
int crystal_get_cspf_max(void) { return WIN_SOUND_PAL_CSPF; }


void crystal_init(void)
{
	MEM_CREATE(crystal,sizeof(Crystal));

	crystal->vdp_cycle=VDP_CYCLE*OC_UPSCALE;
	crystal->vdp_active=VDP_ACTIVE*OC_UPSCALE;
	crystal->vdp_hblank=VDP_HBLANK*OC_UPSCALE;
	crystal->vdp_scanline=VDP_SCANLINE*OC_UPSCALE;
	crystal->max_cycles=PAL_FRAME/CPU_CYCLE; /* used to calculate sound buffer size */

	LOG(
        LOG_VERBOSE,
    #ifdef MEISEI_ESP
        "cristal inicializado\n"
    #else
        "crystal initialised\n"
    #endif // MEISEI_ESP
    );
}

void crystal_settings_load(void)
{
	int i;

	crystal->overclock=100;
	if (SETTINGS_GET_INT(settings_info(SETTINGS_CPUSPEED),&i)) { CLAMP(i,OC_MIN,OC_MAX); crystal->overclock=i; }

	crystal->speed_percentage=300;
	if (SETTINGS_GET_INT(settings_info(SETTINGS_SPEEDPERC),&i)) { CLAMP(i,101,SPEED_MAX); crystal->speed_percentage=i; }

	crystal->slow_percentage=30;
	if (SETTINGS_GET_INT(settings_info(SETTINGS_SLOWPERC),&i)) { CLAMP(i,SLOW_MAX,99); crystal->slow_percentage=i; }
}

void crystal_clean(void)
{
	LOG(
        LOG_VERBOSE,
    #ifdef MEISEI_ESP
        "cristal limpiado\n"
    #else
        "crystal cleaned\n"
    #endif // MEISEI_ESP
    );

	MEM_CLEAN(crystal);
}

static __inline__ void crystal_set_speed_imm(float f)
{
	crystal->slice[0]=crystal->slice_orig[0]*f;
	crystal->slice[1]=crystal->slice_orig[1]*f;

	crystal->fast=f>1.0;
}

static __inline__ void crystal_set_speed(void)
{
	float f;

	if (crystal->speed<0) f=1.0-(-crystal->speed/(float)(SP_STEPS/3))*(float)(1.0-(crystal->slow_percentage/100.0));
	else f=1.0+((crystal->speed/(float)SP_STEPS)*(float)(crystal->speed_percentage-100))/100.0;

	crystal_set_speed_imm(f);
}

int crystal_check_dj(void)
{
	static int prevrev=0;
	int ret=0;

	crystal->dj_reverse=0;

	if (input_trigger_held(CONT_T_MAX+INPUT_T_DJHOLD)) {
		long l=input_get_axis(CONT_A_MAX+INPUT_A_DJSPIN);

		ret|=1;
		crystal->dj_reverse=2;
		crystal->speed=0;

		if (l<0&&!reverse_is_enabled()) l=0;

		if (l==0) {
			if (prevrev) { prevrev=0; crystal->dj_reverse=1; }
			msx_set_paused(msx_get_paused()|4);
			ret|=4;
		}
		else {
			float f;

			if (l<0) { l=-l; prevrev=crystal->dj_reverse=1; }
			else prevrev=0;
			f=0.25+l*0.08; CLAMP(f,0.3,10.0);
			crystal_set_speed_imm(f);

			ret|=2;
		}
	}
	else prevrev=0;

	return ret;
}

void crystal_speed(void)
{
	crystal->dj_reverse=0;

	if (netplay_is_active()||msx_get_frame_advance()) return;

	if (!crystal_check_dj()) {
		/* left/right masking algorithm by blargg */
		static int mask=2;
		static int prev=0;
		int pressed;

		pressed=input_trigger_held(CONT_T_MAX+INPUT_T_SPEEDUP)<<1|input_trigger_held(CONT_T_MAX+INPUT_T_SLOWDOWN);
		if ((pressed^prev)&3&&pressed&~mask&3) mask^=3;
		prev=pressed;
		pressed&=mask;

		if (pressed&2) crystal->speed+=(crystal->speed<SP_STEPS);
		else crystal->speed-=(crystal->speed>0);
		if (pressed&1) crystal->speed-=(crystal->speed>-(SP_STEPS/3));
		else crystal->speed+=(crystal->speed<0);

		crystal_set_speed();
	}
}

void crystal_set_cpuspeed(int p)
{
	float oc;

	CLAMP( p, OC_MIN, OC_MAX );
	if ( p != crystal->overclock )
    {
		crystal->overclock=p;
		LOG(
            LOG_MISC | LOG_COLOUR(LC_GREEN) | LOG_TYPE(LT_CPUSPEED),
        #ifdef MEISEI_ESP
            "%d%% Z80 velocidad reloj    ",
        #else
            "%d%% Z80 clock speed   ",
        #endif // MEISEI_ESP
            p
        );
		if ((p%50)==0) input_cpuspeed_ticks_stall();
	}

	oc=crystal->overclock;
	oc=(float)OC_UPSCALE*(100.0/oc);

	crystal->cycle=CPU_CYCLE*oc;

	if (msx_is_running()) {
		z80_fill_op_cycles_lookup();
		z80_adjust_rel_cycles();
	}
}

void crystal_set_mode(int show)
{
	int i,t=0;
	char c[STRING_SIZE]={0};
	int pal=draw_get_vidformat();

	if (show&&pal!=crystal->mode) {
        LOG(
            LOG_MISC | LOG_COLOUR(LC_GREEN) | LOG_TYPE(LT_VIDFORMAT),
        #ifdef MEISEI_ESP
            "%s frecuencia de vídeo",
        #else
            "%s video frequency ",
        #endif // MEISEI_ESP
            pal ? "50Hz" : "60Hz"
        );
	}
	crystal->mode=pal;

	crystal_set_cpuspeed(crystal->overclock);
	crystal->rel_cycle=CPU_CYCLE*OC_UPSCALE;
	crystal->lines=pal?PAL_LINES:NTSC_LINES;
	crystal->frame=(pal?PAL_FRAME:NTSC_FRAME)*OC_UPSCALE;
	crystal->vblank_trigger=(pal?PAL_VBLANK_TRIGGER:NTSC_VBLANK_TRIGGER)*OC_UPSCALE;

	if (draw_get_5060_auto()) {
		int mf=draw_get_refreshrate();
		if ( mf > 0 ) { t = mf % ( pal ? 50 : 60 ); t = t == 0; }
	}

	if (t|draw_get_5060()&&!netplay_is_active()) {
		crystal->slice_orig[0]=pal?WIN_SOUND_PALs_SLICE_1:WIN_SOUND_NTSCs_SLICE_1;
		crystal->slice_orig[1]=pal?WIN_SOUND_PALs_SLICE_2:WIN_SOUND_NTSCs_SLICE_2;
		crystal->base=pal?WIN_SOUND_PALs_BASE:WIN_SOUND_NTSCs_BASE;
		crystal->sub=pal?WIN_SOUND_PALs_SUB:WIN_SOUND_NTSCs_SUB;
		crystal->cspf=pal?WIN_SOUND_PALs_CSPF:WIN_SOUND_NTSCs_CSPF;
	}
	else {
		crystal->slice_orig[0]=pal?WIN_SOUND_PAL_SLICE_1:WIN_SOUND_NTSC_SLICE_1;
		crystal->slice_orig[1]=pal?WIN_SOUND_PAL_SLICE_2:WIN_SOUND_NTSC_SLICE_2;
		crystal->base=pal?WIN_SOUND_PAL_BASE:WIN_SOUND_NTSC_BASE;
		crystal->sub=pal?WIN_SOUND_PAL_SUB:WIN_SOUND_NTSC_SUB;
		crystal->cspf=pal?WIN_SOUND_PAL_CSPF:WIN_SOUND_NTSC_CSPF;
	}

	crystal_set_speed();

	for (i=0;i<=PAL_LINES;i++) {
		crystal->hc[i]=crystal->frame-(i*crystal->vdp_scanline)-crystal->vdp_active;
		crystal->sc[i]=crystal->frame-(i*crystal->vdp_scanline);
	}

	vdp_set_chiptype_vf(pal);

	/* update menu */
	main_menu_caption_get(IDM_VIDEOFORMAT,c);
#ifdef MEISEI_ESP
    c[10]=crystal->mode?'6':'5';
#else
    c[11]=crystal->mode?'6':'5';
#endif // MEISEI_ESP


	main_menu_caption_put(IDM_VIDEOFORMAT,c);
}

void crystal_new_frame(void)
{
	crystal->fc++;
	crystal->fc_h+=(crystal->fc==0);
}

/* timing dialog */
static int crystal_get_input_trigger_caption(char* c,u32 idi)
{
	const char* src=input_get_trigger_set(idi);
	memset(c,0,STRING_SIZE);

	if (stricmp(src,"nothing")!=0) {
		int i=0,j,len=strlen(src);

		for (j=0;j<len;j++) {
			/* cap at | */
			if (src[j]=='|') {
				if (i&&c[i-1]==' ') c[i-1]=0;
				break;
			}

			/* convert & to + (or dialog will interpret it as shortcut) */
			if (src[j]=='&') {
				if (i&&c[i-1]==' ') c[i-1]='+';
				else c[i++]='+';
				if (src[j+1]==' ') j++;
			}

			else c[i++]=src[j];
		}
	}

	return strlen(c);
}

static void crystal_truncate_input_trigger_caption(char* c)
{
	if (strlen(c)>32) { c[29]=c[30]=c[31]='.'; c[32]='\0'; }
}

static void crystal_set_z80_info(HWND dialog)
{
	char c[0x10];
	int i; float f;

	if (!dialog||!GetDlgItem(dialog,IDC_TIMING_Z80_EDIT)||!GetDlgItem(dialog,IDC_TIMING_Z80_INFO)) return;

	i=GetDlgItemInt(dialog,IDC_TIMING_Z80_EDIT,NULL,FALSE); CLAMP(i,OC_MIN,OC_MAX);
	f=(i*3579545.45)/100000000.0;
	sprintf(c,"%1.*f MHz",f<10.0?2:f<100.0?1:0,f);
	SetDlgItemText(dialog,IDC_TIMING_Z80_INFO,c);
}

INT_PTR CALLBACK crystal_timing( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam )
{
    lParam = lParam;
	switch (msg) {

		case WM_INITDIALOG: {
			int i;
			char c[STRING_SIZE];
			char ic[STRING_SIZE];

			/* fill groupboxes caption */
			sprintf(
                c,
            #ifdef MEISEI_ESP
                "Inversa"
            #else
                "Reverse"
            #endif
            );
			if (crystal_get_input_trigger_caption(ic,CONT_T_MAX+INPUT_T_REVERSE)) {
				crystal_truncate_input_trigger_caption(ic);
				strcat(c," ("); strcat(c,ic); strcat(c,")");
			}
			SetDlgItemText(dialog,IDC_TIMING_REVERSE_GROUP,c);

			sprintf(
                c,
            #ifdef MEISEI_ESP
                "Velocidad"
            #else
                "Speed"
            #endif // MEISEI_ESP
            );
			if (crystal_get_input_trigger_caption(ic,CONT_T_MAX+INPUT_T_SLOWDOWN)||crystal_get_input_trigger_caption(ic,CONT_T_MAX+INPUT_T_SPEEDUP)) {
				char ic2[STRING_SIZE]; ic[0]=ic[1]=0; ic2[0]=ic2[1]=0;
				crystal_get_input_trigger_caption(ic2,CONT_T_MAX+INPUT_T_SLOWDOWN);
				crystal_get_input_trigger_caption(ic,CONT_T_MAX+INPUT_T_SPEEDUP);
				if (strlen(ic)&&strlen(ic2)) strcat(ic2,", ");
				strcat(ic2,ic);
				crystal_truncate_input_trigger_caption(ic2);
				strcat(c," ("); strcat(c,ic2); strcat(c,")");
			}
			SetDlgItemText(dialog,IDC_TIMING_SPEED_GROUP,c);

			sprintf(c,"Z80");
			if (crystal_get_input_trigger_caption(ic,CONT_T_MAX+INPUT_T_CPUSPEEDMIN)||crystal_get_input_trigger_caption(ic,CONT_T_MAX+INPUT_T_CPUSPEEDPLUS)) {
				char ic2[STRING_SIZE]; ic[0]=ic[1]=0; ic2[0]=ic2[1]=0;
				crystal_get_input_trigger_caption(ic2,CONT_T_MAX+INPUT_T_CPUSPEEDMIN);
				crystal_get_input_trigger_caption(ic,CONT_T_MAX+INPUT_T_CPUSPEEDPLUS);
				if (strlen(ic)&&strlen(ic2)) strcat(ic2,", ");
				strcat(ic2,ic);
				crystal_truncate_input_trigger_caption(ic2);
				strcat(c," ("); strcat(c,ic2); strcat(c,")");
			}
			SetDlgItemText(dialog,IDC_TIMING_Z80_GROUP,c);

			/* init slider */
			SendDlgItemMessage(dialog,IDC_TIMING_REVERSE_SLIDER,TBM_SETRANGE,FALSE,(LPARAM)MAKELONG(0,(REVERSE_BUFFER_SIZE_MAX-1)));
			SendDlgItemMessage(dialog,IDC_TIMING_REVERSE_SLIDER,TBM_SETPOS,TRUE,(LPARAM)(LONG)reverse_get_buffer_size());

			/* init spins */
			SendDlgItemMessage(dialog,IDC_TIMING_SLOWDOWN_SPIN,UDM_SETRANGE,0,(LPARAM)MAKELONG(99,SLOW_MAX));
			SendDlgItemMessage(dialog,IDC_TIMING_SLOWDOWN_EDIT,EM_LIMITTEXT,2,0);
			SetDlgItemInt(dialog,IDC_TIMING_SLOWDOWN_EDIT,crystal->slow_percentage,FALSE);

			SendDlgItemMessage(dialog,IDC_TIMING_SPEEDUP_SPIN,UDM_SETRANGE,0,(LPARAM)MAKELONG(SPEED_MAX,101));
			SendDlgItemMessage(dialog,IDC_TIMING_SPEEDUP_EDIT,EM_LIMITTEXT,3,0);
			SetDlgItemInt(dialog,IDC_TIMING_SPEEDUP_EDIT,crystal->speed_percentage,FALSE);

			SendDlgItemMessage(dialog,IDC_TIMING_Z80_SPIN,UDM_SETRANGE,0,(LPARAM)MAKELONG(OC_MAX,OC_MIN));
			SendDlgItemMessage(dialog,IDC_TIMING_Z80_EDIT,EM_LIMITTEXT,4,0);
			SetDlgItemInt(dialog,IDC_TIMING_Z80_EDIT,crystal->overclock,FALSE);
			crystal_set_z80_info(dialog);

			/* can't change Z80 speed while movie is busy */
			if (movie_get_active_state()) {
				for (i=IDC_TIMING_Z80_GROUP;i<=IDC_TIMING_Z80_INFO;i++) EnableWindow(GetDlgItem(dialog,i),FALSE);
			}

			main_parent_window(dialog,0,0,0,0,0);
			return 1;
			break;
		}

		case WM_NOTIFY:

			switch (wParam) {

				/* change slider */
				case IDC_TIMING_REVERSE_SLIDER: {
					int pos=SendDlgItemMessage(dialog,IDC_TIMING_REVERSE_SLIDER,TBM_GETPOS,0,0);
					SetDlgItemInt(dialog,IDC_TIMING_REVERSE_INFO,pos,FALSE);
					if (pos) {
						// char c[0x10]; // Warning -Wformat-overvlow
						char c[0x20];
						int sec=((reverse_get_buffer_size_size(pos)-1)*100)/(crystal->mode?50:60);
						sprintf(
                            c,
                            "%d:%02d, %d MB",
                            sec/60,
                            sec%60,
                            (
                                200 * reverse_get_state_size() + reverse_get_buffer_size_size(pos) *
                                ( reverse_get_state_size() + 100 * CONT_MAX_MOVIE_SIZE )
                             ) >> 20
                        );
						SetDlgItemText( dialog,IDC_TIMING_REVERSE_TIME, c );
					}
					else SetDlgItemText(dialog,IDC_TIMING_REVERSE_TIME,"disabled");

					break;
				}

				default: break;
			}
			break;

		case WM_COMMAND:

			switch (LOWORD(wParam)) {

				/* change edits */
				#define CLAMP_EDIT(min,max)											\
				if (HIWORD(wParam)==EN_KILLFOCUS) {									\
					int i=GetDlgItemInt(dialog,LOWORD(wParam),NULL,FALSE);			\
					CLAMP(i,min,max); SetDlgItemInt(dialog,LOWORD(wParam),i,FALSE);	\
				}

				case IDC_TIMING_SLOWDOWN_EDIT: CLAMP_EDIT(SLOW_MAX,99) break;
				case IDC_TIMING_SPEEDUP_EDIT: CLAMP_EDIT(101,SPEED_MAX) break;
				case IDC_TIMING_Z80_EDIT: CLAMP_EDIT(OC_MIN,OC_MAX) crystal_set_z80_info(dialog); break;


				/* close dialog */
				case IDCANCEL:
					EndDialog(dialog,0);
					break;

				case IDOK: {
					int i;
					msx_wait();

					/* reverse */
					i=SendDlgItemMessage(dialog,IDC_TIMING_REVERSE_SLIDER,TBM_GETPOS,0,0);
					CLAMP(i,0,REVERSE_BUFFER_SIZE_MAX-1);
					if (i!=reverse_get_buffer_size()) {
						reverse_set_enable(TRUE);
						reverse_set_buffer_size(i);
						reverse_invalidate();
						if (i==0) reverse_clean();
					}

					/* slowdown/speedup */
					crystal->speed_percentage=GetDlgItemInt(dialog,IDC_TIMING_SPEEDUP_EDIT,NULL,FALSE); CLAMP(crystal->speed_percentage,101,SPEED_MAX);
					crystal->slow_percentage=GetDlgItemInt(dialog,IDC_TIMING_SLOWDOWN_EDIT,NULL,FALSE); CLAMP(crystal->slow_percentage,SLOW_MAX,99);

					/* z80 */
					i=GetDlgItemInt(dialog,IDC_TIMING_Z80_EDIT,NULL,FALSE); CLAMP(i,OC_MIN,OC_MAX);
					if (i!=crystal->overclock) { crystal->overclock=i; crystal_set_cpuspeed(i); }

					msx_wait_done();

					EndDialog(dialog,0);
					break;
				}

				default: break;
			}
			break;

		default: break;
	}

	return 0;
}
