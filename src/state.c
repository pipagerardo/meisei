/******************************************************************************
 *                                                                            *
 *                                "state.c"                                   *
 *                                                                            *
 ******************************************************************************/

#include <windows.h>

#include "global.h"
#include "state.h"
#include "draw.h"
#include "z80.h"
#include "vdp.h"
#include "cont.h"
#include "io.h"
#include "psg.h"
#include "mapper.h"
#include "msx.h"
#include "main.h"
#include "resource.h"
#include "file.h"
#include "netplay.h"
#include "crystal.h"
#include "reverse.h"
#include "movie.h"
#include "settings.h"
#include "psglogger.h"
#include "tape.h"

/*

note: It's possible to crash the emulator by editing save states, though the user would have to know what to edit.
 This could be fixed by adding a checksum, or zlib compression. Then again, that would cause a speed hit, and more importantly,
 unfriendly to people who want to hack savestates.

state layout/size:
 header		12
 main id	1

 rest:
 x id		1
 x size		3
 x version	2
 x			x size

 msx, z80, vdp, cont, io, psg, mapper, tape

 end id		1

log_type LT_STATE max:
selected file slot 1 (state+movie)

log type LT_STATEFAIL:
max: expected cart1 $00000000 in file slot 1

*/

static int curslot=-1;
static u8* mem_state=NULL;
static int mem_state_size=0;
int semaphore=0;

static _msx_state msxstate;

static char custom_name[STRING_SIZE]={0};
static char custom_dir[STRING_SIZE]={0};
static int open_fi=1;

void state_reset_custom_name(void) { custom_name[0]=0; }
int state_get_filterindex(void) { return open_fi; }

int state_get_slot(void) { return curslot; }
void* state_get_msxstate(void) { return (void*)&msxstate; }

static u32 state_get_size(void)
{
	return 14+(6*8)			+
	 msx_state_get_size()	+
	 z80_state_get_size()	+
	 vdp_state_get_size()	+
	 cont_state_get_size()	+
	 io_state_get_size()	+
	 psg_state_get_size()	+
	 mapper_state_get_size()+
	 tape_state_get_size();
}

static void state_set_file(int movie,int custom)
{
	char name[STRING_SIZE];
	char ext[0x10];

	if (custom) {
		file_setfile(NULL,custom_name,NULL,NULL);
		if (strlen(custom_name)&&file->dir&&strlen(file->dir)) strcpy(custom_dir,file->dir);
	}
	else {
		mapper_get_current_name(name);
		if (movie) sprintf(ext,"m%x",curslot);
		else sprintf(ext,"s%x",curslot);
		file_setfile(&file->statedir,name,ext,NULL);
	}
}

char* state_get_slotdesc(int s,char* slotdesc)
{
	switch (s) {
		case STATE_SLOT_CUSTOM: sprintf(slotdesc,"custom slot"); break;
		case STATE_SLOT_MEMORY: sprintf(slotdesc,"memory slot"); break;
		default: sprintf(slotdesc,"file slot %X",s); break;
	}

	return slotdesc;
}

void state_set_slot(int s,int internal)
{
	char use[0x100];

	if (semaphore) return;
	semaphore=1;

	if (s<0) s=STATE_SLOT_MEMORY;
	else if (s>STATE_SLOT_MEMORY) s=0;
	curslot=s;

	sprintf(use,"(empty)       ");

	switch (s) {
		case STATE_SLOT_CUSTOM: sprintf(use,"              "); break;
		case STATE_SLOT_MEMORY: if (mem_state) sprintf(use,"(state)       "); break;

		default:
			/* file */
			msx_wait();

			state_set_file(FALSE,FALSE);
			if (file_accessible()) {
				sprintf(use,"(state");
				state_set_file(TRUE,FALSE);
				if (file_accessible()) strcat(use,"+movie) ");
				else strcat(use,")       ");
			}
			else {
				state_set_file(TRUE,FALSE);
				if (file_accessible()) sprintf(use,"(movie)       ");
			}

			msx_wait_done();

			break;
	}

	if (!internal) {
		char slotdesc[0x10];
		LOG(LOG_MISC|LOG_COLOUR(LC_GREEN)|LOG_TYPE(LT_STATE),"selected %s %s",state_get_slotdesc(curslot,slotdesc),use);
	}

	main_menu_radio(IDM_STATE_0+curslot,IDM_STATE_0,IDM_STATE_M);

	semaphore=0;
}


int state_set_custom_name_open(void)
{
	const char* filter="meisei State Files\0*.state;*.s0;*.s1;*.s2;*.s3;*.s4;*.s5;*.s6;*.s7;*.s8;*.s9;*.sa;*.sb;*.sc;*.sd;*.se;*.sf\0All Files\0*.*\0\0";
	const char* title="Load State";
	char fn[STRING_SIZE]={0};
	OPENFILENAME of;
	int o;

	state_reset_custom_name();

	if (curslot!=STATE_SLOT_CUSTOM) return TRUE;
	if (!msx_is_running()||semaphore||movie_get_active_state()||netplay_is_active()||psglogger_is_active()) return FALSE; /* same check as state_load */

	MAIN->dialog=TRUE;

	memset(&of,0,sizeof(OPENFILENAME));

	of.lStructSize=sizeof(OPENFILENAME);
	of.hwndOwner=MAIN->window;
	of.hInstance=MAIN->module;
	of.lpstrFile=fn;
	of.lpstrFilter=filter;
	of.lpstrTitle=title;
	of.nMaxFile=STRING_SIZE;
	of.nFilterIndex=open_fi;
	of.Flags=OFN_ENABLESIZING|OFN_HIDEREADONLY|OFN_FILEMUSTEXIST;
	of.lpstrInitialDir=strlen(custom_dir)?custom_dir:file->statedir?file->statedir:file->appdir;

	o=GetOpenFileName(&of);

	if (o) {
		strcpy(custom_name,fn);
		open_fi=of.nFilterIndex;
	}

	MAIN->dialog=FALSE;
	return o;
}

int state_set_custom_name_save(void)
{
	const char* filter="meisei State File\0*.state;*.s0;*.s1;*.s2;*.s3;*.s4;*.s5;*.s6;*.s7;*.s8;*.s9;*.sa;*.sb;*.sc;*.sd;*.se;*.sf\0\0";
	const char* defext="state";
	const char* title="Save State As";
	char fn[STRING_SIZE]={0};
	OPENFILENAME of;
	int o;

	state_reset_custom_name();

	if (curslot!=STATE_SLOT_CUSTOM) return TRUE;
	if (!msx_is_running()||semaphore) return FALSE; /* same check as state_save */

	MAIN->dialog=TRUE;

	memset(&of,0,sizeof(OPENFILENAME));

	of.lStructSize=sizeof(OPENFILENAME);
	of.hwndOwner=MAIN->window;
	of.hInstance=MAIN->module;
	of.lpstrFile=fn;
	of.lpstrDefExt=defext;
	of.lpstrFilter=filter;
	of.lpstrTitle=title;
	of.nMaxFile=STRING_SIZE;
	of.nFilterIndex=1;
	of.Flags=OFN_ENABLESIZING|OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
	of.lpstrInitialDir=strlen(custom_dir)?custom_dir:file->statedir?file->statedir:file->appdir;

	mapper_get_current_name(fn);

	o=GetSaveFileName(&of);

	if (o) strcpy(custom_name,fn);

	MAIN->dialog=FALSE;
	return o;
}


void state_load(void)
{
	u8* state; u8* state_rel; u8** s;
	u8* state_b; u8* state_b_rel; /* backup */
	char header[0x10];
	char slotdesc[0x10];
	char conflict[0x100]={0};
	char t[0x100];
	u32 i; int j,v;

	if (!msx_is_running()||semaphore||movie_get_active_state()) return;
	if (netplay_is_active()) { LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_STATEFAIL),"can't load state during netplay            "); return; }
	if (psglogger_is_active()) { LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_STATEFAIL),"can't load state during PSG log            "); return; }
	semaphore=1;

	msx_wait();

	state_get_slotdesc(curslot,slotdesc);

	/* save state backup */
	MEM_CREATE_N(state_b,STATE_MAX_SIZE)
	state_b_rel=state_b;
	z80_state_save(&state_b_rel);
	vdp_state_save(&state_b_rel);
	cont_state_save(&state_b_rel);
	io_state_save(&state_b_rel);
	psg_state_save(&state_b_rel);
	mapper_state_save(&state_b_rel);
	tape_state_save(&state_b_rel);

	/* load */
	MEM_CREATE(state,STATE_MAX_SIZE);

	if (curslot<STATE_SLOT_MEMORY) {
		/* file */
		state_set_file(FALSE,curslot==STATE_SLOT_CUSTOM);
		if (!file_open()||file->size<STATE_MIN_SIZE||file->size>STATE_MAX_SIZE||!file_read(state,file->size)) {
			file_close();
			goto loadstate_error;
		}
		file_close();
	}
	else {
		/* memory */
		if (mem_state) memcpy(state,mem_state,mem_state_size);
		else goto loadstate_error;
	}

	state_rel=state; s=&state_rel;

	STATE_LOAD_C(header,12);	if (memcmp(header,STATE_HEADER,12)!=0) goto loadstate_error;
	STATE_LOAD_1(i);			if (i!=STATE_ID_STATE) { LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_STATEFAIL),"%s is not a savestate             ",slotdesc); goto loadstate_error_nolog; }

	STATE_LOAD_1(i);			if (i!=STATE_ID_MSX) goto loadstate_error;
	STATE_LOAD_3(i);			/* msx size, don't care (yet) */
	STATE_LOAD_2(i);
	if (!msx_state_load(i,s,&msxstate,conflict)) {
		sprintf(t,"%s in %s                                       ",conflict,slotdesc); t[42]=0;
		LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_STATEFAIL),t);
		goto loadstate_error_nolog;
	}
	if (strlen(conflict)) {
		/* ignored conflict? */
		sprintf(t,"ignored %s                                 ",conflict); t[42]=0;
		LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_STATEFAIL),t);
	}

	#define LOAD_SECTION(id,x)										\
		STATE_LOAD_1(i);		if (i!=id) goto loadstate_error;	\
		STATE_LOAD_3(i);		/* size, don't care (yet) */		\
		STATE_LOAD_2(i);		j=i; if (!x##_state_load(i,s)) goto loadstate_error /* version */

	LOAD_SECTION(STATE_ID_Z80,z80);
	LOAD_SECTION(STATE_ID_VDP,vdp);
	LOAD_SECTION(STATE_ID_CONT,cont);
	LOAD_SECTION(STATE_ID_IO,io); v=j;
	LOAD_SECTION(STATE_ID_PSG,psg);
	LOAD_SECTION(STATE_ID_MAPPER,mapper);

	/* mapper internal error */
	if (mapper_get_mel_error()&1) {
		LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_STATEFAIL),"mapper state conflict in %s       ",slotdesc);
		goto loadstate_error_nolog;
	}

	/* tape since meisei 1.2.2 (io version 2) */
	if (v>=2) { LOAD_SECTION(STATE_ID_TAPE,tape); }

	STATE_LOAD_1(i);			if (i!=STATE_ID_END) goto loadstate_error;

	/* success */

	if (curslot<STATE_SLOT_MEMORY) { MEM_CLEAN(state); }
	else {
		/* memory */
		MEM_CLEAN(mem_state);
		mem_state=state;
		mem_state_size=STATE_MAX_SIZE;
	}

	MEM_CLEAN(state_b);

	if (msx_get_paused()&1) SendMessage(MAIN->window,WM_COMMAND,IDM_PAUSEEMU,0); /* force unpause */
	LOG(LOG_MISC|LOG_COLOUR(LC_GREEN)|LOG_TYPE(LT_STATE),"loaded state from %s      ",slotdesc);

	/* (don't care about region or 50/60hz hack flag for now, nor psg/vdp chiptype) */
	j=z80_get_cycles(); /* changes at crystal_set_mode */
	draw_set_vidformat((msxstate.flags&MSX_FLAG_VF)!=0);
	crystal->overclock=msxstate.cpu_speed;
	crystal_set_mode(TRUE);
	z80_set_cycles(j);

	draw_reset_bc_prev();
	cont_insert(CONT_P1,cont_get_port(CONT_P1),TRUE,FALSE);
	cont_insert(CONT_P2,cont_get_port(CONT_P2),TRUE,FALSE);
	mapper_refresh_cb(0);
	mapper_refresh_cb(1);
	mapper_refresh_pslot_read();
	mapper_refresh_pslot_write();

	reverse_invalidate();

	msx_wait_done();

	semaphore=0;
	return;

	/* error handling */
loadstate_error:

	LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_STATEFAIL),"couldn't load state from %s       ",slotdesc);

loadstate_error_nolog:

	/* load state backup */
	state_b_rel=state_b;
	z80_state_load_cur(&state_b_rel);
	vdp_state_load_cur(&state_b_rel);
	cont_state_load_cur(&state_b_rel);
	io_state_load_cur(&state_b_rel);
	psg_state_load_cur(&state_b_rel);
	mapper_state_load_cur(&state_b_rel);
	tape_state_load_cur(&state_b_rel);

	MEM_CLEAN(state);
	MEM_CLEAN(state_b);

	msx_wait_done();

	semaphore=0;
}

void state_save(void)
{
	u8* state; u8* state_rel; u8** s;
	char header[0x10];
	char slotdesc[0x10];
	u32 i,size;

	if (!msx_is_running()||semaphore) return;
	semaphore=1;

	msx_wait();

	state_get_slotdesc(curslot,slotdesc);
	size=state_get_size();
	MEM_CREATE_N(state,size)
	state_rel=state; s=&state_rel;

	sprintf(header,STATE_HEADER); /* 12 */

	/* save */
	STATE_SAVE_C(header,12);
	STATE_SAVE_1(STATE_ID_STATE);

	#define SAVE_SECTION(id,x)							\
		STATE_SAVE_1(id);								\
		i=x##_state_get_size();		STATE_SAVE_3(i);	\
		i=x##_state_get_version();	STATE_SAVE_2(i);	\
		x##_state_save(s)

	SAVE_SECTION(STATE_ID_MSX,msx);
	SAVE_SECTION(STATE_ID_Z80,z80);
	SAVE_SECTION(STATE_ID_VDP,vdp);
	SAVE_SECTION(STATE_ID_CONT,cont);
	SAVE_SECTION(STATE_ID_IO,io);
	SAVE_SECTION(STATE_ID_PSG,psg);
	SAVE_SECTION(STATE_ID_MAPPER,mapper);
	SAVE_SECTION(STATE_ID_TAPE,tape);

	STATE_SAVE_1(STATE_ID_END);

	if (curslot<STATE_SLOT_MEMORY) {
		/* file */
		state_set_file(FALSE,curslot==STATE_SLOT_CUSTOM);
		if (!file_save()||!file_write(state,size)) LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_STATEFAIL),"couldn't save state to %s         ",slotdesc);
		else LOG(LOG_MISC|LOG_COLOUR(LC_GREEN)|LOG_TYPE(LT_STATE),"saved state to %s          ",slotdesc);
		file_close();

		MEM_CLEAN(state);
	}
	else {
		/* memory */
		MEM_CLEAN(mem_state);
		mem_state=state;
		mem_state_size=size;
		LOG(LOG_MISC|LOG_COLOUR(LC_GREEN)|LOG_TYPE(LT_STATE),"saved state to %s          ",slotdesc);
	}

	msx_wait_done();

	semaphore=0;
}


void state_init(void)
{
	state_set_slot(STATE_SLOT_CUSTOM,TRUE);

	open_fi=1;
	SETTINGS_GET_INT(settings_info(SETTINGS_FILTERINDEX_STATE),&open_fi);
	CLAMP(open_fi,1,2);
}

void state_clean(void)
{
	MEM_CLEAN(mem_state);
}
