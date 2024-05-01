/******************************************************************************
 *                                                                            *
 *                               "psgtoy.c"                                   *
 *                                                                            *
 ******************************************************************************/

/* tool window */
#include <windows.h>
#include <commctrl.h>
#include <math.h>

#include "global.h"
#include "zlib.h"
#include "settings.h"
#include "tool.h"
#include "resource.h"
#include "file.h"
#include "main.h"
#include "input.h"
#include "psgtoy.h"
#include "psg.h"
#include "sound.h"
#include "psglogger.h"
#include "movie.h"
#include "tileview.h"

static struct {
	int busy;

	/* output activity */
	int logging;
	int logging_vis;
	int logging_done_vis;
	int log_done;
	int log_frames;
	DWORD log_ticks;

	/* tone channels */
	int c_vol[3];
	int c_freq[3];
	int c_en[3];

	/* noise */
	int n_freq;

	/* envelope */
	int e_vol;
	int e_shape;
	int e_freq;

	/* piano bmp */
	struct {
		HWND wnd;
		HDC wdc;
		HDC bdc;
		int* screen;
		HBITMAP bmp;
		BITMAPINFO info;
	} piano;

	void* p_sample;
	int p_playing;
	DWORD p_ticks;

	int note_prev[3];
	int popup_active;
	POINT popup_p;
	int prbdown;
	int centcorrection_set;
	int centcorrection_d;

	/* custom wave */
	int drawing;
	int custom_enabled_prev;
	int enabled;
	int click;
	HCURSOR cursor_cross;

	void* sample;
	int pkey_prev;
	int playing;

	int cur_chan;
	int cur_chan_prev;
	int preset_prev;
	int hl_prev;

	int ckey_prev;
	int paste_valid;
	u8 copy_data[0x100];
	u8 open_data[0x300];
	int open_amplitude[3];
	int opened;

	/* wave bmp */
	struct {
		HWND wnd;
		HDC wdc;
		HDC bdc;
		u8* screen;
		HBITMAP bmp;
		BITMAPINFO info;
		u32 pal[0x100];
	} wave;

	int in_wave;
	int in_wave_prev;
	POINT p_wave;
	POINT p_wave_prev;

} psgtoy;


static int act_notepos_lut[0x1000];
static int cdefgabc_bri_lut[17][32];	/* piano bar brightness table */
static int cdefgabc_pos_lut[13][7];		/* piano bar coordinates table */

#define PIANO_OCTAVEC					0x111112

/* wav file header */
static const u8 wav_header[44]={'R','I','F','F',0x24,1,0,0,'W','A','V','E','f','m','t',' ',0x10,0,0,0,1,0,1,0,0x44,0xac,0,0,0x44,0xac,0,0,1,0,8,0,'d','a','t','a',0,1,0,0};
#define WH_FILESIZE_OFFSET				4
#define WH_RATE_OFFSET					24
#define WH_DATASIZE_OFFSET				40

static int* psg_regs;

/* presets */
#define PSGTOY_PRESET_CSIZE				10454
#define PSGTOY_PRESET_DEFAULT			27
#define PSGTOY_PRESET_CUSTOM			84
#define PSGTOY_PRESET_MAX				(PSGTOY_PRESET_CUSTOM+1)

static char* custom_preset_name[PSGTOY_PRESET_MAX];

const char* psgtoy_get_preset_name(u32 i) { if (i>=PSGTOY_PRESET_MAX) return NULL; else return custom_preset_name[i]; }
static u8 custom_preset_wave[PSGTOY_PRESET_MAX][0x100];
static int custom_preset_amplitude[PSGTOY_PRESET_MAX];

/* non-volatile */
static char psgtoy_dir[STRING_SIZE]={0};
static int psgtoy_open_fi; /* 1: all supp. 2: .cw, 3: .cw3, 4: wav, 5: any */
static int psgtoy_save_fi; /* 1: .cw, 2: .cw3, 3: wav */

static int pvis_mask[3]={0xffff,0xffff,0xffff};
static int centcorrection;
static int custom_enabled;
static int custom_preset[3];
static int custom_amplitude[3];
static u8 custom_wave[3][0x100];
static int custom_wave_changed[3];

#define CUSTOM_AMP_MAX 200

int __fastcall psgtoy_get_act_notepos(u32 i) { return act_notepos_lut[i]; }
int psgtoy_get_open_fi(void) { return psgtoy_open_fi; }
int psgtoy_get_save_fi(void) { return psgtoy_save_fi; }
int psgtoy_get_centcorrection(void) { return centcorrection; }
int psgtoy_get_custom_enabled(void) { return custom_enabled; }
int psgtoy_get_custom_preset(u32 i) { return custom_preset[i]; }
int __fastcall psgtoy_get_custom_amplitude(u32 i) { return custom_amplitude[i]; }
int __fastcall psgtoy_reset_custom_wave_changed(u32 i) { int c=custom_wave_changed[i]; custom_wave_changed[i]=0; return c; }
u8* psgtoy_get_custom_wave_ptr(u32 i) { if (i>2) i=2; return custom_wave[i]; }

void psgtoy_get_custom_wave(u32 i,u8* wave)
{
	if (!wave) return;
	if (i>2) i=2;

	memcpy(wave,custom_wave[i],0x100);
}

void psgtoy_get_custom_wave_string(u32 i,char* c)
{
	int j;
	char x[0x10];

	if (!c) return;
	if (i>2) i=2;

	c[0]=x[2]=0;

	for (j=0;j<0x100;j++) {
		sprintf(x,"%02X",custom_wave[i][j]);
		strcat(c,x);
	}

	c[0x200]=0;
}


static void fill_act_notepos(void)
{
	int i,o,n;
	double f,q,d;

	#define CORRECTCENT()															\
		if (centcorrection<0) { i=-centcorrection; while (i--) f*=0.9994225441; }	\
		else if (centcorrection>0) { i=centcorrection; while (i--) f*=1.0005777895; }

	q=3579545.454545/32.0;
	/* middle A to O0A */
	o=0x40; n=9; f=440.0*0.9715319412; /* -50 */
	CORRECTCENT()
	act_notepos_lut[0xfe]= o|n;
	for (i=0xff;i<0x1000;i++) {
		d=q/(double)i;
		if (d<f) {
			n--;
			if (n==-1) { n=11; o-=0x10; }
			f*=0.9438743127; /* -100 */
		}
		act_notepos_lut[i]=o|n;
	}
	/* middle A to O8B */
	o=0x40; n=9; f=440.0*1.0293022366; /* +50 */
	CORRECTCENT()
	for (i=0xfd;i>0xd;i--) {
		d=q/(double)i;
		if (d>=f) {
			n=(n+1)%12;
			if (n==0) o+=0x10;
			f*=1.0594630944; /* +100 */
		}
		act_notepos_lut[i]=o|n;
	}
	/* rest O9C */
	for (;i>=0;i--) act_notepos_lut[i]=0x80|12;

	#undef CORRECTCENT
}

static void stop_psglog(HWND dialog,int save)
{
	int s=psglogger_stop(save);
	psgtoy.log_frames=0;

	switch (s) {
		case 1:
			psgtoy.logging=FALSE;
			LOG_ERROR_WINDOW(dialog,"Couldn't save PSG log!");
			break;

		case 2:
			psgtoy.logging=FALSE;
			LOG_ERROR_WINDOW(dialog,"PSG log silence out of bounds!");
			break;

		default:
			psgtoy.logging=FALSE;
			psgtoy.log_frames=s;
			break;
	}
}

static void custom_enable_window(HWND dialog)
{
	int i=(custom_enabled!=0)&(psgtoy.playing==0);
	psgtoy.enabled=i;

	EnableWindow(GetDlgItem(dialog,IDC_PSGTOY_CUSTOM),psgtoy.playing==0);

	#define E(x) EnableWindow(GetDlgItem(dialog,x),i)
	E(IDC_PSGTOY_CAR);		E(IDC_PSGTOY_CBR);		E(IDC_PSGTOY_CCR);
	E(IDC_PSGTOY_PRESET);	E(IDC_PSGTOY_PRESETT);
	E(IDC_PSGTOY_OPEN);		E(IDC_PSGTOY_SAVE);
	E(IDC_PSGTOY_VEDIT);	E(IDC_PSGTOY_VSPIN);	E(IDC_PSGTOY_VTEXT);	E(IDC_PSGTOY_VPER);
	#undef E
}

static __fastcall void bresenham(int cc,int xs,int ys,int xd,int yd)
{
	u8* screen=psgtoy.wave.screen;
	int i=0,x=xs,y=ys,dx,dy,e;

	xs=dx=xd-xs; CLAMP(xs,-1,1);
	ys=dy=yd-ys; CLAMP(ys,-1,1);
	if(dx<0) dx=-dx;
	if(dy<0) dy=-dy;
	xd=xs; yd=ys;
	if (dx>dy) ys=0;
	else { xs=dy; dy=dx; dx=xs; xs=0; }
	e=dx>>1;

	/* draw line */
	screen[y<<8|x]=cc<<1|1;
	while (i<dx) {
		if ((e-=dy)<0) { e+=dx; x+=xd; y+=yd; }
		else { x+=xs; y+=ys; }
		screen[y<<8|x]=cc<<1|1;

		i++;
	}
}

/* cents offset dialog */
static INT_PTR CALLBACK psgtoy_centoffset_dialog( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam )
{
    lParam = lParam;
	switch (msg) {

		case WM_INITDIALOG: {
			char t[0x10];
			RECT r;
			int i;

			/* init slider */
			psgtoy.centcorrection_d=centcorrection;
			SendDlgItemMessage(dialog,IDC_PSGTOYCENT_S,TBM_SETRANGE,FALSE,(LPARAM)MAKELONG(0,50));
			for (i=5;i<50;i+=5) SendDlgItemMessage(dialog,IDC_PSGTOYCENT_S,TBM_SETTIC,0,(LPARAM)(LONG)i);
			SendDlgItemMessage(dialog,IDC_PSGTOYCENT_S,TBM_SETPOS,TRUE,(LPARAM)(LONG)(psgtoy.centcorrection_d+25));

			sprintf(t,"%s%d",(psgtoy.centcorrection_d>0)?"+":(psgtoy.centcorrection_d==0)?" ":"",psgtoy.centcorrection_d);
			SetDlgItemText(dialog,IDC_PSGTOYCENT_T,t);

			/* position window on popup menu location */
			GetWindowRect(GetParent(dialog),&r);
			r.top=psgtoy.popup_p.y-r.top; if (r.top<0) r.top=0;
			r.left=psgtoy.popup_p.x-r.left; if (r.left<0) r.left=0;
			main_parent_window(dialog,MAIN_PW_LEFT,MAIN_PW_LEFT,r.left,r.top,0);

			break;
		}

		/* change slider */
		case WM_NOTIFY:
			if (wParam==IDC_PSGTOYCENT_S) {
				int i=SendDlgItemMessage(dialog,IDC_PSGTOYCENT_S,TBM_GETPOS,0,0)-25;
				CLAMP(i,-25,25);

				if (i!=psgtoy.centcorrection_d) {
					char t[0x10];
					psgtoy.centcorrection_d=i;
					sprintf(t,"%s%d",(psgtoy.centcorrection_d>0)?"+":(psgtoy.centcorrection_d==0)?" ":"",psgtoy.centcorrection_d);
					SetDlgItemText(dialog,IDC_PSGTOYCENT_T,t);
				}
			}

			break;

		case WM_COMMAND:

			switch (LOWORD(wParam)) {

				/* close dialog */
				case IDOK:
					EndDialog(dialog,1);
					break;

				case IDCANCEL:
					EndDialog(dialog,0);
					break;

				default: break;
			}

			break;

		default: break;
	}

	return 0;
}


/* piano subwindow */
static BOOL CALLBACK psgtoy_sub_piano(HWND wnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	if (!psgtoy.piano.wnd) return DefWindowProc(wnd,msg,wParam,lParam);

	switch (msg) {

		case WM_LBUTTONDOWN: {
			int j,cc=psgtoy.cur_chan;
			s16 b[0x80];

			if (psgtoy.drawing||psgtoy.popup_active||!psgtoy.enabled||psgtoy.p_playing) break;

			/* play sample */
			for (j=0;j<0x80;j++) b[j]=(custom_wave[cc][j<<1]+custom_wave[cc][j<<1|1])*(custom_amplitude[cc]/20.0);
			psgtoy.p_sample=sound_create_sample(b,0x100,TRUE);
			sound_play_sample(psgtoy.p_sample,TRUE);

			psgtoy.p_ticks=GetTickCount()+350; /* stop after 0.35s */

			psgtoy.p_playing=TRUE;
			PostMessage(GetParent(wnd),WM_NEXTDLGCTL,(WPARAM)GetDlgItem(GetParent(wnd),IDOK),TRUE);

			break;
		}

		/* popup menu */
		case WM_RBUTTONDOWN:
			psgtoy.prbdown=TRUE;
		case WM_MOUSEMOVE: {
			TRACKMOUSEEVENT tme;
			tme.cbSize=sizeof(TRACKMOUSEEVENT);
			tme.dwFlags=TME_LEAVE;
			tme.hwndTrack=wnd;
			tme.dwHoverTime=0;
			TrackMouseEvent(&tme);
			break;
		}
		case WM_RBUTTONUP:
			if (!psgtoy.prbdown||psgtoy.drawing||psgtoy.popup_active) break;
			PostMessage(GetParent(wnd),TOOL_POPUP,0,0);
		case WM_MOUSELEAVE:
			psgtoy.prbdown=FALSE;
			break;

		case WM_PAINT: {
			PAINTSTRUCT ps;
			HGDIOBJ obj;
			HDC dc;

			if (psgtoy.busy) break;

			dc=BeginPaint(wnd,&ps);

			obj=SelectObject(psgtoy.piano.bdc,psgtoy.piano.bmp);
			BitBlt(dc,0,0,257,20,psgtoy.piano.bdc,0,0,SRCCOPY);
			SelectObject(psgtoy.piano.bdc,obj);

			EndPaint(wnd,&ps);

			break;
		}

		default: break;
	}

	return DefWindowProc(wnd,msg,wParam,lParam);
}

/* wave subwindow */
static BOOL CALLBACK psgtoy_sub_wave(HWND wnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	if (!psgtoy.wave.wnd) return DefWindowProc(wnd,msg,wParam,lParam);

	switch (msg) {

		case WM_SETCURSOR:
			if (!psgtoy.enabled) break;
			SetCursor(psgtoy.cursor_cross);
			return 1;
			break;

		case WM_MOUSEMOVE:
			psgtoy.in_wave=input_mouse_in_client(wnd,NULL,&psgtoy.p_wave,TRUE);
			break;

		case WM_LBUTTONDOWN: case WM_RBUTTONDOWN:
			if (!psgtoy.enabled) break;

			/* start drawing */
			if (GetCapture()!=wnd) {
				RECT r;
				POINT p={0,0};

				/* remember starting position */
				ClientToScreen(wnd,&p);
				GetClientRect(wnd,&r);
				OffsetRect(&r,p.x,p.y);
				GetCursorPos(&p);
				psgtoy.p_wave_prev.x=p.x-r.left;
				psgtoy.p_wave_prev.y=p.y-r.top;

				SetCapture(wnd);
			}
			psgtoy.click=psgtoy.drawing=1;
			psgtoy.drawing+=(msg==WM_RBUTTONDOWN); /* rmb, smoothing */
			break;

		case WM_LBUTTONUP: case WM_RBUTTONUP:
			if (psgtoy.drawing==(1+(msg==WM_LBUTTONUP))) break; /* ignore if it was doing the other */
			/* end drawing */
			if (GetCapture()==wnd) ReleaseCapture();
		case WM_CAPTURECHANGED:
			psgtoy.drawing=0;
			break;

		case WM_PAINT: {
			PAINTSTRUCT ps;
			HGDIOBJ obj;
			HDC dc;

			if (psgtoy.busy) break;

			dc=BeginPaint(wnd,&ps);

			obj=SelectObject(psgtoy.wave.bdc,psgtoy.wave.bmp);
			BitBlt(dc,0,0,256,256,psgtoy.wave.bdc,0,0,SRCCOPY);
			SelectObject(psgtoy.wave.bdc,obj);

			EndPaint(wnd,&ps);

			break;
		}

		default: break;
	}

	return DefWindowProc(wnd,msg,wParam,lParam);
}

/* main window */
INT_PTR CALLBACK psgtoy_window( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch (msg) {

		case WM_INITDIALOG: {
			int i,x,y,o,m;

			psgtoy.busy=TRUE;

			/* output activity */
			for (i=0;i<3;i++) psgtoy.c_vol[i]=psgtoy.c_freq[i]=psgtoy.c_en[i]=-1;
			psgtoy.n_freq=psgtoy.e_freq=psgtoy.e_vol=psgtoy.e_shape=-1;

			/* init piano bmp area */
			psgtoy.piano.wnd=GetDlgItem(dialog,IDC_PSGTOY_PIANO);
			toolwindow_resize(psgtoy.piano.wnd,257,20);
			psgtoy.piano.wdc=GetDC(psgtoy.piano.wnd);
			psgtoy.piano.bdc=CreateCompatibleDC(psgtoy.piano.wdc);
			tool_init_bmi(&psgtoy.piano.info,257,20,32);
			psgtoy.piano.bmp=CreateDIBSection(psgtoy.piano.wdc,&psgtoy.piano.info,DIB_RGB_COLORS,(void*)&psgtoy.piano.screen,NULL,0);
			SetWindowLongPtr(psgtoy.piano.wnd,GWLP_WNDPROC,(LONG_PTR)psgtoy_sub_piano);

			/* draw initial piano */
			for (i=0;i<257;i++) psgtoy.piano.screen[i]=0x404040; /* top line */
			for (i=257;i<513;i++) psgtoy.piano.screen[i]=0x505060;
			psgtoy.piano.screen[513]=0x404040; /* 2nd line */
			for (i=514;i<4883;i+=257) psgtoy.piano.screen[i]=0x505060; /* left vertical line */
			for (i=4883;i<5140;i++) psgtoy.piano.screen[i]=0x202020; /* bottom line */

			for (o=0;o<10;o++) {
				/* keys */
				for (y=0;y<17;y++) {
					if (o==9) { m=4; i=28; }
					else { m=28; i=0; }
					for (x=0;x<m;x++) {
						psgtoy.piano.screen[515+x+o*28+y*257]=cdefgabc_bri_lut[y][x+i]<<16|cdefgabc_bri_lut[y][x+i]<<8|cdefgabc_bri_lut[y][x+i];
					}
				}

				/* octave number */
				for (y=0;y<5;y++) {
					for (x=0;x<3;x++) {
						if (tileview_get_raw_font(y<<4|o)>>(2-x)&1) psgtoy.piano.screen[3342+x+o*28+y*257]=PIANO_OCTAVEC;
					}
				}
			}

			psgtoy.note_prev[0]=psgtoy.note_prev[1]=psgtoy.note_prev[2]=1; /* O0C#, unused */
			psgtoy.p_playing=psgtoy.popup_active=FALSE;
			psgtoy.centcorrection_d=psgtoy.centcorrection_set=centcorrection;

			/* (actually, psglogger is always inactive on init) */
			if (movie_is_playing()|psglogger_is_active()) EnableWindow(GetDlgItem(dialog,IDC_PSGTOY_LOGSTART),FALSE);
			if (!psglogger_is_active()) EnableWindow(GetDlgItem(dialog,IDC_PSGTOY_LOGSTOP),FALSE);
			psgtoy.logging=psglogger_is_active()!=0;
			psgtoy.log_frames=psgtoy.log_done=psgtoy.logging_vis=psgtoy.logging_done_vis=0;
			psgtoy.log_ticks=GetTickCount();

			psgtoy.cursor_cross=LoadCursor(NULL,IDC_CROSS);

			/* init custom wave controls */
			psgtoy.custom_enabled_prev=psgtoy.enabled=custom_enabled;
			psgtoy.cur_chan=0; psgtoy.cur_chan_prev=psgtoy.preset_prev=-1;
			for (i=0;i<PSGTOY_PRESET_MAX;i++) SendDlgItemMessage(dialog,IDC_PSGTOY_PRESET,CB_ADDSTRING,0,(LPARAM)((LPCTSTR)custom_preset_name[i]));
			SendDlgItemMessage(dialog,IDC_PSGTOY_PRESET,CB_SETCURSEL,custom_preset[0],0);
			CheckRadioButton(dialog,IDC_PSGTOY_CAR,IDC_PSGTOY_CCR,IDC_PSGTOY_CAR);
			CheckDlgButton(dialog,IDC_PSGTOY_CUSTOM,custom_enabled?BST_CHECKED:BST_UNCHECKED);
			SendDlgItemMessage(dialog,IDC_PSGTOY_VSPIN,UDM_SETRANGE,0,(LPARAM)MAKELONG(CUSTOM_AMP_MAX,0));
			SendDlgItemMessage(dialog,IDC_PSGTOY_VEDIT,EM_LIMITTEXT,3,0);
			SetDlgItemInt(dialog,IDC_PSGTOY_VEDIT,custom_amplitude[0],FALSE);
			custom_enable_window(dialog);

			/* init custom wave bmp area */
			psgtoy.in_wave=FALSE; psgtoy.in_wave_prev=psgtoy.hl_prev=-1;
			psgtoy.p_wave.x=psgtoy.p_wave.y=0;
			psgtoy.p_wave_prev.x=psgtoy.p_wave_prev.y=0xff;
			ShowWindow(GetDlgItem(dialog,IDC_PSGTOY_XY),SW_HIDE);
			ShowWindow(GetDlgItem(dialog,IDC_PSGTOY_XYS),SW_HIDE);
			psgtoy.wave.wnd=GetDlgItem(dialog,IDC_PSGTOY_WAVE);
			toolwindow_resize(psgtoy.wave.wnd,256,256);
			psgtoy.wave.wdc=GetDC(psgtoy.wave.wnd);
			psgtoy.wave.bdc=CreateCompatibleDC(psgtoy.wave.wdc);
			tool_init_bmi(&psgtoy.wave.info,256,256,8);

			/* palette */
			#define PAL(x)								\
				i=0;								\
				psgtoy.wave.info.bmiColors[i].rgbRed=(BYTE)((x)>>16&0xff);	\
				psgtoy.wave.info.bmiColors[i].rgbGreen=(BYTE)((x)>>8&0xff);	\
				psgtoy.wave.info.bmiColors[i++].rgbBlue=(BYTE)((x)&0xff)

			PAL(TOOL_DBM_EMPTY);	/* 0, bg */
			PAL(0xffb8a0);			/* 1, A wave */
			PAL(0x606478);			/* 2, axes */
			PAL(0xb0ffc8);			/* 3, B wave */
			PAL(0x404050);			/* 4, grid */
			PAL(0x8080ff);			/* 5, C wave */
			/* lighter background when disabled */
			PAL(TOOL_DBM_EMPTY|0x808080);
			PAL(0); PAL(0x606478|0x808080);
			PAL(0); PAL(0x404050|0x808080);
			#undef PAL

			psgtoy.wave.bmp=CreateDIBSection(psgtoy.wave.wdc,&psgtoy.wave.info,DIB_RGB_COLORS,(void*)&psgtoy.wave.screen,NULL,0);
			SetWindowLongPtr(psgtoy.wave.wnd,GWLP_WNDPROC,(LONG_PTR)psgtoy_sub_wave);

			psgtoy.sample=psgtoy.p_sample=NULL;
			psgtoy.pkey_prev=0x8000;
			psgtoy.ckey_prev=0xc000;
			psgtoy.opened=psgtoy.playing=psgtoy.click=psgtoy.drawing=psgtoy.prbdown=FALSE;
			toolwindow_setpos(dialog,TOOL_WINDOW_PSGTOY,MAIN_PW_OUTERL,MAIN_PW_CENTER,-8,0,0);

			psgtoy.busy=FALSE;

			return 1;
			break;
		}

		case WM_CLOSE:
			if (tool_get_window(TOOL_WINDOW_PSGTOY)) {

				GdiFlush();
				psgtoy.busy=TRUE;

				/* stop psglogger */
				if (psgtoy.logging) {
					int i=MessageBox(dialog,"Save PSG log before closing?","meisei",MB_ICONEXCLAMATION|(tool_is_quitting()?MB_YESNO:(MB_YESNOCANCEL|MB_DEFBUTTON3)));
					if (i==IDCANCEL) return 1; /* don't close dialog */
					stop_psglog(dialog,i==IDYES);
				}

				toolwindow_savepos(dialog,TOOL_WINDOW_PSGTOY);
				main_menu_check(IDM_PSGTOY,FALSE);

				sound_clean_sample(psgtoy.sample); psgtoy.sample=NULL;
				sound_clean_sample(psgtoy.p_sample); psgtoy.p_sample=NULL;

				/* clean up piano bmp */
				psgtoy.piano.screen=NULL;
				if (psgtoy.piano.bmp) { DeleteObject(psgtoy.piano.bmp); psgtoy.piano.bmp=NULL; }
				if (psgtoy.piano.bdc) { DeleteDC(psgtoy.piano.bdc); psgtoy.piano.bdc=NULL; }
				if (psgtoy.piano.wdc) { ReleaseDC(psgtoy.piano.wnd,psgtoy.piano.wdc); psgtoy.piano.wdc=NULL; }
				psgtoy.piano.wnd=NULL;

				/* clean up wave bmp */
				psgtoy.wave.screen=NULL;
				if (psgtoy.wave.bmp) { DeleteObject(psgtoy.wave.bmp); psgtoy.wave.bmp=NULL; }
				if (psgtoy.wave.bdc) { DeleteDC(psgtoy.wave.bdc); psgtoy.wave.bdc=NULL; }
				if (psgtoy.wave.wdc) { ReleaseDC(psgtoy.wave.wnd,psgtoy.wave.wdc); psgtoy.wave.wdc=NULL; }
				psgtoy.wave.wnd=NULL;

				main_menu_enable(IDM_PSGTOY,TRUE);
				DestroyWindow(dialog);
				tool_reset_window(TOOL_WINDOW_PSGTOY);
			}

			break;

		case WM_CTLCOLORSTATIC: {
			UINT i=GetDlgCtrlID((HWND)lParam);

			if (psgtoy.wave.wnd&&(i==IDC_PSGTOY_ACTAT)|(i==IDC_PSGTOY_ACTBT)|(i==IDC_PSGTOY_ACTCT)) {
				/* set to channel colour */
				switch (i) {
					case IDC_PSGTOY_ACTAT: SetTextColor((HDC)wParam,RGB(160,0,0)); break;
					case IDC_PSGTOY_ACTBT: SetTextColor((HDC)wParam,RGB(0,140,0)); break;
					case IDC_PSGTOY_ACTCT: SetTextColor((HDC)wParam,RGB(0,0,170)); break;

					default: break;
				}

				SetBkMode((HDC)wParam,TRANSPARENT);
				return (HBRUSH)GetStockObject(NULL_BRUSH) == NULL ? FALSE : TRUE;
			}

			break;
		}

		case WM_COMMAND:

			switch (LOWORD(wParam)) {

				/* start psglogger */
				case IDC_PSGTOY_LOGSTART:

					if (!psgtoy.wave.wnd) break;

					if (psglogger_start(dialog,&psgtoy.wave.wnd)>0) {
						psgtoy.log_ticks=GetTickCount();
						psgtoy.logging_vis=psgtoy.logging_done_vis=psgtoy.log_done=0;
						psgtoy.logging=TRUE;
					}

					PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDOK),TRUE);
					break;

				/* stop psglogger */
				case IDC_PSGTOY_LOGSTOP: {
					char t[0x10];

					if (!psgtoy.wave.wnd) break;

					sprintf(t," ");
					ShowWindow(GetDlgItem(dialog,IDC_PSGTOY_LOGINFO),SW_HIDE);
					SetDlgItemText(dialog,IDC_PSGTOY_LOGINFO,t);

					stop_psglog(dialog,TRUE);
					psgtoy.log_ticks=GetTickCount();
					psgtoy.logging_done_vis=TRUE;
					psgtoy.log_done=TRUE;
					PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDOK),TRUE);

					ShowWindow(GetDlgItem(dialog,IDC_PSGTOY_LOGINFO),SW_NORMAL);
					break;
				}

				/* load waveform */
				case IDC_PSGTOY_OPEN: {
					const char* filter="All Supported Files\0*.cw;*.cw3;*.wav\0Channel Waveform Files (*.cw)\0*.cw\0Channel Waveform Files (all channels, *.cw3)\0*.cw3\0WAV Files (8-bit mono PCM, *.wav)\0*.wav\0All Files (*.*)\0*.*\0\0";
					const char* title="Open Waveform";
					char fn[STRING_SIZE]={0};
					OPENFILENAME of;

					if (!psgtoy.wave.wnd||!psgtoy.enabled||psgtoy.drawing) break;

					psgtoy.in_wave=FALSE;

					memset(&of,0,sizeof(OPENFILENAME));

					of.lStructSize=sizeof(OPENFILENAME);
					of.hwndOwner=dialog;
					of.hInstance=MAIN->module;
					of.lpstrFile=fn;
					of.lpstrFilter=filter;
					of.lpstrTitle=title;
					of.nMaxFile=STRING_SIZE;
					of.nFilterIndex=psgtoy_open_fi;
					of.Flags=OFN_ENABLESIZING|OFN_HIDEREADONLY|OFN_FILEMUSTEXIST;
					of.lpstrInitialDir=strlen(psgtoy_dir)?psgtoy_dir:file->tooldir?file->tooldir:file->appdir;

					main_menu_enable(IDM_PSGTOY,FALSE); /* resource leak if forced to close */
					if (GetOpenFileName(&of)) {
						int size,i,fi;
						u8 data[0x400];
						FILE* fd=NULL;
						int success=FALSE;

						if (!psgtoy.wave.wnd) {
							/* cleaned up already (check again since time passed) */
							main_menu_enable(IDM_PSGTOY,TRUE);
							break;
						}

						memset(data,0,0x400);
						fi=psgtoy_open_fi=of.nFilterIndex;
						fi--; if (fi<1) fi=4; /* all supported == all files */

						if (strlen(fn)&&(size=file_open_custom(&fd,fn))>0) {
							if (size>0x400) size=0x400;
							if (size>0xff&&file_read_custom(fd,data,size)) {
								int cc=psgtoy.cur_chan;
								u32 l=0;

								/* detect file type */
								if (fi>3) {
									if (size==0x303) fi=2; /* cw3 */
									if (!memcmp(data,wav_header,4)&&size>0x101) fi=3; /* RIFF */
									else fi=1; /* cw */
								}

								switch (fi) {
									/* cw */
									case 1:
										memcpy(psgtoy.open_data,data,0x100);
										if (size==0x101) psgtoy.open_amplitude[0]=data[0x100];
										else psgtoy.open_amplitude[0]=custom_amplitude[cc];
										success=TRUE;
										break;

									/* cw3 */
									case 2:
										if (size==0x303) {
											for (i=0;i<3;i++) {
												memcpy(psgtoy.open_data+(i<<8),data+(i*0x101),0x100);
												psgtoy.open_amplitude[i]=data[0x100+(i*0x101)];
											}
											success=TRUE;
										}
										break;

									/* wav */
									default:
										memcpy(&l,data+WH_DATASIZE_OFFSET,4);
										memcpy(data+WH_FILESIZE_OFFSET,wav_header+WH_FILESIZE_OFFSET,4);
										memcpy(data+WH_RATE_OFFSET,wav_header+WH_RATE_OFFSET,8);
										memcpy(data+WH_DATASIZE_OFFSET,wav_header+WH_DATASIZE_OFFSET,4);

										if (size>=300&&l>=0x100&&!memcmp(data,wav_header,44)) {
											memcpy(psgtoy.open_data,data+44,0x100);
											psgtoy.open_amplitude[0]=custom_amplitude[cc];
											success=TRUE;
										}
										break;
								}
							}
						}

						file_close_custom(fd);

						if (success) psgtoy.opened=1+(fi==2);
						else LOG_ERROR_WINDOW(dialog,"Couldn't load waveform!");

						if (strlen(fn)&&of.nFileOffset) {
							fn[of.nFileOffset]=0; strcpy(psgtoy_dir,fn);
						}
					}

					PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDOK),TRUE);
					main_menu_enable(IDM_PSGTOY,TRUE);

					break;
				}

				/* save waveform */
				case IDC_PSGTOY_SAVE: {
					const char* filter="Channel Waveform File (*.cw)\0*.cw\0Channel Waveform File (all channels, *.cw3)\0*.cw3\0WAV File (8-bit mono PCM, *.wav)\0*.wav\0\0";
					const char* defext="cw";
					const char* title="Save Waveform As";
					char fn[STRING_SIZE]={0};
					OPENFILENAME of;

					if (!psgtoy.wave.wnd||!psgtoy.enabled||psgtoy.drawing) break;

					psgtoy.in_wave=FALSE;

					memset(&of,0,sizeof(OPENFILENAME));

					of.lStructSize=sizeof(OPENFILENAME);
					of.hwndOwner=dialog;
					of.hInstance=MAIN->module;
					of.lpstrFile=fn;
					of.lpstrDefExt=defext;
					of.lpstrFilter=filter;
					of.lpstrTitle=title;
					of.nMaxFile=STRING_SIZE;
					of.nFilterIndex=psgtoy_save_fi;
					of.Flags=OFN_ENABLESIZING|OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
					of.lpstrInitialDir=strlen(psgtoy_dir)?psgtoy_dir:file->tooldir?file->tooldir:file->appdir;

					main_menu_enable(IDM_PSGTOY,FALSE); /* resource leak if forced to close */
					if (GetSaveFileName(&of)) {
						int i,cc=psgtoy.cur_chan;
						u8 data[0x303];
						FILE* fd=NULL;

						if (!psgtoy.wave.wnd) {
							/* cleaned up already (check again since time passed) */
							main_menu_enable(IDM_PSGTOY,TRUE);
							break;
						}

						psgtoy_save_fi=of.nFilterIndex;

						switch (psgtoy_save_fi) {
							/* cw */
							case 1:
								memcpy(data,custom_wave[cc],0x100);
								data[0x100]=custom_amplitude[cc];
								if (!strlen(fn)||!file_save_custom(&fd,fn)||!file_write_custom(fd,data,0x101)) LOG_ERROR_WINDOW(dialog,"Couldn't save waveform!");
								break;

							/* cw3 */
							case 2:
								for (i=0;i<3;i++) {
									memcpy(data+(i*0x101),custom_wave[i],0x100);
									data[0x100+(i*0x101)]=custom_amplitude[i];
								}
								if (!strlen(fn)||!file_save_custom(&fd,fn)||!file_write_custom(fd,data,0x303)) LOG_ERROR_WINDOW(dialog,"Couldn't save waveforms!");
								break;

							/* wav */
							default:
								memcpy(data,wav_header,44);
								memcpy(data+44,custom_wave[cc],0x100);
								if (!strlen(fn)||!file_save_custom(&fd,fn)||!file_write_custom(fd,data,300)) LOG_ERROR_WINDOW(dialog,"Couldn't save WAV!");
								break;
						}

						file_close_custom(fd);
						if (strlen(fn)&&of.nFileOffset) {
							fn[of.nFileOffset]=0; strcpy(psgtoy_dir,fn);
						}
					}

					PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDOK),TRUE);
					main_menu_enable(IDM_PSGTOY,TRUE);

					break;
				}

				/* channel waveforms checkbox */
				case IDC_PSGTOY_CUSTOM:
					if (!psgtoy.wave.wnd||psgtoy.drawing||psgtoy.playing) break;
					custom_enabled=IsDlgButtonChecked(dialog,IDC_PSGTOY_CUSTOM)==BST_CHECKED;
					psg_custom_enable(custom_enabled);

					ShowWindow(GetDlgItem(dialog,IDC_PSGTOY_XY),(custom_enabled&psgtoy.in_wave)?SW_NORMAL:SW_HIDE);
					ShowWindow(GetDlgItem(dialog,IDC_PSGTOY_XYS),(custom_enabled&psgtoy.in_wave)?SW_NORMAL:SW_HIDE);
					custom_enable_window(dialog);
					break;

				/* current channel radiobutton */
				case IDC_PSGTOY_CAR: case IDC_PSGTOY_CBR: case IDC_PSGTOY_CCR:
					if (!psgtoy.wave.wnd||!psgtoy.enabled||psgtoy.drawing) break;
					if (IsDlgButtonChecked(dialog,IDC_PSGTOY_CCR)==BST_CHECKED) psgtoy.cur_chan=2;
					else if (IsDlgButtonChecked(dialog,IDC_PSGTOY_CBR)==BST_CHECKED) psgtoy.cur_chan=1;
					else psgtoy.cur_chan=0;
					break;

				/* preset combobox */
				case IDC_PSGTOY_PRESET: {
					int s;

					if (psgtoy.wave.wnd&&!psgtoy.drawing&&psgtoy.enabled&&(HIWORD(wParam))==CBN_SELCHANGE&&(s=SendDlgItemMessage(dialog,LOWORD(wParam),CB_GETCURSEL,0,0))!=CB_ERR) {
						custom_preset[psgtoy.cur_chan]=s;
					}
					break;
				}

				/* change value */
				case IDC_PSGTOY_VEDIT:
					if (psgtoy.wave.wnd&&!psgtoy.drawing&&psgtoy.enabled&&GetDlgItem(dialog,IDC_PSGTOY_VEDIT)) {
						int cc=psgtoy.cur_chan;
						u32 i=GetDlgItemInt(dialog,IDC_PSGTOY_VEDIT,NULL,FALSE);
						if (i>(u32)CUSTOM_AMP_MAX) { i=CUSTOM_AMP_MAX; SetDlgItemInt(dialog,IDC_PSGTOY_VEDIT,i,FALSE); }
						if (i!=(u32)custom_amplitude[cc]) custom_amplitude[cc]=i;

						/* set preset to custom if changed */
						if (custom_preset[cc]!=PSGTOY_PRESET_CUSTOM&&i!=(u32)custom_preset_amplitude[custom_preset[cc]]) {
							custom_preset[cc]=PSGTOY_PRESET_CUSTOM;
							SendDlgItemMessage(dialog,IDC_PSGTOY_PRESET,CB_SETCURSEL,custom_preset[cc],0);
						}
					}
					break;

				/* close dialog manually */
				case IDOK: case IDCANCEL:
					PostMessage(dialog,WM_CLOSE,0,0);
					break;

				default: break;
			} /* WM_COMMAND */

			break;

		case TOOL_REPAINT: {
			int c_vol[3],c_freq[3],c_en[3],c_note[3];
			const int n_freq=psg_regs[6]&0x1f;
			const int e_vol=tool_get_local_psg_e_vol()>>3;
			const int e_shape=psg_regs[13]&0xf;
			const int e_freq=psg_regs[12]<<8|psg_regs[11];
			int cc=psgtoy.cur_chan;
			int i,j,k,x,y,px,py,xyc,r=FALSE,p_r=FALSE;
			HWND h; int hen;
			u8* screen=psgtoy.wave.screen;

			if (!psgtoy.wave.wnd||psgtoy.busy) break;
			psgtoy.busy=TRUE;

			/* output activity */
			/* recalculate note position table */
			if (psgtoy.centcorrection_set!=centcorrection) {
				centcorrection=psgtoy.centcorrection_set;
				fill_act_notepos();
			}

			/* channels */
			for (i=0;i<3;i++) {
				/* note (fetch info only) */
				c_vol[i]=psg_regs[8+i]&0xf;
				c_freq[i]=(psg_regs[i<<1|1]<<8&0xf00)|psg_regs[i<<1];
				c_en[i]=(psg_regs[7]>>i&1)|(psg_regs[7]>>(i+2)&2)|(psg_regs[8+i]>>2&4);

				c_note[i]=act_notepos_lut[c_freq[i]]|(c_en[i]<<8&0x100);
				if (c_en[i]&4) c_note[i]|=(e_vol<<12);
				else c_note[i]|=(c_vol[i]<<12);
				if (c_freq[i]<8||c_en[i]&1||(c_note[i]&0xf000)==0) c_note[i]=0;
				c_note[i]&=pvis_mask[i];

				/* frequency */
				if (psgtoy.c_freq[i]!=c_freq[i]) {
					char t[0x10];
					sprintf(t,"$%03X",c_freq[i]);
					SetDlgItemText(dialog,IDC_PSGTOY_ACTAD+i,t);

					psgtoy.c_freq[i]=c_freq[i];
				}

				/* volume */
				if (psgtoy.c_vol[i]!=c_vol[i]) {
					char t[0x10];
					sprintf(t,"/  %X",c_vol[i]);
					SetDlgItemText(dialog,IDC_PSGTOY_ACTAV+i,t);

					psgtoy.c_vol[i]=c_vol[i];
				}

				/* enabled */
				if (psgtoy.c_en[i]!=c_en[i]) {
					// char t[0x10];
					char t[0x20];   // Warning -Wformat-overvlow
					char tt[0x10];
					char tn[0x10];
					char te[0x10]={0,0};
					if (c_en[i]&1) sprintf(tt," ");  else sprintf(tt,"t");
					if (c_en[i]&2) sprintf(tn,"  "); else sprintf(tn,"n");
					if (c_en[i]&4) sprintf(te,"e");
					sprintf( t, "%s%s%s", tt, tn, te );
					SetDlgItemText( dialog, IDC_PSGTOY_ACTAS + i, t );

					psgtoy.c_en[i]=c_en[i];
				}
			}

			/* noise frequency */
			if (psgtoy.n_freq!=n_freq) {
				char t[0x10];
				sprintf(t,"$%02X",n_freq);
				SetDlgItemText(dialog,IDC_PSGTOY_ACTND,t);

				psgtoy.n_freq=n_freq;
			}

			/* envelope frequency */
			if (psgtoy.e_freq!=e_freq) {
				char t[0x10];
				sprintf(t,"$%04X",e_freq);
				SetDlgItemText(dialog,IDC_PSGTOY_ACTED,t);

				psgtoy.e_freq=e_freq;
			}

			/* envelope shape */
			if (psgtoy.e_shape!=e_shape) {
				char t[0x10];
				sprintf(t,"$%X",e_shape);
				SetDlgItemText(dialog,IDC_PSGTOY_ACTES,t);

				psgtoy.e_shape=e_shape;
			}

			/* envelope volume */
			if (psgtoy.e_vol!=e_vol) {
				char t[0x10];
				sprintf(t,"/  %X",e_vol);
				SetDlgItemText(dialog,IDC_PSGTOY_ACTEV,t);

				psgtoy.e_vol=e_vol;
			}

			GdiFlush();

			/* update piano */
			if (c_note[0]!=psgtoy.note_prev[0]||c_note[1]!=psgtoy.note_prev[1]||c_note[2]!=psgtoy.note_prev[2]) {
				int base[3];
				int bc,bcc,l,m,n;
				const int bt[]={0xef,0xeb,0xe7,0xe2,0xdc,0xd5,0xcc,0xc2,0xb6,0xa7,0x96,0x80,0x67,0x49,0x25,0};

				for (i=0;i<3;i++) {
					/* erase previous */
					j=psgtoy.note_prev[i]&0xf;
					for (x=0;x<3;x++) {
						for (y=cdefgabc_pos_lut[j][1+x];y<cdefgabc_pos_lut[j][4+x];y++) {
							k=515+cdefgabc_pos_lut[j][0]+x+(psgtoy.note_prev[i]>>4&0xf)*28+y*257;
							if (psgtoy.piano.screen[k]!=PIANO_OCTAVEC) psgtoy.piano.screen[k]=cdefgabc_bri_lut[y][x+cdefgabc_pos_lut[j][0]]<<16|cdefgabc_bri_lut[y][x+cdefgabc_pos_lut[j][0]]<<8|cdefgabc_bri_lut[y][x+cdefgabc_pos_lut[j][0]];
						}
					}

					/* get base colour */
					if (c_note[i]) base[i]=(0xff-bt[c_note[i]>>12&0xf])<<((2-i)<<3);
					else base[i]=0;
				}

				/* draw notes */
				for (i=0;i<3;i++) {
					bc=base[i];

					/* combine same notes */
					for (j=1;j<3;j++) {
						if ((c_note[i]&0xff)==(c_note[(i+j)%3]&0xff)) bc|=base[(i+j)%3];
					}

					/* adjust white key brightness */
					j=c_note[i]&0xf;
					if (!(cdefgabc_pos_lut[j][0]&2)&&bc) {
						if ((bc>>16&0xff)>=(bc>>8&0xff)&&(bc>>16&0xff)>=(bc&0xff)) k=bc>>16&0xff;
						else if ((bc>>8&0xff)>=(bc&0xff)) k=bc>>8&0xff;
						else k=bc&0xff;

						k^=0xff; k=k<<16|k<<8|k;
						bc+=k;

						/* invert if all the same */
						if ((bc&0xff)==(bc>>8&0xff)&&(bc&0xff)==(bc>>16&0xff)) {
							bc^=0xffffff;
							if (bc==0) bc=0x010101;
						}
					}

					/* draw */
					for (x=0;x<3;x++) {
						for (y=cdefgabc_pos_lut[j][1+x];y<cdefgabc_pos_lut[j][4+x];y++) {
							l=cdefgabc_pos_lut[j][0]+x;
							k=515+l+(c_note[i]>>4&0xf)*28+y*257;
							if (bc&&psgtoy.piano.screen[k]!=PIANO_OCTAVEC) {
								bcc=bc;
								if (cdefgabc_pos_lut[j][0]&2) {
									/* black key */
									m=cdefgabc_bri_lut[y-(y>0)][l];
									n=((bcc>>16)&0xff)+m; CLAMP(n,0,0xff); bcc=(bcc&0xffff)|n<<16;
									n=((bcc>>8)&0xff)+m; CLAMP(n,0,0xff); bcc=(bcc&0xff00ff)|n<<8;
									n=(bcc&0xff)+m; CLAMP(n,0,0xff); bcc=(bcc&0xffff00)|n;
								}
								else {
									/* white key */
									m=cdefgabc_bri_lut[y-(y>10)][l]^0xff;
									n=((bcc>>16)&0xff)-m; CLAMP(n,0,0xff); bcc=(bcc&0xffff)|n<<16;
									n=((bcc>>8)&0xff)-m; CLAMP(n,0,0xff); bcc=(bcc&0xff00ff)|n<<8;
									n=(bcc&0xff)-m; CLAMP(n,0,0xff); bcc=(bcc&0xffff00)|n;
								}
								psgtoy.piano.screen[k]=bcc;
							}
						}
					}
				}

				psgtoy.note_prev[0]=c_note[0];
				psgtoy.note_prev[1]=c_note[1];
				psgtoy.note_prev[2]=c_note[2];

				p_r=TRUE;
			}

			/* logger */
			if (psgtoy.logging) {
				/* show logging status */
				DWORD ticks=GetTickCount();
				if (ticks>psgtoy.log_ticks) {
					int i=psgtoy.logging_vis;
					char t[0x10];

					sprintf(t,"Logging");
					while (i--) strcat(t,".");
					psgtoy.logging_vis++;
					if (psgtoy.logging_vis==4) psgtoy.logging_vis=0;

					SetDlgItemText(dialog,IDC_PSGTOY_LOGINFO,t);
					psgtoy.log_ticks=ticks+600; /* 0.6 seconds */
				}
			}
			else if (psgtoy.log_done) {
				/* show done status */
				if (psgtoy.log_frames) {
					DWORD ticks=GetTickCount();
					if (ticks>psgtoy.log_ticks) {
						if (psgtoy.logging_done_vis) {
							char t[0x100];
							psgtoy.log_ticks=ticks+4000; /* 4 seconds */
							psgtoy.logging_done_vis=0;

							sprintf(t,"Saved %d frames",psgtoy.log_frames);
							SetDlgItemText(dialog,IDC_PSGTOY_LOGINFO,t);
						}
						else psgtoy.log_frames=0;
					}
				}
				else {
					char t[0x10];
					sprintf(t," ");
					SetDlgItemText(dialog,IDC_PSGTOY_LOGINFO,t);
					psgtoy.log_done=FALSE;
				}
			}


			/* custom wave */
			/* coordinates */
			if (psgtoy.in_wave) psgtoy.in_wave&=input_mouse_in_client(psgtoy.wave.wnd,dialog,&psgtoy.p_wave,GetForegroundWindow()==dialog);

			x=psgtoy.p_wave.x; y=psgtoy.p_wave.y;
			px=psgtoy.p_wave_prev.x; py=psgtoy.p_wave_prev.y;
			/* make sure x,y is in bounds */
			CLAMP(x,0,255); CLAMP(px,0,255);
			CLAMP(y,0,255); CLAMP(py,0,255);
			xyc=(x!=px)|(y!=py);

			if (custom_enabled&&psgtoy.in_wave!=psgtoy.in_wave_prev&&!(psgtoy.drawing&(psgtoy.in_wave!=1))) {
				psgtoy.in_wave_prev=psgtoy.in_wave;
				ShowWindow(GetDlgItem(dialog,IDC_PSGTOY_XY),psgtoy.in_wave_prev?SW_NORMAL:SW_HIDE);
				ShowWindow(GetDlgItem(dialog,IDC_PSGTOY_XYS),psgtoy.in_wave_prev?SW_NORMAL:SW_HIDE);
			}
			if (xyc) {
				char t[0x10];
				sprintf(t,"(%d,$%02X)",x,y^0xff);
				SetDlgItemText(dialog,IDC_PSGTOY_XY,t);
			}

			/* current sample on X */
			if (psgtoy.hl_prev!=custom_wave[cc][x]) {
				char t[0x10];
				psgtoy.hl_prev=custom_wave[cc][x];
				sprintf(t,"S: $%02X",psgtoy.hl_prev);
				SetDlgItemText(dialog,IDC_PSGTOY_XYS,t);
			}

			/* different channel */
			if (cc!=psgtoy.cur_chan_prev&&!psgtoy.drawing) {
				psgtoy.cur_chan_prev=cc;
				SetDlgItemInt(dialog,IDC_PSGTOY_VEDIT,custom_amplitude[cc],FALSE);
				SendDlgItemMessage(dialog,IDC_PSGTOY_PRESET,CB_SETCURSEL,custom_preset[cc],0);

				r=TRUE;
			}

			/* opened file */
			if (psgtoy.opened) {

				k=custom_preset[cc];

				if (psgtoy.opened&2) {
					/* 3 channels */
					for (i=0;i<3;i++) {
						/* copy */
						memcpy(custom_wave[i],psgtoy.open_data+(i<<8),0x100);
						custom_wave_changed[i]=TRUE;
						custom_amplitude[i]=psgtoy.open_amplitude[i];

						/* set preset */
						for (j=0;j<PSGTOY_PRESET_CUSTOM;j++) {
							if (!memcmp(custom_wave[i],custom_preset_wave[j],0x100)&&custom_amplitude[i]==custom_preset_amplitude[j]) break;
						}
						custom_preset[i]=j;
					}
				}
				else {
					/* copy */
					memcpy(custom_wave[cc],psgtoy.open_data,0x100);
					custom_wave_changed[cc]=TRUE;
					custom_amplitude[cc]=psgtoy.open_amplitude[0];

					/* set preset */
					for (j=0;j<PSGTOY_PRESET_CUSTOM;j++) {
						if (!memcmp(custom_wave[cc],custom_preset_wave[j],0x100)&&custom_amplitude[cc]==custom_preset_amplitude[j]) break;
					}
					custom_preset[cc]=j;
				}

				psgtoy.opened=FALSE;

				/* set preset combobox */
				if (custom_preset[cc]!=k) SendDlgItemMessage(dialog,IDC_PSGTOY_PRESET,CB_SETCURSEL,custom_preset[cc],0);

				SetDlgItemInt(dialog,IDC_PSGTOY_VEDIT,custom_amplitude[cc],FALSE);

				r=TRUE;
			}

			/* different preset */
			i=custom_preset[cc];
			if (i!=psgtoy.preset_prev&&!psgtoy.drawing) {
				psgtoy.preset_prev=i;

				/* copy preset data */
				if (i!=PSGTOY_PRESET_CUSTOM) {
					memcpy(custom_wave[cc],custom_preset_wave[i],0x100);
					custom_amplitude[cc]=custom_preset_amplitude[i];
					custom_wave_changed[cc]=TRUE;
					SetDlgItemInt(dialog,IDC_PSGTOY_VEDIT,custom_amplitude[cc],FALSE);

					r=TRUE;
				}
			}

			h=GetFocus();
			hen=(h!=GetDlgItem(dialog,IDC_PSGTOY_PRESET))&(h!=GetDlgItem(dialog,IDC_PSGTOY_VEDIT));

			/* play/stop wave */
			i=GetAsyncKeyState(0x50)&0x8000; /* p */
			if (i&&!psgtoy.pkey_prev&&dialog==GetForegroundWindow()&&!psgtoy.drawing&&psgtoy.enabled&&!psgtoy.p_playing&&hen) {
				/* play */
				s16 b[0x80];

				for (j=0;j<0x80;j++) b[j]=(custom_wave[cc][j<<1]+custom_wave[cc][j<<1|1])*(custom_amplitude[cc]/20.0);
				psgtoy.sample=sound_create_sample(b,0x100,TRUE);
				sound_play_sample(psgtoy.sample,TRUE);

				psgtoy.playing=TRUE;
				custom_enable_window(dialog);
			}
			else if (!i&&psgtoy.playing) {
				/* stop playing */
				sound_clean_sample(psgtoy.sample);
				psgtoy.sample=NULL;

				psgtoy.playing=FALSE;
				custom_enable_window(dialog);
			}
			psgtoy.pkey_prev=i;
			/* stop sample played from pianoclick */
			if (psgtoy.p_playing&&GetTickCount()>psgtoy.p_ticks) {
				sound_clean_sample(psgtoy.p_sample);
				psgtoy.p_sample=NULL;

				psgtoy.p_playing=FALSE;
			}

			/* copy/paste */
			i=(GetAsyncKeyState(0x43)&0x8000)|(GetAsyncKeyState(0x56)>>1&0x4000); /* c/v */
			if (i&&!psgtoy.ckey_prev&&dialog==GetForegroundWindow()&&!psgtoy.drawing&&psgtoy.enabled&&hen) {
				if (i&0x8000) {
					/* copy */
					memcpy(psgtoy.copy_data,custom_wave[cc],0x100);
					psgtoy.paste_valid=TRUE;
				}
				else if (psgtoy.paste_valid) {
					/* paste */
					memcpy(custom_wave[cc],psgtoy.copy_data,0x100);
					custom_wave_changed[cc]=TRUE;

					/* set preset to custom */
					if (custom_preset[cc]!=PSGTOY_PRESET_CUSTOM) {
						custom_preset[cc]=PSGTOY_PRESET_CUSTOM;
						SendDlgItemMessage(dialog,IDC_PSGTOY_PRESET,CB_SETCURSEL,custom_preset[cc],0);
					}

					r=TRUE;
				}
			}
			psgtoy.ckey_prev=i;

			/* drawing wave */
			if (xyc|psgtoy.click&&psgtoy.drawing&&!r) {
				int xs,xd;
				if (x<px) { xs=x; xd=px; }
				else { xs=px; xd=x; }
				xs-=(xs!=0); xd+=(xd!=0xff);

				/* erase */
				j=(xd-xs)+1;
				for (i=xs;i<0x10000;i+=0x100) memset(screen+i,0,j);

				/* draw */
				if (psgtoy.drawing==2) {
					if (x==0||px==0) custom_wave[cc][0]=custom_wave[cc][xd];
					if (x==0xff||px==0xff) custom_wave[cc][0xff]=custom_wave[cc][xs];
					bresenham(cc,xs,custom_wave[cc][xs]^0xff,xd,custom_wave[cc][xd]^0xff);
				}
				else {
					bresenham(cc,px,py,x,y);
					custom_wave[cc][px]=py^0xff;
					custom_wave[cc][x]=y^0xff;
					xs+=(xs!=0); xd-=(xd!=0xff);
				}
				xs++; xd--;

				/* read new values bottom to top */
				while (xs<=xd) {
					i=0xff00+xs; j=0;
					while (i>=0) {
						if (screen[i]&1) break;
						i-=0x100; j++;
					}
					custom_wave[cc][xs++]=j;
				}

				custom_wave_changed[cc]=TRUE;

				/* set preset to custom */
				if (custom_preset[cc]!=PSGTOY_PRESET_CUSTOM) {
					custom_preset[cc]=PSGTOY_PRESET_CUSTOM;
					SendDlgItemMessage(dialog,IDC_PSGTOY_PRESET,CB_SETCURSEL,custom_preset[cc],0);
				}

				if (psgtoy.click) PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDOK),TRUE);

				r=TRUE;
			}

			if (psgtoy.custom_enabled_prev!=custom_enabled) {
				r=TRUE;
				psgtoy.custom_enabled_prev=custom_enabled;
			}

			/* redraw */
			if (r) {
				if (custom_enabled) j=0;
				else j=6; /* lighter background when disabled */

				/* background */
				memset(screen,0+j,0x10000);

				/* grid */
				for (i=0xf00;i<0x10000;i+=0x1000) memset(screen+i,4+j,0x100); /* x */
				for (i=0;i<0x10000;i+=0x10) screen[i]=4+j; /* y */

				/* axes */
				memset(screen+0x7f00,2+j,0x100); /* x */
				for (i=0x80;i<0x10000;i+=0x100) screen[i]=2+j; /* y */

				/* wave */
				for (i=0;i<0xff;i++) bresenham(cc,i,custom_wave[cc][i]^0xff,i+1,custom_wave[cc][i+1]^0xff);
			}

			psgtoy.p_wave_prev.x=x; psgtoy.p_wave_prev.y=y;
			psgtoy.click=psgtoy.busy=FALSE;

			if (p_r) InvalidateRect(psgtoy.piano.wnd,NULL,FALSE);
			if (r) InvalidateRect(psgtoy.wave.wnd,NULL,FALSE);

			break;
		} /* TOOL_REPAINT */

		case TOOL_POPUP: {
			HMENU menu,popup;
			UINT id;
			int i,gray;

			if (!psgtoy.piano.wnd||psgtoy.popup_active) break;
			if ((menu=LoadMenu(MAIN->module,"psgtoypianopopup"))==NULL) break;

			psgtoy.popup_active=TRUE;
			psgtoy.in_wave=FALSE;

			for (i=0;i<3;i++) CheckMenuItem(menu,IDM_PSGTOYPIANO_A+i,pvis_mask[i]?MF_CHECKED:MF_UNCHECKED);
			gray=(psglogger_is_active()!=0)&(psglogger_get_fi()!=PSGLOGGER_TYPE_YM);
			if (gray) EnableMenuItem(menu,IDM_PSGTOYPIANO_CENT,MF_GRAYED);

			popup=GetSubMenu(menu,0);
			psgtoy.popup_p.x=psgtoy.popup_p.y=0;
			GetCursorPos(&psgtoy.popup_p);

			#define CLEAN_MENU() if (menu) { DestroyMenu(menu); menu=NULL; } psgtoy.popup_active=FALSE

			id=TrackPopupMenuEx(popup,TPM_LEFTALIGN|TPM_TOPALIGN|TPM_NONOTIFY|TPM_RETURNCMD|TPM_RIGHTBUTTON,(int)psgtoy.popup_p.x,(int)psgtoy.popup_p.y,dialog,NULL);
			switch (id) {
				/* show a/b/c checkbox */
				case IDM_PSGTOYPIANO_A: case IDM_PSGTOYPIANO_B: case IDM_PSGTOYPIANO_C:
					pvis_mask[id-IDM_PSGTOYPIANO_A]^=0xffff;
					break;

				case IDM_PSGTOYPIANO_CENT:
					if (gray) break;

					CLEAN_MENU();
					if ( DialogBox( MAIN->module, MAKEINTRESOURCE(IDD_PSGTOY_CENT), dialog, (DLGPROC)psgtoy_centoffset_dialog ) == 1 ) {
						if ((psglogger_is_active()!=0)&(psglogger_get_fi()!=PSGLOGGER_TYPE_YM)) break; /* shouldn't happen */
						psgtoy.centcorrection_set=psgtoy.centcorrection_d;
					}

					break;

				/* cancel */
				default: break;
			}

			PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDOK),TRUE);

			CLEAN_MENU();
			#undef CLEAN_MENU

			break;
		}

		case TOOL_MENUCHANGED:
			if (!psgtoy.wave.wnd) break;
			EnableWindow(GetDlgItem(dialog,IDC_PSGTOY_LOGSTART),(movie_is_playing()|psglogger_is_active())==0);
			EnableWindow(GetDlgItem(dialog,IDC_PSGTOY_LOGSTOP),psglogger_is_active()!=0);
			break;
	}

	return 0;
}


/* init/clean (only once) */
static int parsehexstring(char* c,u8* data,u32 size)
{
	int i; u32 j;
	char x[0x10];

	if (!c||!data||!size||strlen(c)!=(size<<1)) return FALSE;

	x[2]=0;

	/* hex string to values */
	for (i=0;(u32)i<size;i++) {
		x[0]=c[i<<1]; x[1]=c[i<<1|1];
		j=0; sscanf(x,"%02X",&j); data[i]=j;
		j=1; sscanf(x,"%02X",&j); if (j!=data[i]) return FALSE;
	}

	return TRUE;
}

void psgtoy_init(void)
{
	const int bar0[]={0xff,0xf6,0,1,2,0xc0,0,1,2,0xc0,0xf6,0x40,0xff,0xf6,0,1,2,0xc0,0,1,2,0xc0,0,1,2,0xc0,0xf6,0x40,0xff,0xf6,0xf6,0x40};
	const int pos0[]={0,0,2,4,6,8,12,14,16,18,20,22,24,28,28};
	int p3[3];
	int size,i,j,k;
	u8 data[0x100];
	u8* pd;
	char* c;
	u8 c_s[0x10000];
	u8 c_d[0x10000];
	uLongf destlen=0xffff;

	psg_regs=tool_get_local_psg_regs_ptr();
	memset(&psgtoy,0,sizeof(psgtoy));

	/* fill activity note frequency position table */
	fill_act_notepos();

	/* fill piano brightness table */
	/* top part (black+white) */
	#define FILLTOP(x)											\
		for (j=0;j<32;j++) {									\
			if (bar0[j]<3) cdefgabc_bri_lut[x][j]=p3[bar0[j]];	\
			else cdefgabc_bri_lut[x][j]=bar0[j];				\
		}
	p3[0]=0x40; p3[1]=p3[2]=0x20;
	for (i=0;i<7;i++) { FILLTOP(i) }
	p3[0]=0x50; FILLTOP(7)
	p3[0]=0x60; FILLTOP(8)
	p3[0]=p3[1]=p3[2]=0; FILLTOP(9)

	/* bottom part (white) */
	#define FILLBOT(x)											\
		for (i=0;i<8;i++) {										\
			for (j=0;j<4;j++) {									\
				if (j<3) cdefgabc_bri_lut[x][i<<2|j]=p3[j];		\
				else cdefgabc_bri_lut[x][i<<2|j]=0x40;			\
			}													\
		}
	p3[0]=0xff; p3[1]=p3[2]=0xf6;
	for (k=10;k<16;k++) { FILLBOT(k) }
	p3[0]=0x90; p3[1]=p3[2]=0x80; FILLBOT(16)
	#undef FILLTOP
	#undef FILLBOT

	/* fill piano coordinates table */
	for (i=0;i<13;i++) {
		cdefgabc_pos_lut[i][0]=pos0[i+1];
		if (cdefgabc_pos_lut[i][0]&2) {
			/* black key */
			cdefgabc_pos_lut[i][1]=cdefgabc_pos_lut[i][2]=cdefgabc_pos_lut[i][3]=0;
			cdefgabc_pos_lut[i][4]=cdefgabc_pos_lut[i][5]=cdefgabc_pos_lut[i][6]=11;
		}
		else {
			/* white key */
			cdefgabc_pos_lut[i][1]=cdefgabc_pos_lut[i][2]=cdefgabc_pos_lut[i][3]=0;
			cdefgabc_pos_lut[i][4]=cdefgabc_pos_lut[i][5]=cdefgabc_pos_lut[i][6]=18;
			if (pos0[i]&2) cdefgabc_pos_lut[i][1]=10;
			if (pos0[i+2]&2) cdefgabc_pos_lut[i][3]=10;
		}
	}

	/* load presets */
	size=0;
	pd=file_from_resource(ID_PSG_PRESETS,&size);
	if (!pd||size!=PSGTOY_PRESET_CSIZE) {
		file_from_resource_close();
		LOG(LOG_MISC|LOG_ERROR,"Can't load PSG presets!\n");
		exit(1);
	}
	file_from_resource_close();
	memcpy(c_s,pd,PSGTOY_PRESET_CSIZE);
	if (uncompress(c_d,&destlen,c_s,PSGTOY_PRESET_CSIZE)!=Z_OK) {
		LOG(LOG_MISC|LOG_ERROR,"Can't decompress PSG presets!\n");
		exit(1);
	}

	j=0;
	for (i=0;i<PSGTOY_PRESET_MAX;i++) {
		/* wave */
		memcpy(custom_preset_wave[i],c_d+j,0x100); j+=0x100;

		/* amp */
		custom_preset_amplitude[i]=c_d[j++];

		/* name */
		size=strlen((char*)(c_d+j))+1;
		MEM_CREATE_N(custom_preset_name[i],size);
		memcpy(custom_preset_name[i],c_d+j,size);
		j+=size;
	}


	/* settings */
	custom_enabled=FALSE;	i=settings_get_yesnoauto(SETTINGS_CUSTOMWAVE); if (i==FALSE||i==TRUE) custom_enabled=i;
	centcorrection=0;		SETTINGS_GET_INT(settings_info(SETTINGS_CORRECTCENTS),&centcorrection);				CLAMP(centcorrection,-25,25);
	psgtoy_open_fi=1;		SETTINGS_GET_INT(settings_info(SETTINGS_FILTERINDEX_OPENCWAVE),&psgtoy_open_fi);	CLAMP(psgtoy_open_fi,1,5);
	psgtoy_save_fi=1;		SETTINGS_GET_INT(settings_info(SETTINGS_FILTERINDEX_SAVECWAVE),&psgtoy_save_fi);	CLAMP(psgtoy_save_fi,1,3);

	/* amplitude */
	c=NULL; SETTINGS_GET_STRING(settings_info(SETTINGS_CUSTOMWAVEAMP),&c);
	if (parsehexstring(c,data,3)) {
		for (i=0;i<3;i++) {
			custom_amplitude[i]=data[i];
			CLAMP(i,0,CUSTOM_AMP_MAX);
		}
	}
	else custom_amplitude[0]=custom_amplitude[1]=custom_amplitude[2]=100;
	MEM_CLEAN(c);

	/* waves */
	for (i=0;i<3;i++) {
		c=NULL; SETTINGS_GET_STRING(settings_info(SETTINGS_CUSTOMWAVEA+i),&c);
		if (parsehexstring(c,data,0x100)) {
			memcpy(custom_wave[i],data,0x100);

			/* find preset, custom if not found */
			for (j=0;j<PSGTOY_PRESET_CUSTOM;j++) {
				if (!memcmp(custom_wave[i],custom_preset_wave[j],0x100)&&custom_amplitude[i]==custom_preset_amplitude[j]) break;
			}
			custom_preset[i]=j;
		}
		else {
			/* set to default preset */
			memcpy(custom_wave[i],custom_preset_wave[PSGTOY_PRESET_DEFAULT],0x100);
			custom_amplitude[i]=custom_preset_amplitude[PSGTOY_PRESET_DEFAULT];
			custom_preset[i]=PSGTOY_PRESET_DEFAULT;
		}

		MEM_CLEAN(c);
	}
}

void psgtoy_clean(void)
{
	int i;

	for (i=0;i<PSGTOY_PRESET_MAX;i++) {
		MEM_CLEAN(custom_preset_name[i]);
	}
}
