/******************************************************************************
 *                                                                            *
 *                               "netplay.c"                                  *
 *                                                                            *
 ******************************************************************************/

#include "global.h"
#include "netplay.h"
#include "file.h"
#include "main.h"
#include "resource.h"
#include "msx.h"
#include "crystal.h"
#include "settings.h"
#include "mapper.h"
#include "version.h"
#include "reverse.h"
#include "cont.h"
#include "tape.h"
#include "zlib.h"

/*
    https://github.com/God-Weapon/SupraclientC
    SupraclientC
    This is unofficially updated version of Supraclient, based on original latest source v87.9 (03-30-2008).
    This version includes bug fixes and other improvements.
*/
#ifdef MEISEI_32BITS        // 32 BITS
    #ifdef MEISEI_ESP       // ESPAÑOL
        #define DLL_FILE    "kaillera/kailleraclient32_esp.dll"
    #else                   // INGLÉS
        #define DLL_FILE    "kaillera/kailleraclient32_eng.dll"
    #endif
#else                       // 64 BITS
    #ifdef MEISEI_ESP       // ESPAÑOL
        #define DLL_FILE    "kaillera/kailleraclient_esp.dll"
    #else                   // INGLÉS
        #define DLL_FILE    "kaillera/kailleraclient_eng.dll"
    #endif  // MEISEI_ESP
#endif      // MEISEI_32BITS

#define PLAY_MUTEX_TIME	7000 /* 7 seconds */

/* hack related */
#define SW_TITLE		"Kaillera Client v0.9"
#define CW_TITLE		"Kaillera v0.9:"
#define DLL_CRC			0x6b50c095

static struct {
	HINSTANCE dll;
	HANDLE thread;
	DWORD tid;

	u32 cont_p1_prev;
	u32 cont_p2_prev;

	int init_done;
	int keyboard_enabled; /* current player only */
	int dialog;
	int playing;
	int player;

	int enable_hack;
	HHOOK callwnd_hook;
	HHOOK getmsg_hook;
	HANDLE play_mutex;

} netplay;

static int netplay_init_done=FALSE;

typedef struct {
	char *appName;
	char *gameList;

	int (WINAPI *gameCallback)(char *game,int player,int numplayers);

	void (WINAPI *chatReceivedCallback)(char *nick,char *text);
	void (WINAPI *clientDroppedCallback)(char *nick,int playernb);

	void (WINAPI *moreInfosCallback)(char *gamename);
} kailleraInfos;

INT_PTR (WINAPI *kailleraGetVersion)(char*);            // WINAPI kailleraGetVersion(char *version);
INT_PTR (WINAPI *kailleraInit)(void);                   // WINAPI kailleraInit();
INT_PTR (WINAPI *kailleraShutdown)(void);               // WINAPI kailleraShutdown();
INT_PTR (WINAPI *kailleraSetInfos)(kailleraInfos*);     // WINAPI kailleraSetInfos(kailleraInfos *infos);
INT_PTR (WINAPI *kailleraSelectServerDialog)(HWND);     // WINAPI kailleraSelectServerDialog(HWND parent);
INT_PTR (WINAPI *kailleraModifyPlayValues)(void*,int);  // WINAPI kailleraModifyPlayValues(void *values, int size);
INT_PTR (WINAPI *kailleraChatSend)(char*);              // WINAPI kailleraChatSend(char *text);
INT_PTR (WINAPI *kailleraEndGame)(void);                // WINAPI kailleraEndGame();

static void netplay_end(void);

int __fastcall netplay_is_active(void) {
    return (netplay.dialog|netplay.playing)&netplay_init_done;
}

int netplay_keyboard_enabled(void) {
    return netplay.keyboard_enabled;
}

static void netplay_clean_kaillera(void)
{
	if (!netplay_init_done) return;

	netplay_end(); netplay.dialog=FALSE;
	kailleraShutdown(); // if kailleraModifyPlayValues was still busy, it may lock up here

	if (netplay.thread) { CloseHandle(netplay.thread); netplay.thread=NULL; }

	FreeLibrary(netplay.dll); netplay.dll=NULL;

	netplay_init_done=FALSE;
}

void netplay_clean(void)
{
	netplay_clean_kaillera();

	if (netplay.play_mutex) { CloseHandle(netplay.play_mutex); netplay.play_mutex=NULL; }
}

void netplay_init(void)
{
	int i;
	memset(&netplay,0,sizeof(netplay));

	netplay.keyboard_enabled=TRUE; i=settings_get_yesnoauto(SETTINGS_KEYNETPLAY); if (i==FALSE||i==TRUE) { netplay.keyboard_enabled=i; }

	if ((netplay.play_mutex=(CreateMutex(NULL,FALSE,NULL)))==NULL) {
		LOG(LOG_MISC|LOG_ERROR,"Can't create netplay mutex!\n"); exit(1);
	}
}

int netplay_init_kaillera(void)
{
	// file_setfile(&file->appdir,DLL_FILE,NULL,NULL);
	file_setfile(NULL,DLL_FILE,NULL,NULL);
	if (file_open()) netplay.enable_hack=file->crc32==DLL_CRC;
	file_close();

	// WINBASEAPI HMODULE WINAPI LoadLibrary(LPCSTR lpLibFileName);
	if ( (netplay.dll=LoadLibrary(file->filename) ) != NULL ) {

        // get functions
#ifdef MEISEI_32BITS
    if (
         (kailleraGetVersion=GetProcAddress(netplay.dll,"kailleraGetVersion@4"))!=NULL				    &&
		 (kailleraInit=GetProcAddress(netplay.dll,"kailleraInit@0"))!=NULL								&&
		 (kailleraShutdown=GetProcAddress(netplay.dll,"kailleraShutdown@0"))!=NULL						&&
		 (kailleraSetInfos=GetProcAddress(netplay.dll,"kailleraSetInfos@4"))!=NULL						&&
		 (kailleraSelectServerDialog=GetProcAddress(netplay.dll,"kailleraSelectServerDialog@4"))!=NULL	&&
		 (kailleraModifyPlayValues=GetProcAddress(netplay.dll,"kailleraModifyPlayValues@8"))!=NULL		&&
		 (kailleraChatSend=GetProcAddress(netplay.dll,"kailleraChatSend@4"))!=NULL						&&
		 (kailleraEndGame=GetProcAddress(netplay.dll,"kailleraEndGame@0"))!=NULL
      ) {
#else   // 64 Bits
    if (
         (kailleraGetVersion=GetProcAddress(netplay.dll,"kailleraGetVersion"))!=NULL				    &&
		 (kailleraInit=GetProcAddress(netplay.dll,"kailleraInit"))!=NULL								&&
		 (kailleraShutdown=GetProcAddress(netplay.dll,"kailleraShutdown"))!=NULL						&&
		 (kailleraSetInfos=GetProcAddress(netplay.dll,"kailleraSetInfos"))!=NULL						&&
		 (kailleraSelectServerDialog=GetProcAddress(netplay.dll,"kailleraSelectServerDialog"))!=NULL	&&
		 (kailleraModifyPlayValues=GetProcAddress(netplay.dll,"kailleraModifyPlayValues"))!=NULL		&&
		 (kailleraChatSend=GetProcAddress(netplay.dll,"kailleraChatSend"))!=NULL						&&
		 (kailleraEndGame=GetProcAddress(netplay.dll,"kailleraEndGame"))!=NULL
      ) {
#endif // MEISEI_32BITS

			netplay_init_done=TRUE;
			kailleraInit();
		} else {
			FreeLibrary(netplay.dll);
			LOG(LOG_MISC|LOG_WARNING,"couldn't initialise Kaillera\n");
		}

	} else {
		LOG(LOG_MISC|LOG_WARNING,"couldn't open %s\n",DLL_FILE);
	}

	return netplay_init_done;
}

/* Kaillera bugs:
- crashes if child window is closed while a game is in progress (fixed in callwnd_proc)
- locks up if main window is closed while child window is open (fixed in getmsg_proc)
- original kaillera is not compatible with WinXP styles, I'm using a client that fixes this already. This one,
  and the 2 hacks above are only applied to this client version, since others might behave differently

these require the user to do something less common or are abnormal. I left them alone, besides, they're harder to fix
- locks up (partial?) if the main window is accessed just before the server list gets downloaded
- crashes if you try to connect to a server while you are connected to one already
- may lock up when exiting the emulator during connection timeout/cancelling an active game
- may crash if the main server is down, or if there's no connection to the internet
  (to "fix" it, press the stop button before it can crash)
- may crash if kaillera.ini (c:\windows) is broken
- harmless/trivial: after opening the Kaillera window, until exiting the emulator, every child window of meisei
  will show the Bubble Bobble icon in the title bar, even common dialogs like messageboxes.. kinda funny actually :P
*/

static LRESULT CALLBACK callwnd_proc(int code,WPARAM wParam,LPARAM lParam)
{
	if (code==HC_ACTION&&lParam) {
		const PCWPSTRUCT msg=(PCWPSTRUCT)lParam;

		if (msg->message==WM_CLOSE) {
			char title[32];
			int size=GetWindowText(msg->hwnd,title,32);
			title[strlen(CW_TITLE)]='\0';

			// prevent crash if child window is closed while a game is in progress
			if (size&&strcmp(title,CW_TITLE)==0) {
				WaitForSingleObject(netplay.play_mutex,PLAY_MUTEX_TIME);
				netplay_end();
				ReleaseMutex(netplay.play_mutex);
			}
		}
	}

	return CallNextHookEx(netplay.callwnd_hook,code,wParam,lParam);
}

static LRESULT CALLBACK getmsg_proc(int code,WPARAM wParam,LPARAM lParam)
{
	if (code==HC_ACTION&&lParam) {
		MSG* msg=(MSG*)lParam;

		if (msg->message==WM_CLOSE) {
			char title[32];
			int size=GetWindowText(msg->hwnd,title,32);
			title[strlen(SW_TITLE)]='\0';

			// prevent infinite loop if main kaillera window is closed while its child window is open, info from Marty (Nestopia)
			if (size&&strcmp(title,SW_TITLE)==0) msg->message=WM_NULL;
		}
	}

	return CallNextHookEx(netplay.getmsg_hook,code,wParam,lParam);
}

static DWORD WINAPI netplay_thread(void*)
{
	u32 crc=0;
	u32 cc[0x100];
	_msx_state state;
	kailleraInfos info={
		"",
		"",
		netplay_starts,
		NULL,
		netplay_drop,
		NULL
	};

	char papp[0x100];
	char pgame[STRING_SIZE];
	char pname[STRING_SIZE] = {0};

	netplay.dialog=TRUE;

	/* disable pause */
	msx_set_paused(FALSE);
	main_menu_check(IDM_PAUSEEMU,FALSE);

	msx_wait();
	reverse_invalidate(); crystal->speed=0;
	crystal_set_mode(FALSE); /* disable 50/60hz hack if it's enabled */

	netplay.cont_p1_prev=cont_get_port(CONT_P1);
	netplay.cont_p2_prev=cont_get_port(CONT_P2);
	if (netplay.cont_p1_prev!=CONT_D_J||netplay.cont_p2_prev!=CONT_D_J) {
		LOG(LOG_MISC|LOG_WARNING,"netplay forced joysticks in ports\n");
		cont_insert(CONT_P1,CONT_D_J,FALSE,TRUE);
		cont_insert(CONT_P2,CONT_D_J,FALSE,TRUE);
	}

	tape_rewind_cur(); /* force tape rewind */

	main_menu_update_enable();

	/* set info */
	sprintf(papp,"%s %s",VERSION_NAME_S,VERSION_NUMBER_S);
	mapper_get_current_name(pname);

	msx_get_state(&state);
	msx_wait_done();
	memset(pgame,0,STRING_SIZE);

	/* combine checksums */
	memset(cc,0,0x100*sizeof(u32));
	cc[0]=tape_get_crc_cur();
	cc[1]=state.bios_crc;
	cc[2]=state.cart1_crc;
	cc[3]=state.cart2_crc;
	cc[4]=VERSION_NUMBER_D^state.cart1_uid<<6^state.cart2_uid<<15^mapper_get_ramsize()<<24;
	cc[5]=(state.flags&MSX_FLAG_VF)^state.cpu_speed<<1^state.vdpchip_uid<<16^state.psgchip_uid<<21^((state.flags&MSX_FMASK_RAMSLOT)>>MSX_FSHIFT_RAMSLOT)<<27;
	mapper_get_extra_configs(0,mapper_get_uid_type(state.cart1_uid),&cc[6]);
	mapper_get_extra_configs(1,mapper_get_uid_type(state.cart2_uid),&cc[8]);
	crc=(u32)crc32(0,(u8*)cc,10*sizeof(u32));

	sprintf( pgame, "%s {%08x}", pname, crc );

	info.appName=papp;
	info.gameList=pgame;
	kailleraSetInfos(&info);

	/* enable bugfix hack */
	if (netplay.enable_hack) {
		netplay.callwnd_hook=SetWindowsHookEx(WH_CALLWNDPROC,(HOOKPROC)callwnd_proc,MAIN->module,netplay.tid);
		netplay.getmsg_hook=SetWindowsHookEx(WH_GETMESSAGE,(HOOKPROC)getmsg_proc,MAIN->module,netplay.tid);
	}

	kailleraSelectServerDialog(NULL);

	if (netplay.callwnd_hook) { UnhookWindowsHookEx(netplay.callwnd_hook); netplay.callwnd_hook=NULL; }
	if (netplay.getmsg_hook) { UnhookWindowsHookEx(netplay.getmsg_hook); netplay.getmsg_hook=NULL; }

	netplay_end();

	netplay.dialog=FALSE;

	main_menu_update_enable(); /* ungray */

	msx_wait();
	crystal_set_mode(FALSE); /* re-enable 50/60hz hack if it was enabled */
	cont_insert(CONT_P1,netplay.cont_p1_prev,FALSE,TRUE);
	cont_insert(CONT_P2,netplay.cont_p2_prev,FALSE,TRUE);
	msx_wait_done();

	return 0;
}

void netplay_open(void)
{
	if (netplay.playing) return;
	if (!netplay_init_done&&!netplay_init_kaillera()) return;

	if (netplay.thread) { CloseHandle(netplay.thread); netplay.thread=NULL; }

	/* in a separate thread to prevent program lockup if kaillera locks up */
	netplay.thread=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)(netplay_thread),NULL,0,&netplay.tid);
}

int WINAPI netplay_starts(char* name,int player,int numplayers)
{
    name = name;
	if (!netplay_init_done||!netplay.dialog) return 0;

	if (netplay.playing) return 0; /* loses connection, locks up if I handle this differently */

	if (numplayers==1) {
		LOG(LOG_MISC|LOG_WARNING,"can't netplay with only 1 player\n");
		kailleraEndGame(); return 0;
	}

	msx_wait(); msx_reset(TRUE); msx_wait_done();
	netplay.playing=TRUE;
	netplay.player=player;

	LOG(LOG_MISC,"netplay started, you are player %d\n",player);
    return 0;
}

static void netplay_end(void)
{
	if (!netplay.playing) return;

	LOG(LOG_MISC|LOG_WARNING,"lost netplay connection\n");

	netplay.playing=FALSE;
	kailleraEndGame();
}

void WINAPI netplay_drop(char* name,int player)
{
	if (player!=netplay.player&&player>2) {
		if (name&&strlen(name)) {
			char namec[0x100];
			int cs=strlen(name)+1;

			#define NAMEMAXLEN 16

			if (cs>0x100) cs=0x100;
			memcpy(namec,name,cs);
			if (cs>NAMEMAXLEN) {
				/* truncate */
				namec[NAMEMAXLEN-1]=namec[NAMEMAXLEN]=namec[NAMEMAXLEN+1]='.';
				namec[NAMEMAXLEN+2]='\0';
			}

			LOG(LOG_MISC|LOG_WARNING,"%s left the game\n",namec);
		}

		else LOG(LOG_MISC|LOG_WARNING,"#%d left the game\n",player);
	}

	else netplay_end();
}

/* joy orig: ..barldu .. l>r, u>d, same conversion as MSX Basic STICK(j) */
static const int joy2net[]={
	6<<2,	/* 0, rldu --> .ld. */
	6<<2,	/* 1, rld. --> .ld. */
	8<<2,	/* 2, rl.u --> .l.u */
	7<<2,	/* 3, rl.. --> .l.. */
	2<<2,	/* 4, r.du --> r..u */
	4<<2,	/* 5, r.d. */
	2<<2,	/* 6, r..u */
	3<<2,	/* 7, r... */
	8<<2,	/* 8, .ldu --> .l.u */
	6<<2,	/* 9, .ld. */
	8<<2,	/* a, .l.u */
	7<<2,	/* b, .l.. */
	1<<2,	/* c, ..du --> ...u */
	5<<2,	/* d, ..d. */
	1<<2,	/* e, ...u */
	0		/* f, .... */
};
static const int net2joy[]={0xf,0xe,6,7,5,0xd,9,0xb,0xa,0xf,0xf,0xf,0xf,0xf,0xf,0xf}; /* same, but backwards and pre-shift */

void netplay_frame( u8* key, int* keyextra, u8* joy )
{
	u8 data[0x100];
	int size=0;

	if (!netplay_init_done) return;

	WaitForSingleObject(netplay.play_mutex,PLAY_MUTEX_TIME);

	if (!netplay.playing) {
		ReleaseMutex(netplay.play_mutex);
		return;
	}

	/* set input */
	data[0]=data[2]=0x83; data[1]=data[3]=0xff;

	if (netplay.player<3) {
		u8 ckey[11];
		int i=11; while(i--) ckey[i]=key[i];

		data[0]=(joy[0]>>4&3)|joy2net[joy[0]&0xf]|(ckey[6]<<3&0x80); /* code key + joystick */

		if (netplay.keyboard_enabled) {
			data[1]=(ckey[8]&0xf1)|(ckey[6]<<1&0xe); /* right down up left . . . space, . . . . graph ctrl shift . */

			/* mask out keys */
			ckey[6]|=0x17; /* code, graph, ctrl, shift */
			ckey[8]|=0xf1; /* right, down, up, left, space */

			/* row 12 and keyextra (127,126,125,124) */
			if (~key[11]&2) data[0]=(data[0]&0x80)|127; /* jikkou */
			else if (~key[11]&8) data[0]=(data[0]&0x80)|126; /* torikeshi */
			else if (*keyextra&CONT_EKEY_SONYPAUSE) data[0]=(data[0]&0x80)|125; /* sony pause */

			else

			/* check for 1 extra key */
			for (i=0;i<11;i++) {
				if (ckey[i]!=0xff) {
					int j=0;
					while (ckey[i]&1) { ckey[i]>>=1; j++; }
					data[0]=(data[0]&0x80)|(36+(i<<3|j));
					break;
				}
			}
		}
		else data[0]|=0x80; /* code key */
	}

	size=kailleraModifyPlayValues((void*)data,2);
	ReleaseMutex(netplay.play_mutex);

	/* empty (at start) */
	if (size==0) {
		/* disable input for this frame */
		memset(key,0xff,16);
		*keyextra=0;
		joy[0]=joy[1]=0x3f;
		return;
	}

	/* error */
	else if (size<2||size>16) {
		netplay_end();
		return;
	}

	/* ok */
	else {
		/* fetch input, backwards from encoding */

		memset(key,0xff,12);
		*keyextra=0;
		key[8]=(data[1]&data[3])|0xe;
		key[6]=((data[1]&data[3])>>1&7)|((data[0]&data[2])>>3&0x10)|0xe8;

		/* joystick/extra key */
		#define JFETCH(p)																		\
			if ((data[p<<1]&0x7f)<36) joy[p]=(data[p<<1]<<4&0x30)|net2joy[data[p<<1]>>2&0xf];	\
			else {																				\
				joy[p]=0x3f;																	\
				switch (data[p<<1]&0x7f) {														\
					case 127: key[11]&=~2; break;												\
					case 126: key[11]&=~8; break;												\
					case 125: *keyextra=(*keyextra&~CONT_EKEY_SONYPAUSE)|CONT_EKEY_SONYPAUSE; break; \
					default: key[(data[p<<1]-36)>>3&0xf]&=~(1<<((data[p<<1]-36)&7)); break;		\
				}																				\
			}

		JFETCH(0) JFETCH(1)
	}
}
