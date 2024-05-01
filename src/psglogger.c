/******************************************************************************
 *                                                                            *
 *                              "psglogger.c"                                 *
 *                                                                            *
 ******************************************************************************/

#include <windows.h>

#include "global.h"
#include "psg.h"
#include "main.h"
#include "movie.h"
#include "msx.h"
#include "mapper.h"
#include "settings.h"
#include "file.h"
#include "resource.h"
#include "crystal.h"
#include "psgtoy.h"
#include "psglogger.h"

static const int psg_mask[0x10]={0xff,0xf,0xff,0xf,0xff,0xf,0x1f,0x3f,0x1f,0x1f,0x1f,0xff,0xff,0xf,0,0};

#define FRAME_MAX 1000000 /* about 5hrs */

typedef struct _psglog {
	u8 regs[0x10];
	struct _psglog* next;
	struct _psglog* prev;
} _psglog;
static _psglog* _psglog_begin;
static _psglog* _psglog_cursor;

static int psglogger_fi;
static char psglogger_fn[STRING_SIZE]={0};
static char psglogger_dir[STRING_SIZE]={0};
static char psglogger_song[STRING_SIZE]={0};
static char psglogger_author[STRING_SIZE]={0};
static char psglogger_comment[STRING_SIZE]={0};

static int is_active=FALSE;
static int interleaved;
static int trimsilence;
static int cur_frame=0;

int __fastcall psglogger_is_active(void) { return is_active; }
int psglogger_get_interleaved(void) { return interleaved; }
int psglogger_get_trimsilence(void) { return trimsilence; }
int psglogger_get_fi(void) { return psglogger_fi; }


/* save window (attached to openfilename dialog) */
static UINT_PTR CALLBACK psglogger_save_hook(HWND dialog,UINT msg,WPARAM wParam,LPARAM lParam)
{
    wParam = wParam;
    lParam = lParam;
	switch (msg) {
		case WM_INITDIALOG:
			/* init checkboxes */
			CheckDlgButton(dialog,IDC_YMLOGSAVE_INTERLEAVED,interleaved?BST_CHECKED:BST_UNCHECKED);
			CheckDlgButton(dialog,IDC_YMLOGSAVE_TRIMSILENCE,trimsilence?BST_CHECKED:BST_UNCHECKED);

			/* init editboxes */
			SendDlgItemMessage(dialog,IDC_YMLOGSAVE_SONG,EM_LIMITTEXT,125,0);
			SendDlgItemMessage(dialog,IDC_YMLOGSAVE_AUTHOR,EM_LIMITTEXT,125,0);
			SendDlgItemMessage(dialog,IDC_YMLOGSAVE_COMMENT,EM_LIMITTEXT,125,0);

			/* disable controls on bin/midi filetype */
			if (psglogger_fi!=PSGLOGGER_TYPE_YM) {
				EnableWindow(GetDlgItem(dialog,IDC_YMLOGSAVE_INTERLEAVED),FALSE);
				EnableWindow(GetDlgItem(dialog,IDC_YMLOGSAVE_SONG),FALSE);
				EnableWindow(GetDlgItem(dialog,IDC_YMLOGSAVE_AUTHOR),FALSE);
				EnableWindow(GetDlgItem(dialog,IDC_YMLOGSAVE_COMMENT),FALSE);
			}

			break;

		case WM_NOTIFY:

			switch (((LPOFNOTIFY)lParam)->hdr.code) {

				/* file type is changed */
				case CDN_TYPECHANGE: {
					int fi=((LPOFNOTIFY)lParam)->lpOFN->nFilterIndex;

					EnableWindow(GetDlgItem(dialog,IDC_YMLOGSAVE_INTERLEAVED),fi==PSGLOGGER_TYPE_YM);
					EnableWindow(GetDlgItem(dialog,IDC_YMLOGSAVE_SONG),fi==PSGLOGGER_TYPE_YM);
					EnableWindow(GetDlgItem(dialog,IDC_YMLOGSAVE_AUTHOR),fi==PSGLOGGER_TYPE_YM);
					EnableWindow(GetDlgItem(dialog,IDC_YMLOGSAVE_COMMENT),fi==PSGLOGGER_TYPE_YM);
					break;
				}

				/* exit dialog affirmatively */
				case CDN_FILEOK:
					interleaved=(IsDlgButtonChecked(dialog,IDC_YMLOGSAVE_INTERLEAVED)==BST_CHECKED);
					trimsilence=(IsDlgButtonChecked(dialog,IDC_YMLOGSAVE_TRIMSILENCE)==BST_CHECKED);

					if (!GetDlgItemText(dialog,IDC_YMLOGSAVE_SONG,psglogger_song,126)) psglogger_song[0]=0;
					if (!GetDlgItemText(dialog,IDC_YMLOGSAVE_AUTHOR,psglogger_author,126)) psglogger_author[0]=0;
					if (!GetDlgItemText(dialog,IDC_YMLOGSAVE_COMMENT,psglogger_comment,126)) psglogger_comment[0]=0;
					break;

				default: break;
			}

			break;

		default: break;
	}

	return 0;
}

/* start/stop */
static void psglogger_clean_frames(void)
{
	/* go to end */
	for (;;) {
		if (_psglog_cursor->next) _psglog_cursor=_psglog_cursor->next;
		else break;
	}

	/* clean from end to start */
	for (;;) {
		_psglog_cursor=_psglog_cursor->prev;
		if (_psglog_cursor->next) { MEM_CLEAN(_psglog_cursor->next); }
		else break;
	}

	cur_frame=0;
}

int psglogger_start(HWND dialog,HWND* dt)
{
	/* only called from psgtoy */
	const char* filter="Binary Stream (*.bin)\0*.bin\0MIDI File (*.mid)\0*.mid\0YM File (*.ym)\0*.ym\0\0";
	const char* defext="ym";
	const char* title="Start PSG Logger";
	char fn[STRING_SIZE]={0};
	OPENFILENAME of;
	int ret=0;

	if (is_active||!msx_is_running()||movie_is_playing()||!*dt) return ret;

	memset(&of,0,sizeof(OPENFILENAME));

	of.lStructSize=sizeof(OPENFILENAME);
	of.hwndOwner=dialog;
	of.hInstance=MAIN->module;
	of.lpstrFile=fn;
	of.lpstrDefExt=defext;
	of.lpstrFilter=filter;
	of.lpstrTitle=title;
	of.nMaxFile=STRING_SIZE;
	of.nFilterIndex=psglogger_fi;
	of.Flags=OFN_ENABLESIZING|OFN_EXPLORER|OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_ENABLETEMPLATE|OFN_ENABLEHOOK;
	of.lpstrInitialDir=strlen(psglogger_dir)?psglogger_dir:file->tooldir?file->tooldir:file->appdir;
	of.lpTemplateName=MAKEINTRESOURCE(IDD_PSGLOG_SAVE);
	of.lpfnHook=psglogger_save_hook;

	mapper_get_current_name(fn);

	main_menu_enable(IDM_PSGTOY,FALSE); /* resource leak if forced to close */
	if (GetSaveFileName(&of)) {
		if (*dt) {
			psglogger_fi=of.nFilterIndex;

			strcpy(psglogger_fn,fn);
			if (strlen(fn)&&of.nFileOffset) {
				fn[of.nFileOffset]=0; strcpy(psglogger_dir,fn);
			}

			psglogger_clean_frames();
			is_active=TRUE;
			main_menu_update_enable();

			ret=1;
		}
		else ret=-1;
	}

	main_menu_enable(IDM_PSGTOY,TRUE);

	return ret;
}

int psglogger_stop(int save)
{
	int ret=0;

	if (!is_active||movie_is_playing()) return ret;

	is_active=FALSE;
	main_menu_update_enable();

	if (cur_frame<=FRAME_MAX&&cur_frame>2&&save) {
		FILE* fd=NULL;
		int o=0,i,j,fps=crystal->mode?50:60;
		_psglog* c=_psglog_begin,*cs;
		u8* data=NULL;
		int start=0,end=cur_frame,total,size;
		MEM_CREATE_N(data,cur_frame*28+0x10000); /* enough for both (8+8+8+4=28=midi trk max, $10=ym max) */

		#define SET16(v) data[o++]=(v)>>8&0xff; data[o++]=(v)&0xff
		#define SET24(v) data[o++]=(v)>>16&0xff; data[o++]=(v)>>8&0xff; data[o++]=(v)&0xff
		#define SET32(v) data[o++]=(v)>>24&0xff; data[o++]=(v)>>16&0xff; data[o++]=(v)>>8&0xff; data[o++]=(v)&0xff


		#define NO_LOOP FALSE /* trim start and a bit of end, for easy creation of songs without a loop point, ending with silence */


		/* trim silence */
		if (trimsilence) {
			/* start */
			for (;;) {
				if (c&&c!=_psglog_cursor&&((((c->regs[8]|c->regs[9]|c->regs[10])&0x1f)==0)|(((~c->regs[7]&0x3f))==0x3f)|((((((c->regs[8]|c->regs[9]|c->regs[10])&0x10)==0)&(c->regs[1]|c->regs[3]|c->regs[5]))==0)&((c->regs[0]|c->regs[2]|c->regs[4])<8)))) {
					start++;
					c=c->next;
				}
				else break;
			}

			/* end */
			for (;;) {
				#define C _psglog_cursor->prev
				if (C!=_psglog_begin&&((((C->regs[8]|C->regs[9]|C->regs[10])&0x1f)==0)|(((~C->regs[7]&0x3f))==0x3f)|((((((C->regs[8]|C->regs[9]|C->regs[10])&0x10)==0)&(C->regs[1]|C->regs[3]|C->regs[5]))==0)&((C->regs[0]|C->regs[2]|C->regs[4])<8)))) {
					end--;
					_psglog_cursor=C;
				}
				else break;
				#undef C
			}
#if NO_LOOP
			for (i=0;i<4;i++) if (_psglog_cursor->next) {
				end++;
				_psglog_cursor=_psglog_cursor->next;
			}
#endif
		}

		cs=c;
		total=end-start;
		if (total<=2) ret=2;
		else {
			if (psglogger_fi==PSGLOGGER_TYPE_YM) {
				/* YM */
				if (!strlen(psglogger_song)) sprintf(psglogger_song,"Untitled");
				if (!strlen(psglogger_author)) sprintf(psglogger_author,"Unknown");
				if (!strlen(psglogger_comment)) sprintf(psglogger_comment,"no comment");
				psglogger_song[126]=psglogger_author[126]=psglogger_comment[126]=0;

				o=0;

				/* header */
				memcpy(data,"YM6!LeOnArD!",12); o+=12; /* sig */
				SET32(0);			/* number of frames (later) */
				SET32(interleaved);	/* song attributes */
				SET16(0);			/* number of digidrums */
				SET32(1789773);		/* YM clock */
				SET16(fps);			/* frames per second */
				SET32(0);			/* loop frame */
				SET16(0);			/* reserved */

				i=strlen(psglogger_song)+1; memcpy(data+o,psglogger_song,i); o+=i;
				i=strlen(psglogger_author)+1; memcpy(data+o,psglogger_author,i); o+=i;
				i=strlen(psglogger_comment)+1; memcpy(data+o,psglogger_comment,i); o+=i;

				/* frames */
				if (interleaved) {
					int ot[0x10];
					i=0x10; while (i--) ot[i]=o+total*i;
					for (;;) {
						i=0x10; while (i--) data[ot[i]++]=c->regs[i];
						c=c->next; if (c==_psglog_cursor) break;
					}
					o+=(total*0x10);
				}
				else {
					for (;;) {
						memcpy(data+o,c->regs,0x10); o+=0x10;
						c=c->next; if (c==_psglog_cursor) break;
					}
				}

				memcpy(data+o,"End!",4);
				size=o+4;

				/* write number of frames */
				o=12; SET32(total);
#if NO_LOOP
				/* write loop frame */
				o=28; SET32(total-3);
#endif
			}
			else if (psglogger_fi==PSGLOGGER_TYPE_MID) {
				/* MID */
				int oc,note,note_prev,vol,cc,dt;
				u32 ds;

				o=0;

				/* header */
				memcpy(data,"MThd",4); o+=4;
				SET32(6); SET16(1);
				SET16(5); /* 3 tone + noise + tempo */
				SET16(32); /* 32 ticks per quarter note */

				/* tempo track */
				memcpy(data+o,"MTrk",4); o+=4;
				SET32(11);
				data[o++]=0; data[o++]=0xff; data[o++]=0x51; data[o++]=3; i=(fps==50)?637972:534021; SET24(i); /* tempo, (32/exact fps)*1000000 */
				data[o++]=0; data[o++]=0xff; data[o++]=0x2f; data[o++]=0; /* end of track */

				/* variable length, max 4 bytes */
				#define WRITE_DELTA()						\
					ds=(dt<<3&0x7f000000)|(dt<<2&0x7f0000)|(dt<<1&0x7f00)|(dt&0x7f)|0x80808000; \
					if (ds&0x7f000000) { SET32(ds); }		\
					else if (ds&0x7f0000) { SET24(ds); }	\
					else if (ds&0x7f00) { SET16(ds); }		\
					else data[o++]=ds

				/* tone channel tracks */
				for (i=0;i<3;i++) {
					memcpy(data+o,"MTrk",4); o+=4;
					oc=o; SET32(0); /* chunk size, unknown yet */

					dt=note_prev=0; cc=i<<1;

					/* track name */
					data[o++]=0; data[o++]=0xff; data[o++]=3; data[o++]=9;
					memcpy(data+o,"Channel ",8); o+=8;
					data[o++]='A'+i;

					/* change instrument to square lead */
					data[o++]=0; data[o++]=0xc0|cc; data[o++]=80; /* channel 0 */
					data[o++]=0; data[o++]=0xc1|cc; data[o++]=80; /* channel 1 */

					c=cs;

					/* notes */
					for (;;) {
						j=(c->regs[i<<1|1]<<8&0xf00)|c->regs[i<<1]; /* frequency */
						note=psgtoy_get_act_notepos(j);
						if (c->regs[8+i]&0x10) vol=0xf; /* envelope, use max volume */
						else vol=c->regs[8+i]&0xf;
						if (c->regs[7]>>i&1||!vol||j<8) note=0; /* tone off */
						else note=(((note>>4)*12)+(note&0xf)+12)|vol<<11;

						/* new midi event */
						if (note!=note_prev) {

							WRITE_DELTA();

							if (note_prev==0) {
								/* note on */
								data[o++]=0x90|cc; data[o++]=note&0x7f; data[o++]=note>>8;
							}
							else if (note==0) {
								/* note off */
								data[o++]=0x80|cc; data[o++]=note_prev&0x7f; data[o++]=0x7f;

								cc=((cc+1)&1)|i<<1;
							}
							else if ((note&0x7f)==(note_prev&0x7f)) {
								/* change volume with aftertouch, though it seems not to be supported well */
								data[o++]=0xa0|cc; data[o++]=note&0x7f; data[o++]=note>>8; /* note aftertouch */
								/* data[o++]=0xd0|cc; data[o++]=note>>8; */ /* channel aftertouch (shorter, applies all?) */
							}
							else {
								/* change note */
								data[o++]=0x80|cc; data[o++]=note_prev&0x7f; data[o++]=0x7f; /* off */

								cc=((cc+1)&1)|i<<1;
								data[o++]=0; data[o++]=0x90|cc; data[o++]=note&0x7f; data[o++]=note>>8; /* on */
							}

							note_prev=note; dt=0;
						}

						dt++; c=c->next; if (c==_psglog_cursor) break;
					}

					if (note_prev) { WRITE_DELTA(); data[o++]=0x80|cc; data[o++]=note_prev&0x7f; data[o++]=0x7f; }
					dt++;

					/* end of track */
					WRITE_DELTA(); data[o++]=0xff; data[o++]=0x2f; data[o++]=0;

					/* write chunk size */
					j=o; o=oc; oc=(j-oc)-4; SET32(oc); o=j;
				}


				/* noise track */
				memcpy(data+o,"MTrk",4); o+=4;
				oc=o; SET32(0);

				/* track name */
				data[o++]=0; data[o++]=0xff; data[o++]=3; data[o++]=5;
				memcpy(data+o,"Noise",5); o+=5;

				dt=note_prev=0; c=cs;

				for (;;) {
					if (!(c->regs[7]&8)&&c->regs[8]) note=1;
					else if (!(c->regs[7]&0x10)&&c->regs[9]) note=1;
					else if (!(c->regs[7]&0x20)&&c->regs[10]) note=1;
					else note=0;

					if (note!=note_prev) {

						WRITE_DELTA();

						if (note) {
							/* drum start */
							data[o++]=0x99; data[o++]=42; data[o++]=0x7f;
						}
						else {
							/* drum end */
							data[o++]=0x89; data[o++]=42; data[o++]=0x7f;
						}

						note_prev=note; dt=0;
					}

					dt++; c=c->next; if (c==_psglog_cursor) break;
				}

				if (note_prev) { WRITE_DELTA(); data[o++]=0x89; data[o++]=42; data[o++]=0x7f; }
				dt++;

				/* end of track */
				WRITE_DELTA(); data[o++]=0xff; data[o++]=0x2f; data[o++]=0;

				/* write chunk size */
				j=o; o=oc; oc=(j-oc)-4; SET32(oc); o=j;

				#undef WRITE_DELTA

				size=o;
			}
			else {
				/* BIN */
				o=0;

				for (;;) {
					memcpy(data+o,c->regs,0x10); o+=0x10;
					c=c->next; if (c==_psglog_cursor) break;
				}

				size=o;
			}

			/* save */
			if (!strlen(psglogger_fn)||!file_save_custom(&fd,psglogger_fn)||!file_write_custom(fd,data,size)) ret=1;
			else ret=total;
			file_close_custom(fd);
		}

		MEM_CLEAN(data);

		#undef SET16
		#undef SET24
		#undef SET32
		#undef NO_LOOP
	}
	else ret=(save!=0);

	psglogger_clean_frames();

	return ret; /* 1: gen error, 2: silence error, >2 frames written */
}


/* frame */
void __fastcall psglogger_frame(void)
{
	int i;

	if (!is_active||cur_frame>FRAME_MAX) return;

	i=0x10;
	while (i--) _psglog_cursor->regs[i]=psg_get_reg(i)&psg_mask[i];

	MEM_CREATE_T(_psglog_cursor->next,sizeof(_psglog),_psglog*);
	_psglog_cursor->next->prev=_psglog_cursor;
	_psglog_cursor=_psglog_cursor->next;

	cur_frame++;
}

void __fastcall psglogger_reverse_2frames(void)
{
	if (!is_active||cur_frame>FRAME_MAX) return;

	_psglog_cursor=_psglog_cursor->prev; MEM_CLEAN(_psglog_cursor->next);
	_psglog_cursor=_psglog_cursor->prev; MEM_CLEAN(_psglog_cursor->next);

	cur_frame-=2;
	if (cur_frame<0) cur_frame=0;
}


/* init/clean */
void psglogger_init(void)
{
	int i;

	trimsilence=TRUE; i=settings_get_yesnoauto(SETTINGS_YMLOGTRIMSILENCE); if (i==FALSE||i==TRUE) trimsilence=i;
	interleaved=TRUE; i=settings_get_yesnoauto(SETTINGS_YMLOGINTERLEAVED); if (i==FALSE||i==TRUE) interleaved=i;
	psglogger_fi=PSGLOGGER_TYPE_BIN; SETTINGS_GET_INT(settings_info(SETTINGS_FILTERINDEX_PSGLOG),&psglogger_fi); CLAMP(psglogger_fi,1,3);

	memset(psglogger_fn,0,STRING_SIZE);
	memset(psglogger_song,0,STRING_SIZE);
	memset(psglogger_author,0,STRING_SIZE);
	memset(psglogger_comment,0,STRING_SIZE);

	MEM_CREATE_T(_psglog_begin,sizeof(_psglog),_psglog*);
	_psglog_begin->prev=_psglog_begin;
	_psglog_cursor=_psglog_begin;
}

void psglogger_clean(void)
{
	psglogger_stop(FALSE);
	psglogger_clean_frames();

	MEM_CLEAN(_psglog_begin);
	_psglog_cursor=NULL;
}
