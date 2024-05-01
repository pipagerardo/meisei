/******************************************************************************
 *                                                                            *
 *                                "paste.c"                                   *
 *                                                                            *
 ******************************************************************************/

#include "global.h"
#include "netplay.h"
#include "main.h"
#include "resource.h"
#include "paste.h"
#include "cont.h"
#include "mapper.h"
#include "tape.h"
#include "reverse.h"
#include "movie.h"

/*

paste text from clipboard to MSX keyboard. I got the idea from Sean Young (featured in his Virtual MSX emulator), it's also featured in openMSX.
Mainly done to do quick automated emulation related tests from MSX Basic.

Korean table is complete?

*/


/* flags */
#define FSHFT	(1<<8)	/* shift */
#define FGRAPH	(1<<9)	/* graph */
#define FCODE	(1<<10)	/* code */
#define FKANA	(1<<11)	/* kana lock (toggle) */
#define FCAPS	(1<<12)	/* caps lock (toggle) */
#define FEXT	(1<<13)	/* key is extended (1+x) */
#define FREASS	(1<<14)	/* reassign key */
#define FKEY	(1<<15)	/* data is key */

#define KEY(row,col)	(((row)<<3)|(col)|FKEY)

#include "paste_table.h" /* in a separate file for readability.. only usable by paste.c */

static struct {
	char* text;
	int pasting;
	int max;
	int cur;
	int delay;
	int delaystate;
	u16 k_prev;
} paste;



void paste_init(void)
{
	memset(&paste,0,sizeof(paste));
}

void paste_clean(void)
{
	MEM_CLEAN(paste.text);
}


/* frame */
static int paste_delay(int x)
{
	if (--paste.delay>0) return TRUE;

	paste.delay=netplay_is_active()?10:4;
	paste.delaystate^=x;

	return FALSE;
}

static u16 paste_getkey(void)
{
	int region=cont_get_region();
	u8 i=(u8)paste.text[paste.cur];
	u16 key=pc2msx[region][i];

	if (i==1) {
		/* extended table */
		char c=paste.text[paste.cur+1];
		if (c>0x40&&c<0x60) key=pc2msxe[region][c-0x41];
	}

	if (key&FREASS) {
		/* reassign */
		if (key&FEXT) {
			u8 c=key&0xff;
			if (c>0x40&&c<0x60) key=pc2msxe[region][c-0x41]&~FEXT;
			else key=0;
		}
		else key=pc2msx[region][key&0xff];
	}

	if (key&FREASS) key=0; /* error */

	return key;
}

void paste_frame(u8* key)
{
	u16 k;

	if (!paste.pasting||reverse_get_active_state()) return;
	if (paste.cur>=paste.max) { paste_stop(); return; }

	memset(key,0xff,16);
	k=paste_getkey();

	if (k&FKEY) {
		/* change key */
		/* with toggle (not fast) */
		if (k&(FKANA|FCAPS)) {
			int p=paste.cur;
			u16 k2;

			if (k&FSHFT) key[6]&=0xfe;
			if (k&FGRAPH) key[6]&=0xfb;

			if (k&FCAPS&&!(paste.delaystate&2)&&paste_delay(2)) { key[6]&=0xf7; return; } /* caps on delay */
			if (k&FKANA&&!(paste.delaystate&4)&&paste_delay(4)) { key[6]&=0xef; return; } /* kana on delay */
			if (!(paste.delaystate&8)&&paste_delay(8)) { key[k>>3&0xf]&=~(1<<(k&7)); return; } /* key delay */

			key[6]|=5;
			if (k&FKANA&&!(paste.delaystate&16)&&paste_delay(16)) { key[6]&=0xef; return; } /* kana off delay */
			if (k&FCAPS&&!(paste.delaystate&32)&&paste_delay(32)) { key[6]&=0xf7; return; } /* caps off delay */

			/* keyoff delay, only needed if the next one uses the same toggle */
			if (paste.text[paste.cur]==1&&k&FEXT) paste.cur++;
			paste.cur++; k2=paste_getkey(); paste.cur=p;
			if ((k&(FKANA|FCAPS))==(k2&(FKANA|FCAPS))&&!(paste.delaystate&64)&&paste_delay(64)) return;

			paste.k_prev=0;
			paste.delaystate=1;
		}

		/* normal */
		else {
			int m=FSHFT|FCODE|FGRAPH;

			/* keyoff delay, only needed if it's the same as the previous key */
			if ((k&0xff)!=(paste.k_prev&0xff)) {
				paste.k_prev=(paste.k_prev&0xff00)|(k&0xff);
				paste.delaystate^=1;
			}
			if ((paste.delaystate&1)&&paste_delay(1)) return;

			if (k&FSHFT) key[6]&=0xfe;
			if (k&FCODE) key[6]&=0xef;
			if (k&FGRAPH) key[6]&=0xfb;

			/* modifier delay (not needed if key is spacebar) */
			if ((k&m)!=(paste.k_prev&m)&&k!=KEY(8,0)) {
				paste.k_prev=(paste.k_prev&~m)|(k&m);
				paste.delaystate^=2;
			}
			if ((paste.delaystate&2)&&paste_delay(2)) return;

			key[k>>3&0xf]&=~(1<<(k&7));

			if (paste_delay(1)) return;
		}
	}

	if (paste.text[paste.cur]==1&&k&FEXT) paste.cur++;
	paste.cur++;
}


/* start/stop */
int paste_stop(void)
{
	if (paste.pasting) {
		/* change menu */
		char sc[0x100];
		char mc[0x100];

		sprintf(sc,"Paste &Text  ");
		main_menu_caption_get(IDM_PASTE,mc);
		memcpy(mc,sc,strlen(sc));
		main_menu_caption_put(IDM_PASTE,mc);

		paste.pasting=FALSE;
		return TRUE;
	}
	return FALSE;
}

static void paste_start(const char* text,SIZE_T size)
{
		MEM_CLEAN(paste.text);
		MEM_CREATE(paste.text,size+3); /* +padding */
		memcpy(paste.text,text,size);

		paste.max=strlen(paste.text);
		paste.cur=paste.delay=paste.delaystate=paste.k_prev=0;
		paste_delay(1);

		paste.pasting=TRUE;
		/* (caps lock state, &hfcab in basic, is ignored) */
}

void paste_start_text(void)
{
	/* start pasting clipboard text */
	HGLOBAL global;
	SIZE_T size;

	if (paste_stop()||movie_is_playing()) return;

	if (!IsClipboardFormatAvailable(CF_TEXT)) {
		LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_PASTEFAIL),"clipboard data type is invalid ");
		return;
	}

	if (OpenClipboard(NULL)&&(global=GetClipboardData(CF_TEXT))!=NULL&&(size=GlobalSize(global))!=0) {
		if (size>0x100000) LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_PASTEFAIL),"clipboard data is too large    ");

		/* ok */
		else {
			char* text=(char*)GlobalLock(global);
			if (text&&strlen(text)) {
				char sc[0x100];
				char mc[0x100];

				paste_start(text,size);

				/* change menu */
				sprintf(sc,"S&top Pasting");
				main_menu_caption_get(IDM_PASTE,mc);
				memcpy(mc,sc,strlen(sc));
				main_menu_caption_put(IDM_PASTE,mc);
			}

			else LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_PASTEFAIL),"clipboard data is invalid      ");

			if (text) GlobalUnlock(global);
		}

		CloseClipboard();
	}
}

void paste_start_boot(void)
{
	/* start pasting media boot command */
	if (paste_stop()||movie_is_playing()) return;

	/* try cartridge */
	if (mapper_get_file(0)||mapper_get_file(1)) {
		const char* boot_cart="defusr=&h7d75:a=usr(0)\n"; /* NYYRIKKI */
		paste_start(boot_cart,strlen(boot_cart));
	}

	/* try tape */
	else if (tape_is_inserted()) {
		const char* boot_tape[3]={
			"bload\"cas:\",r\n",
			"cload\n",
			"run\"cas:\"\n"
		};
		int type=tape_get_blocktype_cur();

		switch (type) {
			case CAS_TYPE_BIN: case CAS_TYPE_BAS: case CAS_TYPE_ASC:
				/* enter tape load command */
				paste_start(boot_tape[type],strlen(boot_tape[type]));
				break;

			case CAS_TYPE_CUS: case CAS_TYPE_NOP: case CAS_TYPE_END:
				LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_PASTEFAIL),"can't boot current tape block  ");
				break;

			default:
				LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_PASTEFAIL),"tape boot command failed       ");
				break;
		}
	}

	else {
		/* nothing to paste */
		LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_PASTEFAIL),"no bootable media detected     ");
	}
}
