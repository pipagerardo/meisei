/******************************************************************************
 *                                                                            *
 *                                 "msx.c"                                    *
 *                                                                            *
 ******************************************************************************/

#include <windows.h>
#include <shlobj.h>

#include "global.h"
#include "crystal.h"
#include "z80.h"
#include "draw.h"
#include "input.h"
#include "sound.h"
#include "file.h"
#include "main.h"
#include "msx.h"
#include "cont.h"
#include "settings.h"
#include "resource.h"
#include "mapper.h"
#include "vdp.h"
#include "io.h"
#include "psg.h"
#include "paste.h"
#include "state.h"
#include "reverse.h"
#include "tool.h"
#include "psglogger.h"


static struct {
	int wait;
	int wait_paused;
	int waiting;

	int ignore_loadstate_crc;
	int main_interrupt;
	int frame_advance;
	int frame_advance_key;
	int is_running;
	void* pause_sample;
	int paused;
	DWORD pauselog_ticks;
	DWORD pauselog_ticks_next;
	int pauselog_visible;
	int semaphore;
	HANDLE thread;
	int priority;
	int priority_auto;
	int thread_priority_prev;
	DWORD app_priority_prev;
} msx;


static const char* msx_priority_name[MSX_PRIORITY_MAX]={"normal","above normal","high","auto"};

int __fastcall msx_is_running(void) { return msx.is_running|(msx.semaphore&1); }
int __fastcall msx_get_paused(void) { return msx.paused; }
void __fastcall msx_set_main_interrupt(void) { msx.main_interrupt=TRUE; }
int msx_get_frame_advance(void) { return msx.frame_advance; }
const char* msx_get_priority_name(u32 p) { if (p>=MSX_PRIORITY_MAX) return NULL; else return msx_priority_name[p]; }
int msx_get_priority(void) { return msx.priority; }
int msx_get_priority_auto(void) { return msx.priority_auto; }
int msx_get_ignore_loadstate_crc(void) { return msx.ignore_loadstate_crc; }


void msx_init(void)
{
	char* c=NULL;
	int i,p,size;
	u8* s;

	memset(&msx,0,sizeof(msx));
	msx.thread_priority_prev=-1;
	msx.app_priority_prev=~0;

	/* settings */
	msx.ignore_loadstate_crc=FALSE; i=settings_get_yesnoauto(SETTINGS_CRCLOADSTATEIGNORE); if (i==FALSE||i==TRUE) msx.ignore_loadstate_crc=i;

	/* priority */
	/* unstable on not-auto: seems to cause very busy app on anything else but 'normal' if app is taking 99% cpu (eg. with sleep off) */
	p=MSX_PRIORITY_AUTO;
	SETTINGS_GET_STRING(settings_info(SETTINGS_PRIORITY),&c);
	if (c&&strlen(c)) {
		for (p=0;p<MSX_PRIORITY_MAX;p++) if (stricmp(c,msx_get_priority_name(p))==0) break;
		if (p==MSX_PRIORITY_MAX) p=MSX_PRIORITY_AUTO;
	}
	MEM_CLEAN(c);
	if (p==MSX_PRIORITY_AUTO) { msx.priority_auto=TRUE; p=MSX_PRIORITY_NORMAL; }
	msx_set_priority(p);

	/* pause sample */
	s=file_from_resource(ID_SAMPLE_PAUSE,&size);
	msx.pause_sample=sound_create_sample((void*)s,size,FALSE);
	file_from_resource_close();

	msx_stop(FALSE);
}

void msx_clean(void)
{
	sound_clean_sample(msx.pause_sample);
	memset(&msx,0,sizeof(msx));
}


/* misc */
void msx_reset(int hard)
{
	if (msx_get_paused()&1) SendMessage(MAIN->window,WM_COMMAND,IDM_PAUSEEMU,0); /* force unpause */
	cont_reset_sonypause(); /* force sony unpause */

	reverse_invalidate();
	paste_stop();

	if (hard) {
		z80_poweron();
		vdp_poweron();
		io_poweron();
		cont_poweron();
		psg_poweron();
		mapper_flush_ram();
		mapper_init_cart(0);
		mapper_init_cart(1);
		mapper_write_pslot(0); /* will also refresh pslot read/write */
	}

	z80_set_busrq(FALSE);
	z80_reset();
	vdp_reset();
}


/* thread */
void __fastcall msx_frame(int reverse)
{
	int i;

	/* new frame */
	sound_new_frame();
	crystal_new_frame();
	z80_new_frame();
	vdp_new_frame();
	psg_new_frame();
	io_new_frame();
	cont_new_frame();
	mapper_new_frame();

	/* active screen */
	for (i=0;i<192;i++) {
		if (z80_get_busrq()) {
			/* z80 halted */
			vdp_line(i);
			z80_steal_cycles(crystal->vdp_scanline);
		}
		else {
			/* active */
			z80_execute(crystal->sc[i+1]+crystal->vdp_hblank);
			vdp_line(i);

			/* hblank */
			z80_execute(crystal->sc[i+1]);
		}
	}

	/* 1 dummy line */
	if (z80_get_busrq()) z80_steal_cycles(crystal->vdp_scanline);
	else z80_execute(crystal->sc[193]);

	/* vblank irq asserted at the last cycle of the first inactive row */
	vdp_vblank();
	if (reverse) reverse_cont_update();
	else {
		psglogger_frame();
		input_update();
		cont_update();
		draw_draw();
	}

	cont_check_sonypause(); /* timed at vsync */

	if (z80_get_busrq()) z80_steal_cycles(crystal->frame-193*crystal->vdp_scanline);
	else { z80_execute(crystal->vdp_scanline); vdp_reset_vblank(); z80_execute(0); }

	/* end frame */
	psg_end_frame();
	vdp_end_frame();
	io_end_frame();
	mapper_end_frame();
	z80_end_frame();

	if (!reverse) sound_compress();
}

static DWORD WINAPI msx_thread(void*)
{
	while (msx.semaphore&2) Sleep(1); /* not done yet opening game */

	if ((CoInitializeEx(NULL,COINIT_APARTMENTTHREADED))!=S_OK) {
		LOG(LOG_MISC|LOG_ERROR,"Can't initialize COM!\n");
		exit(1);
	}

	/* init */
	sound_create_ll();
	crystal->fc=0;
	input_affirm_ports();

	draw_init_palette(NULL);

	z80_init();
	psg_init_sound();
	io_init_click_sound();
	if (vdp_luminoise_get()) {
		vdp_luminoise_set(FALSE);
		vdp_luminoise_set(TRUE);
	}

	input_mouse_cursor_reset_ticks();

	msx_reset(TRUE);

	sound_reset_write_cursor();
	sound_reset_capt_cursor();
	sound_reset_lostframes();
	sound_reset_slept();

	/* run */
	while (msx.is_running) {

		msx_frame(FALSE);

		/* wait request? (time for external changes) */
		while (msx.is_running&(msx.waiting=msx.wait)) Sleep(1);

		tool_copy_locals();

		/* end frame advance */
		msx.frame_advance_key=msx.frame_advance&msx.is_running;
		msx.frame_advance=FALSE;
		msx.paused|=msx.frame_advance_key;

		if (msx.paused) {

			sound_silence(TRUE); sound_reset_lostframes();
			sound_set_ticks(0); sound_set_ticks_prev(0);
			msx.pauselog_visible=TRUE; msx.pauselog_ticks_next=0;
			msx.main_interrupt=FALSE;

			while (msx.is_running&&msx.paused) {

				Sleep(3);

				msx.pauselog_ticks=GetTickCount();
				if (msx.pauselog_ticks>msx.pauselog_ticks_next) { msx.pauselog_visible^=1; msx.pauselog_ticks_next=msx.pauselog_ticks+500; }

				if (!msx.wait_paused) {
					if (msx.paused&3) {
						LOG( LOG_MISC | LOG_COLOUR(LC_ORANGE) | LOG_TYPE(LT_PAUSE),
                        #ifdef MEISEI_ESP
                            msx.pauselog_visible ? "= pausado =" : "           "
                        #else
                            msx.pauselog_visible ? "= paused =" : "          "
                        #endif // MEISEI_ESP
                        );
						msx.paused&=3;
					}

					/* main sends "interrupt" */
					if (msx.main_interrupt) {
						msx.main_interrupt=FALSE;
						input_tick(); input_read(); input_read_shortcuts();
						draw_draw();

						if (msx.paused&4) {
							int i=crystal_check_dj();
							if (i&2||i==0) msx.paused&=3;
						}
						else msx_check_frame_advance();
					}
				}

				if (msx.priority_auto) msx_set_priority(msx.priority);
			}

			while (msx.is_running&msx.wait_paused) Sleep(1);
			WaitForSingleObject(MAIN->mutex,INFINITE); ReleaseMutex(MAIN->mutex);

			LOG(LOG_MISC|LOG_COLOUR(LC_ORANGE)|LOG_TYPE(LT_PAUSE|LT_IGNORE),"          ");

			if (!msx.frame_advance) sound_stop_sample(msx.pause_sample,TRUE);
			sound_set_ticks(SOUND_TICKS_LEN); sound_reset_write_cursor();
		}

		crystal_speed();
		reverse_frame();
		sound_write();

		if (msx.priority_auto) msx_set_priority(msx.priority);
	}

	/* clean */

	sound_clean_ll();
	sound_silence(TRUE);
	sound_reset_lostframes();
	sound_reset_slept();

	CoUninitialize();

	return 0;
}

int msx_start(void)
{
	DWORD tid; /* unused */

	msx_stop(FALSE);

	sound_create_secondary_buffer(FALSE);
	msx.is_running=TRUE;
	msx_wait_done();
	WaitForSingleObject(MAIN->mutex,INFINITE); ReleaseMutex(MAIN->mutex);

	draw_set_repaint(1);
	msx.thread=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)msx_thread,NULL,0,&tid);
	SetThreadAffinityMask(msx.thread,1);

	msx.thread_priority_prev=-1;
	msx.app_priority_prev=~0;
	msx_set_priority(msx.priority);

	return TRUE;
}

void msx_stop(int pal)
{
	if (!msx_is_running()) return;

	msx.semaphore=TRUE;
	msx.is_running=msx.paused=FALSE;
	main_menu_check(IDM_PAUSEEMU,FALSE);

	if (msx.thread!=NULL) {
		WaitForSingleObject(msx.thread,INFINITE);
		CloseHandle(msx.thread);
	}

	if (pal) draw_init_palette(NULL);
	main_titlebar(NULL);

	msx.semaphore=FALSE;
	msx_set_priority(msx.priority);

	input_affirm_ports();
}

void msx_wait(void)
{
	if (!msx_is_running()||msx_get_paused()) {
		msx.wait_paused=TRUE;
		return;
	}

	while (msx.waiting|msx.wait|msx.wait_paused) Sleep(1);
	msx.wait=TRUE;
	while (!msx.waiting) Sleep(1);
}

void msx_wait_done(void)
{
	msx.wait_paused=msx.wait=FALSE;
}

void __fastcall msx_set_priority(int p)
{
	int tp,min=FALSE;
	DWORD ap;
	WINDOWPLACEMENT wp={0};

	wp.length=sizeof(WINDOWPLACEMENT);
	if (GetWindowPlacement(MAIN->window,&wp)&&wp.showCmd==SW_SHOWMINIMIZED) min=TRUE;

	msx.priority=p;

	switch (msx.priority) {
		case 1: tp=THREAD_PRIORITY_ABOVE_NORMAL; ap=ABOVE_NORMAL_PRIORITY_CLASS; break;
		case 2: tp=THREAD_PRIORITY_HIGHEST; ap=HIGH_PRIORITY_CLASS; break;
		default: tp=THREAD_PRIORITY_NORMAL; ap=NORMAL_PRIORITY_CLASS; break;
	}

	/* app */
	if (msx.priority_auto) {
		if (!min&&!msx.paused&&msx.is_running&&MAIN->foreground&&!MAIN->dialog&&sound_get_slept()) ap=HIGH_PRIORITY_CLASS;
		else ap=NORMAL_PRIORITY_CLASS;
	}

	if (ap!=msx.app_priority_prev) {
		msx.app_priority_prev=ap;
		if (!SetPriorityClass(GetCurrentProcess(),ap)&&ap==ABOVE_NORMAL_PRIORITY_CLASS) SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS); /* 'above normal' not supported in winnt/95/98/me */
	}

	/* thread */
	if (msx.is_running) {
		if (msx.priority_auto) {
			if (!min&&!msx.paused&&MAIN->foreground&&!MAIN->dialog&&sound_get_slept()) tp=THREAD_PRIORITY_ABOVE_NORMAL;
			else tp=THREAD_PRIORITY_NORMAL;
		}

		if (tp!=msx.thread_priority_prev) {
			msx.thread_priority_prev=tp;
			SetThreadPriority(msx.thread,tp);
		}
	}
}

void __fastcall msx_set_paused(int p)
{
	if (msx_is_running()) {
		int po=msx.paused;
		msx.paused=p;

		/* play Konami pause sound, thanks to Samor for this funny idea */
		if ((po^1)&p&1) sound_play_sample(msx.pause_sample,FALSE);
	}
	else msx.paused=FALSE;
}

void msx_check_frame_advance(void)
{
	int k;

	if (~msx.paused&1) return;

	k=input_trigger_held(CONT_T_MAX+INPUT_T_SPEEDUP)|input_trigger_held(CONT_T_MAX+INPUT_T_REVERSE);
	if (!msx.frame_advance_key&&k) {
		msx.frame_advance=TRUE;
		msx.paused&=~1;
	}

	msx.frame_advance_key=k;
}


/* state				size
bios crc				4
cart1 crc				4
cart2 crc				4
cart1 uid				1
cart2 uid				1
region					1
cpu speed				2
flags					4
vdp chiptype uid		1
psg chiptype uid		1

==						23
*/
#define STATE_VERSION	2
/* version history:
1: initial
2: added vdp and psg chiptype
*/
#define STATE_SIZE		23

int __fastcall msx_state_get_version(void)
{
	return STATE_VERSION;
}

int __fastcall msx_state_get_size(void)
{
	return STATE_SIZE;
}

/* save/get */
void __fastcall msx_state_save(u8** s)
{
	u32 i;

	i=mapper_get_bioscrc();		STATE_SAVE_4(i);
	i=mapper_get_cartcrc(0);	STATE_SAVE_4(i);
	i=mapper_get_cartcrc(1);	STATE_SAVE_4(i);
	STATE_SAVE_1(mapper_get_type_uid(mapper_get_carttype(0)));
	STATE_SAVE_1(mapper_get_type_uid(mapper_get_carttype(1)));
	STATE_SAVE_1(cont_get_region());

	STATE_SAVE_2(crystal->overclock);

	i=0;

	if (crystal->mode) i|=MSX_FLAG_VF;
	if (draw_get_5060()) i|=MSX_FLAG_5060;
	if (draw_get_5060_auto()) i|=MSX_FLAG_5060A;

	STATE_SAVE_4(i);

	STATE_SAVE_1(vdp_get_chiptype_uid(vdp_get_chiptype()));
	STATE_SAVE_1(psg_get_chiptype_uid(psg_get_chiptype()));
}

void __fastcall msx_get_state(_msx_state* state)
{
	memset(state,0,sizeof(_msx_state));

	state->bios_crc=mapper_get_bioscrc();
	state->cart1_crc=mapper_get_cartcrc(0);
	state->cart1_uid=mapper_get_type_uid(mapper_get_carttype(0));
	state->cart2_crc=mapper_get_cartcrc(1);
	state->cart2_uid=mapper_get_type_uid(mapper_get_carttype(1));
	state->cpu_speed=crystal->overclock;
	state->region=cont_get_region();
	state->vdpchip_uid=vdp_get_chiptype_uid(vdp_get_chiptype());
	state->psgchip_uid=psg_get_chiptype_uid(psg_get_chiptype());

	if (crystal->mode) state->flags|=MSX_FLAG_VF;
	if (draw_get_5060()) state->flags|=MSX_FLAG_5060;
	if (draw_get_5060_auto()) state->flags|=MSX_FLAG_5060A;
}

/* load */
static int __fastcall msx_state_load_h(int v,u8** s,_msx_state* state,char* conflict)
{
    v = v;
	/* first half same for v1 and v2 */
	u32 i,j;

	/* always fatal */
	STATE_LOAD_4(i);	if (i!=mapper_get_bioscrc()) { sprintf(conflict,"expected BIOS $%08X",i); if (!msx.ignore_loadstate_crc) return FALSE; }
	STATE_LOAD_4(i);	if (i!=mapper_get_cartcrc(0)) { sprintf(conflict,"expected cart1 $%08X",i); if (!msx.ignore_loadstate_crc) return FALSE; }
	STATE_LOAD_4(i);	if (i!=mapper_get_cartcrc(1)) { sprintf(conflict,"expected cart2 $%08X",i); if (!msx.ignore_loadstate_crc) return FALSE; }
	STATE_LOAD_1(i);	if (i>CARTUID_MAX) i=0; if (mapper_get_type_uid(mapper_get_uid_type(i))!=mapper_get_type_uid(mapper_get_carttype(0))) { sprintf(conflict,"expected cart1 %s",mapper_get_type_shortname(mapper_get_uid_type(i)&0x3fffffff)); return FALSE; }				state->cart1_uid=i;
	STATE_LOAD_1(i);	if (i>CARTUID_MAX) i=0; if (mapper_get_type_uid(mapper_get_uid_type(i|0x80000000))!=mapper_get_type_uid(mapper_get_carttype(1))) { sprintf(conflict,"expected cart2 %s",mapper_get_type_shortname(mapper_get_uid_type(i|0x80000000)&0x3fffffff)); return FALSE; }	state->cart2_uid=i;

	/* not necessarily fatal */
	STATE_LOAD_1(state->region);
	STATE_LOAD_2(state->cpu_speed);

	/* flags */
	STATE_LOAD_4(i);

	j=(~i&MSX_FMASK_RAMSLOT)>>MSX_FSHIFT_RAMSLOT;
	if (j!=(u32)mapper_get_ramslot()) { sprintf(conflict,"expected RAM in slot %d",j); return FALSE; }

	state->flags=i;

	return TRUE;
}

int __fastcall msx_state_load(int v,u8** s,_msx_state* state,char* conflict)
{
	switch (v) {
		case 1:
			if (!msx_state_load_h(v,s,state,conflict)) return FALSE;

			state->vdpchip_uid=vdp_get_chiptype_uid((state->flags&MSX_FLAG_VF)?VDP_CHIPTYPE_TMS9129:VDP_CHIPTYPE_TMS9118);
			state->psgchip_uid=psg_get_chiptype_uid(PSG_CHIPTYPE_YM2149);

			break;

		case STATE_VERSION:
			if (!msx_state_load_h(v,s,state,conflict)) return FALSE;

			/* not necessarily fatal */
			STATE_LOAD_1(state->vdpchip_uid);
			STATE_LOAD_1(state->psgchip_uid);

			break;

		default:
			sprintf(conflict,"conflicting media setup");
			return FALSE;
	}

	return TRUE;
}
