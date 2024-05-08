/******************************************************************************
 *                                                                            *
 *                                "media.c"                                   *
 *                                                                            *
 ******************************************************************************/

#include <windows.h>
#include <shlobj.h>

#include "global.h"
#include "settings.h"
#include "resource.h"
#include "main.h"
#include "file.h"
#include "media.h"
#include "mapper.h"
#include "msx.h"
#include "netplay.h"
#include "draw.h"
#include "crystal.h"
#include "movie.h"
#include "psg.h"
#include "psglogger.h"
#include "tape.h"
#include "reverse.h"
#include "vdp.h"

enum {
	MEDIA_BIOS=0,
	MEDIA_CART1,
	MEDIA_CART2,
	MEDIA_TAPE,

	MEDIA_MAX
};

enum {
	MEDIA_TAB_CARTS=0,
	MEDIA_TAB_TAPE,
	MEDIA_TAB_INTERNAL,

	MEDIA_TAB_MAX
};

static struct {
	int fi_bios;
	int fi_cart_set;
	int fi_cart[2];
	int fi_tape;

	int current_tab;
	int prev_tab;

	char cart_fn[2][STRING_SIZE];
	char tape_fn[STRING_SIZE];
	char bios_fn[STRING_SIZE];

	int last_offset;
	int shift_open;
	int shift_cart[2];

	u32 tape_crc_prev;
	int tape_insertion_changed;
} media;

int media_get_filterindex_bios(void) { return media.fi_bios; }
int media_get_filterindex_tape(void) { return media.fi_tape; }
int media_get_filterindex_cart(void) { return media.fi_cart_set; }

void media_init(void)
{
	memset(&media,0,sizeof(media));

	/* file filter index settings */
	media.fi_bios=1;		SETTINGS_GET_INT(settings_info(SETTINGS_FILTERINDEX_BIOS),&media.fi_bios);		CLAMP(media.fi_bios,1,2);
	media.fi_tape=1;		SETTINGS_GET_INT(settings_info(SETTINGS_FILTERINDEX_TAPE),&media.fi_tape);		CLAMP(media.fi_tape,1,2);
	media.fi_cart_set=1;	SETTINGS_GET_INT(settings_info(SETTINGS_FILTERINDEX_CART),&media.fi_cart_set);	CLAMP(media.fi_cart_set,1,3);
	media.fi_cart[0]=media.fi_cart[1]=media.fi_cart_set;
}


int media_open_single(const char* fn)
{
	/* try cas */
	file_setfile(NULL,fn,NULL,"cas");
	if (file->ext&&(strlen(file->ext)==3)&&(stricmp(file->ext,"cas")==0)) {
		tape_cur_to_od();
		tape_flush_cache_od();
		if (!tape_save_od()) {
            LOG(
                LOG_MISC|LOG_WARNING,
            #ifdef MEISEI_ESP
                "no se pudo guardar la cinta\n"
            #else
                "couldn't save tape\n"
            #endif
            );
		}

		if (tape_open_od(TRUE)) {
			char c[STRING_SIZE];

			media.prev_tab=MEDIA_TAB_TAPE;

			reverse_invalidate();
			tape_od_to_cur();
			tape_set_runtime_block_inserted(FALSE);
			tape_log_info();
			mapper_update_bios_hack();

			main_titlebar(mapper_get_current_name(c));

			return 2;
		}

		tape_clean_od();
	}

	/* try rom */
	file_setfile(NULL,fn,NULL,"rom");
	if (mapper_open_cart(0,fn,TRUE)) {
		char c[STRING_SIZE];

		media.prev_tab=MEDIA_TAB_CARTS;

		mapper_set_carttype(0,mapper_autodetect_cart(0,fn));

		main_titlebar(mapper_get_current_name(c));

		mapper_log_type(0,FALSE);
		LOG(LOG_MISC,"\n");

		return 1;
	}

	return 0;
}

void media_drop(WPARAM p)
{
	int i;

	char fn[STRING_SIZE]={0};
	HDROP drop=(HDROP)p;
	UINT numfiles=DragQueryFile(drop,~0,NULL,0);

	if (netplay_is_active()) { LOG(LOG_MISC|LOG_WARNING,"can't open files during netplay\n"); return; }
	if (movie_get_active_state()) { LOG(LOG_MISC|LOG_WARNING,"can't open files during movie\n"); return; }
	if (psglogger_is_active()) { LOG(LOG_MISC|LOG_WARNING,"can't open files during PSG log\n"); return; }

	DragQueryFile(drop,0,fn,STRING_SIZE);
	DragFinish(drop);

	if (numfiles!=1) {
        LOG(
            LOG_MISC|LOG_WARNING,
        #ifdef MEISEI_ESP
            "no se pueden abrir varios archivos\n"
        #else
            "can't open multiple files\n"
        #endif // MEISEI_ESP
        );
        return;
    }

	msx_wait();
	if ((i=media_open_single(fn))) msx_reset(TRUE);
	msx_wait_done();

	SetForegroundWindow(MAIN->window);
}




static int openfilenamedialog(HWND dialog,UINT type)
{
	const char* filter[]={
		"BIOS ROMs (*.rom, *.zip)\0*.rom;*.ri;*.zip\0All Files\0*.*\0\0",
		"All Supported Media\0*.rom;*.ri;*.dsk;*.di1;*.di2;*.zip\0MSX ROMs (*.rom, *.zip)\0*.rom;*.ri;*.zip\0MSX Disks (*.dsk, *.zip)\0*.dsk;*.di1;*.di2;*.zip\0All Files\0*.*\0\0",
		"fMSX CAS Files (*.cas, *.zip)\0*.cas;*.zip\0All Files\0*.*\0\0"
	};
	const char* title[]={
		"Open BIOS",
		"Open Cartridge 1",
		"Open Cartridge 2",
		"Open Tape"
	};
	char fn[STRING_SIZE]={0};
	char dir[STRING_SIZE]={0};
	OPENFILENAME of;
	BOOL o;

	/* type-specific */
	UINT eid=IDC_MEDIA_SLOT1FILE;	/* editbox id */
	char* fn_source=NULL;			/* initial file */
	char* dir_source=file->romdir;	/* initial dir */
	int* fi=&media.fi_cart[0];		/* filterindex */

	media.shift_open=GetAsyncKeyState(VK_SHIFT);

	memset(&of,0,sizeof(OPENFILENAME));

	switch (type) {
		case MEDIA_BIOS:  eid=IDC_MEDIA_BIOSFILE;  fn_source=media.bios_fn;    dir_source=file->biosdir; fi=&media.fi_bios;    of.lpstrFilter=filter[0]; of.lpstrTitle=title[0]; break;
		case MEDIA_CART1: eid=IDC_MEDIA_SLOT1FILE; fn_source=media.cart_fn[0]; dir_source=file->romdir;  fi=&media.fi_cart[0]; of.lpstrFilter=filter[1]; of.lpstrTitle=title[1]; break;
		case MEDIA_CART2: eid=IDC_MEDIA_SLOT2FILE; fn_source=media.cart_fn[1]; dir_source=file->romdir;  fi=&media.fi_cart[1]; of.lpstrFilter=filter[1]; of.lpstrTitle=title[2]; break;
		case MEDIA_TAPE:  eid=IDC_MEDIA_TAPEFILE;  fn_source=media.tape_fn;    dir_source=file->casdir;  fi=&media.fi_tape;    of.lpstrFilter=filter[2]; of.lpstrTitle=title[3]; break;

		default: break;
	}

	/* check+set initial file/dir */
	if (GetDlgItemText(dialog,eid,fn,STRING_SIZE)) {
		int i,len=strlen(fn);

		for (i=0;i<len;i++) {
			/* contains custom path? */
			if (fn[i]==':'||fn[i]=='\\'||fn[i]=='/') break;
		}

		if (i==len&&strlen(fn_source)) {
			/* no */
			file_setfile(NULL,fn_source,NULL,type==MEDIA_TAPE?"cas":"rom");
			if (file->dir&&strlen(file->dir)) sprintf(dir,"%s\\",file->dir);
			strcat(dir,fn); file_setfile(NULL,dir,NULL,type==MEDIA_TAPE?"cas":"rom");
		}
		else {
			/* yes, or no initial path */
			file_setfile(NULL,fn,NULL,type==MEDIA_TAPE?"cas":"rom");
		}

		if (file->filename&&file_open()) strcpy(fn,file->filename);
		else fn[0]=0;

		file_close();
	}

	/* opendialog */
	of.lStructSize=sizeof(OPENFILENAME);
	of.hwndOwner=dialog;
	of.hInstance=MAIN->module;
	of.lpstrFile=fn;
	of.nMaxFile=STRING_SIZE;
	of.nFilterIndex=*fi;
	of.Flags=OFN_ENABLESIZING|OFN_HIDEREADONLY|OFN_FILEMUSTEXIST;
	of.lpstrInitialDir=strlen(fn)?NULL:dir_source?dir_source:file->appdir;

	o=GetOpenFileName(&of);
	*fi=of.nFilterIndex;

	media.shift_open=(media.shift_open|GetAsyncKeyState(VK_SHIFT))&0x8000;

	if (!o) return FALSE;

	fn_source[0]=0;
	if (strlen(fn)) {
		strcpy(fn_source,fn);
		if (strlen(fn+of.nFileOffset)) media.last_offset=of.nFileOffset;
		else media.last_offset=0;
	}

	return TRUE;
}


static void media_tab(HWND dialog,int tab)
{
	int i,j;

	if (tab==media.current_tab) return;

	media.current_tab=tab;

	#define HIDETAB(x,y) for (i=x;i<=y;i++) { EnableWindow(GetDlgItem(dialog,i),FALSE); ShowWindow(GetDlgItem(dialog,i),SW_HIDE); }
	#define SHOWTAB(x,y) for (i=x;i<=y;i++) { EnableWindow(GetDlgItem(dialog,i),TRUE); ShowWindow(GetDlgItem(dialog,i),SW_NORMAL); }

	/* hide+disable all */
	HIDETAB(IDC_MEDIA_TAB0START,IDC_MEDIA_TAB0END)
	HIDETAB(IDC_MEDIA_TAB1START,IDC_MEDIA_TAB1END)
	HIDETAB(IDC_MEDIA_TAB2START,IDC_MEDIA_TAB2END)

	switch (tab) {
		/* carts */
		case MEDIA_TAB_CARTS:
			SHOWTAB(IDC_MEDIA_TAB0START,IDC_MEDIA_TAB0END)

			/* extra config buttons */
			for (j=0;j<2;j++)
			if ((i=SendDlgItemMessage(dialog,IDC_MEDIA_SLOT1TYPE+j,CB_GETCURSEL,0,0))==CB_ERR||~mapper_get_type_flags(i)&MCF_EXTRA) {
				EnableWindow(GetDlgItem(dialog,IDC_MEDIA_SLOT1EXTRA+j),FALSE);
			}
			break;

		/* tape */
		case MEDIA_TAB_TAPE:
			SHOWTAB(IDC_MEDIA_TAB1START,IDC_MEDIA_TAB1END)
			EnableWindow(GetDlgItem(dialog,IDC_MEDIA_TAPER),FALSE);
			EnableWindow(GetDlgItem(dialog,IDC_MEDIA_TAPEPOS),FALSE);
			EnableWindow(GetDlgItem(dialog,IDC_MEDIA_TAPESIZE),FALSE);

			tape_draw_gui(dialog);

			PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDC_MEDIA_TAPECONTENTS),TRUE);
			break;

		/* internal */
		case MEDIA_TAB_INTERNAL:
			SHOWTAB(IDC_MEDIA_TAB2START,IDC_MEDIA_TAB2END)
			break;

		default: break;
	}

	#undef HIDETAB
	#undef SHOWTAB
}

INT_PTR CALLBACK media_dialog( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch (msg) {
		case WM_INITDIALOG: {
			int i,j,slot;
			char t[STRING_SIZE];
			HWND c1,c2;

			TCITEM pitem;
			HWND tab=GetDlgItem(dialog,IDC_MEDIA_TAB);
#ifdef MEISEI_ESP
			const char* tabtext[]={"Cartuchos","Cinta","Interno"};
#else
            const char* tabtext[]={"Cartridges","Tape","Internal"};
#endif // MEISEI_ESP


			memset(&pitem,0,sizeof(TCITEM));

			/* init carts */
			for (slot=0;slot<2;slot++) {
				media.cart_fn[slot][0]=0;
				media.shift_cart[slot]=0;
				if (mapper_get_file(slot)) {
					/* file */
					strcpy(media.cart_fn[slot],mapper_get_file(slot));

					/* editbox */
					file_setfile(NULL,media.cart_fn[slot],NULL,NULL);
					if (file->name&&strlen(file->name)) {
						if (file->ext&&strlen(file->ext)) {
							sprintf(t,"%s.%s",file->name,file->ext);
							SetDlgItemText(dialog,IDC_MEDIA_SLOT1FILE+slot,t);
						}
						else SetDlgItemText(dialog,IDC_MEDIA_SLOT1FILE+slot,file->name);
					}
					else media.cart_fn[slot][0]=0;
				}
			}

			/* init cart comboboxes */
			c1=GetDlgItem(dialog,IDC_MEDIA_SLOT1TYPE);
			c2=GetDlgItem(dialog,IDC_MEDIA_SLOT2TYPE);
			for (i=0;i<CARTTYPE_MAX;i++) {
				SendMessage(c1,CB_ADDSTRING,0,(LPARAM)((LPCTSTR)mapper_get_type_longname(i)));
				SendMessage(c2,CB_ADDSTRING,0,(LPARAM)((LPCTSTR)mapper_get_type_longname(i)));
			}
			SendMessage(c1,CB_SETCURSEL,mapper_get_carttype(0),0);
			SendMessage(c2,CB_SETCURSEL,mapper_get_carttype(1),0);

			/* internal comboboxes */
			c1=GetDlgItem(dialog,IDC_MEDIA_VDP);
			for (i=0;i<VDP_CHIPTYPE_MAX;i++) {
				sprintf(t,"%s (%d0Hz)",vdp_get_chiptype_name(i),6-vdp_get_chiptype_vf(i));
				if (i==VDP_CHIPTYPE_DEFAULT) strcat(t," (Default)");
				SendMessage(c1,CB_ADDSTRING,0,(LPARAM)((LPCTSTR)t));
			}
			SendMessage(c1,CB_SETCURSEL,vdp_get_chiptype(),0);

			c1=GetDlgItem(dialog,IDC_MEDIA_PSG);
			for (i=0;i<PSG_CHIPTYPE_MAX;i++) {
				sprintf(t,"%s",psg_get_chiptype_name(i));
				if (i==PSG_CHIPTYPE_DEFAULT) strcat(t," (Default)");
				SendMessage(c1,CB_ADDSTRING,0,(LPARAM)((LPCTSTR)t));
			}
			SendMessage(c1,CB_SETCURSEL,psg_get_chiptype(),0);

			/* ram */
			c1=GetDlgItem(dialog,IDC_MEDIA_RAMSIZE);
			c2=GetDlgItem(dialog,IDC_MEDIA_RAMSLOT);
			sprintf(t,"No RAM"); SendMessage(c1,CB_ADDSTRING,0,(LPARAM)((LPCTSTR)t));
			for (i=0;i<4;i++) {
				sprintf(t,"%dKB",8<<i);
				if ((8<<i)==mapper_get_default_ramsize()) strcat(t," (D.)");
				SendMessage(c1,CB_ADDSTRING,0,(LPARAM)((LPCTSTR)t));

				sprintf(t,"Slot %d",i);
				if (i==mapper_get_default_ramslot()) strcat(t," (D.)");
				SendMessage(c2,CB_ADDSTRING,0,(LPARAM)((LPCTSTR)t));
			}
			i=mapper_get_ramsize()>>3; j=0;
			if (i) for (j=1;~i&1;i>>=1,j++) { ; }
			SendMessage(c1,CB_SETCURSEL,j,0);
			SendMessage(c2,CB_SETCURSEL,mapper_get_ramslot(),0);

			media.bios_fn[0]=0;
			if (mapper_get_bios_file()) strcpy(media.bios_fn,mapper_get_bios_file());
			if (mapper_get_bios_name()) SetDlgItemText(dialog,IDC_MEDIA_BIOSFILE,mapper_get_bios_name());

			/* init tape */
			media.tape_insertion_changed=FALSE;
			msx_wait(); tape_cur_to_od(); msx_wait_done();
			tape_flush_cache_od();
			media.tape_crc_prev=tape_get_crc_od();
			tape_init_gui(SetWindowLongPtr(GetDlgItem(dialog,IDC_MEDIA_TAPECONTENTS),GWLP_WNDPROC,(LONG_PTR)tape_sub_listbox));
			tape_draw_listbox(GetDlgItem(dialog,IDC_MEDIA_TAPECONTENTS));
			tape_draw_gui(dialog);
			if (tape_get_fn_od()) strcpy(media.tape_fn,tape_get_fn_od());
			else media.tape_fn[0]=0;

			/* init tab */
			media.current_tab=-1;

			pitem.mask=TCIF_TEXT;
			pitem.iImage=-1;

			pitem.pszText=(LPTSTR)tabtext[0];	SendMessage(tab,TCM_INSERTITEM,0,(LPARAM)&pitem);
			pitem.pszText=(LPTSTR)tabtext[1];	SendMessage(tab,TCM_INSERTITEM,1,(LPARAM)&pitem);
			pitem.pszText=(LPTSTR)tabtext[2];	SendMessage(tab,TCM_INSERTITEM,2,(LPARAM)&pitem);

			SendMessage(tab,TCM_SETCURSEL,media.prev_tab,0);
			media_tab(dialog,media.prev_tab);

			main_parent_window(dialog,0,0,0,0,0);

			return 1;
			break;
		}

		case WM_CTLCOLORSTATIC: /* case WM_CTLCOLORBTN: */ {
			UINT id=GetDlgCtrlID((HWND)lParam);
			if (id==IDC_MEDIA_TAPEFILE) break;

			SetBkMode((HDC)wParam,TRANSPARENT);
			return (HBRUSH)GetStockObject(NULL_BRUSH) == NULL ? FALSE : TRUE;
		}

		case WM_NOTIFY:
			switch (LOWORD(wParam)) {

				/* tab changes */
				case IDC_MEDIA_TAB:
					if (((LPOFNOTIFY)lParam)->hdr.code==TCN_SELCHANGE) {
						int tab;
						if ((tab=SendDlgItemMessage(dialog,IDC_MEDIA_TAB,TCM_GETCURSEL,0,0))!=-1) media_tab(dialog,tab);
					}
					break;

				default: break;
			}

			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {

				/* bios */
				case IDC_MEDIA_BIOSBROWSE:
					if (openfilenamedialog(dialog,MEDIA_BIOS)) SetDlgItemText(dialog,IDC_MEDIA_BIOSFILE,media.bios_fn+media.last_offset);
					PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDOK),TRUE);
					break;
				case IDC_MEDIA_BIOSDEFAULT: {
					char c[0x100]={0};
					mapper_get_defaultbios_name(c); SetDlgItemText(dialog,IDC_MEDIA_BIOSFILE,c);
					media.bios_fn[0]=0; strcpy(media.bios_fn,mapper_get_defaultbios_file());
					PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDOK),TRUE);
					break;
				}

				/* cart file browse buttons */
				case IDC_MEDIA_SLOT1BROWSE: case IDC_MEDIA_SLOT2BROWSE: {
					int slot=(LOWORD(wParam))==IDC_MEDIA_SLOT2BROWSE;

					if (openfilenamedialog(dialog,MEDIA_CART1+slot)) {
						int type=mapper_autodetect_cart(slot,media.cart_fn[slot]);
						media.shift_cart[slot]=media.shift_open;
						SetDlgItemText(dialog,IDC_MEDIA_SLOT1FILE+slot,media.cart_fn[slot]+media.last_offset);
						SendDlgItemMessage(dialog,IDC_MEDIA_SLOT1TYPE+slot,CB_SETCURSEL,type,0);
						EnableWindow(GetDlgItem(dialog,IDC_MEDIA_SLOT1EXTRA+slot),(mapper_get_type_flags(type)&MCF_EXTRA)!=0);

						/* remember dir */
						file_setfile(NULL,media.cart_fn[slot],NULL,"rom");
						file_setdirectory(&file->romdir);
					}
					media.fi_cart_set=media.fi_cart[slot];
					PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDOK),TRUE);
					break;
				}

				/* cart type comboboxes */
				case IDC_MEDIA_SLOT1TYPE: case IDC_MEDIA_SLOT2TYPE: {
					int type,slot=(LOWORD(wParam))==IDC_MEDIA_SLOT2TYPE;

					if ((HIWORD(wParam))==CBN_SELCHANGE&&(type=SendDlgItemMessage(dialog,LOWORD(wParam),CB_GETCURSEL,0,0))!=CB_ERR) {
						EnableWindow(GetDlgItem(dialog,IDC_MEDIA_SLOT1EXTRA+slot),(mapper_get_type_flags(type)&MCF_EXTRA)!=0);
					}
					break;
				}

				/* cart eject buttons */
				case IDC_MEDIA_SLOT1EJECT: case IDC_MEDIA_SLOT2EJECT: {
					int slot=(LOWORD(wParam))==IDC_MEDIA_SLOT2EJECT;

					media.cart_fn[slot][0]=0;
					SetDlgItemText(dialog,IDC_MEDIA_SLOT1FILE+slot,"");
					SendDlgItemMessage(dialog,IDC_MEDIA_SLOT1TYPE+slot,CB_SETCURSEL,CARTTYPE_NOMAPPER,0);
					EnableWindow(GetDlgItem(dialog,IDC_MEDIA_SLOT1EXTRA+slot),TRUE); /* knowing that CARTTYPE_NOMAPPER has extra config */
					mapper_extra_configs_reset(slot);
					PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDOK),TRUE);
					break;
				}

				/* cart extra config buttons */
				case IDC_MEDIA_SLOT1EXTRA: case IDC_MEDIA_SLOT2EXTRA: {
					RECT r; POINT p;
					int type,slot=(LOWORD(wParam))==IDC_MEDIA_SLOT2EXTRA;
					if ((type=SendDlgItemMessage(dialog,IDC_MEDIA_SLOT1TYPE+slot,CB_GETCURSEL,0,0))==CB_ERR) break;

					GetWindowRect(GetDlgItem(dialog,LOWORD(wParam)),&r);
					p.x=r.left; p.y=r.top; ScreenToClient(dialog,&p);
					mapper_extra_configs_dialog(dialog,slot,type,p.x,p.y);

					PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDOK),TRUE);
					break;
				}

				/* tape browse */
				case IDC_MEDIA_TAPEBROWSE:
					/* openfilenamedialog here instead of tape.c, tapes can be opened elsewhere too */
					if (openfilenamedialog(dialog,MEDIA_TAPE)&&strlen(media.tape_fn)) {
						if (!tape_save_od()) LOG_ERROR_WINDOW(dialog,"Couldn't save tape!");
						file_setfile(NULL,media.tape_fn,NULL,"cas");
						if (tape_open_od(media.shift_open==0)) {
							file_setdirectory(&file->casdir); /* remember dir */
							tape_draw_listbox(GetDlgItem(dialog,IDC_MEDIA_TAPECONTENTS));
							tape_draw_gui(dialog);
							tape_set_updated_od();
							media.tape_insertion_changed=TRUE;
						}
						else {
							LOG_ERROR_WINDOW(dialog,"Couldn't open tape!");
						}
					}
					PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDC_MEDIA_TAPECONTENTS),TRUE);
					InvalidateRect(dialog,NULL,FALSE);
					break;

				/* tape eject */
				case IDC_MEDIA_TAPEEJECT:
					if (!tape_save_od()) {
                        LOG_ERROR_WINDOW(
                            dialog,
                        #ifdef MEISEI_ESP
                            "¡No se pudo guardar la cinta!"
                        #else
                            "Couldn't save tape!"
                        #endif
                        );
					}
					tape_clean_od();
					tape_draw_listbox(GetDlgItem(dialog,IDC_MEDIA_TAPECONTENTS));
					tape_draw_gui(dialog);
					tape_set_updated_od();
					media.tape_insertion_changed=TRUE;
					PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDOK),TRUE);
					break;

				/* tape new */
				case IDC_MEDIA_TAPENEW:
					if (tape_new_od(dialog)) {
						tape_draw_listbox(GetDlgItem(dialog,IDC_MEDIA_TAPECONTENTS));
						tape_draw_gui(dialog);
						tape_set_updated_od();
						media.tape_insertion_changed=TRUE;
					}
					PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDC_MEDIA_TAPECONTENTS),TRUE);
					break;

				/* tape listbox */
				case IDC_MEDIA_TAPECONTENTS:
					switch (HIWORD(wParam)) {

						/* clicked/changed */
						case LBN_SELCHANGE: {
							LRESULT cb=SendDlgItemMessage(dialog,IDC_MEDIA_TAPECONTENTS,LB_GETCURSEL,0,0);
							if (cb==LB_ERR) break;;

							tape_set_cur_block_od(cb);
							tape_draw_pos(dialog);
							tape_set_updated_od();

							break;
						}

						/* doubleclick = get block info */
						case LBN_DBLCLK:
							if (!GetListBoxInfo(GetDlgItem(dialog,IDC_MEDIA_TAPECONTENTS))) break;
							if (!tape_cur_block_od_is_empty()) {
								tape_set_popup_pos();
								DialogBox( MAIN->module, MAKEINTRESOURCE(IDD_MEDIATAPE_INFO), dialog, (DLGPROC)tape_blockinfo_dialog );
							}
							break;

						default: break;
					}

					break;

				/* ok/ok + reset button */
				case IDOK: case IDC_MEDIA_OK: {
					char fn[STRING_SIZE]={0};
					char d[STRING_SIZE]={0};
					char e[STRING_SIZE]={0};
					int slot,c,reset=(LOWORD(wParam)==IDOK);
					int ci[2]={FALSE,FALSE};

					/* error checking */
					if ((slot=SendDlgItemMessage(dialog,IDC_MEDIA_RAMSLOT,CB_GETCURSEL,0,0))==CB_ERR) slot=mapper_get_default_ramslot();
					if ((c=SendDlgItemMessage(dialog,IDC_MEDIA_RAMSIZE,CB_GETCURSEL,0,0))==CB_ERR) c=mapper_get_default_ramsize();
					if (c) c=4<<c;

					if (slot==0&&c==64) {
						LOG_ERROR_WINDOW(
                            dialog,
                        #ifdef MEISEI_ESP
                            "No se puede 64KB de RAM en el slot 0."
                        #else
                            "Can't have 64KB RAM in slot 0."
                        #endif
                        );
						return 0;
					}

					if (media.current_tab!=-1) media.prev_tab=media.current_tab;

					msx_wait();

					/* internal tab */
					/* ram */
					if (mapper_init_ramslot(slot,c)) reverse_invalidate();

					/* bios */
					if (!strlen(media.bios_fn)) mapper_open_bios(mapper_get_defaultbios_file());
					else mapper_open_bios(media.bios_fn);

					/* vdp */
					if ((c=SendDlgItemMessage(dialog,IDC_MEDIA_VDP,CB_GETCURSEL,0,0))!=CB_ERR) {
						vdp_set_chiptype(c);
						draw_set_vidformat(vdp_get_chiptype_vf(vdp_get_chiptype()));
						crystal_set_mode(reset^1);
					}

					/* psg */
					if ((c=SendDlgItemMessage(dialog,IDC_MEDIA_PSG,CB_GETCURSEL,0,0))!=CB_ERR) {
						psg_set_chiptype(c);
						psg_init_amp();
					}

					/* carts tab */
					for (slot=0;slot<2;slot++) {

						/* get filename */
						d[0]=0; e[0]=0; fn[0]=0;
						if (GetDlgItemText(dialog,IDC_MEDIA_SLOT1FILE+slot,e,STRING_SIZE)) {
							int i,len=strlen(e);

							for (i=0;i<len;i++) {
								/* contains custom path? */
								if (e[i]==':'||e[i]=='\\'||e[i]=='/') break;
							}

							if (i==len&&strlen(media.cart_fn[slot])) {
								/* no */
								file_setfile(NULL,media.cart_fn[slot],NULL,NULL);
								if (file->dir&&strlen(file->dir)) sprintf(d,"%s\\",file->dir);
								strcat(d,e); strcpy(fn,d);
							}
							else {
								/* yes, or no initial path */
								file_setfile(NULL,e,NULL,NULL);
								if (file->filename&&strlen(file->filename)) strcpy(fn,file->filename);
							}
						}

						if ((c=SendDlgItemMessage(dialog,IDC_MEDIA_SLOT1TYPE+slot,CB_GETCURSEL,0,0))==CB_ERR) {
							c=CARTTYPE_NOMAPPER;
							mapper_extra_configs_reset(slot);
						}

						/* open */
						if (~mapper_get_type_flags(c)&MCF_EMPTY&&strlen(fn)) {
							if (mapper_get_file(slot)==NULL||stricmp(fn,mapper_get_file(slot))!=0) {
								if (mapper_open_cart(slot,fn,media.shift_cart[slot]==0)) {
									mapper_set_carttype(slot,c);
									if (!reset) mapper_init_cart(slot);
									ci[slot]=TRUE;
								}
								else c=mapper_get_carttype(slot);
							}
						}
						else if (mapper_get_file(slot)!=NULL) {
							mapper_close_cart(slot);
							mapper_set_carttype(slot,c);
							if (!reset) mapper_init_cart(slot);
							ci[slot]=TRUE;
						}

						if (c!=mapper_get_carttype(slot)||mapper_extra_configs_differ(slot,c)) {
							mapper_set_carttype(slot,c);
							if (!reset) mapper_init_cart(slot);
							ci[slot]=TRUE;
						}
					}

					/* show romtype */
					if (ci[0]) {
						mapper_log_type(0,ci[1]);
						if (ci[1]) LOG(LOG_MISC,", ");
						else LOG(LOG_MISC,"\n");
					}
					if (ci[1]) {
						mapper_log_type(1,ci[0]);
						LOG(LOG_MISC,"\n");
					}

					/* tape tab */
					c=SendDlgItemMessage(dialog,IDC_MEDIA_TAPECONTENTS,LB_GETCURSEL,0,0);
					if (c==LB_ERR) c=0;
					if (media.tape_crc_prev!=tape_get_crc_od()||tape_get_updated_od()) {
						/* changed */
						reverse_invalidate();
						tape_set_cur_block_od(c);
						tape_od_to_cur();
						tape_set_runtime_block_inserted(FALSE);

						if (media.tape_insertion_changed) tape_log_info();
					}
					else tape_clean_od();

					mapper_update_bios_hack();


					main_titlebar(mapper_get_current_name(fn));

					/* reset */
					if (reset) msx_reset(TRUE);
					else {
						mapper_refresh_pslot_read();
						mapper_refresh_pslot_write();
					}

					msx_wait_done();

					EndDialog(dialog,0);
					break;
				}

				case IDCANCEL:
					if (media.current_tab!=-1) media.prev_tab=media.current_tab;
					tape_clean_od(); /* clean temp tape */

					mapper_extra_configs_revert(0);
					mapper_extra_configs_revert(1);

					EndDialog(dialog,0);
					break;

				default: break;
			}
			break;

		default: break;
	}

	return 0;
}
