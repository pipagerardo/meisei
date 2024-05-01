/******************************************************************************
 *                                                                            *
 *                           BEGIN "mapper.c"                                 *
 *                                                                            *
 ******************************************************************************/

#include <windows.h>

#include "global.h"
#include "mapper.h"
#include "file.h"
#include "settings.h"
#include "sample.h"
#include "state.h"
#include "scc.h"
#include "z80.h"
#include "crystal.h"
#include "sound.h"
#include "psg.h"
#include "netplay.h"
#include "am29f040b.h"
#include "io.h"
#include "tape.h"
#include "reverse.h"
#include "resource.h"
#include "main.h"
#include "cont.h"
#include "msx.h"

/* EXTERN */
fp_mapread mapread[8]; /* 64KB (8*8KB) Z80 memmap */
fp_mapwrite mapwrite[8];

/* declarations */
typedef __fastcall u8(*fp_mapread_custom)(int,u16);
typedef __fastcall void(*fp_mapwrite_custom)(int,u16,u8);
static void mapsetcustom_read(int,int,int,fp_mapread_custom);
static void mapsetcustom_write(int,int,int,fp_mapwrite_custom);
static int mapcart_getpslot(int);

static int sram_load(int,u8**,u32,const char*,u8);
static void sram_save(int,u8*,u32,const char*);
static void mapper_set_bios_default(void);
static __inline__ void refresh_cb(int);

/* value RAM is filled with on power on. alternating strings of 00s and $ffs on most MSXes */
#define RAMINIT			0xff

/* unmapped read return value, $ff on almost all MSXes, undefined on some. */
/* MSXes known to return an undefined value:
 Canon V-20 (UK): last opcode (or opcode operand) ORed with some value (open-bus) */
#define UNMAPPED		0xff

/* defaults */
// #define DEFAULT_BIOS	"cbios_main_msx1_0.23.rom"
// #define DEFAULT_BIOS	"cbios_main_msx1_0.29_50Hz.rom
// #define DEFAULT_BIOS	"bios/cbios.rom"
#define DEFAULT_BIOS	"bios/default_bios.zip"

#define DEFAULT_RAMSIZE	64
#define DEFAULT_RAMSLOT	3

static int pslot=0;						/* PPI primary slot register */

static fp_mapread mapread_std[4][8];	/* 4 slots standard handlers */
static fp_mapwrite mapwrite_std[4][8];

static char* fn_default_bios;
static char* fn_bios=NULL; static char* n_bios=NULL;
static u32 bioscrc; static u32 default_bioscrc;
static int autodetect_rom;

static int ramsize=DEFAULT_RAMSIZE;		/* RAM size in KB */
static int ramslot=DEFAULT_RAMSLOT;		/* RAM primary slot */
static u8 ram[0x10000];					/* 64KB */
static u8 bios[0x10000];				/* 64KB, normally 32KB, but room for more (eg. cbios + logo) */
static u8 default_bios[0x8000];			/* 32KB, default C-BIOS */
static u8 dummy_u[0x2000];				/* 8KB filled with UNMAPPED */
static u8 dummy_ff[0x2000];				/* 8KB filled with $ff */
static u8* tempfile[ROM_MAXPAGES];

static u8* cart[2][ROM_MAXPAGES+2+2];	/* max 4MB (+2 padding +2 dummy) */
static u8* cartmap[2][8];				/* cart memmaps */
static int cartbank[2][8];				/* cart banknumbers */
static u8* cartsram[2]={NULL,NULL};		/* sram */
static u8* cartsramcopy[2]={NULL,NULL};	/* sram copy */
static int cartsramsize[2]={0,0};		/* sram mem size */
static char* cartfile[2]={NULL,NULL};	/* path+filename */
static u32 cartcrc[2]={0,0};			/* checksum */
static int carttype[2];					/* romtype */
static int cartsize[2];					/* file size */
static int cartpages[2];				/* number of pages */
static int cartmask[2];					/* and mask for megaroms */

/* extra config references */
static int mapper_extra_ref_slot=0;
static int mapper_extra_ref_type=CARTTYPE_NOMAPPER;
static POINT mapper_extra_ref_pos;

/* specific save/load state */
typedef int __fastcall (*_mapgetstatesize)(int);
static int __fastcall mapgetstatesize_nothing(int slot) { slot = slot; return 0; }
static _mapgetstatesize mapgetstatesize[2]={mapgetstatesize_nothing,mapgetstatesize_nothing};

typedef void __fastcall (*_mapsavestate)(int,u8**);
static void __fastcall mapsavestate_nothing(int slot,u8** s) { slot = slot; s = s; }
static _mapsavestate mapsavestate[2]={mapsavestate_nothing,mapsavestate_nothing};

typedef void __fastcall (*_maploadstatecur)(int,u8**);
static void __fastcall maploadstatecur_nothing(int slot,u8** s) { slot = slot; s = s; }
static _maploadstatecur maploadstatecur[2]={maploadstatecur_nothing,maploadstatecur_nothing};

typedef int __fastcall (*_maploadstate)(int,int,u8**);
static int __fastcall maploadstate_nothing(int slot,int v,u8** s) { slot = slot; v = v; s = s; return TRUE; }
static _maploadstate maploadstate[2]={maploadstate_nothing,maploadstate_nothing};

static int loadstate_skip_sram[2]={0,0};
static int mel_error=0;
int mapper_get_mel_error(void) { return mel_error; }

/* specific new/end frame */
typedef void (*_mapnf)(int);
static void mapnf_nothing(int slot) { slot = slot; }
static _mapnf mapnf[2]={mapnf_nothing,mapnf_nothing};
void mapper_new_frame(void) { mapnf[0](0); mapnf[1](1); }

typedef void (*_mapef)(int);
static void mapef_nothing(int slot) { slot = slot; }
static _mapef mapef[2]={mapef_nothing,mapef_nothing};
void mapper_end_frame(void) { mapef[0](0); mapef[1](1); }

/* specific sound set buffercounter */
typedef void (*_mapsetbc)(int,int);
static void mapsetbc_nothing(int slot,int bc) { slot = slot; bc = bc; }
static _mapsetbc mapsetbc[2]={mapsetbc_nothing,mapsetbc_nothing};
void mapper_set_buffer_counter(int bc) { mapsetbc[0](0,bc); mapsetbc[1](1,bc); }

/* specific soundstream */
typedef void (*_mapstream)(int,signed short*,int);
static void mapstream_nothing(int slot,signed short* stream,int len) { slot = slot; stream = stream; len = len; }
static _mapstream mapstream[2]={mapstream_nothing,mapstream_nothing};
void mapper_sound_stream(signed short* stream,int len) { mapstream[0](0,stream,len); mapstream[1](1,stream,len); }

/* specific cleanup */
typedef void (*_mapclean)(int);
static void mapclean_nothing(int slot) { slot = slot; }
static _mapclean mapclean[2]={mapclean_nothing,mapclean_nothing};

#include "mapper_table.h" /* in a separate file for readability.. only usable by mapper*.c */

typedef struct {
	int uid;
	char* longname;
	char* shortname;
	void (*init)(int);
	u32 flags;
} _maptype;

/* deprecated uids, for savestate backward compatibility, don't reuse */
/* removed in meisei 1.3: */
#define DEPRECATED_START0000		1	/* small, start at $0000 */
#define DEPRECATED_START4000		2	/* small, start at $4000 */
#define DEPRECATED_START8000		3	/* small, start at $8000 (BASIC) */
#define DEPRECATED_STARTC000		26	/* small, start at $c000 */
#define DEPRECATED_ASCII8			4	/* ASCII8 std */
#define DEPRECATED_ASCII16			5	/* ASCII16 std */
#define DEPRECATED_ASCII16_2		15	/* ASCII16 + 2KB SRAM */
#define DEPRECATED_ASCII8_8			16	/* ASCII8 + 8KB SRAM */
#define DEPRECATED_ASCII8_32		17	/* ASCII8 + 32KB SRAM (Koei) */
#define DEPRECATED_ZEMINAZMB		25	/* Zemina "ZMB" Box */



/******************************************************************************
 *                                                                            *
 *                   #include "mapper_nomapper.c"                             *
 *                                                                            *
 ******************************************************************************/
// #include "mapper_nomapper.c"

/* Simple no-mapper types, used by almost all small ROMs, page layout is configurable.
This also supports RAM cartridges, and (officially very rare) mixed ROM/RAM.

0-7   = 8KB page in the 1st 64KB
u($f) = unmapped
e($e) = empty section (filled with $ff) (ROM)

mirrored RAM < 8KB:
o($a) = 8 * 1KB
q($a) = 4 * 2KB
d($a) = 2 * 4KB

The most common ones are 16KB or 32KB ROMs that start at $4000, with page layout
uu01uuuu or uu0123uu, eg. Antarctic Adventure, Arkanoid, Doki Doki Penguin Land,
Eggerland Mystery, Knightmare, Magical Tree, Road Fighter, The Castle, The Goonies,
Thexder, Warroid, Yie ar Kung Fu 1, 2, Zanac, ...

2nd most common type is ROMs that start at $8000, aka BASIC ROMs, with page layout
uuuu01uu, eg. Candoo Ninja, Choro Q, Hole in One, Rise Out, Roller Ball, Rotors, ...

*/

static u32 nomapper_rom_layout_temp[2];
static u32 nomapper_ram_layout_temp[2];
static u32 nomapper_rom_layout[2];
static u32 nomapper_ram_layout[2];
static int nomapper_ram_mask[2];

static int nomapper_ram_page[2][8];
static int nomapper_ram_minmax[2][2];
static u8 nomapper_ram[2][0x10000];

static void nomapper_rom_config_default(int slot,int pages,int start)
{
	/* mirrored */
	if (start==-1) {
		if (pages>8) pages=8;

		switch (pages) {
			/* 8KB: mirrored 8 times */
			case 1: nomapper_rom_layout_temp[slot]=0x00000000; break;

			/* 16KB: mirrored 4 times */
			case 2: nomapper_rom_layout_temp[slot]=0x01010101; break;

			/* 24KB/32KB/40KB: start at $4000, mirror rest */
			case 3: nomapper_rom_layout_temp[slot]=0x22012201; break;
			case 4: nomapper_rom_layout_temp[slot]=0x23012301; break;
			case 5: nomapper_rom_layout_temp[slot]=0x23012341; break;

			/* >=48KB: start at 0, mirror rest */
			case 6: nomapper_rom_layout_temp[slot]=0x01234501; break;
			case 7: nomapper_rom_layout_temp[slot]=0x01234560; break;
			case 8: nomapper_rom_layout_temp[slot]=0x01234567; break;

			default: break;
		}
	}

	/* by startpage */
	else {
		int i;

		nomapper_rom_layout_temp[slot]=~0;

		start>>=13;
		pages+=start;
		if (pages>8) pages=8;

		for (i=start;i<pages;i++) nomapper_rom_layout_temp[slot]=(nomapper_rom_layout_temp[slot]^(0xf<<((i^7)<<2)))|((i-start)<<((i^7)<<2));
	}
}

/* i/o */
static u8 __fastcall mapread_nomapper_ram(int slot,u16 a)
{
	return nomapper_ram[slot][nomapper_ram_page[slot][a>>13]|(a&nomapper_ram_mask[slot])];
}

static void __fastcall mapwrite_nomapper_ram(int slot,u16 a,u8 v)
{
	nomapper_ram[slot][nomapper_ram_page[slot][a>>13]|(a&nomapper_ram_mask[slot])]=v;
}

/* state						size
rom layout						4
ram layout						4
ram								variable

==								8 (+ram)
*/
#define STATE_VERSION_NOMAPPER	3 /* mapper.c */
/* version history:
1: (no savestates yet)
2: (no savestates yet)
3: initial
*/
#define STATE_SIZE_NOMAPPER		8

static int __fastcall mapgetstatesize_nomapper(int slot)
{
	return STATE_SIZE_NOMAPPER+(nomapper_ram_minmax[slot][1]-nomapper_ram_minmax[slot][0]);
}

/* save */
static void __fastcall mapsavestate_nomapper(int slot,u8** s)
{
	STATE_SAVE_4(nomapper_rom_layout[slot]);
	STATE_SAVE_4(nomapper_ram_layout[slot]);

	if (nomapper_ram_minmax[slot][1]) {
		STATE_SAVE_C(nomapper_ram[slot]+nomapper_ram_minmax[slot][0],nomapper_ram_minmax[slot][1]-nomapper_ram_minmax[slot][0]);
	}
}

/* load */
static void __fastcall maploadstatecur_nomapper(int slot,u8** s)
{
	u32 i;

	/* don't care about rom layout with normal loadstate */
	STATE_LOAD_4(i);
	if (i!=nomapper_rom_layout[slot]) mel_error|=2;

	STATE_LOAD_4(i);
	if (i!=nomapper_ram_layout[slot]) {
		mel_error|=2;

		/* if no ram, no 1 error */
		if (i!=(u32)~0) mel_error|=1;
	}
	else if (nomapper_ram_minmax[slot][1]) {
		/* load ram */
		STATE_LOAD_C(nomapper_ram[slot]+nomapper_ram_minmax[slot][0],nomapper_ram_minmax[slot][1]-nomapper_ram_minmax[slot][0]);
	}
}

static int __fastcall maploadstate_nomapper(int slot,int v,u8** s)
{
	switch (v) {
		case 1: case 2: break; /* nothing */
		case STATE_VERSION_NOMAPPER:
			maploadstatecur_nomapper(slot,s);
			break;

		default: return FALSE;
	}

	return TRUE;
}

/* init */
static void mapinit_nomapper(int slot)
{
	int i,page;
	int min=0xc,max=0;

	mapgetstatesize[slot]=mapgetstatesize_nomapper;
	mapsavestate[slot]=mapsavestate_nomapper;
	maploadstatecur[slot]=maploadstatecur_nomapper;
	maploadstate[slot]=maploadstate_nomapper;

	memset(nomapper_ram[slot],RAMINIT,0x10000);

	for (i=0;i<8;i++) {
		/* ram */
		page=nomapper_ram_layout[slot]>>((i^7)<<2)&0xf;
		if (page<8) {
			if (page<min) min=page;
			if (page>max) max=page;

			nomapper_ram_page[slot][i]=page<<13;
			mapsetcustom_read(slot,i,1,mapread_nomapper_ram);
			mapsetcustom_write(slot,i,1,mapwrite_nomapper_ram);
		}
		else if (page<0xd) {
			min=max=page;
			nomapper_ram_page[slot][i]=0;
			mapsetcustom_read(slot,i,1,mapread_nomapper_ram);
			mapsetcustom_write(slot,i,1,mapwrite_nomapper_ram);
		}

		/* rom */
		page=nomapper_rom_layout[slot]>>((i^7)<<2)&0xf;
		switch (page) {
			case 0xf:       // Warning -Wimplicit-fallthrough
			    cartbank[slot][i]=ROMPAGE_DUMMY_U;
			case 0xe:       // Warning -Wimplicit-fallthrough
			    cartbank[slot][i]=ROMPAGE_DUMMY_FF;
			default:
			    cartbank[slot][i]=page;
		}
	}

	if (nomapper_ram_layout[slot]!=(u32)~0) {
		/* has ram */
		if (max&&min==max) {
			/* < 8KB */
			nomapper_ram_minmax[slot][0]=0;
			nomapper_ram_minmax[slot][1]=(max-9)<<10;
			nomapper_ram_mask[slot]=nomapper_ram_minmax[slot][1]-1;
		}
		else {
			nomapper_ram_minmax[slot][0]=min<<13;
			nomapper_ram_minmax[slot][1]=(max+1)<<13;
			nomapper_ram_mask[slot]=0x1fff;
		}

		/* if it has no rom, but a file was loaded anyway, copy it to ram */
		if (nomapper_rom_layout[slot]==(u32)(~0&&cartpages[slot])) {
			page=cartpages[slot]; if (page>8) page=8;
			for (i=0;i<page;i++) memcpy(nomapper_ram[slot]+0x2000*i,cart[slot][i],0x2000);
		}
	}
	else nomapper_ram_minmax[slot][0]=nomapper_ram_minmax[slot][1]=0;
}

/* media config dialog */
static INT_PTR CALLBACK mce_nm_dialog( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam )
{
    lParam = lParam;
	switch (msg) {

		case WM_INITDIALOG: {
			int i;
			char t[0x100];

			/* init editboxes */
			#define NM_INIT_EDIT(e,v)							\
				SendDlgItemMessage(dialog,e,EM_LIMITTEXT,8,0);	\
				sprintf(t,"%08x",v);							\
				for (i=0;i<8;i++) {								\
					if (t[i]=='a') t[i]='o';					\
					if (t[i]=='b') t[i]='q';					\
					if (t[i]=='c') t[i]='d';					\
					if (t[i]=='f') t[i]='u';					\
				}												\
				SetDlgItemText(dialog,e,t)

			NM_INIT_EDIT(IDC_MCE_NM_ROM,nomapper_rom_layout_temp[mapper_extra_ref_slot]);
			NM_INIT_EDIT(IDC_MCE_NM_RAM,nomapper_ram_layout_temp[mapper_extra_ref_slot]);

			#undef NM_INIT_EDIT

			sprintf(t,"Slot %d: %s",mapper_extra_ref_slot+1,mapper_get_type_longname(mapper_extra_ref_type));
			SetWindowText(dialog,t);

			main_parent_window(dialog,MAIN_PW_LEFT,MAIN_PW_LEFT,mapper_extra_ref_pos.x,mapper_extra_ref_pos.y,0);

			break;
		}

		case WM_COMMAND:

			switch ( LOWORD(wParam) )
			{

				/* close dialog */
				case IDOK:                      // Warning -Wimplicit-fallthrough
                {
					int i,mram=0,nram=0;
					char t[0x200]; char trom[0x10]; char tram[0x10];
					memset(t,0,0x200); memset(trom,0,0x10); memset(tram,0,0x10);
					GetDlgItemText(dialog,IDC_MCE_NM_ROM,trom,10);
					GetDlgItemText(dialog,IDC_MCE_NM_RAM,tram,10);

					/* error checking */
					if (strlen(trom)!=8||strlen(tram)!=8) {
						LOG_ERROR_WINDOW(dialog,"Input fields must be 8 characters.");
						return 0;
					}

					for (i=0;i<8;i++)
					switch (trom[i]) {
						case 'u': case 'U': trom[i]='f'; break;				/* unmapped */
						case 'e': case 'E': trom[i]='e'; break;				/* empty */
						case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': break;
						default:
							sprintf(t,"ROM block '%c' is unknown. Use 0-7 for mapped pages,\n'e' for empty pages, and 'u' for unmapped sections.",trom[i]);
							LOG_ERROR_WINDOW(dialog,t);
							return 0;
					}

					for (i=0;i<8;i++)
					switch (tram[i]) {
						case 'u': case 'U': tram[i]='f'; break;				/* unmapped */
						case 'o': case 'O': tram[i]='a'; mram|=1; break;	/* 8 * 1KB */
						case 'q': case 'Q': tram[i]='b'; mram|=2; break;	/* 4 * 2KB */
						case 'd': case 'D': tram[i]='c'; mram|=3; break;	/* 2 * 4KB */
						case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7':
							nram++; break;									/* 1 * 8KB */

						default:
							sprintf(t,"RAM block '%c' is unknown. Use 0-7 for mapped pages,\nand 'u' for unmapped sections. For RAM < 8KB, use\n'o' for 8 * 1KB, 'q' for 4 * 2KB, and 'd' for 2 * 4KB.",tram[i]);
							LOG_ERROR_WINDOW(dialog,t);
							return 0;
					}

					if ((mram&&nram)||(mram!=0&&mram!=1&&mram!=2&&mram!=4)) {
						LOG_ERROR_WINDOW(dialog,"Can't combine different RAM sizes.");
						return 0;
					}

					for (i=0;i<8;i++)
					if (trom[i]!='f'&&tram[i]!='f') {
						sprintf(t,"ROM-RAM collision at character %d.",i+1);
						LOG_ERROR_WINDOW(dialog,t);
						return 0;
					}

					sscanf(trom,"%x",&nomapper_rom_layout_temp[mapper_extra_ref_slot]);
					sscanf(tram,"%x",&nomapper_ram_layout_temp[mapper_extra_ref_slot]);

					/* fall through */
				}
				case IDCANCEL:
					EndDialog(dialog,0);
                break;

				default:
                break;
			}
			break;

		default:
        break;
	}

	return 0;
}

/******************************************************************************
 *                                                                            *
 *                   #include "mapper_misc.c"                                 *
 *                                                                            *
 ******************************************************************************/
// #include "mapper_misc.c"
/* miscellaneous mappers */

/* --- dB-Soft Cross Blaim (single game), discrete logic, board: (no label) */
static void __fastcall mapwrite_crossblaim(int slot,u16 a,u8 v)
{
    a = a;
	/* 2nd page switchable at $0000-$ffff */
	/* xxxxxxrb: x=don't care, r=romchip, b=bank */
	if (v&2) {
		/* 32KB ROM 1 */
		cartbank[slot][0]=cartbank[slot][1]=cartbank[slot][6]=cartbank[slot][7]=ROMPAGE_DUMMY_U;
		cartbank[slot][4]=v<<1&7&cartmask[slot];
		cartbank[slot][5]=(v<<1&7&cartmask[slot])|1;
	}
	else {
		/* 32KB ROM 0, fixed, and mirrored to page 0 and page 3 */
		cartbank[slot][0]=cartbank[slot][4]=cartbank[slot][6]=2&cartmask[slot];
		cartbank[slot][1]=cartbank[slot][5]=cartbank[slot][7]=3&cartmask[slot];
	}

	refresh_cb(slot);
}

static void mapinit_crossblaim(int slot)
{
	/* two 32KB romchips. 2 pages of 16KB at $4000-$bfff */
	cartbank[slot][2]=0; cartbank[slot][3]=1; /* bank 0 fixed to page 1 */
	cartbank[slot][0]=cartbank[slot][4]=cartbank[slot][6]=2&cartmask[slot];
	cartbank[slot][1]=cartbank[slot][5]=cartbank[slot][7]=3&cartmask[slot];

	mapsetcustom_write(slot,0,8,mapwrite_crossblaim);
}





/* --- MicroCabin Harry Fox (single game: Harry Fox - Yuki no Maou Hen), discrete logic, board: DSK-1 */
static void __fastcall mapwrite_harryfox(int slot,u16 a,u8 v)
{
	/* page 1 switchable at $6xxx, page 2 switchable at $7xxx, bit 0:select 32KB ROM */
	int p=a>>11&2;
	int b=((v<<2&4)|p)&cartmask[slot];
	p+=2;

	cartbank[slot][p++]=b++;
	cartbank[slot][p]=b;
	refresh_cb(slot);
}

static void mapinit_harryfox(int slot)
{
	int i;

	/* two 32KB romchips. 2 pages of 16KB at $4000-$bfff, init to 0,1 */
	for (i=0;i<4;i++) cartbank[slot][i+2]=i&cartmask[slot];

	mapsetcustom_write(slot,3,1,mapwrite_harryfox);
}





/* --- Irem TAM-S1 (single game: R-Type), mapper chip: Irem TAM-S1, board: MSX-004 */
static void __fastcall mapwrite_irem(int slot,u16 a,u8 v)
{
    a = a;
	/* 2nd page switchable at $4000-$7fff */
	/* xxxrbbbb: x=don't care, b=bank, r=romchip (xxx1xbbb for 2nd ROM) */
	int mask=(v&0x10)+0x1e;

	cartbank[slot][2]=mask;			cartbank[slot][3]=mask|1;
	cartbank[slot][4]=v<<1&mask;	cartbank[slot][5]=(v<<1&mask)|1;
	refresh_cb(slot);
}

static void mapinit_irem(int slot)
{
	/* 2 romchips: one 256KB, one 128KB. 2 pages of 16KB at $4000-$bfff, 1st page fixed to last bank of current romchip */
	cartbank[slot][2]=0x1e;	cartbank[slot][3]=0x1f;
	cartbank[slot][4]=0;	cartbank[slot][5]=1;

	mapsetcustom_write(slot,2,2,mapwrite_irem);
}





/* --- Al-Alamiah Al-Qur'an (single program), mapper chip: Yamaha XE297A0 (protection is separate via resistor network), board: GCMK-16X, 2 ROM chips */
/* (without having been hardware reverse engineered, and only 1 program to test it with, I'm not sure if this implementation is accurate) */
static u8 quran_lutp[0x100];
static int quran_m1[2];

/* i/o */
static u8 __fastcall mapread_quran(int slot,u16 a)
{
	if (quran_m1[slot]) return quran_lutp[cartmap[slot][a>>13][a&0x1fff]];

	/* M1 pulse (pin 9) disables protection */
	quran_m1[slot]=a==z80_get_pc();
	return cartmap[slot][a>>13][a&0x1fff];
}

static void __fastcall mapwrite_quran(int slot,u16 a,u8 v)
{
	if (a&0x1000) {
		/* all pages switchable at $5000-$5fff */
		cartbank[slot][(a>>10&3)+2]=v&cartmask[slot];
		refresh_cb(slot);
	}
}

/* state					size
m1							1

==							1
*/
#define STATE_VERSION_QURAN	3 /* mapper.c */
/* version history:
1: (didn't exist yet)
2: (didn't exist yet)
3: initial
*/
#define STATE_SIZE_QURAN	1

static int __fastcall mapgetstatesize_quran(int slot)
{
    slot = slot;
	return STATE_SIZE_QURAN;
}

/* save */
static void __fastcall mapsavestate_quran(int slot,u8** s)
{
	STATE_SAVE_1(quran_m1[slot]);
}

/* load */
static void __fastcall maploadstatecur_quran(int slot,u8** s)
{
	STATE_LOAD_1(quran_m1[slot]);
}

static int __fastcall maploadstate_quran(int slot,int v,u8** s)
{
	switch (v) {
		case STATE_VERSION_QURAN:
			maploadstatecur_quran(slot,s);
			break;

		default: return FALSE;
	}

	return TRUE;
}

/* init */
static void mapinit_quran(int slot)
{
	int i;

	/* 4 pages of 8KB at $4000-$bfff, init to 0 */
	for (i=2;i<6;i++) cartbank[slot][i]=0;

	mapgetstatesize[slot]=mapgetstatesize_quran;
	mapsavestate[slot]=mapsavestate_quran;
	maploadstatecur[slot]=maploadstatecur_quran;
	maploadstate[slot]=maploadstate_quran;

	mapsetcustom_write(slot,2,1,mapwrite_quran);
	mapsetcustom_read(slot,2,4,mapread_quran);
	quran_m1[slot]=FALSE;
	/* protection uses a simple rotation on databus, some lines inverted:
		D0   D4			D4   D5
		D1 ~ D3			D5 ~ D2
		D2 ~ D6			D6   D7
		D3 ~ D0			D7   D1 */
	for (i=0;i<0x100;i++) quran_lutp[i]=((i<<4&0x50)|(i>>3&5)|(i<<1&0xa0)|(i<<2&8)|(i>>6&2))^0x4d;
}





/* --- Matra INK (single game), AMD Am29F040B flash memory */
static _am29f* ink_chip[2]={NULL,NULL};

/* i/o */
static u8 __fastcall mapread_matraink(int slot,u16 a)
{
	return am29f_read(ink_chip[slot],a);
}

static void __fastcall mapwrite_matraink(int slot,u16 a,u8 v)
{
	am29f_write(ink_chip[slot],a,v);
}

/* state					size
amd mode					2
amd inc						1

==							3
*/
#define STATE_VERSION_INK	3 /* mapper.c */
/* version history:
1: initial
2: no changes (mapper.c)
3: no changes (mapper.c)
*/
#define STATE_SIZE_INK		3

static int __fastcall mapgetstatesize_matraink(int slot)
{
    slot = slot;
	return STATE_SIZE_INK;
}

/* save */
static void __fastcall mapsavestate_matraink(int slot,u8** s)
{
	STATE_SAVE_2(ink_chip[slot]->mode);
	STATE_SAVE_1(ink_chip[slot]->inc);
}

/* load */
static void __fastcall maploadstatecur_matraink(int slot,u8** s)
{
	STATE_LOAD_2(ink_chip[slot]->mode);
	STATE_LOAD_1(ink_chip[slot]->inc);
}

static int __fastcall maploadstate_matraink(int slot,int v,u8** s)
{
	switch (v) {
		case 1: case 2: case STATE_VERSION_INK:
			maploadstatecur_matraink(slot,s);
			break;

		default: return FALSE;
	}

	return TRUE;
}

/* init/clean */
static void mapclean_matraink(int slot)
{
	am29f_clean(ink_chip[slot]);
	ink_chip[slot]=NULL;
}

static void mapinit_matraink(int slot)
{
	int i;

	ink_chip[slot]=am29f_init(TRUE);
	mapclean[slot]=mapclean_matraink;

	/* upload to flash */
	for (i=0;i<0x40;i++) memcpy(ink_chip[slot]->data+0x2000*i,cart[slot][i],0x2000);

	for (i=0;i<8;i++) ink_chip[slot]->unprotect[i]=TRUE;
	ink_chip[slot]->readsector=0;
	ink_chip[slot]->writesector=0x40000;
	am29f_reset(ink_chip[slot]);

	mapgetstatesize[slot]=mapgetstatesize_matraink;
	mapsavestate[slot]=mapsavestate_matraink;
	maploadstatecur[slot]=maploadstatecur_matraink;
	maploadstate[slot]=maploadstate_matraink;

	mapsetcustom_read(slot,0,8,mapread_matraink);
	mapsetcustom_write(slot,0,8,mapwrite_matraink);
}





/* --- Sony Playball (single game), discrete logic, board: Sony 1-621-028

NOTE: If samples are used, it can cause reverse and speed up problems, unable to fix it for now.
 (eg. reverse -> instant replay, can give different playback due to sample/buffer offset)

*/
static _sample* playball_sample[2]={NULL,NULL};

static u8 __fastcall mapread_playball(int slot,u16 a)
{
	/* register at $bfff, bit 0: 1=ready, 0=busy */
	if (a==0xbfff) return playball_sample[slot]->playing^0xff;

	return cartmap[slot][5][a&0x1fff];
}

static void __fastcall mapwrite_playball(int slot,u16 a,u8 v)
{
	if (a==0xbfff&&v<15) {
		/* register at $bfff, play sample if ready */
		if (!playball_sample[slot]->playing) sample_play(playball_sample[slot],slot,v);
	}
}

static void mapstream_playball(int slot,signed short* stream,int len)
{
	sample_stream(playball_sample[slot],slot,stream,len);
}

/* state: sample.c */
static int __fastcall mapgetstatesize_playball(int slot) { slot = slot; return sample_state_get_size(); }
static void __fastcall mapsavestate_playball(int slot,u8** s) { sample_state_save(playball_sample[slot],s); }
static void __fastcall maploadstatecur_playball(int slot,u8** s) { sample_state_load_cur(playball_sample[slot],s); }
static int __fastcall maploadstate_playball(int slot,int v,u8** s) { return sample_state_load(playball_sample[slot],v,s); }

static void mapclean_playball(int slot)
{
	sample_stop(playball_sample[slot]);
	sample_clean(playball_sample[slot]);
	playball_sample[slot]=NULL;
}

static void mapinit_playball(int slot)
{
	int i,j;

	playball_sample[slot]=sample_init();
	MEM_CLEAN(file->name);
	if (mapper_get_file(slot)) file_setfile(NULL,mapper_get_file(slot),NULL,NULL);
	sample_load(playball_sample[slot],slot,file->name);

	mapclean[slot]=mapclean_playball;
	mapgetstatesize[slot]=mapgetstatesize_playball;
	mapsavestate[slot]=mapsavestate_playball;
	maploadstatecur[slot]=maploadstatecur_playball;
	maploadstate[slot]=maploadstate_playball;
	mapstream[slot]=mapstream_playball;

	mapsetcustom_read(slot,5,1,mapread_playball);
	mapsetcustom_write(slot,5,1,mapwrite_playball);

	/* 32KB uu0123uu */
	j=cartpages[slot]; if (j>6) j=6;
	for (i=0;i<j;i++) cartbank[slot][i+2]=i;
}





/* --- Konami Synthesizer (single game), discrete logic and lots of resistors */
#define KSYN_VOL_FACTOR 12 /* guessed */
static int* konamisyn_dac[2];
static int konamisyn_output[2];
static int konamisyn_cc[2];
static int konamisyn_bc[2];

/* sound */
static __inline__ void konamisyn_update(int slot)
{
	int i=(konamisyn_cc[slot]-z80_get_rel_cycles())/crystal->rel_cycle;
	if (i<=0) return;
	konamisyn_cc[slot]=z80_get_rel_cycles();

	while (i--) konamisyn_dac[slot][konamisyn_bc[slot]++]=konamisyn_output[slot];
}

static void mapsetbc_konamisyn(int slot,int bc)
{
	if (bc!=0&&bc<konamisyn_bc[slot]) {
		/* copy leftover to start */
		int i=bc,j=konamisyn_bc[slot];
		while (i--) konamisyn_dac[slot][i]=konamisyn_dac[slot][--j];
	}

	konamisyn_bc[slot]=bc;
}

static void mapnf_konamisyn(int slot) { konamisyn_cc[slot]+=crystal->frame; }
static void mapef_konamisyn(int slot) { konamisyn_update(slot); }

/* i/o */
static void __fastcall mapwrite_konamisyn(int slot,u16 a,u8 v)
{
	/* 8 bit PCM at $4000-$7fff (bit 4 clear) */
	if (~a&0x10) {
		konamisyn_update(slot);
		konamisyn_output[slot]=v*KSYN_VOL_FACTOR;
	}
}

/* state					size
cc							4
output						4

==							8
*/
#define STATE_VERSION_KSYN	3 /* mapper.c */
/* version history:
1: initial
2: no changes (mapper.c)
3: no changes (mapper.c)
*/
#define STATE_SIZE_KSYN		8

static int __fastcall mapgetstatesize_konamisyn(int slot)
{
    slot = slot;
	return STATE_SIZE_KSYN;
}

/* save */
static void __fastcall mapsavestate_konamisyn(int slot,u8** s)
{
	STATE_SAVE_4(konamisyn_cc[slot]);
	STATE_SAVE_4(konamisyn_output[slot]);
}

/* load */
static void __fastcall maploadstatecur_konamisyn(int slot,u8** s)
{
	STATE_LOAD_4(konamisyn_cc[slot]);
	STATE_LOAD_4(konamisyn_output[slot]);
}

static int __fastcall maploadstate_konamisyn(int slot,int v,u8** s)
{
	switch (v) {
		case 1: case 2: case STATE_VERSION_KSYN:
			maploadstatecur_konamisyn(slot,s);
			break;

		default: return FALSE;
	}

	return TRUE;
}

/* init/clean */
static void mapclean_konamisyn(int slot)
{
	sound_clean_dac(konamisyn_dac[slot]);
	konamisyn_dac[slot]=NULL;
}

static void mapinit_konamisyn(int slot)
{
	int i,j;

	konamisyn_dac[slot]=sound_create_dac();

	mapclean[slot]=mapclean_konamisyn;
	mapnf[slot]=mapnf_konamisyn;
	mapef[slot]=mapef_konamisyn;
	mapsetbc[slot]=mapsetbc_konamisyn;
	mapgetstatesize[slot]=mapgetstatesize_konamisyn;
	mapsavestate[slot]=mapsavestate_konamisyn;
	maploadstatecur[slot]=maploadstatecur_konamisyn;
	maploadstate[slot]=maploadstate_konamisyn;

	konamisyn_output[slot]=0;

	/* sync */
	konamisyn_cc[slot]=psg_get_cc();
	konamisyn_bc[slot]=psg_get_buffer_counter();

	mapsetcustom_write(slot,2,2,mapwrite_konamisyn);

	/* 32KB uu0123uu */
	j=cartpages[slot]; if (j>6) j=6;
	for (i=0;i<j;i++) cartbank[slot][i+2]=i;
}

/******************************************************************************
 *                                                                            *
 *                   #include "mapper_konami.c"                               *
 *                                                                            *
 ******************************************************************************/
// #include "mapper_konami.c"
/* Konami mappers (and SCC clones) */

/* --- Konami VRC, mapper chip: Konami VRC007431, board: AICA A-2 (or custom KONAMI [number])
	Konami MegaROMs without SCC, eg.
	Gradius (Nemesis), Maze of Galious, Penguin Adventure, Shalom

From what is known, Zemina MegaROM cartridges behave the same way,
though they use discrete logic instead of this (or a) mapper chip.
The known similarities are: mirroring, bank size and locations,
write addresses, and the fact that the 1st bank is fixed.

Some Zemina bootlegs use a different system, maybe pre-configured
for one of the (unemulated) ZMB RAM boxes.
*/
static int konamivrc_mask[2];

static void __fastcall mapwrite_konamivrc(int slot,u16 a,u8 v)
{
	/* last 3 pages switchable at $6000-$bfff */
	cartbank[slot][a>>13]=v&konamivrc_mask[slot];
	refresh_cb(slot);
}

static void mapinit_konamivrc(int slot)
{
	int i;

	/* 4 pages of 8KB at $4000-$bfff */
	for (i=2;i<6;i++) cartbank[slot][i]=i-2;

	/* $0000-$3fff is a mirror of $4000-$7fff, $c000-$ffff is a mirror of $8000-$bfff */
	i=mapcart_getpslot(slot);
	mapread_std[i][0]=mapread_std[i][2]; mapread_std[i][1]=mapread_std[i][3];
	mapread_std[i][6]=mapread_std[i][4]; mapread_std[i][7]=mapread_std[i][5];

	mapsetcustom_write(slot,3,3,mapwrite_konamivrc);

	/* 256KB mapper, but allow hacks too if it turns out the ROM is larger */
	/* in reality, bit 4 selects the ROM chip in the case of 256KB games */
	if (cartmask[slot]<0x1f) konamivrc_mask[slot]=0x1f;
	else konamivrc_mask[slot]=cartmask[slot];
}





/* --- Konami Game Master 2 (single game), Konami VRC007431 mapper and some discrete logic to handle SRAM */
static int gm2_is_sram[2][4];
static int gm2_sram_bank[2][4];

/* i/o */
static u8 __fastcall mapread_gm2(int slot,u16 a)
{
	int bank=a>>13;

	/* page 0,3 (read-only, like VRC) */
	if (a<0x4000||a>0xbfff) bank^=2;

	/* read from sram ($6000-$bfff) */
	if (gm2_is_sram[slot][bank-2]) return cartsram[slot][(a&0xfff)|gm2_sram_bank[slot][bank-2]];

	/* normal read */
	return cartmap[slot][bank][a&0x1fff];
}

static void __fastcall mapwrite_gm2(int slot,u16 a,u8 v)
{
	int bank=a>>13;

	if (a&0x1000) {
		bank-=2;

		/* write to sram (only writable in the 2nd half of the last bank) */
		if (bank==3&&gm2_is_sram[slot][bank]) cartsram[slot][(a&0xfff)|gm2_sram_bank[slot][bank]]=v;
	}

	else {
		/* standard bankswitch */
		cartbank[slot][bank]=v&cartmask[slot];
		refresh_cb(slot);

		/* switch sram, 2*4K */
		bank-=2;
		gm2_is_sram[slot][bank]=v&(cartmask[slot]+1);
		gm2_sram_bank[slot][bank]=((v>>1&(cartmask[slot]+1))!=0)<<12;
	}
}

/* state					size
sram bank enabled			3*1
sram bank numbers			3*2

==							9
*/
#define STATE_VERSION_GM2	3 /* mapper.c */
/* version history:
1: initial
2: no changes (mapper.c)
3: no changes (mapper.c)
*/
#define STATE_SIZE_GM2		9

static int __fastcall mapgetstatesize_gm2(int slot)
{
    slot = slot;
	return STATE_SIZE_GM2;
}

/* save */
static void __fastcall mapsavestate_gm2(int slot,u8** s)
{
	int i=3;
	while (i--) {
		STATE_SAVE_1(gm2_is_sram[slot][i+1]);
		STATE_SAVE_2(gm2_sram_bank[slot][i+1]);
	}
}

/* load */
static void __fastcall maploadstatecur_gm2(int slot,u8** s)
{
	int i=3;
	while (i--) {
		STATE_LOAD_1(gm2_is_sram[slot][i+1]);
		STATE_LOAD_2(gm2_sram_bank[slot][i+1]);
	}
}

static int __fastcall maploadstate_gm2(int slot,int v,u8** s)
{
	switch (v) {
		case 1: case 2: case STATE_VERSION_GM2:
			maploadstatecur_gm2(slot,s);
			break;

		default: return FALSE;
	}

	return TRUE;
}

/* init */
static void mapinit_gm2(int slot)
{
	int i;

	sram_load(slot,&cartsram[slot],0x2000,NULL,0xff);
	for (i=0;i<4;i++) gm2_is_sram[slot][i]=FALSE;

	mapgetstatesize[slot]=mapgetstatesize_gm2;
	mapsavestate[slot]=mapsavestate_gm2;
	maploadstatecur[slot]=maploadstatecur_gm2;
	maploadstate[slot]=maploadstate_gm2;

	/* similar to standard Konami */
	for (i=2;i<6;i++) cartbank[slot][i]=i-2;
	mapsetcustom_read(slot,0,8,mapread_gm2);
	mapsetcustom_write(slot,3,3,mapwrite_gm2);
}





/* --- Konami SCC, mapper chip: Konami 051649 2212P003, board: custom KONAMI [number]
	Konami MegaROMs with SCC, eg.
	F1 Spirit, Gofer no Yabou 2 (Nemesis 3), Gradius 2, King's Valley 2, Parodius, Salamander
*/
static int scc_enabled[2];
static _scc* scc_scc[2]={NULL,NULL};

/* i/o */
static u8 __fastcall mapread_konamiscc(int slot,u16 a)
{
	/* read from SCC */
	if (scc_enabled[slot]&&a&0x1800) return scc_read(scc_scc[slot],a);

	/* default */
	return cartmap[slot][4][a&0x1fff];
}

static void __fastcall mapwrite_konamiscc(int slot,u16 a,u8 v)
{
	if ((a&0xf000)==0x9000) {
		if (a&0x800) {
			/* write to SCC */
			if (scc_enabled[slot]) scc_write(scc_scc[slot],a,v);
			return;
		}

		scc_enabled[slot]=(v&0x3f)==0x3f;
	}

	if ((a&0x1800)==0x1000) {
		/* all pages switchable at $4000-$bfff */
		cartbank[slot][a>>13]=v&cartmask[slot];
		refresh_cb(slot);
	}
}

static void mapnf_konamiscc(int slot) { scc_new_frame(scc_scc[slot]); }
static void mapef_konamiscc(int slot) { scc_end_frame(scc_scc[slot]); }
static void mapsetbc_konamiscc(int slot,int bc) { scc_set_buffer_counter(scc_scc[slot],bc); }

/* state: size 1 (enabled) + scc */
#define STATE_VERSION_KSCC	3 /* mapper.c */
/* version history:
1: initial
2: (changes in scc.c)
3: no changes (mapper.c)
*/
#define STATE_SIZE_KSCC		1 /* + SCC */

static int __fastcall mapgetstatesize_konamiscc(int slot)
{
    slot = slot;
	return STATE_SIZE_KSCC+scc_state_get_size();
}

/* save */
static void __fastcall mapsavestate_konamiscc(int slot,u8** s)
{
	STATE_SAVE_1(scc_enabled[slot]);

	scc_state_save(scc_scc[slot],s);
}

/* load */
static void __fastcall maploadstatecur_konamiscc_inc(int slot,u8** s)
{
	STATE_LOAD_1(scc_enabled[slot]);
}

static void __fastcall maploadstatecur_konamiscc(int slot,u8** s)
{
	maploadstatecur_konamiscc_inc(slot,s);

	scc_state_load_cur(scc_scc[slot],s);
}

static int __fastcall maploadstate_konamiscc(int slot,int v,u8** s)
{
	switch (v) {
		case 1:
			maploadstatecur_konamiscc_inc(slot,s);
			scc_state_load(scc_scc[slot],v,s);
			break;
		case 2: case STATE_VERSION_KSCC:
			maploadstatecur_konamiscc(slot,s);
			break;

		default: return FALSE;
	}

	return TRUE;
}

/* init/clean */
static void mapclean_konamiscc(int slot)
{
	scc_clean(scc_scc[slot]);
	scc_scc[slot]=NULL;
}

static void mapinit_konamiscc(int slot)
{
	int i;

	if (mapclean[slot]==mapclean_nothing) {
		scc_scc[slot]=scc_init(SCC_VF_DEFAULT,SCC_MODEL);
		mapclean[slot]=mapclean_konamiscc;
	}

	mapgetstatesize[slot]=mapgetstatesize_konamiscc;
	mapsavestate[slot]=mapsavestate_konamiscc;
	maploadstatecur[slot]=maploadstatecur_konamiscc;
	maploadstate[slot]=maploadstate_konamiscc;

	mapnf[slot]=mapnf_konamiscc;
	mapef[slot]=mapef_konamiscc;
	mapsetbc[slot]=mapsetbc_konamiscc;

	/* 4 pages of 8KB at $4000-$bfff */
	for (i=2;i<6;i++) cartbank[slot][i]=i-2;

	/* $0000-$3fff is a mirror of $8000-$bfff, $c000-$ffff is a mirror of $4000-$7fff */
	i=mapcart_getpslot(slot);
	mapread_std[i][0]=mapread_std[i][4]; mapread_std[i][1]=mapread_std[i][5];
	mapread_std[i][6]=mapread_std[i][2]; mapread_std[i][7]=mapread_std[i][3];

	mapsetcustom_read(slot,4,1,mapread_konamiscc);
	mapsetcustom_write(slot,2,4,mapwrite_konamiscc);

	scc_poweron(scc_scc[slot]);
	scc_enabled[slot]=FALSE;
}





/* --- Konami Sound Cartridge, mapper chip: Konami 052539 SCC-I 2312P001, board: KONAMI
	SCC-I with (configurable) RAM, supported by the Game Collections (see below) (and some hacked ROMs)
GC 1: 1:Knightmare(YES), 2:Antarctic Adventure(YES), 3:Yie ar Kung-Fu(YES), 4:Yie ar Kung-Fu 2(YES), 5:King's Valley(NO)
GC 2: 1:Konami's Boxing(YES), 2:Konami's Tennis(NO), 3:Video Hustler(YES), 4:Hyper Olympic 1(NO), 5:Hyper Sports 2(YES)
GC 3: 1:Twinbee(YES), 2:Super Cobra(YES), 3:Sky Jaguar(YES), 4:Time Pilot(NO), 5:Nemesis(YES)
GC 4: 1:Konami's Soccer(YES), 2:Konami's Ping Pong(YES), 3:Konami's Golf(NO), 4:Hyper Olympic 2(NO), 5:Hyper Sports 3(YES)
GC S: 1.1:Pippols(YES), 1.2:Hyper Rally(YES), 1.3:Road Fighter(YES) (other choices are MSX2-only)
*/
static int scci_mode[2];
static int scci_bank[2][4];
static int scci_we[2][4]; /* write enabled */
static u8 scci_ram[2][0x20000]; /* 128KB */

static u32 scci_ram_layout_temp[2];
static u32 scci_ram_layout[2];

/* i/o */
static u8 __fastcall mapread_konamiscci(int slot,u16 a)
{
	int bank=a>>13;

	/* read from SCC-I, SCC mode */
	if ((a&0xf800)==0x9800&&scc_enabled[slot]&1&&!(scci_mode[slot]&0x20)) return scc_read(scc_scc[slot],a);

	/* read from SCC-I, SCC-I mode */
	if ((a&0xf800)==0xb800&&scc_enabled[slot]&0x80&scci_mode[slot]<<2) return scci_read(scc_scc[slot],a);

	/* page 0,3 (read-only, like SCC) */
	if (a<0x4000||a>0xbfff) bank^=4;

	/* read from ram */
	return scci_ram[slot][scci_bank[slot][bank-2]|(a&0x1fff)];
}

static void __fastcall mapwrite_konamiscci(int slot,u16 a,u8 v)
{
	int bank=a>>13;

	if ((a&0xfffe)==0xbffe) {
		/* mode register */
		scci_mode[slot]=v;

		scci_we[slot][0]=v&0x11;
		scci_we[slot][1]=v&0x12;
		scci_we[slot][2]=(v>>3&v&4)|(v&0x10);
		scci_we[slot][3]=v&0x10;

		/* RAM in this bank is unaffected */
		return;
	}

	bank-=2;

	if (scci_we[slot][bank]) {
		/* write to RAM */
		bank=scci_bank[slot][bank];
		if (scci_ram_layout[slot]<<16&~bank||scci_ram_layout[slot]<<15&bank&0x10000) scci_ram[slot][bank|(a&0x1fff)]=v;
	}
	else {
		switch (a&0xf800) {
			case 0x9000:
				/* enable SCC mode */
				scc_enabled[slot]=(scc_enabled[slot]&0x80)|((v&0x3f)==0x3f);
				break;
			case 0x9800:
				/* write to SCC-I, SCC mode */
				if (scc_enabled[slot]&1&&!(scci_mode[slot]&0x20)) scc_write(scc_scc[slot],a,v);
				break;
			case 0xb000:
				/* enable SCC-I mode */
				scc_enabled[slot]=(scc_enabled[slot]&1)|(v&0x80);
				break;
			case 0xb800:
				/* write to SCC-I, SCC-I mode */
				if (scc_enabled[slot]&0x80&scci_mode[slot]<<2) scci_write(scc_scc[slot],a,v);
				break;

			default: break;
		}

		if ((a&0x1800)==0x1000) {
			/* bankswitch */
			scci_bank[slot][bank]=v<<13&0x1ffff;
		}
	}
}

/* state					size
enabled						1
mode register				1
banks						4*3
banks write enable			4
ram layout					1
ram (after scc)				variable (0, 0x10000, or 0x20000)

==							19 (+scc +ram)
*/
#define STATE_VERSION_KSCCI	3 /* mapper.c */
/* version history:
1: (didn't exist yet)
2: initial
3: custom RAM layout
*/
#define STATE_SIZE_KSCCI	19

static int __fastcall mapgetstatesize_konamiscci(int slot)
{
	int size=STATE_SIZE_KSCCI;
	if (scci_ram_layout[slot]&1) size+=0x10000;
	if (scci_ram_layout[slot]&2) size+=0x10000;
	return size+scc_state_get_size();
}

/* save */
static void __fastcall mapsavestate_konamiscci(int slot,u8** s)
{
	int i=4;
	while (i--) {
		STATE_SAVE_3(scci_bank[slot][i]);
		STATE_SAVE_1(scci_we[slot][i]);
	}

	STATE_SAVE_1(scc_enabled[slot]);
	STATE_SAVE_1(scci_mode[slot]);

	scc_state_save(scc_scc[slot],s);

	/* ram */
	STATE_SAVE_1(scci_ram_layout[slot]);
	if (scci_ram_layout[slot]&1) { STATE_SAVE_C(scci_ram[slot],0x10000); }
	if (scci_ram_layout[slot]&2) { STATE_SAVE_C(scci_ram[slot]+0x10000,0x10000); }
}

/* load */
static void __fastcall maploadstatecur_konamiscci(int slot,u8** s)
{
	int i=4;
	while (i--) {
		STATE_LOAD_3(scci_bank[slot][i]);
		STATE_LOAD_1(scci_we[slot][i]);
	}

	STATE_LOAD_1(scc_enabled[slot]);
	STATE_LOAD_1(scci_mode[slot]);

	scc_state_load_cur(scc_scc[slot],s);

	/* ram */
	STATE_LOAD_1(i);
	if ((u32)i!=scci_ram_layout[slot]) mel_error|=2;

	switch (i&3) {
		/* no ram, don't bother */
		case 0: break;

		/* lower 64KB */
		case 1:
			if (scci_ram_layout[slot]&1) {
				if (scci_ram_layout[slot]&2) memset(scci_ram[slot]+0x10000,0,0x10000);
				STATE_LOAD_C(scci_ram[slot],0x10000);
			}
			else mel_error|=1;

			break;

		/* upper 64KB */
		case 2:
			if (scci_ram_layout[slot]&2) {
				if (scci_ram_layout[slot]&1) memset(scci_ram[slot],0,0x10000);
				STATE_LOAD_C(scci_ram[slot]+0x10000,0x10000);
			}
			else mel_error|=1;

			break;

		/* 128KB */
		case 3:
			if (scci_ram_layout[slot]==3) { STATE_LOAD_C(scci_ram[slot],0x20000); }
			else mel_error|=1;
			break;
	}
}

static void __fastcall maploadstate_konamiscci_2(int slot,u8** s)
{
	/* version 2 */
	int i=4;
	while (i--) {
		STATE_LOAD_3(scci_bank[slot][i]);
		STATE_LOAD_1(scci_we[slot][i]);
	}

	STATE_LOAD_1(scc_enabled[slot]);
	STATE_LOAD_1(scci_mode[slot]);

	scc_state_load(scc_scc[slot],2,s);

	/* due to a bug, version 2 didn't save ram */
}

static int __fastcall maploadstate_konamiscci(int slot,int v,u8** s)
{
	switch (v) {
		case 2:
			maploadstate_konamiscci_2(slot,s);
			break;
		case STATE_VERSION_KSCC:
			maploadstatecur_konamiscci(slot,s);
			break;

		default: return FALSE;
	}

	return TRUE;
}

/* init */
static void mapinit_konamiscci(int slot)
{
	int i;

	scc_scc[slot]=scc_init(SCC_VF_DEFAULT,SCCI_MODEL);
	mapclean[slot]=mapclean_konamiscc;

	memset(scci_ram[slot],0,0x20000); /* can be alternating areas of $00 and $ff */

	/* 4 pages of 8KB at $4000-$bfff (like Konami SCC) */
	mapsetcustom_read(slot,0,8,mapread_konamiscci);
	mapsetcustom_write(slot,2,4,mapwrite_konamiscci);

	for (i=0;i<4;i++) {
		scci_we[slot][i]=FALSE;
		scci_bank[slot][i]=i<<13;
	}

	scc_poweron(scc_scc[slot]);
	scc_enabled[slot]=0;
	scci_mode[slot]=0;

	mapnf[slot]=mapnf_konamiscc;
	mapef[slot]=mapef_konamiscc;
	mapsetbc[slot]=mapsetbc_konamiscc;

	mapgetstatesize[slot]=mapgetstatesize_konamiscci;
	mapsavestate[slot]=mapsavestate_konamiscci;
	maploadstatecur[slot]=maploadstatecur_konamiscci;
	maploadstate[slot]=maploadstate_konamiscci;

	/* file -> RAM (normally unsupported) */
	if (cartpages[slot]&&scci_ram_layout[slot]) {
		if (cartpages[slot]>8) {
			if (scci_ram_layout[slot]==3) {
				/* only for 128KB */
				int p=cartpages[slot];
				if (p>0x10) p=0x10;
				for (i=0;i<p;i++) memcpy(scci_ram[slot]+0x2000*i,cart[slot][i],0x2000);

				scci_mode[slot]=0x20; /* pre-init */
			}
		}
		else {
			/* mirrored */
			for (i=0;i<cartpages[slot];i++) {
				memcpy(scci_ram[slot]+0x2000*i,cart[slot][i],0x2000);
				memcpy(scci_ram[slot]+0x10000+0x2000*i,cart[slot][i],0x2000);
			}

			scci_mode[slot]=0x20;

			/* on the SD Snatcher cartridge, boot from the upper bank */
			if (scci_ram_layout[slot]==2) for (i=0;i<4;i++) scci_bank[slot][i]|=0x10000;
		}
	}

	/* extra config */
	if (~scci_ram_layout[slot]&1) memset(scci_ram[slot],UNMAPPED,0x10000);
	if (~scci_ram_layout[slot]&2) memset(scci_ram[slot]+0x10000,UNMAPPED,0x10000);
}

/* media config dialog */
static INT_PTR CALLBACK mce_scci_dialog( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam )
{
    lParam = lParam;
	switch (msg) {

		case WM_INITDIALOG: {
			char t[0x100];

			/* init combobox */
			HWND c=GetDlgItem(dialog,IDC_MCE_SCCI_RAM);

			#define SCCI_COMBO(x)	\
				sprintf(t,x);		\
				SendMessage(c,CB_ADDSTRING,0,(LPARAM)((LPCTSTR)t))

			SCCI_COMBO("No RAM");
			SCCI_COMBO("64KB (lower, Snatcher)");
			SCCI_COMBO("64KB (upper, SD Snatcher)");
			SCCI_COMBO("128KB");

			#undef SCCI_COMBO

			SendMessage(c,CB_SETCURSEL,scci_ram_layout_temp[mapper_extra_ref_slot],0);

			sprintf(t,"Slot %d: %s",mapper_extra_ref_slot+1,mapper_get_type_longname(mapper_extra_ref_type));
			SetWindowText(dialog,t);

			main_parent_window(dialog,MAIN_PW_LEFT,MAIN_PW_LEFT,mapper_extra_ref_pos.x,mapper_extra_ref_pos.y,0);

			break;
		}

		case WM_COMMAND:

			switch (LOWORD(wParam)) {

				/* close dialog */
				case IDOK: {        // Warning -Wimplicit-fallthrough
					int i;

					if ((i=SendDlgItemMessage(dialog,IDC_MCE_SCCI_RAM,CB_GETCURSEL,0,0))==CB_ERR) i=3;
					scci_ram_layout_temp[mapper_extra_ref_slot]=i;

                /* fall through */
				}
				case IDCANCEL:
					EndDialog(dialog,0);
                break;

				default:
                break;
			}

			break;

		default: break;
	}

	return 0;
}

/* --- Manuel Mega Flash ROM SCC, Am29F040B + SCC mapper
	Lotus F3, ..
*/
static int mfrscc_sector[2][4];
static int mfrscc_bank[2][4];
static _am29f* mfrscc_chip[2]={NULL,NULL};

/* i/o */
static u8 __fastcall mapread_mfrscc(int slot,u16 a)
{
	/* like Konami SCC */
	int bank=a>>13;

	if (scc_enabled[slot]&&(a&0xf800)==0x9800) return scc_read(scc_scc[slot],a);

	/* page 0,3 */
	if (a<0x4000||a>0xbfff) bank^=4;

	bank-=2;

	/* from flash */
	mfrscc_chip[slot]->readsector=mfrscc_chip[slot]->writesector=mfrscc_sector[slot][bank];
	return am29f_read(mfrscc_chip[slot],mfrscc_bank[slot][bank]|(a&0x1fff));
}

static void __fastcall mapwrite_mfrscc(int slot,u16 a,u8 v)
{
	/* like Konami SCC */
	int bank=a>>13;

	/* page 0,3 */
	if (a<0x4000||a>0xbfff) bank^=4;

	/* 2212P003 / 051649 */
	else {
		if ((a&0xf000)==0x9000) {
			if (a&0x800) {
				/* write to SCC */
				if (scc_enabled[slot]) scc_write(scc_scc[slot],a,v);
			}
			else scc_enabled[slot]=(v&0x3f)==0x3f;
		}

		if ((a&0x1800)==0x1000) {
			/* bankswitch */
			int s=v<<13;
			mfrscc_sector[slot][bank-2]=s&0x70000;
			mfrscc_bank[slot][bank-2]=s&0xf000;
		}
	}

	bank-=2;

	/* to flash */
	mfrscc_chip[slot]->readsector=mfrscc_chip[slot]->writesector=mfrscc_sector[slot][bank];
	am29f_write(mfrscc_chip[slot],mfrscc_bank[slot][bank]|(a&0x1fff),v);
}

/* state						size
sector							3*4
bank							2*4
SCC enabled						1

(+SCC+amf29f)

==								21
*/
#define STATE_VERSION_MFRSCC	3 /* mapper.c */
/* version history:
1: initial
2: (changes in scc.c)
3: no changes (mapper.c)
*/
#define STATE_SIZE_MFRSCC		21

static int __fastcall mapgetstatesize_mfrscc(int slot)
{
    slot = slot;
	return STATE_SIZE_MFRSCC+scc_state_get_size()+am29f_state_get_size();
}

/* save */
static void __fastcall mapsavestate_mfrscc(int slot,u8** s)
{
	int i=4;
	while (i--) {
		STATE_SAVE_3(mfrscc_sector[slot][i]);
		STATE_SAVE_2(mfrscc_bank[slot][i]);
	}
	STATE_SAVE_1(scc_enabled[slot]);

	scc_state_save(scc_scc[slot],s);
	am29f_state_save(mfrscc_chip[slot],s);
}

/* load */
static void __fastcall maploadstatecur_mfrscc_inc(int slot,u8** s)
{
	int i=4;
	while (i--) {
		STATE_LOAD_3(mfrscc_sector[slot][i]);
		STATE_LOAD_2(mfrscc_bank[slot][i]);
	}
	STATE_LOAD_1(scc_enabled[slot]);
}

static void __fastcall maploadstatecur_mfrscc(int slot,u8** s)
{
	maploadstatecur_mfrscc_inc(slot,s);

	scc_state_load_cur(scc_scc[slot],s);
	am29f_state_load_cur(mfrscc_chip[slot],s);
}

static int __fastcall maploadstate_mfrscc(int slot,int v,u8** s)
{
	switch (v) {
		case 1:
			maploadstatecur_mfrscc_inc(slot,s);
			scc_state_load(scc_scc[slot],v,s);
			am29f_state_load(mfrscc_chip[slot],v,s);
			break;
		case 2: case STATE_VERSION_MFRSCC:
			maploadstatecur_mfrscc(slot,s);
			break;

		default: return FALSE;
	}

	return TRUE;
}

/* init/clean */
static void mapclean_mfrscc(int slot)
{
	mapclean_konamiscc(slot);
	sram_save(slot,mfrscc_chip[slot]->data,0x80000,NULL);
	am29f_clean(mfrscc_chip[slot]);
	mfrscc_chip[slot]=NULL;
}

static void mapinit_mfrscc(int slot)
{
	int i;

	mfrscc_chip[slot]=am29f_init(FALSE);
	for (i=0;i<8;i++) mfrscc_chip[slot]->unprotect[i]=TRUE;

	if (!sram_load(slot,&mfrscc_chip[slot]->data,0x80000,NULL,0xff)) {
		/* upload game to flash */
		for (i=0;i<0x40;i++) memcpy(mfrscc_chip[slot]->data+0x2000*i,cart[slot][i],0x2000);
	}

	scc_scc[slot]=scc_init(SCC_VF_MFR,SCC_MODEL);
	mapclean[slot]=mapclean_mfrscc;

	mfrscc_chip[slot]->readsector=mfrscc_chip[slot]->writesector=0;
	am29f_reset(mfrscc_chip[slot]);
	scc_poweron(scc_scc[slot]);
	scc_enabled[slot]=FALSE;

	for (i=0;i<4;i++) {
		mfrscc_sector[slot][i]=0;
		mfrscc_bank[slot][i]=i<<13;
	}

	mapsetcustom_read(slot,0,8,mapread_mfrscc);
	mapsetcustom_write(slot,0,8,mapwrite_mfrscc);

	mapnf[slot]=mapnf_konamiscc;
	mapef[slot]=mapef_konamiscc;
	mapsetbc[slot]=mapsetbc_konamiscc;

	mapgetstatesize[slot]=mapgetstatesize_mfrscc;
	mapsavestate[slot]=mapsavestate_mfrscc;
	maploadstatecur[slot]=maploadstatecur_mfrscc;
	maploadstate[slot]=maploadstate_mfrscc;
}

/* --- Vincent DSK2ROM (uses Konami SCC)
	any disk file, read-only
*/
static u32 dsk2rom_crcprev[2]={0,0};

/* init/clean */
static void mapclean_dsk2rom(int slot)
{
	int i;

	/* restructure (backwards) */
	MEM_CLEAN(cart[slot][0]); MEM_CLEAN(cart[slot][1]);
	for (i=2;i<cartpages[slot];i++) cart[slot][i-2]=cart[slot][i];
	cart[slot][cartpages[slot]-1]=cart[slot][cartpages[slot]-2]=dummy_u;
	cartpages[slot]-=2;
	for (cartmask[slot]=1;cartmask[slot]<cartpages[slot];cartmask[slot]<<=1) { ; }
	cartmask[slot]--;

	cartcrc[slot]=dsk2rom_crcprev[slot];

	/* konamiscc part */
	mapclean_konamiscc(slot);
}

static void mapinit_dsk2rom(int slot)
{
	int i,yay=FALSE;
	u8* dsk2rom[2];

	#define DFN_MAX 4

	/* possible dsk2rom filenames */
	/*
	const char* fn[DFN_MAX][3]={
	{ "dsk2rom-0.80", "zip", "rom" },
	{ "dsk2rom",      "rom", NULL  },
	{ "dsk2rom",      "zip", "rom" },
	{ "dsk2rom-0.70", "zip", "rom" }
	};
    */
	const char* fn[DFN_MAX][3]={
	{ "bios/dsk2rom-0.80", "zip", "rom" },
	{ "bios/dsk2rom",      "rom", NULL  },
	{ "bios/dsk2rom",      "zip", "rom" },
	{ "bios/dsk2rom-0.70", "zip", "rom" }
	};

	MEM_CREATE(dsk2rom[0],0x2000); MEM_CREATE(dsk2rom[1],0x2000);

	/* open dsk2rom */
	for (i=0;i<DFN_MAX;i++) {
		// file_setfile(&file->appdir,fn[i][0],fn[i][1],fn[i][2]);
		file_setfile(NULL,fn[i][0],fn[i][1],fn[i][2]);
		if (file_open()&&file->size==0x4000&&file_read(dsk2rom[0],0x2000)&&file_read(dsk2rom[1],0x2000)) {
			yay=TRUE; file_close();
			break;
		}
		else file_close();
	}

	#undef DFN_MAX

	if (!yay) LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_SLOT1FAIL+slot),"slot %d: couldn't open DSK2ROM",slot+1);
	else if (cartpages[slot]!=0&&(cartpages[slot]+2)<=ROM_MAXPAGES&&memcmp(cart[slot][0],dsk2rom[0],0x2000)!=0&&memcmp(cart[slot][1],dsk2rom[1],0x2000)!=0)
    {
		/* restructure */
		int i=cartpages[slot];
		while (i--) cart[slot][i+2]=cart[slot][i];
		cart[slot][0]=dsk2rom[0]; cart[slot][1]=dsk2rom[1];
		dsk2rom[0]=dsk2rom[1]=NULL;
		cartpages[slot]+=2;
		for (cartmask[slot]=1;cartmask[slot]<cartpages[slot];cartmask[slot]<<=1) { ; }
		cartmask[slot]--;

		dsk2rom_crcprev[slot]=cartcrc[slot];
		cartcrc[slot]=(cartcrc[slot]<<1|(cartcrc[slot]>>31&1))^file->crc32; /* combine crc with dsk2rom crc */

		mapclean[slot]=mapclean_dsk2rom;

		/* konamiscc part */
		scc_scc[slot]=scc_init(SCC_VF_DEFAULT,SCC_MODEL);
	}

	MEM_CLEAN(dsk2rom[0]); MEM_CLEAN(dsk2rom[1]);

	mapinit_konamiscc(slot);
}

/******************************************************************************
 *                                                                            *
 *                   #include "mapper_ascii.c"                                *
 *                                                                            *
 ******************************************************************************/
// #include "mapper_ascii.c"
/* ASCII type mappers, 8KB or 16KB, with or without SRAM
used by most MegaROMs, see mapper_table.h for more information */

static u32 ascii_mainw_temp[2];
static u32 ascii_board_temp[2];
static u32 ascii_board[2];
static u32 ascii_mainw[2];

static int ascii_bankbit[2];
static int ascii_bankmask[2];

static int ascii_srammask[2];
static int ascii_srambit[2];
static int ascii_is_sram[2][4];
static int ascii_protectbit[2];
static int ascii_sramwrite[2][4];
static int ascii_srambank[2][4];

/* i/o */
static __inline__ void ascii_bankswitch_rom(int slot,int bank,int is16,int v)
{
	cartbank[slot][bank]=v<<is16&cartmask[slot];
	cartbank[slot][bank|is16]=(v<<is16&cartmask[slot])|is16;
	refresh_cb(slot);
}

static void __fastcall mapwrite_ascii(int slot,u16 a,u8 v)
{
	/* 16KB: no effect on $6800-$6fff or $7800-$7fff */
	if (a&ascii_bankbit[slot]&0x800) return;

	ascii_bankswitch_rom(slot,(a>>11&3)+2,ascii_bankbit[slot]&1,v);
}

static void __fastcall mapwrite_ascii_sram(int slot,u16 a,u8 v)
{
	/* mapper (rom bankswitch is like normal ascii) */
	if ((a&0xe000)==0x6000) {
		int bank,is16;

		if (a&ascii_bankbit[slot]&0x800) return;

		bank=a>>11&3;
		is16=ascii_bankbit[slot]&1;

		/* bankswitch sram/normal */
		if ((ascii_is_sram[slot][bank]=ascii_is_sram[slot][bank|is16]=((v&ascii_srambit[slot])!=0)&(((v^ascii_srambit[slot])<<is16&ascii_bankmask[slot])==0))) {
			ascii_srambank[slot][bank]=v<<(13+is16)&(cartsramsize[slot]-1);
			ascii_srambank[slot][bank|is16]=(v<<(13+is16)|(ascii_bankbit[slot]&0x2000))&(cartsramsize[slot]-1);

			/* write-protect bit */
			ascii_sramwrite[slot][bank]=ascii_sramwrite[slot][bank|is16]=((v&ascii_protectbit[slot])==0);
		}
		else ascii_bankswitch_rom(slot,bank+2,is16,v);
	}

	/* write to sram */
	else {
		int bank=(a>>13)-2;

		if (ascii_is_sram[slot][bank]&ascii_sramwrite[slot][bank]) cartsram[slot][ascii_srambank[slot][bank]|(a&ascii_srammask[slot])]=v;
	}
}

static u8 __fastcall mapread_ascii_sram(int slot,u16 a)
{
	int bank=a>>13;

	/* read from sram */
	if (ascii_is_sram[slot][bank-2]) return cartsram[slot][ascii_srambank[slot][bank-2]|(a&ascii_srammask[slot])];

	/* normal read */
	return cartmap[slot][bank][a&0x1fff];
}

/* state					size
main bank size				2
main config					4
board config				4
sram bank enabled			4*1
sram bank write enabled		4*1
sram bank numbers			4*3

==							30
*/
#define STATE_VERSION_ASCII	3 /* mapper.c */
/* version history:
1: initial
2: no changes (mapper.c)
3: extra config, merged ascii8/16/sram into one type
*/
#define STATE_SIZE_ASCII	30 /* (sram in mapper.c) */

static int __fastcall mapgetstatesize_ascii(int slot)
{
    slot = slot;
	return STATE_SIZE_ASCII;
}

/* save */
static void __fastcall mapsavestate_ascii(int slot,u8** s)
{
	int i;

	STATE_SAVE_2(ascii_bankbit[slot]);
	STATE_SAVE_4(ascii_mainw[slot]);
	STATE_SAVE_4(ascii_board[slot]);

	i=4; while (i--) {
		STATE_SAVE_1(ascii_is_sram[slot][i]);
		STATE_SAVE_1(ascii_sramwrite[slot][i]);
		STATE_SAVE_3(ascii_srambank[slot][i]);
	}
}

/* load */
static void __fastcall maploadstatecur_ascii(int slot,u8** s)
{
	u32 i,j,b;

	STATE_LOAD_2(i); /* not used yet */
	STATE_LOAD_4(i);
	STATE_LOAD_4(b);
	if (i!=ascii_mainw[slot]||b!=ascii_board[slot]) mel_error|=2;

	i=4; while (i--) {
		STATE_LOAD_1(ascii_is_sram[slot][i]);
		STATE_LOAD_1(ascii_sramwrite[slot][i]);
		STATE_LOAD_3(ascii_srambank[slot][i]);
	}

	/* sram here instead of mapper.c */
	loadstate_skip_sram[slot]=TRUE;

	if (b&A_SS_MASK) {
		i=(u32)0x200<<((b&(u32)A_SS_MASK)>>(u32)A_SS_SHIFT);
		if (i>(u32)cartsramsize[slot]) {
			/* source sram larger than destination */
			j=4; while (j--) ascii_srambank[slot][j]&=(cartsramsize[slot]-1);
			if (cartsramsize[slot]) { STATE_LOAD_C(cartsram[slot],cartsramsize[slot]); }
			else { j=4; while (j--) ascii_is_sram[slot][j]=ascii_sramwrite[slot][j]=FALSE; }
			(*s)+=(i-cartsramsize[slot]); /* skip rest */
		}
		else { STATE_LOAD_C(cartsram[slot],i); }
	}
}

static void __fastcall maploadstate_ascii_12(int slot,u8** s)
{
	/* version 1,2 (SRAM was already loaded) */
	int i,j,uid;

	_msx_state* msxstate=(_msx_state*)state_get_msxstate();
	uid=slot?msxstate->cart2_uid:msxstate->cart1_uid;

	switch (uid) {
		/* ASCII16 + 2KB SRAM */
		case DEPRECATED_ASCII16_2:
			if ((ascii_board[slot]&A_SS_MASK)==A_SS_02) {
				STATE_LOAD_1(j); ascii_is_sram[slot][0]=ascii_is_sram[slot][1]=(j!=0);
				STATE_LOAD_1(j); ascii_is_sram[slot][2]=ascii_is_sram[slot][3]=(j!=0);
			}
			else mel_error|=3;
			break;

		/* ASCII8 + 8KB SRAM */
		case DEPRECATED_ASCII8_8:
			if ((ascii_board[slot]&A_SS_MASK)==A_SS_08) {
				for (i=0;i<4;i++) { STATE_LOAD_1(j); ascii_is_sram[slot][i]=(j!=0); }
			}
			else mel_error|=3;
			break;

		/* ASCII8 + 32KB SRAM (Koei) */
		case DEPRECATED_ASCII8_32:
			if ((ascii_board[slot]&A_SS_MASK)==A_SS_32) {
				i=4; while (i--) {
					STATE_LOAD_1(j); ascii_is_sram[slot][i]=(j!=0);
					STATE_LOAD_2(ascii_srambank[slot][i]);
				}
			}
			else mel_error|=3;
			break;

		/* no SRAM */
		default:
			if (cartsramsize[slot]) {
				/* undo garbage sram load (fails if both slots were in use though) */
				memset(cartsram[slot],cartsramsize[slot],0xff);
				(*s)-=cartsramsize[slot];
			}

			i=4; while (i--) ascii_is_sram[slot][i]=0;
			break;
	}

	/* enable write */
	i=4; while (i--) ascii_sramwrite[slot][i]=1;
}

static int __fastcall maploadstate_ascii(int slot,int v,u8** s)
{
	switch (v) {
		case 1: case 2:
			maploadstate_ascii_12(slot,s);
			break;
		case STATE_VERSION_ASCII:
			maploadstatecur_ascii(slot,s);
			break;

		default: return FALSE;
	}

	return TRUE;
}

/* init */
static void mapinit_ascii(int slot)
{
	int f=0,i,m=0,p,s=8;

	/* init board */
	for (i=0;i<8;i++) {
		p=ascii_mainw[slot]>>(i<<2)&0xf;
		if (p==5) s=i; /* sram bit */
		m=m|(p&1)<<i; /* range mask */
	}

	ascii_bankbit[slot]=((ascii_board[slot]&A_BS_MASK)==A_16);
	ascii_bankmask[slot]=m<<ascii_bankbit[slot];
	cartmask[slot]|=ascii_bankmask[slot];

	/* sram */
	if (~s&8&&ascii_board[slot]&A_SS_MASK) {
		sram_load(slot,&cartsram[slot],0x200<<((ascii_board[slot]&A_SS_MASK)>>A_SS_SHIFT),NULL,0xff);

		ascii_srammask[slot]=(cartsramsize[slot]-1)&0x1fff;
		ascii_protectbit[slot]=((ascii_board[slot]&A_SP_MASK)!=0)<<(((ascii_board[slot]&A_SP_MASK)>>A_SP_SHIFT)-1)&0xff;
		ascii_srambit[slot]=1<<s;
	}
	else ascii_srambit[slot]=0;

	/* init handlers */
	mapgetstatesize[slot]=mapgetstatesize_ascii;
	mapsavestate[slot]=mapsavestate_ascii;
	maploadstatecur[slot]=maploadstatecur_ascii;
	maploadstate[slot]=maploadstate_ascii;

	for (i=0;i<4;i++) {
		/* 4 pages of 8KB or 2 pages of 16KB at $4000-$bfff, init to 0 */
		cartbank[slot][i+2]=i&ascii_bankbit[slot];

		ascii_is_sram[slot][i]=FALSE;
		ascii_sramwrite[slot][i]=TRUE;
		ascii_srambank[slot][i]=0;
	}

	/* flags */
	for (i=0;lutasciichip[i].uid!=(ascii_board[slot]&A_MC_MASK);i++) { ; }
	f=lutasciichip[i].flags;

	/* mapper chip at $6000-$7fff, sram at $4000-$bfff (if there) */
	if (cartsramsize[slot]) {
		mapsetcustom_read(slot,2,4,mapread_ascii_sram);
		mapsetcustom_write(slot,2+((f&A_MCF_SWLOW)==0),4-((f&A_MCF_SWLOW)==0),mapwrite_ascii_sram);
	}
	else mapsetcustom_write(slot,3,1,mapwrite_ascii);

	/* 16KB: bit 0,11,13 */
	if (ascii_bankbit[slot]) ascii_bankbit[slot]=0x2801;
}

/* media config dialog */
static void mce_a_setboard(HWND dialog,u32 w,u32 b)
{
	char t[0x10];
	int i;

	/* mapper chip */
	for (i=0;lutasciichip[i].uid!=(b&A_MC_MASK);i++) { ; }
	SendDlgItemMessage(dialog,IDC_MCE_A_MAPPER,CB_SETCURSEL,i,0);

	/* bank size */
	SendDlgItemMessage(dialog,IDC_MCE_A_BANKS,CB_SETCURSEL,(b&A_BS_MASK)==A_16,0);

	/* write editbox */
	sprintf(t,"%08x",w);
	for (i=0;i<8;i++)
	switch (t[i]) {
		case '5': t[i]='s'; break;
		case 'f': t[i]='u'; break;
		default:  t[i]='x'; break;
	}
	if (b&A_SP_MASK) t[(((b&A_SP_MASK)>>A_SP_SHIFT)-1)^7]='p';
	SetDlgItemText(dialog,IDC_MCE_A_WRITE,t);

	/* sram size */
	if (b&A_SS_MASK) SetDlgItemInt(dialog,IDC_MCE_A_SRAM,1<<(((b&A_SS_MASK)>>A_SS_SHIFT)-1),FALSE);
	else SetDlgItemInt(dialog,IDC_MCE_A_SRAM,0,FALSE);
}

static INT_PTR CALLBACK mce_a_dialog( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam )
{
    lParam = lParam;
	static int initial_board=0;
	static int cur_board=0;

	switch (msg) {

		case WM_INITDIALOG: {
			char t[0x100];
			HWND c;
			int i;

			SendDlgItemMessage(dialog,IDC_MCE_A_WRITE,EM_LIMITTEXT,8,0);
			SendDlgItemMessage(dialog,IDC_MCE_A_SRAM,EM_LIMITTEXT,2,0);

			/* fill comboboxes */
			c=GetDlgItem(dialog,IDC_MCE_A_BOARD);
			for (i=0;lutasciiboard[i].board!=(u32)~0;i++) {
				sprintf(t,"%s %s",lutasciiboard[i].company,lutasciiboard[i].name);
				SendMessage(c,CB_ADDSTRING,0,(LPARAM)((LPCTSTR)t));
			}

			c=GetDlgItem(dialog,IDC_MCE_A_MAPPER);
			for (i=0;lutasciichip[i].uid!=(u32)~0;i++) {
				sprintf(t,"%s %s",lutasciichip[i].company,lutasciichip[i].name);
				SendMessage(c,CB_ADDSTRING,0,(LPARAM)((LPCTSTR)t));
			}

			sprintf(t,"4*8KB"); SendDlgItemMessage(dialog,IDC_MCE_A_BANKS,CB_ADDSTRING,0,(LPARAM)((LPCTSTR)t));
			sprintf(t,"2*16KB"); SendDlgItemMessage(dialog,IDC_MCE_A_BANKS,CB_ADDSTRING,0,(LPARAM)((LPCTSTR)t));

			/* init board properties */
			for (i=0;(lutasciiboard[i].board&AB_MASK)!=(ascii_board_temp[mapper_extra_ref_slot]&AB_MASK);i++) { ; }
			SendDlgItemMessage(dialog,IDC_MCE_A_BOARD,CB_SETCURSEL,i,0); initial_board=cur_board=i;
			mce_a_setboard(dialog,ascii_mainw_temp[mapper_extra_ref_slot],ascii_board_temp[mapper_extra_ref_slot]);

			if ((ascii_board_temp[mapper_extra_ref_slot]&AB_MASK)!=AB_CUSTOM) {
				for (i=IDC_MCE_A_GROUPSTART;i<=IDC_MCE_A_GROUPEND;i++) EnableWindow(GetDlgItem(dialog,i),FALSE);
			}

			sprintf(t,"Slot %d: %s",mapper_extra_ref_slot+1,mapper_get_type_longname(mapper_extra_ref_type));
			SetWindowText(dialog,t);

			main_parent_window(dialog,MAIN_PW_LEFT,MAIN_PW_LEFT,mapper_extra_ref_pos.x,mapper_extra_ref_pos.y,0);

			break;
		}

		case WM_COMMAND:

			switch ( LOWORD(wParam) )
			{

				/* change board */
				case IDC_MCE_A_BOARD:       // Warning -Wimplicit-fallthrough
                {
					int i;
					if ((HIWORD(wParam))==CBN_SELCHANGE&&(i=SendDlgItemMessage(dialog,LOWORD(wParam),CB_GETCURSEL,0,0))!=CB_ERR&&i!=cur_board) {
						if ((lutasciiboard[i].board&AB_MASK)!=AB_CUSTOM) mce_a_setboard(dialog,lutasciiboard[i].mainw,lutasciiboard[i].board);
						cur_board=i;
						for (i=IDC_MCE_A_GROUPSTART;i<=IDC_MCE_A_GROUPEND;i++) EnableWindow( GetDlgItem(dialog,i),(lutasciiboard[cur_board].board&AB_MASK ) == AB_CUSTOM );
					}
				}
                break;
				/* close dialog */
				case IDOK:                  // Warning -Wimplicit-fallthrough
                {
					int i,j,mc;
					char t[0x100];
					u32 b=0;
					memset(t,0,0x100);

					/* get properties / error checking */
					if (initial_board==cur_board&&(lutasciiboard[cur_board].board&AB_MASK)!=AB_CUSTOM) EndDialog(dialog,0); /* no change */
					b|=(lutasciiboard[cur_board].board&AB_MASK);

					if ((i=SendDlgItemMessage(dialog,IDC_MCE_A_MAPPER,CB_GETCURSEL,0,0))!=CB_ERR) b|=lutasciichip[i].uid;
					for (mc=0;lutasciichip[mc].uid!=(b&A_MC_MASK);mc++) { ; }

					if ((i=SendDlgItemMessage(dialog,IDC_MCE_A_BANKS,CB_GETCURSEL,0,0))!=CB_ERR&&i) b|=A_16;

					i=GetDlgItemInt(dialog,IDC_MCE_A_SRAM,NULL,FALSE);
					if (i!=0&&i!=1&&i!=2&&i!=4&&i!=8&&i!=16&&i!=32&&i!=64) {
						LOG_ERROR_WINDOW(dialog,"Invalid SRAM size.");
						return 0;
					}
					if (i) {
						for (j=1;~i&1;j++,i>>=1) { ; }
						b|=j<<A_SS_SHIFT;

						if (~lutasciichip[mc].flags&A_MCF_SRAM) {
							sprintf(t,"%s can't have SRAM.",lutasciichip[mc].name);
							LOG_ERROR_WINDOW(dialog,t);
							return 0;
						}
					}

					j=0;
					GetDlgItemText(dialog,IDC_MCE_A_WRITE,t,10);
					if (strlen(t)!=8) {
						LOG_ERROR_WINDOW(dialog,"Write bitmask must be 8 characters.");
						return 0;
					}
					for (i=0;i<8;i++)
					switch (t[i]) {
						case 'u': case 'U': t[i]='f'; break;
						case 'x': case 'X': t[i]='e'; break;

						/* sram bit */
						case 's': case 'S':
							if (j++) {
								LOG_ERROR_WINDOW(dialog,"Only one 's' allowed in write bitmask.");
								return 0;
							}
							t[i]='5'; break;

						/* write-protect bit */
						case 'p': case 'P':
							if (~lutasciichip[mc].flags&A_MCF_PROTECT) {
								sprintf(t,"%s can't have write-protect.",lutasciichip[mc].name);
								LOG_ERROR_WINDOW(dialog,t);
								return 0;
							}

							if (b&A_SP_MASK) {
								LOG_ERROR_WINDOW(dialog,"Only one 'p' allowed in write bitmask.");
								return 0;
							}

							b|=((i^7)+1)<<A_SP_SHIFT;
							t[i]='e'; break;

						default:
							sprintf(t,"Write bit '%c' is unknown. Use 'u' for unmapped bits, 'x' for\ndon't care bits, 's' for SRAM bit, and 'p' for write-protect bit.",t[i]);
							LOG_ERROR_WINDOW(dialog,t);
							return 0;
					}

					if ((!j&&b&A_SS_MASK)||(j&&!(b&A_SS_MASK))||(!j&&b&A_SP_MASK)) {
						LOG_ERROR_WINDOW(dialog,"Write bitmask SRAM conflict.");
						return 0;
					}

					sscanf(t,"%x",&ascii_mainw_temp[mapper_extra_ref_slot]);
					ascii_board_temp[mapper_extra_ref_slot]=b;

					/* fall through */
				}
				case IDCANCEL:
					EndDialog(dialog,0);
                break;

				default:
                break;
			}
			break;

		default:
        break;
	}

	return 0;
}

/******************************************************************************
 *                                                                            *
 *                   #include "mapper_panasonic.c"                            *
 *                                                                            *
 ******************************************************************************/
// #include "mapper_panasonic.c"
/* Panasonic mappers */

/* --- Panasoft PAC, mapper chip: MEI MB674175U, board: JCI-C1H DFUP0204ZAJ
	8KB SRAM cart supported by Hydlide 3, Psychic War, ..?
*/
static const char* pac_fn="sram.pac";

/* 8KB SRAM in $4000-$5fff if $5ffe=$4d and $5fff=$69 */
static u8 __fastcall mapread_pac(int slot,u16 a)
{
	if ((cartsram[slot][0x1ffe]|cartsram[slot][0x1fff]<<8)==0x694d) return cartsram[slot][a&0x1fff];
	else return UNMAPPED;
}

static void __fastcall mapwrite_pac(int slot,u16 a,u8 v)
{
	a&=0x1fff;

	if (a>0x1ffd) cartsram[slot][a]=v;
	else if ((cartsram[slot][0x1ffe]|cartsram[slot][0x1fff]<<8)==0x694d) cartsram[slot][a]=v;
}

/* init/clean */
static void mapclean_pac(int slot)
{
	if (cartsram[slot]) {
		/* SRAM is saved in MSX compatible FM-PAC util format:
		16 bytes text header + 8192-2 bytes SRAM data */
		memmove(cartsram[slot]+16,cartsram[slot],0x1ffe);
		memcpy(cartsram[slot],"PAC2 BACKUP DATA",16);
	}

	sram_save(slot,cartsram[slot],8206,pac_fn);
}

static void mapinit_pac(int slot)
{
	if (sram_load(slot,&cartsram[slot],8206,pac_fn,0xff)) {
		memmove(cartsram[slot],cartsram[slot]+16,0x1ffe);
		cartsram[slot][0x1ffe]=cartsram[slot][0x1fff]=0xff;
	}
	cartsramsize[slot]=0x2000; /* mem size */
	mapclean[slot]=mapclean_pac;

	/* not mirrored */
	mapsetcustom_read(slot,2,1,mapread_pac);
	mapsetcustom_write(slot,2,1,mapwrite_pac);
}

/******************************************************************************
 *                                                                            *
 *                   #include "mapper_bootleg.c"                              *
 *                                                                            *
 ******************************************************************************/
// #include "mapper_bootleg.c"

/* Korean bootleg carts */

/* multicarts */
/*
	interesting games (don't know of existing stand-alone dumps):
	90-in-1 -> page 2 -> 01:			"Cannon Turbo" (Takeru)
	90-in-1 -> page 2 -> 18:			"Apollo Technica" (?)

	64-in-1 -> page 3 -> col 3 row 1:	"Safari-X" (Prosoft, 1987)
	126-in-1 -> page 6 -> col 2 row 1:	"Safari-X" (Prosoft, 1987)
	90-in-1 -> page 2 -> 05:			"Super Columns/Tetris" (Hi-Com, 1990)
	30-in-1 -> page 1 -> 03:			"Tetris II" (Prosoft, 1989)
	64-in-1 -> page 1 -> col 1 row 3:	"Tetris II" (Prosoft, 1989)
	126-in-1 -> page 3 -> col 2 row 6:	"Tetris II" (Prosoft, 1989)
*/

/* --- Bootleg 80-in-1
	used by 30-in-1, 64-in-1, 80-in-1
*/
/* (without having been hardware reverse engineered, and not many games to test it with, I'm not sure if this implementation is accurate) */
static void __fastcall mapwrite_btl80(int slot,u16 a,u8 v)
{
	/* all pages switchable at $4000-$7fff */
	cartbank[slot][(a&3)+2]=v&cartmask[slot];
	refresh_cb(slot);
}

static void mapinit_btl80(int slot)
{
	int i;

	/* 4 pages of 8KB at $4000-$bfff, init to 0 */
	for (i=2;i<6;i++) cartbank[slot][i]=0;

	mapsetcustom_write(slot,2,2,mapwrite_btl80);
}





/* --- Bootleg 90-in-1 (single game) */
/* (without having been hardware reverse engineered, and only 1 game to test it with, I'm not sure if this implementation is accurate) */
static int btl90_init_done[2]={0,0};

static __inline__ void btl90_bankswitch(int slot,u8 v)
{
	if (v&0x80) {
		/* 32KB mode */
		int bank=(v&0xfe)<<1&cartmask[slot];
		cartbank[slot][2]=bank++;
		cartbank[slot][3]=bank++;
		cartbank[slot][4]=bank++;
		cartbank[slot][5]=bank;
	}
	else {
		/* 16KB mode */
		cartbank[slot][2]=cartbank[slot][4]=v<<1&cartmask[slot];
		cartbank[slot][3]=cartbank[slot][5]=(v<<1&cartmask[slot])|1;
	}

	refresh_cb(slot);
}

static void __fastcall mapwrite_btl90(u8 v)
{
	if (btl90_init_done[0]) btl90_bankswitch(0,v);
	if (btl90_init_done[1]) btl90_bankswitch(1,v);
}

static void mapclean_btl90(int slot)
{
	btl90_init_done[slot]=FALSE;
	if (!btl90_init_done[slot^1]) io_setwriteport(0x77,NULL);
}

static void mapinit_btl90(int slot)
{
	mapclean[slot]=mapclean_btl90;
	btl90_init_done[slot]=TRUE;

	/* I/O at port $77 instead of standard memory mapped I/O */
	io_setwriteport(0x77,mapwrite_btl90);

	/* init to 0 */
	cartbank[slot][2]=cartbank[slot][4]=0;
	cartbank[slot][3]=cartbank[slot][5]=1;
}





/* --- Bootleg 126-in-1 (single game) */
/* (without having been hardware reverse engineered, and only 1 game to test it with, I'm not sure if this implementation is accurate) */
static void __fastcall mapwrite_btl126(int slot,u16 a,u8 v)
{
	/* both pages switchable at $4000-$7fff */
	int page=(a<<1&2)+2;

	cartbank[slot][page++]=v<<1&cartmask[slot];
	cartbank[slot][page]=(v<<1&cartmask[slot])|1;
	refresh_cb(slot);
}

static void mapinit_btl126(int slot)
{
	/* 2 pages of 16KB at $4000-$bfff, init to 0 */
	cartbank[slot][2]=cartbank[slot][4]=0;
	cartbank[slot][3]=cartbank[slot][5]=1;

	mapsetcustom_write(slot,2,2,mapwrite_btl126);
}

/******************************************************************************
 *                                                                            *
 *                         END INCLUDE MAPPERS                                *
 *                                                                            *
 ******************************************************************************/

#define UMAX CARTUID_MAX /* mapper.h */
static const _maptype maptype[CARTTYPE_MAX]={
/* uid	long name					short name	init function		flags */
{ UMAX,	"Al-Alamiah Al-Qur'an",		"Qur'an",	mapinit_quran,		0 },
{ 27,	"ASCII MegaROM",			"ASCII",	mapinit_ascii,		MCF_EXTRA },
{ 21,	"Bootleg 80-in-1",			"BTL80",	mapinit_btl80,		0 },
{ 22,	"Bootleg 90-in-1",			"BTL90",	mapinit_btl90,		0 },
{ 23,	"Bootleg 126-in-1",			"BTL126",	mapinit_btl126,		0 },
{ 9,	"dB-Soft Cross Blaim",		"CBlaim",	mapinit_crossblaim,	0 },
{ 6,	"Irem TAM-S1",				"TAM-S1",	mapinit_irem,		0 },
{ 19,	"Konami Game Master 2",		"GM2",		mapinit_gm2,		MCF_SRAM },
{ 8,	"Konami SCC",				"SCC",		mapinit_konamiscc,	0 },
{ 24,	"Konami Sound Cartridge",	"SCC-I",	mapinit_konamiscci,	MCF_EXTRA },
{ 13,	"Konami Synthesizer",		"KSyn",		mapinit_konamisyn,	0 },
{ 7,	"Konami VRC",				"VRC",		mapinit_konamivrc,	0 },
{ 14,	"Matra INK",				"INK",		mapinit_matraink,	0 },
{ 10,	"MicroCabin Harry Fox",		"HFox",		mapinit_harryfox,	0 },
{ 0,	"No mapper",				"NM",		mapinit_nomapper,	MCF_EXTRA },
{ 18,	"Panasoft PAC",				"PAC",		mapinit_pac,		MCF_SRAM | MCF_EMPTY },
{ 20,	"Pazos Mega Flash ROM SCC",	"MFRSCC",	mapinit_mfrscc,		MCF_SRAM },
{ 12,	"Sony Playball",			"Playball",	mapinit_playball,	0 },
{ 11,	"Vincent DSK2ROM",			"DSK2ROM",	mapinit_dsk2rom,	0 }
};
#undef UMAX

int mapper_get_type_uid(u32 type)
{
	if (type>=CARTTYPE_MAX) type=CARTTYPE_NOMAPPER;
	return maptype[type].uid;
}

int mapper_get_uid_type(u32 uid)
{
	int i,slot=((uid&0x80000000)!=0);
	uid&=0x7fffffff;
	if (uid>CARTUID_MAX) uid=0;

	/* look in maptype list */
	for (i=0;i<CARTTYPE_MAX;i++) if ((u32)maptype[i].uid==uid) return i;

	/* look in deprecated list (only accessed from msx.c) */
	switch (uid) {
		case DEPRECATED_START0000: case DEPRECATED_START4000: case DEPRECATED_START8000: case DEPRECATED_STARTC000: return CARTTYPE_NOMAPPER;
		case DEPRECATED_ASCII8: case DEPRECATED_ASCII16: return CARTTYPE_ASCII;
		case DEPRECATED_ZEMINAZMB: return CARTTYPE_KONAMIVRC;

		/* error if sram size is not the same */
		case DEPRECATED_ASCII16_2: return CARTTYPE_ASCII|((ascii_board[slot]&A_SS_MASK)!=A_SS_02)<<30;
		case DEPRECATED_ASCII8_8: return CARTTYPE_ASCII|((ascii_board[slot]&A_SS_MASK)!=A_SS_08)<<30;
		case DEPRECATED_ASCII8_32: return CARTTYPE_ASCII|((ascii_board[slot]&A_SS_MASK)!=A_SS_32)<<30;

		default: break;
	}

	return CARTTYPE_NOMAPPER;
}

const char* mapper_get_type_longname(u32 type)
{
	if (type>=CARTTYPE_MAX) type=CARTTYPE_NOMAPPER;
	return maptype[type].longname;
}

const char* mapper_get_type_shortname(u32 type)
{
	if (type>=CARTTYPE_MAX) type=CARTTYPE_NOMAPPER;
	if (maptype[type].shortname) return maptype[type].shortname;
	else return maptype[type].longname;
}

u32 mapper_get_type_flags(u32 type)
{
	if (type>=CARTTYPE_MAX) type=CARTTYPE_NOMAPPER;
	return maptype[type].flags;
}

void mapper_set_carttype(int slot,u32 type)
{
	carttype[slot&1]=type;
	mapper_extra_configs_apply(slot&1);
}

int mapper_get_carttype(int slot) { return carttype[slot&1]; }
int mapper_get_cartsize(int slot) { return cartsize[slot&1]; }
u32 mapper_get_cartcrc(int slot) { return cartcrc[slot&1]; }
u32 mapper_get_bioscrc(void) { return bioscrc; }
int mapper_get_autodetect_rom(void) { return autodetect_rom; }
const char* mapper_get_file(int slot) { if (cartfile[slot&1]&&strlen(cartfile[slot&1])) return cartfile[slot&1]; else return NULL; }
const char* mapper_get_defaultbios_file(void) { return fn_default_bios; }
void mapper_get_defaultbios_name(char* c) { strcpy(c,DEFAULT_BIOS); }
const char* mapper_get_bios_file(void) { return fn_bios; }
const char* mapper_get_bios_name(void) { return n_bios; }
void mapper_flush_ram(void) { memset(ram,RAMINIT,0x10000); }
int mapper_get_ramsize(void) { return ramsize; }
int mapper_get_ramslot(void) { return ramslot; }
int mapper_get_default_ramsize(void) { return DEFAULT_RAMSIZE; }
int mapper_get_default_ramslot(void) { return DEFAULT_RAMSLOT; }

char* mapper_get_current_name(char* c)
{
	MEM_CLEAN(file->name); /* set to NULL */
	if (mapper_get_file(1)) file_setfile(NULL,mapper_get_file(1),NULL,NULL);
	if (mapper_get_file(0)) file_setfile(NULL,mapper_get_file(0),NULL,NULL);

	if (file->name) sprintf(c,"%s",file->name);
	else {
		/* tape */
		if (tape_get_fn_cur()) file_setfile(NULL,tape_get_fn_cur(),NULL,NULL);
		if (file->name) sprintf(c,"%s",file->name);
		else {
			/* bios */
			if (fn_bios) file_setfile(NULL,fn_bios,NULL,NULL);
			if (file->name) sprintf(c,"%s",file->name);
			else sprintf(c,"none");
		}
	}

	return c;
}

void mapper_log_type(int slot,int one)
{
	/* only called from media.c */
	int s=mapper_get_cartsize(slot);
	char ls[0x10];

	if (s==0) sprintf(ls,"empty");
	else if (s<0x2000) sprintf(ls,"%dKB",(s>>10)+((s>>10)==0)); /* multiples of 1KB */
	else sprintf(ls,"%dKB",8*((s>>13)+((s&0x1fff)!=0))); /* multiples of 8KB */

	LOG(LOG_MISC,"%s%d: %s %s",one?"s":"slot ",slot+1,ls,mapper_get_type_shortname(mapper_get_carttype(slot)));
}

/* type extra configs */
void mapper_extra_configs_reset(int slot)
{
	/* no mapper */
	nomapper_rom_layout_temp[slot]=~0;	/* unmapped */
	nomapper_ram_layout_temp[slot]=~0;	/* no RAM */

	/* ascii */
	ascii_mainw_temp[slot]=0;			/* mirrored */
	ascii_board_temp[slot]=0;			/* default */

	/* SCC-I */
	scci_ram_layout_temp[slot]=3;		/* 128KB */
}

void mapper_extra_configs_apply(int slot)
{
	/* temp to cur */
	/* no mapper */
	nomapper_rom_layout[slot]=nomapper_rom_layout_temp[slot];
	nomapper_ram_layout[slot]=nomapper_ram_layout_temp[slot];

	/* ascii */
	ascii_mainw[slot]=ascii_mainw_temp[slot];
	ascii_board[slot]=ascii_board_temp[slot];

	/* SCC-I */
	scci_ram_layout[slot]=scci_ram_layout_temp[slot];
}

void mapper_extra_configs_revert(int slot)
{
	/* cur to temp */
	/* no mapper */
	nomapper_rom_layout_temp[slot]=nomapper_rom_layout[slot];
	nomapper_ram_layout_temp[slot]=nomapper_ram_layout[slot];

	/* ascii */
	ascii_mainw_temp[slot]=ascii_mainw[slot];
	ascii_board_temp[slot]=ascii_board[slot];

	/* SCC-I */
	scci_ram_layout_temp[slot]=scci_ram_layout[slot];
}

int mapper_extra_configs_differ(int slot,int type)
{
	if (~mapper_get_type_flags(type)&MCF_EXTRA) return FALSE;

	switch (type) {
		case CARTTYPE_NOMAPPER:
			if (nomapper_rom_layout_temp[slot]!=nomapper_rom_layout[slot]) return TRUE;
			if (nomapper_ram_layout_temp[slot]!=nomapper_ram_layout[slot]) return TRUE;
			break;
		case CARTTYPE_ASCII:
			if (ascii_mainw_temp[slot]!=ascii_mainw[slot]) return TRUE;
			if (ascii_board_temp[slot]!=ascii_board[slot]) return TRUE;
			break;
		case CARTTYPE_KONAMISCCI:
			if (scci_ram_layout_temp[slot]!=scci_ram_layout[slot]) return TRUE;
			break;
		default: break;
	}

	return FALSE;
}

void mapper_get_extra_configs(int slot,int type,u32* p)
{
	if (~mapper_get_type_flags(type)&MCF_EXTRA) return;

	/* max 64 bits */
	switch (type) {
		case CARTTYPE_NOMAPPER:
			p[0]=nomapper_rom_layout_temp[slot]; p[1]=nomapper_ram_layout_temp[slot]; break;
		case CARTTYPE_ASCII:
			p[0]=ascii_mainw_temp[slot]; p[1]=ascii_board_temp[slot]; break;
		case CARTTYPE_KONAMISCCI:
			p[0]=scci_ram_layout_temp[slot]; break;
		default: break;
	}
}

void mapper_extra_configs_dialog(HWND callwnd,int slot,int type,int x,int y)
{
	if (~mapper_get_type_flags(type)&MCF_EXTRA) return;

	mapper_extra_ref_slot=slot;
	mapper_extra_ref_type=type;
	mapper_extra_ref_pos.x=x; mapper_extra_ref_pos.y=y;

	switch (type) {
		case CARTTYPE_NOMAPPER:
         DialogBox( MAIN->module, MAKEINTRESOURCE(IDD_MEDIACART_E_NM), callwnd, (DLGPROC)mce_nm_dialog );
        break;
		case CARTTYPE_ASCII:
			DialogBox( MAIN->module, MAKEINTRESOURCE(IDD_MEDIACART_E_A), callwnd, (DLGPROC)mce_a_dialog );
        break;
		case CARTTYPE_KONAMISCCI:
			DialogBox( MAIN->module, MAKEINTRESOURCE(IDD_MEDIACART_E_SCCI), callwnd, (DLGPROC)mce_scci_dialog );
        break;
		default: break;
	}
}

static u8 __fastcall mapreadnull(u16 a) { a = a; return UNMAPPED; }
static void __fastcall mapwritenull(u16 a,u8 v) { a = a; v = v; }
static u8 __fastcall mapreadbios(u16 a) { a = a; return bios[a]; }

/* only active when tape is inserted */
static int bh_tapion_offset=0;
static int bh_tapin_offset=0;
static int bh_tapoon_offset=0;
static int bh_tapout_offset=0;

static u8 __fastcall mapreadbioshack(u16 a)
{
	if (bh_tapin_offset==a&&bh_tapin_offset==z80_get_pc()) { tape_read_byte(); return 0xc9; }
	else if (bh_tapout_offset==a&&bh_tapout_offset==z80_get_pc()) { tape_write_byte(); return 0xc9; }
	else if (bh_tapion_offset==a&&bh_tapion_offset==z80_get_pc()) { tape_read_header(); return 0xc9; }
	else if (bh_tapoon_offset==a&&bh_tapoon_offset==z80_get_pc()) { tape_write_header(); return 0xc9; }

	return bios[a];
}

static int mapper_patch_bios(u32 crc,u8* data)
{
	/* patch C-BIOS 0.21 PSG bug that's critical since meisei 1.2 (due to emulating the pin 6/7 quirk) */
	/* fixed since C-BIOS 0.22, I'll leave the 0.21 patch in for backwards compatibility */
	if (crc==0x1b3ab47f) {
		/* 2 instances of writing $80 to psg reg 15, change to $8f */
		data[0x152e]=data[0x15f1]=0x8f;
		return TRUE;
	}

	return FALSE;
}

static u8 __fastcall mapreadram(u16 a) { return ram[a]; }
static void __fastcall mapwriteram(u16 a,u8 v) { ram[a]=v; }

/* standard cart read (fast) */
static u8 __fastcall mapreadc10(u16 a) { return cartmap[0][0][a&0x1fff]; }	static u8 __fastcall mapreadc20(u16 a) { return cartmap[1][0][a&0x1fff]; }
static u8 __fastcall mapreadc12(u16 a) { return cartmap[0][1][a&0x1fff]; }	static u8 __fastcall mapreadc22(u16 a) { return cartmap[1][1][a&0x1fff]; }
static u8 __fastcall mapreadc14(u16 a) { return cartmap[0][2][a&0x1fff]; }	static u8 __fastcall mapreadc24(u16 a) { return cartmap[1][2][a&0x1fff]; }
static u8 __fastcall mapreadc16(u16 a) { return cartmap[0][3][a&0x1fff]; }	static u8 __fastcall mapreadc26(u16 a) { return cartmap[1][3][a&0x1fff]; }
static u8 __fastcall mapreadc18(u16 a) { return cartmap[0][4][a&0x1fff]; }	static u8 __fastcall mapreadc28(u16 a) { return cartmap[1][4][a&0x1fff]; }
static u8 __fastcall mapreadc1a(u16 a) { return cartmap[0][5][a&0x1fff]; }	static u8 __fastcall mapreadc2a(u16 a) { return cartmap[1][5][a&0x1fff]; }
static u8 __fastcall mapreadc1c(u16 a) { return cartmap[0][6][a&0x1fff]; }	static u8 __fastcall mapreadc2c(u16 a) { return cartmap[1][6][a&0x1fff]; }
static u8 __fastcall mapreadc1e(u16 a) { return cartmap[0][7][a&0x1fff]; }	static u8 __fastcall mapreadc2e(u16 a) { return cartmap[1][7][a&0x1fff]; }

static const fp_mapread lut_mapread[2][8]={
{ mapreadc10,	mapreadc12,	mapreadc14,	mapreadc16,	mapreadc18,	mapreadc1a,	mapreadc1c,	mapreadc1e },
{ mapreadc20,	mapreadc22,	mapreadc24,	mapreadc26,	mapreadc28,	mapreadc2a,	mapreadc2c,	mapreadc2e }
};

/* custom cart read/write */
static fp_mapread_custom mapread_custom_c1;					static fp_mapread_custom mapread_custom_c2;
static u8 __fastcall mapreadc1x(u16 a) { return mapread_custom_c1(0,a); }	static u8 __fastcall mapreadc2x(u16 a) { return mapread_custom_c2(1,a); }
static fp_mapwrite_custom mapwrite_custom_c1;					static fp_mapwrite_custom mapwrite_custom_c2;
static void __fastcall mapwritec1x(u16 a,u8 v) { mapwrite_custom_c1(0,a,v); }	static void __fastcall mapwritec2x(u16 a,u8 v) { mapwrite_custom_c2(1,a,v); }

static void mapsetcustom_read(int slot,int rb,int re,fp_mapread_custom f)
{
	int i; re+=rb;
	if (slot) {	slot=mapcart_getpslot(slot); for (i=rb;i<re;i++) mapread_std[slot][i]=mapreadc2x; mapread_custom_c2=f; }
	else {		slot=mapcart_getpslot(slot); for (i=rb;i<re;i++) mapread_std[slot][i]=mapreadc1x; mapread_custom_c1=f; }
}

static void mapsetcustom_write(int slot,int rb,int re,fp_mapwrite_custom f)
{
	int i; re+=rb;
	if (slot) {	slot=mapcart_getpslot(slot); for (i=rb;i<re;i++) mapwrite_std[slot][i]=mapwritec2x; mapwrite_custom_c2=f; }
	else {		slot=mapcart_getpslot(slot); for (i=rb;i<re;i++) mapwrite_std[slot][i]=mapwritec1x; mapwrite_custom_c1=f; }
}

static int mapcart_getpslot(int slot)
{
	/* ram  c1  c2
	   0    1   2
	   1    2   3
	   2    1   3
	   3    1   2 */
	if (slot) return 3-((ramslot==3)|(ramslot==0));
	else return 1+(ramslot==1);
}


int mapper_init_ramslot(int slot,int size)
{
	fp_mapread mapread_cc[2][8];
	fp_mapwrite mapwrite_cc[2][8];
	int i,j,c;

	if (slot==ramslot&&size==ramsize) return FALSE;

	/* remember cartridge handlers */
	for (i=0;i<2;i++) {
		c=mapcart_getpslot(i);
		for (j=0;j<8;j++) {
			mapread_cc[i][j]=mapread_std[c][j];
			mapwrite_cc[i][j]=mapwrite_std[c][j];
		}
	}

	/* reset standard handlers */
	for (j=1;j<4;j++)
	for (i=0;i<8;i++) {
		mapread_std[j][i]=mapreadnull;
		mapwrite_std[j][i]=mapwritenull;
	}
	/* slot 0: bios */
	for (i=4;i<8;i++) mapread_std[0][i]=mapreadbios;

	/* relocate cartridge handlers */
	ramslot=slot;
	for (i=0;i<2;i++) {
		c=mapcart_getpslot(i);
		for (j=0;j<8;j++) {
			mapread_std[c][j]=mapread_cc[i][j];
			mapwrite_std[c][j]=mapwrite_cc[i][j];
		}
	}

	/* set ram slot */
	ramsize=size;
	c=size>>3;

	if (c!=8) memset(ram,RAMINIT,0x10000-(0x200<<c));

	for (i=8-c;i<8;i++) {
		mapread_std[slot][i]=mapreadram;
		mapwrite_std[slot][i]=mapwriteram;
	}

	mapper_refresh_pslot_read();
	mapper_refresh_pslot_write();

	return TRUE;
}

void mapper_init(void)
{
	int i,j;
	int ramslot_set,ramsize_set;

#if 0
	/* check mapper table for duplicates */
	u32 c; i=0;
	for (;;) {
		if (lutmaptype[i].type==CARTTYPE_MAX) break;
		c=lutmaptype[i].crc;

		j=0;
		for (;;) {
			if (lutmaptype[j].type==CARTTYPE_MAX) break;
			if (c==lutmaptype[j].crc&&i!=j) printf("dup %d %d - %08X\n",i,j,c);
			j++;
		}

		i++;
	}
#endif

	/* open default bios */
	// file_setfile(&file->appdir,DEFAULT_BIOS,NULL,NULL);
	file_setfile(NULL,DEFAULT_BIOS,NULL,NULL);              // HEY BABY!
	if (!file_open()||file->size!=0x8000||!file_read(default_bios,0x8000)) {
		file_close();
		LOG(LOG_MISC|LOG_ERROR,"Couldn't open default BIOS ROM.\nEnsure that %s\nis in the application directory.",DEFAULT_BIOS); exit(1);
	}
	default_bioscrc=file->crc32;
	MEM_CREATE(fn_default_bios,strlen(file->filename)+1);
	strcpy(fn_default_bios,file->filename);
	file_close();

	mapper_patch_bios(default_bioscrc,default_bios);

	memset(dummy_u,UNMAPPED,0x2000);
	memset(dummy_ff,0xff,0x2000);

	autodetect_rom=TRUE; i=settings_get_yesnoauto(SETTINGS_AUTODETECTROM); if (i==FALSE||i==TRUE) autodetect_rom=i;

	scc_init_volume();

	/* ram settings */
	ramsize_set=DEFAULT_RAMSIZE;
	if (SETTINGS_GET_INT(settings_info(SETTINGS_RAMSIZE),&i)) {
		if (i!=0&&i!=8&&i!=16&&i!=32&&i!=64) i=DEFAULT_RAMSIZE;
		ramsize_set=i;
	}

	ramslot_set=DEFAULT_RAMSLOT;
	if (SETTINGS_GET_INT(settings_info(SETTINGS_RAMSLOT),&i)) {
		CLAMP(i,0,3);
		ramslot_set=i;
	}

	/* get bios filename */
	n_bios=NULL; SETTINGS_GET_STRING(settings_info(SETTINGS_BIOS),&n_bios);
	if (n_bios==NULL||strlen(n_bios)==0) mapper_set_bios_default();
	else {
		file_setfile(&file->biosdir,n_bios,NULL,NULL);
		MEM_CREATE(fn_bios,strlen(file->filename)+1);
		strcpy(fn_bios,file->filename);
	}

	/* set standard handlers */
	for (j=0;j<4;j++)
	for (i=0;i<8;i++) {
		mapread_std[j][i]=mapreadnull;
		mapwrite_std[j][i]=mapwritenull;
	}
	/* slot 0: bios */
	for (i=0;i<8;i++) mapread_std[0][i]=mapreadbios;

	/* carts */
	for (j=0;j<2;j++)
	for (i=0;i<ROM_MAXPAGES;i++) {
		cart[j][i]=dummy_u;
	}
	cart[0][ROMPAGE_DUMMY_U]=cart[1][ROMPAGE_DUMMY_U]=dummy_u;
	cart[0][ROMPAGE_DUMMY_FF]=cart[1][ROMPAGE_DUMMY_FF]=dummy_ff;

	/* ram */
	ramsize=-1;
	if (ramsize_set==64&&ramslot_set==0) ramsize_set=32; /* first half of slot 0 must be bios */
	mapper_init_ramslot(ramslot_set,ramsize_set);
}

void mapper_clean(void)
{
	mapper_close_cart(0);
	mapper_close_cart(1);

	MEM_CLEAN(fn_default_bios);
	MEM_CLEAN(fn_bios); MEM_CLEAN(n_bios);
}


/* bios */
static void mapper_patch_bios_comply_standards(void)
{
#if COMPLY_STANDARDS_TEST
	bios[6]=COMPLY_STANDARDS_VDPR;
	bios[7]=COMPLY_STANDARDS_VDPW;
#endif
	;
}

static void mapper_set_bios_default(void)
{
	memset(bios+0x8000,UNMAPPED,0x8000);
	memcpy(bios,default_bios,0x8000);
	mapper_patch_bios_comply_standards();
	bioscrc=default_bioscrc;

	MEM_CLEAN(n_bios);
	MEM_CREATE(n_bios,strlen(DEFAULT_BIOS)+1);
	sprintf(n_bios,DEFAULT_BIOS);

	MEM_CLEAN(fn_bios);
	MEM_CREATE(fn_bios,strlen(fn_default_bios)+1);
	strcpy(fn_bios,fn_default_bios);

	file_setfile(NULL,fn_bios,NULL,NULL);
	file_setdirectory(&file->biosdir);
}

void mapper_open_bios(const char* f)
{
	if (f==NULL) file_setfile(&file->biosdir,n_bios,NULL,"rom");
	else file_setfile(NULL,f,NULL,"rom");

	memset(bios,UNMAPPED,0x10000);

	if (!file_open()||file->size>0x10000||file->size<2||!file_read(bios,file->size)) {
		file_close();
		LOG(LOG_MISC|LOG_WARNING,"couldn't open BIOS, reverting to default\n");
		mapper_set_bios_default();
	}
	else {
		/* user specified */
		int len=strlen(file->name)+1;
		int ext=FALSE;
		if (file->ext&&strlen(file->ext)) {
			ext=TRUE;
			len+=strlen(file->ext)+1;
		}
		bioscrc=file->crc32;

		mapper_patch_bios(bioscrc,bios);

		MEM_CLEAN(n_bios);
		MEM_CREATE(n_bios,len);
		strcpy(n_bios,file->name);
		if (ext) {
			strcat(n_bios,".");
			strcat(n_bios,file->ext);
		}

		MEM_CLEAN(fn_bios);
		MEM_CREATE(fn_bios,strlen(file->filename)+1);
		strcpy(fn_bios,file->filename);

		file_close();
		file_setdirectory(&file->biosdir);

		mapper_patch_bios_comply_standards();
	}

	if (cont_get_auto_region()) {
		/* autodetect keyboard region */
		int i=cont_get_region();

		/* info in bios */
		switch (bios[0x2c]&0xf) {
			case 0: {
				/* japanese/korean */
				u16 a=bios[5]<<8|bios[4]; /* CGTABL */
				u8 e[0x10]; memset(e,0,0x10);

				/* korean characters $fc and $fd are empty */
				if (a<=0x3800&&!memcmp(bios+a+0x7e0,e,0x10)) i=CONT_REGION_KOREAN;
				else i=CONT_REGION_JAPANESE;

				break;
			}

			case 3: i=CONT_REGION_UK; break;	/* UK */
			case 1:								/* international */
			case 2:								/* french */
			case 4:								/* DIN (german) */
			case 6:								/* spanish */
			default: /* unknown */
				i=CONT_REGION_INTERNATIONAL; break;
		}

		cont_set_region(i);
	}
}

void mapper_update_bios_hack(void)
{
	int i;

	if (tape_is_inserted()) {
		/* hacked */
		for (i=0;i<4;i++) mapread_std[0][i]=mapreadbioshack;

		/* set offsets */
		i=bios[0xe2]|bios[0xe3]<<8;
		if (bios[0xe1]==0xc3&&i<0x4000) bh_tapion_offset=i;
		else bh_tapion_offset=0xe1;

		i=bios[0xe5]|bios[0xe6]<<8;
		if (bios[0xe4]==0xc3&&i<0x4000) bh_tapin_offset=i;
		else bh_tapin_offset=0xe4;

		i=bios[0xeb]|bios[0xec]<<8;
		if (bios[0xea]==0xc3&&i<0x4000) bh_tapoon_offset=i;
		else bh_tapoon_offset=0xea;

		i=bios[0xee]|bios[0xef]<<8;
		if (bios[0xed]==0xc3&&i<0x4000) bh_tapout_offset=i;
		else bh_tapout_offset=0xed;
	}
	else {
		/* normal */
		for (i=0;i<4;i++) mapread_std[0][i]=mapreadbios;
	}
}


/* carts */
int mapper_open_cart(int slot,const char* f,int auto_patch)
{
	#define ROM_CLOSE(x)					\
		file_patch_close(); file_close();	\
		if (!x) LOG(LOG_MISC|LOG_WARNING,"slot %d: couldn't open file\n",slot+1); \
		return x

	int i=TRUE;
	u32 crc,crc_p=0;
	char n[STRING_SIZE]={0};

	slot&=1;

	/* open file, just to get checksum before patching */
	file_setfile(NULL,f,NULL,"rom");
	if (!file_open()) { ROM_CLOSE(FALSE); }
	crc=file->crc32;
	file_close();

	if (auto_patch) {
		/* open optional ips (ones that enlarge filesize are unsupported in this case) */
		strcpy(n,file->name);
		file_setfile(&file->patchdir,n,"ips",NULL);
		if (!file_open()) {
			file_close();
			file_setfile(&file->patchdir,n,"zip","ips");
			if (!file_open()) { i=FALSE; file_close(); }
		}
		crc_p=file->crc32;
		if (i&(crc_p!=crc)) {
			LOG(LOG_MISC,"slot %d: applying patch (%sIPS)\n",slot+1,file->is_zip?"zipped ":"");
			if (!file_patch_init()) { file_patch_close(); crc_p=0; }
		}
		else crc_p=0;
		file_close();

		/* set to rom file again */
		file_setfile(NULL,f,NULL,"rom");
	}

	if (file_open()) {
		char* temp_fn;
		u32 temp_crc32;
		u32 temp_size;

		int pages=(file->size>>13)+((file->size&0x1fff)!=0);
		if (pages>ROM_MAXPAGES) { ROM_CLOSE(FALSE); }

		/* read rom */
		for (i=0;i<pages;i++) {
			MEM_CREATE(tempfile[i],0x2000); memset(tempfile[i],UNMAPPED,0x2000);
			if (!file_read(tempfile[i],0x2000)) {
				i++;
				while (i--) { MEM_CLEAN(tempfile[i]); }

				ROM_CLOSE(FALSE);
			}
		}

		/* success */
		MEM_CREATE(temp_fn,strlen(file->filename)+1);
		strcpy(temp_fn,file->filename);
		temp_crc32=file->crc32;
		temp_size=file->size;

		file_close();
		mapper_close_cart(slot); /* close previous */

		cartfile[slot]=temp_fn;
		cartcrc[slot]=temp_crc32;
		if (crc_p) cartcrc[slot]=(cartcrc[slot]<<1|(cartcrc[slot]>>31&1))^crc_p; /* combine with patch crc */

		/* set cart */
		for (i=0;i<pages;i++) cart[slot][i]=tempfile[i];
		for (cartmask[slot]=1;cartmask[slot]<pages;cartmask[slot]<<=1) { ; }
		cartmask[slot]--; cartsize[slot]=temp_size; cartpages[slot]=pages;
	}
	else { ROM_CLOSE(FALSE); }

	ROM_CLOSE(TRUE);
}

static void mapper_clean_custom(int slot)
{
	if (mapclean[slot]==mapclean_nothing&&cartsramsize[slot]) {
		/* default sram save */
		sram_save(slot,cartsram[slot],cartsramsize[slot],NULL);
	}
	else mapclean[slot](slot);

	mapclean[slot]=mapclean_nothing;
	mapgetstatesize[slot]=mapgetstatesize_nothing;
	mapsavestate[slot]=mapsavestate_nothing;
	maploadstatecur[slot]=maploadstatecur_nothing;
	maploadstate[slot]=maploadstate_nothing;
	mapsetbc[slot]=mapsetbc_nothing;
	mapstream[slot]=mapstream_nothing;
	mapnf[slot]=mapnf_nothing;
	mapef[slot]=mapef_nothing;
}

void mapper_close_cart(int slot)
{
	int i;

	slot&=1;
	mapper_clean_custom(slot);

	for (i=0;i<ROM_MAXPAGES;i++) {
		if (cart[slot][i]!=dummy_u&&cart[slot][i]!=dummy_ff) { MEM_CLEAN(cart[slot][i]); }
		cart[slot][i]=dummy_u;
	}

	MEM_CLEAN(cartfile[slot]);
	cartcrc[slot]=cartpages[slot]=cartmask[slot]=cartsize[slot]=cartsramsize[slot]=0;
	cartsram[slot]=NULL;
}

u32 mapper_autodetect_cart(int slot,const char* fn)
{
	int i,pages;
	u8* f=NULL;
	u32 ret;

	mapper_extra_configs_reset(slot);

	/* open */
	file_setfile(NULL,fn,NULL,"rom");
	if (!file_open()||file->size>ROM_MAXSIZE) {
		file_close();
		return CARTTYPE_NOMAPPER;
	}

	pages=(file->size>>13)+((file->size&0x1fff)!=0);
	MEM_CREATE_N(f,pages*0x2000);

	if (!file_read(f,file->size)) {
		MEM_CLEAN(f); file_close();
		return CARTTYPE_NOMAPPER;
	}
	file_close();

	/* set extra configs to default */
	/* (not needed with ascii or SCC-I) */
	nomapper_rom_config_default(slot,pages,-1);

	/* try parse, {x} in filename where x=maptype shortname */
	if (file->name&&strlen(file->name)) {
		int begin,end,len=strlen(file->name),found=0;

		for (end=len-1;end;end--) if (file->name[end]=='}') { found|=1; break; }
		for (begin=end;begin>=0;begin--) if (file->name[begin]=='{') { found|=2; break; }
		begin++;

		if (begin<end&&found==3&&(end-begin)<0x10) {
			char shortname_fn[0x10];
			memset(shortname_fn,0,0x10);
			memcpy(shortname_fn,file->name+begin,end-begin);

			/* special case for ascii */
			if (!stricmp(shortname_fn,"ASCII8"))  { ascii_board_temp[slot]=A_08; return CARTTYPE_ASCII; }
			if (!stricmp(shortname_fn,"ASCII16")) { ascii_board_temp[slot]=A_16; return CARTTYPE_ASCII; }

			for (i=0;i<CARTTYPE_MAX;i++)
			if (!stricmp(shortname_fn,mapper_get_type_shortname(i))) {
				MEM_CLEAN(f);
				return i;
			}
		}
	}

	if (!autodetect_rom) return CARTTYPE_NOMAPPER;

	/* try lookuptable */
	for (i=0;lutmaptype[i].type!=CARTTYPE_MAX;i++) {
		if (lutmaptype[i].crc==file->crc32&&lutmaptype[i].size==file->size) {
			MEM_CLEAN(f);

			/* copy extra configs */
			switch (lutmaptype[i].type) {
				case CARTTYPE_NOMAPPER:
					nomapper_rom_layout_temp[slot]=lutmaptype[i].extra1;
					nomapper_ram_layout_temp[slot]=lutmaptype[i].extra2;
					break;
				case CARTTYPE_ASCII:
					ascii_mainw_temp[slot]=lutmaptype[i].extra1;
					ascii_board_temp[slot]=lutmaptype[i].extra2;
					break;
				case CARTTYPE_KONAMISCCI:
					scci_ram_layout_temp[slot]=lutmaptype[i].extra1;
					break;
				default: break;
			}

			return lutmaptype[i].type;
		}
	}

	if (file->size&0x1fff) memset(f+file->size,0xff,0x2000-(file->size&0x1fff));
	ret=CARTTYPE_NOMAPPER;

	/* guess type */
	switch (pages) {
		/* small ROMs are almost always detected correctly */

		/* 8KB/16KB */
		case 1: case 2: {
			u16 start=f[3]<<8|f[2];

			/* start address of $0000: call address in the $4000 region: $4000, else $8000 */
			if (start==0) {
				if ((f[5]&0xc0)==0x40) nomapper_rom_config_default(slot,pages,0x4000);
				else nomapper_rom_config_default(slot,pages,0x8000);
			}

			/* start address in the $8000 region: $8000, else default */
			else if ((start&0xc000)==0x8000) nomapper_rom_config_default(slot,pages,0x8000);

			break;
		}

		/* 32KB */
		case 4:
			/* no "AB" at $0000, but "AB" at $4000 */
			if (f[0]!='A'&&f[1]!='B'&&f[0x4000]=='A'&&f[0x4001]=='B') {
				u16 start=f[0x4003]<<8|f[0x4002];

				/* start address of $0000 and call address in the $4000 region, or start address outside the $8000 region: $0000, else default */
				if ((start==0&&(f[0x4005]&0xc0)==0x40)||start<0x8000||start>=0xc000) nomapper_rom_config_default(slot,4,0x0000);
			}

			break;

		/* 48KB */
		case 6:
			/* "AB" at $0000, but no "AB" at $4000, not "AB": $0000 */
			if (f[0]=='A'&&f[1]=='B'&&f[0x4000]!='A'&&f[0x4001]!='B') nomapper_rom_config_default(slot,6,0x4000);
			else nomapper_rom_config_default(slot,6,0x0000);

			break;

		/* 384KB, only game with that size is R-Type */
		case 0x30: ret=CARTTYPE_IREMTAMS1; break;

		/* other */
		default:
			/* disk sizes (360KB and 720KB are most common) */
			if (file->size==163840||file->size==184320||file->size==327680||file->size==368640||file->size==655360||file->size==737280||file->size==1474560) ret=CARTTYPE_DSK2ROM;

			/* 64KB and no "AB" at start, or smaller than 64KB: no mapper */
			else if (pages<8||(pages==8&&f[0]!='A'&&f[1]!='B')) break;

			/* megarom */
			else {
				/* Idea from fMSX, assume that games write to standard mapper addresses
				(which almost all of them do) and count ld (address),a ($32) occurences.
				Excluding unique or SRAM types, it guesses correctly ~95% of the time. */

				/* incomplete list of tested roms:
				- set -				- tested -	- exceptions -
				hacked tape-to-A16	126/128		MSX1: Bubbler, Spirits  --  MSX2: - (they're all MSX1)
				homebrew			20/23		MSX1: Monster Hunter (English, Spanish) (works ok, just no SCC), Mr. Mole (SCC version)  --  MSX2: -
				official ASCII 8	142/152		MSX1: Bomber King, MSX English-Japanese Dictionary  --  MSX2: Aoki Ookami to Shiroki Mejika - Genchou Hishi, Genghis Khan, Fleet Commander II, Japanese MSX-Write II, Mad Rider, Nobunaga no Yabou - Sengoku Gunyuu Den, Nobunaga no Yabou - Zenkokuban, Royal Blood
				official ASCII 16	54/59		MSX1: The Black Onyx II  --  MSX2: Daisenryaku, Ikari, Penguin-Kun Wars 2, Zoids - Chuuoudai Riku no Tatakai
				official Konami's	48/48		-
				Zemina A/K clones	12/12		- */
				#define GUESS_AS8	0 /* $6000, $6800, $7000, $7800 */
				#define GUESS_KOV	1 /* $6000, $8000, $a000 */
				#define GUESS_A16	2 /* $6000, $7000 */
				#define GUESS_SCC	3 /* $5000, $7000, $9000, $b000 */
				int type[]={0,0,0,0};
				int guess=GUESS_AS8;

				for (i=0;(u32)i<(file->size-2u);i++) {
					if (f[i]==0x32)
					switch (f[i+2]<<8|f[i+1]) {
						case 0x6000: type[GUESS_AS8]+=3; type[GUESS_KOV]+=2; type[GUESS_A16]+=4; break;
						case 0x7000: type[GUESS_AS8]+=3; type[GUESS_A16]+=4; type[GUESS_SCC]+=2; break;
						case 0x6800: case 0x7800: type[GUESS_AS8]+=3; break;
						case 0x8000: case 0xa000: type[GUESS_KOV]+=3; break;
						case 0x5000: case 0x9000: case 0xb000: type[GUESS_SCC]+=2; break;
						case 0x77ff: type[GUESS_A16]+=1; break; /* used sometimes by A16 */
						default: break;
					}
				}

				/* check winner */
				for (i=1;i<4;i++) {
					if (type[i]>type[guess]) guess=i;
				}

				switch (guess) {
					case GUESS_KOV: ret=CARTTYPE_KONAMIVRC; break;
					case GUESS_A16: ret=CARTTYPE_ASCII; ascii_board_temp[slot]=A_16; break;
					case GUESS_SCC: ret=CARTTYPE_KONAMISCC; break;
					default: ret=CARTTYPE_ASCII; ascii_board_temp[slot]=A_08; break;
				}
			}

			break;
	}

	MEM_CLEAN(f);
	return ret;
}

void mapper_init_cart(int slot)
{
	int i,p;

	slot&=1;
	mapper_clean_custom(slot);
	p=mapcart_getpslot(slot);

	for (i=0;i<8;i++) {
		/* default read handlers */
		mapread_std[p][i]=lut_mapread[slot][i];

		/* default write handlers */
		mapwrite_std[p][i]=mapwritenull;

		/* empty memmap */
		cartbank[slot][i]=ROMPAGE_DUMMY_U;
		cartmap[slot][i]=dummy_u;
	}

	maptype[carttype[slot]].init(slot);
	refresh_cb(slot);

	reverse_invalidate();
}

static __inline__ void refresh_cb(int slot)
{
	/* refresh cart banks */
	cartmap[slot][0]=cart[slot][cartbank[slot][0]];
	cartmap[slot][1]=cart[slot][cartbank[slot][1]];
	cartmap[slot][2]=cart[slot][cartbank[slot][2]];
	cartmap[slot][3]=cart[slot][cartbank[slot][3]];
	cartmap[slot][4]=cart[slot][cartbank[slot][4]];
	cartmap[slot][5]=cart[slot][cartbank[slot][5]];
	cartmap[slot][6]=cart[slot][cartbank[slot][6]];
	cartmap[slot][7]=cart[slot][cartbank[slot][7]];
}

void __fastcall mapper_refresh_cb(int slot) { refresh_cb(slot&1); }


/* primary slot register */
void __fastcall mapper_refresh_pslot_read(void)
{
	mapread[0]=mapread_std[pslot&3][0];			mapread[1]=mapread_std[pslot&3][1];
	mapread[2]=mapread_std[pslot>>2&3][2];		mapread[3]=mapread_std[pslot>>2&3][3];
	mapread[4]=mapread_std[pslot>>4&3][4];		mapread[5]=mapread_std[pslot>>4&3][5];
	mapread[6]=mapread_std[pslot>>6&3][6];		mapread[7]=mapread_std[pslot>>6&3][7];
}

void __fastcall mapper_refresh_pslot_write(void)
{
	mapwrite[0]=mapwrite_std[pslot&3][0];		mapwrite[1]=mapwrite_std[pslot&3][1];
	mapwrite[2]=mapwrite_std[pslot>>2&3][2];	mapwrite[3]=mapwrite_std[pslot>>2&3][3];
	mapwrite[4]=mapwrite_std[pslot>>4&3][4];	mapwrite[5]=mapwrite_std[pslot>>4&3][5];
	mapwrite[6]=mapwrite_std[pslot>>6&3][6];	mapwrite[7]=mapwrite_std[pslot>>6&3][7];
}

void __fastcall mapper_write_pslot(u8 v)
{
	pslot=v;

	mapper_refresh_pslot_read();
	mapper_refresh_pslot_write();
}

u8 __fastcall mapper_read_pslot(void) { return pslot; }


/* SRAM */
static int sram_netplay[2]={0,0};

static int sram_load(int slot,u8** d,u32 size,const char* fnc,u8 fill)
{
	int ret=FALSE;
	char fn[STRING_SIZE];

	if (size==0) return FALSE;

	MEM_CREATE_N(*d,size);
	memset(*d,fill,size);
	cartsram[slot]=*d; cartsramsize[slot]=size;

	if (!fnc&&!mapper_get_file(slot)) return FALSE; /* empty slot */

	if (netplay_is_active()) {
		/* don't load (or save) if netplay */
		LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_NETPLAYSRAM),"SRAM is volatile during netplay");
		sram_netplay[slot]=TRUE;
		return FALSE;
	}

	if (fnc) file_setfile(&file->batterydir,fnc,NULL,NULL);
	else {
		file_setfile(NULL,mapper_get_file(slot),NULL,NULL);
		strcpy(fn,file->name);
		file_setfile(&file->batterydir,fn,"sav",NULL);
	}

	if (!file_accessible()) {
		/* SRAM file didn't exist yet */
		if (!file_save()||!file_write(*d,size)) LOG(LOG_MISC|LOG_WARNING,"couldn't init slot %d battery backed SRAM\n",slot+1);
	}

	/* load */
	else if (!file_open()||file->size!=size||!file_read(*d,size)) {
		LOG(LOG_MISC|LOG_WARNING,"couldn't load slot %d battery backed SRAM\n",slot+1);
		memset(*d,fill,size);
	}

	else ret=TRUE;

	file_close();

	MEM_CREATE(cartsramcopy[slot],size);
	memcpy(cartsramcopy[slot],*d,size);

	return ret;
}

static void sram_save(int slot,u8* d,u32 size,const char* fnc)
{
	int err=FALSE;
	char fn[STRING_SIZE];

	if (sram_netplay[slot]||d==NULL||size==0||(!fnc&&!mapper_get_file(slot))||(cartsramcopy[slot]&&memcmp(cartsramcopy[slot],d,size)==0)) err=TRUE;

	sram_netplay[slot]=FALSE;
	MEM_CLEAN(cartsramcopy[slot]);

	cartsramsize[slot]=0;
	if (err) { MEM_CLEAN(d); return; }

	if (fnc) file_setfile(&file->batterydir,fnc,NULL,NULL);
	else {
		file_setfile(NULL,mapper_get_file(slot),NULL,NULL);
		strcpy(fn,file->name);
		file_setfile(&file->batterydir,fn,"sav",NULL);
	}

	/* save */
	if (!file_save()||!file_write(d,size)) LOG(LOG_MISC|LOG_WARNING,"couldn't save slot %d battery backed SRAM\n",slot+1);
	file_close();

	/* clean */
	MEM_CLEAN(d);
}


/* state				size
ram slot				1
ram size				1
ram						0x10000
pslot					1

cart banknums			2*2*8 (32)

==						0x10000+35

type specific			?

*/
/* version derivatives: mapper*.c, am29f040b.c, sample.c, scc.c */
#define STATE_VERSION	3
/* version history:
1: initial
2: SRAM crash bugfix + changes in scc.c
3: configurable RAM size/slot, cartridge extra configs (ascii, no mapper, scc-i), changed order
*/
#define STATE_SIZE		(0x10000+35)

int __fastcall mapper_state_get_version(void)
{
	return STATE_VERSION;
}

int __fastcall mapper_state_get_size(void)
{
	return STATE_SIZE+cartsramsize[0]+cartsramsize[1]+mapgetstatesize[0](0)+mapgetstatesize[1](1);
}

/* shortcuts */
#define SCB(x,y) STATE_SAVE_2(cartbank[x][y])
#define LCB(x,y) STATE_LOAD_2(cartbank[x][y])

/* save */
void __fastcall mapper_state_save(u8** s)
{
	/* msx ram, just save the whole 64KB */
	STATE_SAVE_1(ramslot);
	STATE_SAVE_1(ramsize);
	STATE_SAVE_C(ram,0x10000);
	STATE_SAVE_1(pslot);

	/* cartridge banks */
	SCB(0,0); SCB(0,1); SCB(0,2); SCB(0,3); SCB(0,4); SCB(0,5); SCB(0,6); SCB(0,7);
	SCB(1,0); SCB(1,1); SCB(1,2); SCB(1,3); SCB(1,4); SCB(1,5); SCB(1,6); SCB(1,7);

	/* cartridge custom */
	mapsavestate[0](0,s); if (cartsramsize[0]) { STATE_SAVE_C(cartsram[0],cartsramsize[0]); }
	mapsavestate[1](1,s); if (cartsramsize[1]) { STATE_SAVE_C(cartsram[1],cartsramsize[1]); }
}

/* load */
void __fastcall mapper_state_load_cur(u8** s)
{
	int i;

	mel_error=0;

	/* msx ram */
	STATE_LOAD_1(i); if (i!=ramslot) mel_error|=3;
	STATE_LOAD_1(i); if (i!=ramsize) mel_error|=2;
	STATE_LOAD_C(ram,0x10000);
	STATE_LOAD_1(pslot);

	/* cartridge banks */
	LCB(0,0); LCB(0,1); LCB(0,2); LCB(0,3); LCB(0,4); LCB(0,5); LCB(0,6); LCB(0,7);
	LCB(1,0); LCB(1,1); LCB(1,2); LCB(1,3); LCB(1,4); LCB(1,5); LCB(1,6); LCB(1,7);

	/* cartridge custom */
	maploadstatecur[0](0,s); if (cartsramsize[0]&&!loadstate_skip_sram[0]) { STATE_LOAD_C(cartsram[0],cartsramsize[0]); }
	maploadstatecur[1](1,s); if (cartsramsize[1]&&!loadstate_skip_sram[1]) { STATE_LOAD_C(cartsram[1],cartsramsize[1]); }

	loadstate_skip_sram[0]=loadstate_skip_sram[1]=FALSE;
}

void __fastcall mapper_state_load_12(int v,u8** s)
{
	/* version 1,2 */

	/* msx ram */
	STATE_LOAD_C(ram,0x10000);
	STATE_LOAD_1(pslot);

	/* cartridge banks */
	LCB(0,0); LCB(0,1); LCB(0,2); LCB(0,3); LCB(0,4); LCB(0,5); LCB(0,6); LCB(0,7);
	LCB(1,0); LCB(1,1); LCB(1,2); LCB(1,3); LCB(1,4); LCB(1,5); LCB(1,6); LCB(1,7);

	/* cartridge custom */
	if (cartsramsize[0]) { STATE_LOAD_C(cartsram[0],cartsramsize[0]); }
	if (cartsramsize[1]) { STATE_LOAD_C(cartsram[1],cartsramsize[1]); }

	maploadstate[0](0,v,s);
	maploadstate[1](1,v,s);
}

int __fastcall mapper_state_load(int v,u8** s)
{
	mel_error=0;

	switch (v) {
		case 1: case 2:
			mapper_state_load_12(v,s);
			break;
		case STATE_VERSION:
			mapper_state_load_cur(s);
			break;

		default: return FALSE;
	}

	return TRUE;
}

#undef SCB
#undef LCB

/******************************************************************************
 *                                                                            *
 *                          END "mapper.c"                                    *
 *                                                                            *
 ******************************************************************************/
