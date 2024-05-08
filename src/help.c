/******************************************************************************
 *                                                                            *
 *                                "help.c"                                    *
 *                                                                            *
 ******************************************************************************/

#include <windows.h>
#include <commctrl.h>

#include "global.h"
#include "resource.h"
#include "main.h"
#include "file.h"
#include "version.h"
#include "settings.h"

/* help window */
INT_PTR CALLBACK help_help( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam )
{
    lParam = lParam;
	switch (msg) {

		case WM_INITDIALOG: {
			char* text=NULL;

			/* open documentation */
#ifdef MEISEI_ESP
         // file_setfile(&file->appdir,"meisei_esp.txt",NULL,NULL);
			file_setfile(NULL,"docs/meisei_esp.txt",NULL,NULL);
#else
         // file_setfile( &file->appdir,"meisei_eng.txt",NULL,NULL );
			file_setfile( NULL,"docs/meisei_eng.txt",NULL,NULL );
#endif // MEISEI_ESP
			if (!file_open()||file->size<100||file->size>100000) {
				LOG(LOG_MISC|LOG_WARNING,"can't open documentation\n");
				file_close(); EndDialog(dialog,0);
				break;
			}
			MEM_CREATE(text,file->size+1);
			if (!file_read((u8*)text,file->size)) {
				LOG(LOG_MISC|LOG_WARNING,"can't open documentation\n");
				file_close(); MEM_CLEAN(text); EndDialog(dialog,0);
				break;
			}
			file_close();

			main_parent_window(dialog,0,0,0,0,0);
			SetFocus(dialog);

			/* fill editbox */
			SetDlgItemText(dialog,IDC_HELPTEXT,text);

			MEM_CLEAN(text);

			break;
		}

		case WM_CTLCOLORSTATIC:
			/* prevent colour changing for read-only editbox */
			return GetSysColorBrush(COLOR_WINDOW) == NULL ? FALSE : TRUE; /* (BOOL) */
			break;

		case WM_COMMAND:

			switch (LOWORD(wParam)) {

				/* close dialog */
				case IDOK: case IDCANCEL:
					EndDialog(dialog,0);
					break;

				default: break;
			}

			break;

		default: break;
	}

	return 0;
}

/* about window */
static int cursor_in_dialogitem(HWND dialog,UINT item)
{
	RECT rect={0,0,0,0};
	POINT p={0,0};

	GetWindowRect(GetDlgItem(dialog,item),&rect);
	GetCursorPos(&p);

	return PtInRect(&rect,p);
}

INT_PTR CALLBACK help_about( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam )
{
	static HCURSOR finger=NULL;
	static HFONT underline=NULL;
	static HICON icon=NULL;
	static int url_clicked=FALSE;
	static int email_clicked=FALSE;

	switch (msg) {

		case WM_INITDIALOG: {
			char c[0x100];
			int t=settings_get_runtimes();

			/* set underline font */
			LOGFONT lf;
			memset(&lf,0,sizeof(LOGFONT));
			underline=(HFONT)SendDlgItemMessage(dialog,IDC_ABOUT_URL,WM_GETFONT,0,0); /* other's the same */
			if (underline==NULL) underline=GetStockObject(SYSTEM_FONT);
			GetObject(underline,sizeof(lf),&lf);
			lf.lfUnderline=TRUE;

			underline=CreateFontIndirect(&lf);
			SendDlgItemMessage(dialog,IDC_ABOUT_URL,WM_SETFONT,(WPARAM)underline,0);
			SendDlgItemMessage(dialog,IDC_ABOUT_EMAIL,WM_SETFONT,(WPARAM)underline,0);

			finger=LoadCursor(NULL,IDC_HAND);

			/* set icon */
			icon=LoadImage(MAIN->module,MAKEINTRESOURCE(ID_ICON),IMAGE_ICON,0,0,LR_DEFAULTSIZE);
			SendDlgItemMessage(dialog,IDC_ABOUT_ICON,STM_SETICON,(WPARAM)icon,0);

			/* set version/ran text */
			sprintf(
                c,
            #ifdef MEISEI_ESP
                "%s %s, %s\ncorriendo %d vece%s",
            #else
                "%s %s, %s\nran %d time%s",
            #endif
                VERSION_NAME_S,
                VERSION_NUMBER_S,
                VERSION_DATE_S,
                t,
                (t!=1)?"s":""
            );
			SetDlgItemText(dialog,IDC_ABOUT_TEXT,c);

			main_parent_window(dialog,0,0,0,0,0);

			return 1;
			break;
		}

		case WM_LBUTTONDOWN: {

			int action=0;

			/* open homepage */
			if (cursor_in_dialogitem(dialog,IDC_ABOUT_URL)) {
				if (((INT_PTR)ShellExecute(NULL,"open",VERSION_URL_S,NULL,NULL,SW_SHOWNORMAL))>32) { url_clicked=TRUE; action=2; }
				else action=1;
			}

			/* open new email */
			else if (cursor_in_dialogitem(dialog,IDC_ABOUT_EMAIL)) {
				char c[0x100];
				sprintf(c,"%s%s","mailto:",VERSION_EMAIL_S);

				if (((INT_PTR)ShellExecute(NULL,"open",c,NULL,NULL,SW_SHOWNORMAL))>32) { email_clicked=TRUE; action=2; }
				else action=1;
			}

			if (action==1) Beep(440,120); /* error */
			else if (action==2) InvalidateRect(dialog,NULL,0); /* repaint */

			break;
		}

		case WM_CTLCOLORSTATIC: {
			UINT id=GetDlgCtrlID((HWND)lParam);

			if (id==IDC_ABOUT_URL||id==IDC_ABOUT_EMAIL) {
				/* change link colours */
				int clicked=(id==IDC_ABOUT_URL)?url_clicked:email_clicked;

				SetTextColor((HDC)wParam,clicked?(RGB(128,0,128)):(RGB(0,0,255)));
				SetBkMode((HDC)wParam,TRANSPARENT);
				return (HBRUSH)GetStockObject(NULL_BRUSH) == NULL ? FALSE : TRUE;
			}

			break;
		}

		case WM_SETCURSOR: {
			/* set finger cursor */
			if (finger&&(cursor_in_dialogitem(dialog,IDC_ABOUT_URL)||cursor_in_dialogitem(dialog,IDC_ABOUT_EMAIL))) {
				SetCursor(finger);
				return 1;
			}

			break;
		}

		case WM_COMMAND:

			switch (LOWORD(wParam)) {

				/* close dialog */
				case IDOK: case IDCANCEL:
					if (underline!=NULL) DeleteObject(underline);
					underline=NULL;
					if (icon!=NULL) DestroyIcon(icon);
					icon=NULL;
					EndDialog(dialog,0);
					break;

				default: break;
			}

			break;

		default: break;
	}

	return 0;
}
