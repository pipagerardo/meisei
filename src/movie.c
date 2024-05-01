/******************************************************************************
 *                                                                            *
 *                                "movie.c"                                   *
 *                                                                            *
 ******************************************************************************/

#include <windows.h>

#include "global.h"
#include "movie.h"
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
#include "version.h"
#include "netplay.h"
#include "file.h"
#include "crystal.h"
#include "reverse.h"
#include "settings.h"
#include "psglogger.h"
#include "tape.h"

/*

movie layout/size:
 header					12
 main id				1
 meisei version			2
 cur savestate			?
 savestate end			1

 rest:
 framecount				4

 rlecount				1
 buttons				?

*/

#define A_STATE_STOP	0
#define A_STATE_PLAY	1
#define A_STATE_RECORD	2
static int active_state=A_STATE_STOP;

static int record_overflow;
static int semaphore;
static int has_subs;
static int timecode;
static int framecount;
static int framecount_max;
static int far_count;
static int near_count;
static int data_size;
static int mstate_size;
static u8* mstate;

typedef struct _movie {
	u8* data;
	struct _movie* next;
	struct _movie* prev;
} _movie;
static _movie* _movie_begin;
static _movie* _movie_cursor;

typedef struct _subs_text {
	int id;
	int colour;
	int y;
	char text[0x100];
	struct _subs_text* next;
} _subs_text;
typedef struct _subs {
	int frame;
	struct _subs_text* sub;
	struct _subs* next;
	struct _subs* prev;
} _subs;
static _subs* _subs_begin;
static _subs* _subs_cursor;
#define SUBS_HEADER "meisei movie subtitles"

static char custom_name[STRING_SIZE]={0};
static char custom_dir[STRING_SIZE]={0};
static int open_fi=1;

void movie_reset_custom_name(void) { custom_name[0]=0; }
int movie_get_filterindex(void) { return open_fi; }

int __fastcall movie_get_active_state(void) { return active_state; }
int __fastcall movie_is_playing(void) { return active_state==A_STATE_PLAY; }
int __fastcall movie_is_recording(void) { return active_state==A_STATE_RECORD; }
int movie_get_timecode(void) { return timecode; }
void movie_set_timecode(int t) { timecode=t; main_menu_check(IDM_TIMECODE,timecode); }


static void movie_update_menu(void)
{
	main_menu_update_enable();

	switch (active_state) {
		case A_STATE_PLAY: main_menu_radio(IDM_MOVIE_PLAY,IDM_MOVIE_RECORD,IDM_MOVIE_STOP); break;
		case A_STATE_RECORD: main_menu_radio(IDM_MOVIE_RECORD,IDM_MOVIE_RECORD,IDM_MOVIE_STOP); break;
		case A_STATE_STOP: main_menu_radio(IDM_MOVIE_STOP,IDM_MOVIE_RECORD,IDM_MOVIE_STOP); break;

		default: break;
	}
}

static int movie_get_state_size(void)
{
	return 16				+
	 msx_state_get_size()	+
	 z80_state_get_size()	+
	 vdp_state_get_size()	+
	 cont_state_get_size()	+
	 io_state_get_size()	+
	 psg_state_get_size()	+
	 mapper_state_get_size()+
	 tape_state_get_size();
}


static void movie_clean_subs(void)
{
	has_subs=FALSE;

	if (_subs_begin==NULL) return;

	_subs_cursor=_subs_begin;

	/* clean frames */
	for (;;) {
		if (_subs_cursor->sub) {
			_subs_text* subp=_subs_cursor->sub;
			_subs_text* subp_prev;

			/* clean subs */
			for (;;) {
				subp_prev=subp;
				subp=subp->next;
				MEM_CLEAN(subp_prev);
				if (subp==NULL) break;
			}
		}

		if (_subs_cursor->next==NULL) {
			MEM_CLEAN(_subs_cursor);
			break;
		}
		_subs_cursor=_subs_cursor->next;
		MEM_CLEAN(_subs_cursor->prev);
	}

	_subs_begin=NULL;
}

static int strip_newline(char* c)
{
	int len;

	if (!c) return 0;

	len=strlen(c);

	/* strip off possible \n and \r at end */
	if (len&&c[len-1]=='\n') {
		len--; c[len]=0;
		if (len&&c[len-1]=='\r') {
			len--; c[len]=0;
		}
	}

	return len;
}

static void movie_open_subs(int fps)
{
	FILE* fd;
	char data[0x100];
	char* ft;
	int subnum=1;
	int len;

	movie_clean_subs(); /* clean previous (just to be sure) */

	if (!file_accessible()) return;

	/* check header */
	memset(data,0,0x20);
	if (!file_open()||file->size<0x20||file->size>20000000||!file_read((u8*)data,strlen(SUBS_HEADER))||memcmp((u8*)data,SUBS_HEADER,strlen(SUBS_HEADER))) {
		LOG(LOG_MISC|LOG_WARNING,"couldn't open movie subtitles\n");
		file_close(); return;
	}

	if (fps) fps=50; else fps=60;
	fd=file_get_fd();

	/* parse */
	for (;;) {
		memset(data,0,0x100);
		ft=fgets(data,0xfe,fd);

		len=ft?strip_newline(data):0;

		if (len&&data[0]!=';') {
			int hh=-1,mm=-1,ss=-1,ff=-1;
			int d_id,d_c,d_y;
			int subnum_f=0;
			int frame;
			_subs_text* subp=NULL;

			/* frame, hh:mm:ss;ff eg. 00:02:31;22 */
			sscanf(data,"%d:%d:%d;%d",&hh,&mm,&ss,&ff);
			if (hh>9000) {
				/* :P */
				LOG(LOG_MISC|LOG_WARNING,"it's over nine thousaaaaand! (%d)\n",subnum);
				file_close(); movie_clean_subs(); return;
			}
			frame=(hh*3600*fps)+(mm*60*fps)+(ss*fps)+ff;
			if (_subs_cursor!=NULL&&frame<=_subs_cursor->frame) frame=-1;
			if (hh<0||mm<0||ss<0||ff<0||hh>9000||mm>59||ss>59||ff>=fps||frame<1||frame>framecount_max) {
				/* scanf error/out of bounds */
				LOG(LOG_MISC|LOG_WARNING,"couldn't read subs frame %d\n",subnum);
				file_close(); movie_clean_subs(); return;
			}

			/* save frame number */
			if (_subs_begin==NULL) {
				MEM_CREATE_T(_subs_begin,sizeof(_subs),_subs*);
				_subs_cursor=_subs_begin;
			}
			else {
				MEM_CREATE_T(_subs_cursor->next,sizeof(_subs),_subs*);
				_subs_cursor->next->prev=_subs_cursor;
				_subs_cursor=_subs_cursor->next;
			}
			_subs_cursor->frame=frame;

			for (;;) {
				memset(data,0,0x100);
				ft=fgets(data,0xfe,fd);

				len=ft?strip_newline(data):0;

				if (len==0) {
					if (subnum_f==0) {
						/* none found */
						LOG(LOG_MISC|LOG_WARNING,"couldn't read subs frame %d\n",subnum);
						file_close(); movie_clean_subs(); return;
					}

					break; /* done */
				}
				subnum_f++; d_id=d_c=d_y=0;

				if (len>0xc0) {
					/* string too large */
					LOG(LOG_MISC|LOG_WARNING,"couldn't read subs %d,%d\n",subnum,subnum_f);
					file_close(); movie_clean_subs(); return;
				}

				if (len>10) {
					/* subs vars */
					char c[11];
					memcpy(c,data,10); c[10]=0;
					if (c[0]=='<'&&c[3]==','&&c[6]==','&&c[9]=='>') {
						/* <xx,xx,xx> */
						d_id=-1; d_c=-1; d_y=-1;
						c[0]=c[3]=c[6]=c[9]=' ';
						sscanf(c," %d %d %d",&d_id,&d_c,&d_y);
						if (d_id<0||d_c<0||d_y<0||d_id>31||d_c>LC_MAX||d_y>31) {
							/* scanf error/out of bounds */
							LOG(LOG_MISC|LOG_WARNING,"couldn't read subs %d,%d\n",subnum,subnum_f);
							file_close(); movie_clean_subs(); return;
						}

						memmove(data,data+10,len-10);
						len-=10; data[len]=0;
					}
				}

				/* save data */
				if (subp==NULL) {
					MEM_CREATE_T(_subs_cursor->sub,sizeof(_subs_text),_subs_text*);
					subp=_subs_cursor->sub;
				}
				else {
					MEM_CREATE_T(subp->next,sizeof(_subs_text),_subs_text*);
					subp=subp->next;
				}

				/* colour */
				switch (d_c) {
					case 1: subp->colour=LOG_COLOUR(LC_GRAY);	break;
					case 2: subp->colour=LOG_COLOUR(LC_BLACK);	break;
					case 3: subp->colour=LOG_COLOUR(LC_RED);	break;
					case 4: subp->colour=LOG_COLOUR(LC_ORANGE);	break;
					case 5: subp->colour=LOG_COLOUR(LC_YELLOW);	break;
					case 6: subp->colour=LOG_COLOUR(LC_GREEN);	break;
					case 7: subp->colour=LOG_COLOUR(LC_CYAN);	break;
					case 8: subp->colour=LOG_COLOUR(LC_BLUE);	break;
					case 9: subp->colour=LOG_COLOUR(LC_PINK);	break;

					default: subp->colour=LOG_COLOUR(LC_WHITE);	break;
				}

				/* other data */
				subp->y=d_y;
				strcpy(subp->text,data);
				if (d_id&0xf) {
					subp->id=LOG_TYPE((d_id&0xf)<<8);
					if (d_id&0x10) subp->id|=LOG_TYPE(LT_IGNORE);
				}
				else strcat(subp->text,"\n");
			}
			subnum++;
		}

		if (feof(fd)) break;
	}

	file_close();

	if (subnum==1) {
		/* none found */
		LOG(LOG_MISC|LOG_WARNING,"couldn't read movie subtitles\n");
		movie_clean_subs(); return;
	}

	_subs_cursor=_subs_begin;
	has_subs=TRUE;
}


int movie_set_custom_name_open(void)
{
	const char* filter="meisei Movie Files\0*.movie;*.m0;*.m1;*.m2;*.m3;*.m4;*.m5;*.m6;*.m7;*.m8;*.m9;*.ma;*.mb;*.mc;*.md;*.me;*.mf\0All Files\0*.*\0\0";
	const char* title="Open Movie";
	char fn[STRING_SIZE]={0};
	OPENFILENAME of;
	int o;

	movie_reset_custom_name();

	if (state_get_slot()!=STATE_SLOT_CUSTOM) return TRUE;
	if (!msx_is_running()||semaphore||record_overflow||netplay_is_active()||psglogger_is_active()) return FALSE; /* same check as movie_play */

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

int movie_confirm_exit(void)
{
	int ret=IDNO;
	int slot=state_get_slot();

	/* same checks as movie_set_custom_name_save */
	if ((slot!=STATE_SLOT_CUSTOM)&(slot!=STATE_SLOT_MEMORY)||active_state!=A_STATE_RECORD||record_overflow||semaphore) return ret;

	MAIN->dialog=TRUE;

	ret=MessageBox(MAIN->window,"Save movie before exiting?","meisei",MB_ICONEXCLAMATION|MB_YESNOCANCEL);
	MAIN->dialog=FALSE;
	return ret;
}

int movie_set_custom_name_save(int confirm)
{
	int slot=state_get_slot();
	const char* filter="meisei Movie File\0*.movie;*.m0;*.m1;*.m2;*.m3;*.m4;*.m5;*.m6;*.m7;*.m8;*.m9;*.ma;*.mb;*.mc;*.md;*.me;*.mf\0\0";
	const char* defext="movie";
	const char* title="Save Movie As";
	char fn[STRING_SIZE]={0};
	OPENFILENAME of;
	int o;

	movie_reset_custom_name();

	if (slot!=STATE_SLOT_CUSTOM&&slot!=STATE_SLOT_MEMORY) return TRUE;

	/* same check as movie_stop + out of bounds check */
	if (active_state!=A_STATE_RECORD||record_overflow) return TRUE;
	if (semaphore) return FALSE;

	if (slot==STATE_SLOT_MEMORY) {
		/* change from memory slot to custom slot */
		state_set_slot(STATE_SLOT_CUSTOM,TRUE);
		LOG(LOG_MISC|LOG_WARNING,"memory slot is invalid, changed to custom\n");
	}

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
	else {
		/* canceled */
		if (confirm&&MessageBox(MAIN->window,"Stop recording without saving?","meisei",MB_ICONEXCLAMATION|MB_YESNO|MB_DEFBUTTON2)==IDYES) {
			movie_reset_custom_name();
			o=TRUE;
		}
	}

	MAIN->dialog=FALSE;
	return o;
}


/* play/record/stop action */
void movie_play(int stop)
{
	char fn[STRING_SIZE]={0}; char ext[0x10];
	char header[0x10];
	char slotdesc[0x10];
	char conflict[0x100]={0};
	_msx_state msxstate;
	u8* state=NULL; u8* state_rel; u8** s;
	u8* state_b=NULL; u8* state_b_rel; /* backup */
	int fc,i,slot,size,msize;
	_movie* cursor;

	if (!msx_is_running()||semaphore||record_overflow) return;
	semaphore=1;

	if (netplay_is_active()) { LOG(LOG_MISC|LOG_WARNING,"can't play movie during netplay\n"); semaphore=0; return; }
	if (psglogger_is_active()) { LOG(LOG_MISC|LOG_WARNING,"can't play movie during PSG log\n"); semaphore=0; return; }
	if (state_get_slot()==STATE_SLOT_MEMORY) { LOG(LOG_MISC|LOG_WARNING,"couldn't play movie from memory slot\n"); semaphore=0; return; }

	msx_wait();

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

	/* load file */
	slot=state_get_slot();
	state_get_slotdesc(slot,slotdesc);
	size=movie_get_state_size();

	if (slot==STATE_SLOT_CUSTOM) {
		if (strlen(custom_name)) strcpy(fn,custom_name);
		file_setfile(NULL,fn,NULL,NULL);
		if (strlen(custom_name)&&file->dir&&strlen(file->dir)) strcpy(custom_dir,file->dir);
	}
	else {
		mapper_get_current_name(fn);
		sprintf(ext,"m%x",slot);
		file_setfile(&file->statedir,fn,ext,NULL);
		if (file->filename&&strlen(file->filename)) strcpy(fn,file->filename);
	}

	if (!file_open()||file->size>20000000) {
		file_close(); goto loadmovie_error;
	}
	if (file->size<(u32)(size+4)) {
		file_close();
		LOG(LOG_MISC|LOG_WARNING,"movie state size conflict in %s\n",slotdesc);
		goto loadmovie_error_nolog;
	}

	MEM_CREATE_N(state,file->size);
	if (!file_read(state,file->size)) { file_close(); goto loadmovie_error; }
	file_close();

	/* load state */
	state_rel=state; s=&state_rel;

	STATE_LOAD_C(header,12);	if (memcmp(header,STATE_HEADER,12)!=0) goto loadmovie_error;
	STATE_LOAD_1(i);			if (i!=STATE_ID_MOVIE) { LOG(LOG_MISC|LOG_WARNING,"%s is not a movie\n",slotdesc); goto loadmovie_error_nolog; }
	STATE_LOAD_2(i);			if (i!=VERSION_NUMBER_D) { LOG(LOG_MISC|LOG_WARNING,"incompatible meisei version in %s\n",slotdesc); goto loadmovie_error_nolog; }

	/* conflict? */
	if (!msx_state_load(msx_state_get_version(),s,&msxstate,conflict)) { LOG(LOG_MISC|LOG_WARNING,"%s in %s\n",conflict,slotdesc); goto loadmovie_error_nolog; }
	if (msxstate.vdpchip_uid!=vdp_get_chiptype_uid(vdp_get_chiptype())) { LOG(LOG_MISC|LOG_WARNING,"expected VDP %s in %s\n",vdp_get_chiptype_name(vdp_get_uid_chiptype(msxstate.vdpchip_uid)),slotdesc); goto loadmovie_error_nolog; }
	if (msxstate.psgchip_uid!=psg_get_chiptype_uid(psg_get_chiptype())) { LOG(LOG_MISC|LOG_WARNING,"expected PSG %s in %s\n",psg_get_chiptype_name(psg_get_uid_chiptype(msxstate.psgchip_uid)),slotdesc); goto loadmovie_error_nolog; }
	if (strlen(conflict)) LOG(LOG_MISC|LOG_WARNING,"ignored %s\n",conflict); /* ignored conflict? */

	z80_state_load_cur(s);
	vdp_state_load_cur(s);
	cont_state_load_cur(s);
	io_state_load_cur(s);
	psg_state_load_cur(s);
	mapper_state_load_cur(s);

	/* mapper internal error */
	if (mapper_get_mel_error()) {
		LOG(LOG_MISC|LOG_WARNING,"mapper state conflict in %s\n",slotdesc);
		goto loadmovie_error_nolog;
	}

	tape_state_load_cur(s);

	STATE_LOAD_1(i);			if (i!=STATE_ID_END) goto loadmovie_error;
	STATE_LOAD_4(fc);			if (fc==0) goto loadmovie_error;

	msize=1+cont_movie_get_size();
	if ((((file->size-size)-4)%msize)!=(u32)0||file->size<(u32)(size+msize+4)) goto loadmovie_error;

	/* success */
	if (!stop) movie_stop(TRUE); /* counters also set to 0 */
	framecount_max=fc;
	cursor=_movie_begin;
	data_size=msize;
	mstate_size=size;
	active_state=A_STATE_PLAY;
	movie_update_menu();

	MEM_CREATE_N(mstate,mstate_size)
	memcpy(mstate,state,mstate_size);

	/* fill */
	i=((file->size-size)-4)/msize;
	while (i--) {
		MEM_CREATE_N(cursor->data,msize)
		memcpy(cursor->data,*s,msize);
		(*s)+=msize;

		MEM_CREATE_T(cursor->next,sizeof(_movie),_movie*);
		cursor->next->prev=cursor;
		cursor=cursor->next;
	}
	cursor=cursor->prev; MEM_CLEAN(cursor->next); /* clean unused last one */

	near_count=_movie_begin->data[0]+1;

	MEM_CLEAN(state);
	MEM_CLEAN(state_b);

	/* open subtitles ([moviefile].txt) */
	strcat(fn,".txt");
	file_setfile(NULL,fn,NULL,NULL);
	movie_open_subs(msxstate.flags&MSX_FLAG_VF);

	if (msx_get_paused()&1) SendMessage(MAIN->window,WM_COMMAND,IDM_PAUSEEMU,0); /* force unpause */
	LOG(LOG_MISC|LOG_COLOUR(LC_GREEN),"started movie playback from %s\n",slotdesc);

	/* (don't care about region or 50/60hz hack flag for now) */
	i=z80_get_cycles(); /* changes at crystal_set_mode */
	draw_set_vidformat((msxstate.flags&MSX_FLAG_VF)!=0);
	crystal->overclock=msxstate.cpu_speed;
	crystal_set_mode(TRUE);
	z80_set_cycles(i);

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
loadmovie_error:

	LOG(LOG_MISC|LOG_WARNING,"couldn't play movie from %s\n",slotdesc);

loadmovie_error_nolog:

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

void movie_record(void)
{
	if (!msx_is_running()||semaphore||active_state==A_STATE_RECORD||record_overflow) return;
	semaphore=1;

	msx_wait();

	switch (active_state) {

		case A_STATE_PLAY: {
			_movie* cursor=_movie_cursor->next;

			/* clean */
			movie_clean_subs();
			if (cursor&&cursor->next) {
				for (;;) {
					MEM_CLEAN(cursor->data);
					cursor=cursor->next;
					MEM_CLEAN(cursor->prev);
					if (cursor->next==NULL) {
						MEM_CLEAN(cursor->data);
						MEM_CLEAN(cursor);
						break;
					}
				}
				_movie_cursor->next=NULL;
			}
			if (_movie_cursor->next) {
				MEM_CLEAN(_movie_cursor->next->data);
				MEM_CLEAN(_movie_cursor->next);
			}

			_movie_cursor->data[0]=(_movie_cursor->data[0]+1)-near_count;
			near_count=_movie_cursor->data[0];
			LOG(LOG_MISC|LOG_COLOUR(LC_GREEN),"continued movie recording at frame %d\n",framecount);

			break;
		}

		case A_STATE_RECORD: break; /* won't get here */

		case A_STATE_STOP: {
			char header[0x10];
			u8* state_rel; u8** s;

			mstate_size=movie_get_state_size();
			data_size=1+cont_movie_get_size();

			/* save state */
			MEM_CREATE_N(mstate,mstate_size)
			state_rel=mstate; s=&state_rel;
			sprintf(header,STATE_HEADER);
			STATE_SAVE_C(header,12);
			STATE_SAVE_1(STATE_ID_MOVIE);
			STATE_SAVE_2(VERSION_NUMBER_D);

			msx_state_save(s);
			z80_state_save(s);
			vdp_state_save(s);
			cont_state_save(s);
			io_state_save(s);
			psg_state_save(s);
			mapper_state_save(s);
			tape_state_save(s);

			STATE_SAVE_1(STATE_ID_END);

			LOG(LOG_MISC|LOG_COLOUR(LC_GREEN),"started movie recording\n");

			break;
		}

		default: break;
	}

	active_state=A_STATE_RECORD;
	movie_update_menu();

	msx_wait_done();
	semaphore=0;
}

void movie_stop(int internal)
{
	if (!internal) {
		if (semaphore) return;
		semaphore=1;
		msx_wait();
	}

	switch (active_state) {

		case A_STATE_PLAY:
			movie_clean_subs();
			LOG(LOG_MISC|LOG_COLOUR(LC_GREEN),"movie playback stopped at frame %d\n",framecount);
			break;

		case A_STATE_RECORD: {
			char fn[STRING_SIZE]; char ext[0x10];
			_movie* cursor=_movie_begin;
			int slot=state_get_slot();
			char slotdesc[0x10];
			state_get_slotdesc(slot,slotdesc);

			if (record_overflow) {
				/* overflow */
				LOG(LOG_MISC|LOG_WARNING,"movie recording stopped, out of bounds\n");
				break;
			}

			if (slot==STATE_SLOT_MEMORY) {
				/* shouldn't normally get here */
				LOG(LOG_MISC|LOG_WARNING,"couldn't save movie to %s\n",slotdesc);
				break;
			}

			/* get filename */
			if (slot==STATE_SLOT_CUSTOM) {
				if (strlen(custom_name)) {
					file_setfile(NULL,custom_name,NULL,NULL);
					if (file->dir&&strlen(file->dir)) strcpy(custom_dir,file->dir);
				}
				else {
					/* cancel when it asks for a filename, or quit meisei during recording */
					LOG(LOG_MISC|LOG_WARNING,"movie recording stopped without saving\n");
					break;
				}
			}
			else {
				mapper_get_current_name(fn);
				sprintf(ext,"m%x",slot);
				file_setfile(&file->statedir,fn,ext,NULL);
			}

			/* save file */
			if (!file_save()||mstate_size==0||mstate==NULL||data_size==0||_movie_cursor->data==NULL||!file_write(mstate,mstate_size)||!file_write((u8*)&framecount,4)) {
				LOG(LOG_MISC|LOG_WARNING,"couldn't save movie to %s\n",slotdesc);
				file_close(); break;
			}

			for (;;) {
				if (!file_write(cursor->data,data_size)) { LOG(LOG_MISC|LOG_WARNING,"couldn't save movie to %s\n",slotdesc); break; }
				if (cursor->next==NULL) { LOG(LOG_MISC|LOG_COLOUR(LC_GREEN),"saved movie to %s, %d frames\n",slotdesc,framecount); break; }
				cursor=cursor->next;
			}
			file_close();

			break;
		}

		case A_STATE_STOP: break;

		default: break;
	}

	/* clean */
	for (;;) {
		MEM_CLEAN(_movie_cursor->data);
		MEM_CLEAN(_movie_cursor->next);
		if (_movie_cursor->prev==NULL) break;
		_movie_cursor=_movie_cursor->prev;
	}
	MEM_CLEAN(mstate);
	record_overflow=framecount=framecount_max=near_count=far_count=mstate_size=data_size=0;

	active_state=A_STATE_STOP;
	movie_update_menu();

	if (!internal) {
		msx_wait_done();
		semaphore=0;
	}
}


/* frame */
void __fastcall movie_frame(void)
{
	int fps=crystal->mode?50:60;

	if (record_overflow) {
		/* overflow, make sure it gets handled */
		PostMessage(MAIN->window,WM_COMMAND,IDM_MOVIE_STOP,0);
		return;
	}

	switch (active_state) {

		case A_STATE_PLAY:
			/* next? */
			if (!--near_count) {
				/* reached end */
				if (_movie_cursor->next==NULL) {
					movie_stop(TRUE);
					break;
				}

				_movie_cursor=_movie_cursor->next;
				near_count=_movie_cursor->data[0];
				far_count++;
			}

			cont_movie_put(_movie_cursor->data+1);

			framecount++;

			/* show subtitles */
			if (has_subs) {
				if (_subs_cursor->prev&&_subs_cursor->prev->frame>=framecount) _subs_cursor=_subs_cursor->prev;

				if (_subs_cursor->frame==framecount) {
					_subs_text* subp=_subs_cursor->sub;
					if (_subs_cursor->next) _subs_cursor=_subs_cursor->next;

					for (;;) {
						if (subp==NULL) break;

						/* y */
						if (subp->y) {
							int y=subp->y;
							int id_dec=LOG_TYPE(255);
							while (y--) {
								if (subp->id) {
									LOG(LOG_MISC|subp->id|id_dec|subp->colour," ");
									id_dec-=LOG_TYPE(1);
								}
								else LOG(LOG_MISC|subp->colour," \n");
							}
						}

						LOG(LOG_MISC|LOG_NOCHOP|subp->id|subp->colour,"%s",subp->text);

						subp=subp->next;
					}
				}
			}

			if (timecode) LOG(LOG_MISC|LOG_COLOUR(LC_ORANGE)|LOG_TYPE(LT_MOVIETIME),"TC: %02d:%02d:%02d;%02d / %02d:%02d:%02d;%02d    ",framecount/(fps*3600),(framecount%(fps*3600))/(fps*60),(framecount%(fps*60))/fps,framecount%fps,framecount_max/(fps*3600),(framecount_max%(fps*3600))/(fps*60),(framecount_max%(fps*60))/fps,framecount_max%fps);

			break;

		case A_STATE_RECORD: {
			u8 c[CONT_MAX_MOVIE_SIZE];
			cont_movie_get(c);

			/* begin? */
			if (_movie_cursor->data==NULL) {
				MEM_CREATE(_movie_cursor->data,data_size);
				memcpy(_movie_cursor->data+1,c,data_size-1);
			}

			/* next? */
			if (memcmp(_movie_cursor->data+1,c,data_size-1)!=0||_movie_cursor->data[0]==0xff) {
				MEM_CREATE_T(_movie_cursor->next,sizeof(_movie),_movie*);
				_movie_cursor->next->prev=_movie_cursor;
				_movie_cursor=_movie_cursor->next;

				MEM_CREATE(_movie_cursor->data,data_size);
				memcpy(_movie_cursor->data+1,c,data_size-1);

				far_count++;
			}

			_movie_cursor->data[0]++;
			near_count=_movie_cursor->data[0];

			framecount++;
			if (timecode) LOG(LOG_MISC|LOG_COLOUR(LC_ORANGE)|LOG_TYPE(LT_MOVIETIME),"TC: %02d:%02d:%02d;%02d                  ",framecount/(fps*3600),(framecount%(fps*3600))/(fps*60),(framecount%(fps*60))/fps,framecount%fps);

			/* forward overflow (very unlikely) */
			if (far_count>=5000000) {
				record_overflow=TRUE;
				PostMessage(MAIN->window,WM_COMMAND,IDM_MOVIE_STOP,0);
			}

			break;
		}

		case A_STATE_STOP: break;

		default: break;
	}
}

void __fastcall movie_reverse_frame(void)
{
	if (record_overflow) return;

	switch (active_state) {

		case A_STATE_PLAY:
			/* prev? */
			if (near_count==_movie_cursor->data[0]) {
				_movie_cursor=_movie_cursor->prev;
				near_count=0;

				far_count--;
			}

			near_count++;
			framecount--;

			break;

		case A_STATE_RECORD:
			/* backward overflow */
			if (framecount<=1) {
				record_overflow=TRUE;
				PostMessage(MAIN->window,WM_COMMAND,IDM_MOVIE_STOP,0);
				break;
			}

			/* prev? */
			if (!--_movie_cursor->data[0]) {
				MEM_CLEAN(_movie_cursor->data);
				_movie_cursor=_movie_cursor->prev;
				MEM_CLEAN(_movie_cursor->next);

				far_count--;
			}

			near_count=_movie_cursor->data[0];
			framecount--;

			break;

		case A_STATE_STOP: break;

		default: break;
	}
}


void movie_init(void)
{
	int i;

	mstate=NULL;
	_subs_begin=_subs_cursor=NULL;
	record_overflow=has_subs=semaphore=framecount=framecount_max=near_count=far_count=mstate_size=data_size=0;

	MEM_CREATE_T(_movie_begin,sizeof(_movie),_movie*);
	_movie_cursor=_movie_begin;

	i=settings_get_yesnoauto(SETTINGS_TIMECODE);
	if (i!=FALSE&&i!=TRUE) i=FALSE;
	movie_set_timecode(i);

	open_fi=1;
	SETTINGS_GET_INT(settings_info(SETTINGS_FILTERINDEX_MOVIE),&open_fi);
	CLAMP(open_fi,1,2);

	active_state=A_STATE_STOP;
	movie_update_menu();
}

void movie_clean(void)
{
	movie_stop(TRUE);
	movie_clean_subs();

	MEM_CLEAN(_movie_begin);
	_movie_cursor=NULL;
}
