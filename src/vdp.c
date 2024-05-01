/******************************************************************************
 *                                                                            *
 *                                 "vdp.c"                                    *
 *                                                                            *
 ******************************************************************************/

#include "global.h"
#include "io.h"
#include "z80.h"
#include "vdp.h"
#include "draw.h"
#include "crystal.h"
#include "state.h"
#include "settings.h"
#include "resource.h"
#include "main.h"
#include "msx.h"
#include "tool.h"
#include "reverse.h"
#include "sound.h"

/*

Mostly tested on my MSX1s:
- Canon V-20 VDP: TMS9929ANL (unsynced crystals)
- Panasonic CF2700, VDP under a heatsink, very likely to be a TMS9129
- National (Panasonic) CF1200 (Japanese, NTSC), VDP: TMS9918A
- Sony HB-10P: T6950 (unsynced crystals)
and smaller tests on other people's MSXes

Known models on MSXes:
- Texas Instruments TMS9918A/TMS9928A/TMS9929A -- 8/9=60Hz/50Hz, 1/2=YIQ/YPbPr
- Texas Instruments TMS9118/TMS9128/TMS9129 -- support for 4KB was removed
- Toshiba T6950 -- 50Hz VDP clone with 42 pins (normal one has 40 pins), one of the extra pins selects PAL/NTSC. Support for
  overlay (video-in) was removed. Used in Sony HB-10P, Toshiba HX-20, Casio MX-15, ..
- Toshiba T7937A -- VDP inside this MSX engine used in some 60Hz MSX1 (eg. Gradiente), same as T6950? -- not tested well yet
- Yamaha V9938/V9958, used in >=MSX2, designed by ASCII, V9938 also used in some MSX1 (Spectravideo SVI-738, Yamaha CX5M-128)

Palettes (colours) may be different due to VDP and/or other circuitry, especially on the T6950.
The evolutions by Yamaha use RGB.

Known differences with quirks:
 group     4K/16K effect?      M3 table masks      M3 sprite clones      M3 mixed mode      M1 bogus mode
TMS99xxA   X                   X                   X                     X                  X
TMS91xx                        X                   X                     X                  X
Toshiba                                                                  X                  X
V99x8                          X                                                                          (may have statusregister differences too)

Not counting VRAM write timing or vblank length, software that doesn't (ab)use the quirks,
should work on all models. Common MSX1 VDP models are emulated here with scanline accuracy.

Unclear:
- pixel-accurate timing and mid-screen VRAM accesses by the Z80
- logical explanation on why sprite cloning behaves the way it does
- mid-screen statusregister reads, especially in screen 0 and blank

*/

static int vdp_chiptype=0;
static int viewbg;
static int viewspr;
static int unlim;		/* will cause glitches in games that use overflow to hide sprite lines (eg. Antarctic Adventure, Athletic Land) */

static u8 ram[0x4000];
static int vblank=FALSE;
static int vblank_suppress=FALSE;
static int last_vram_access=0;

#define UPLOAD_MAX 0x100
static int upload_pending=FALSE;
static int upload_count=0;
static int upload_address[UPLOAD_MAX];
static int upload_size[UPLOAD_MAX];
static u8* upload_data[UPLOAD_MAX];

/* lumi noise */
static void vdp_luminoise_recalc(void);
static void vdp_luminoise_end_frame(void);
static int luminoise=0;
static int lumivol=100;	/* in percent */

/* regs */
static int regs[8];		/* 8 control registers */
static int bgc=0;		/* bg colour at vblank */
static int status=0;	/* status register */
static int statuslow=0;

static int latch=0;		/* 1st/2nd write */
static int address=0;
static int read=0;		/* read ahead buffer */

static const char* vdp_chiptype_name[VDP_CHIPTYPE_MAX]={
/*	NTSC				PAL */
	"TI TMS9118",		"TI TMS9129",
	"TI TMS9918A",		"TI TMS9929A",
	"Toshiba T7937A",	"Toshiba T6950"
};
static const int vdp_chiptype_uid[VDP_CHIPTYPE_MAX]={ 0, 1, 2, 3, 4, 5 };

const char* vdp_get_chiptype_name(u32 i) { if (i>=VDP_CHIPTYPE_MAX) return NULL; else return vdp_chiptype_name[i]; }
int __fastcall vdp_get_chiptype(void) { return vdp_chiptype; }
void vdp_set_chiptype(u32 i) { if (i>=VDP_CHIPTYPE_MAX) i=0; vdp_chiptype=i; vdp_luminoise_recalc(); }
void vdp_set_chiptype_vf(int i) { vdp_set_chiptype((vdp_chiptype&~1)|(i!=0)); }
int vdp_get_chiptype_vf(int t) { return t&1; }
int vdp_luminoise_get(void) { return luminoise; }
int vdp_luminoise_get_volume(void) { return lumivol; }

int vdp_get_chiptype_uid(u32 type)
{
	if (type>=VDP_CHIPTYPE_MAX) type=0;
	return vdp_chiptype_uid[type];
}

int vdp_get_uid_chiptype(u32 id)
{
	int type;

	for (type=0;type<VDP_CHIPTYPE_MAX;type++) {
		if ((u32)vdp_chiptype_uid[type]==id) return type;
	}

	return 0;
}

int __fastcall vdp_get_address(void) { return address; }
int __fastcall vdp_get_reg(u32 r) { return regs[r&7]; }
int __fastcall vdp_get_status(void) { return status; }
int __fastcall vdp_get_statuslow(void) { return statuslow; }
u8* __fastcall vdp_get_ram(void) { return ram; }
void __fastcall vdp_reset_vblank(void) { vblank=FALSE; }

int vdp_get_bgc(void) { return bgc; }
void vdp_set_bgc(int c) { bgc=c; }
int vdp_get_bg_enabled(void) { return viewbg==0; }
void vdp_set_bg_enabled(int i) { main_menu_check(IDM_LAYERB,i); viewbg=i?0:8; }
int vdp_get_spr_enabled(void) { return viewspr!=0; }
void vdp_set_spr_enabled(int i) { main_menu_check(IDM_LAYERS,i); viewspr=i?0xff:0; }
int vdp_get_spr_unlim(void) { return unlim; }
void vdp_set_spr_unlim(int i) { main_menu_check(IDM_LAYERUNL,i); unlim=i; }

/* debug related */
void __fastcall vdp_whereami(char* d)
{
	int i;
	int c=*z80_get_cycles_ptr();
	char s[0x10];

	if (c<0) c+=crystal->frame;

	for (i=0;i<crystal->lines;i++) {
		if (c<=crystal->sc[i]&&c>=crystal->sc[i+1]) {
			sprintf(s,"%03d,%06.2f",i,((crystal->sc[i]-c)/(float)crystal->vdp_scanline)*342.0);
			break;
		}
	}

	if (i==crystal->lines) sprintf(s,"???,???.??");

	if (d) strcpy(d,s);
	else printf("%s\n",s);
}

void __fastcall vdp_z80toofastvram(void)
{
	int c=z80_get_rel_cycles();
	int pc=z80_get_pc();

	if ((last_vram_access-c)<(29*crystal->cycle)/*&&pc!=0x6800&&pc!=0x6846*/) {
		printf("vdpa PC $%04X, cc %02d, ",pc,(last_vram_access-c)/crystal->cycle);
		vdp_whereami(NULL);
	}

	if (!vblank&&regs[1]&0x40) last_vram_access=c;
}


void vdp_init(void)
{
	char* c=NULL;
	int i=8; while (i--) regs[i]=0;
	for (i=0;i<UPLOAD_MAX;i++) upload_data[i]=NULL;

	i=settings_get_yesnoauto(SETTINGS_LAYERB);		if (i!=FALSE&&i!=TRUE) i=TRUE;	vdp_set_bg_enabled(i);
	i=settings_get_yesnoauto(SETTINGS_LAYERS);		if (i!=FALSE&&i!=TRUE) i=TRUE;	vdp_set_spr_enabled(i);
	i=settings_get_yesnoauto(SETTINGS_LAYERUNL);	if (i!=FALSE&&i!=TRUE) i=FALSE;	vdp_set_spr_unlim(i);
	i=settings_get_yesnoauto(SETTINGS_LUMINOISE);	if (i!=FALSE&&i!=TRUE) i=FALSE;	luminoise=i;

	if (SETTINGS_GET_INT(settings_info(SETTINGS_LUMINOISE_VOLUME),&i)) { CLAMP(i,0,5000); lumivol=i; }

	i=VDP_CHIPTYPE_DEFAULT;
	SETTINGS_GET_STRING(settings_info(SETTINGS_VDPCHIP),&c);
	if (c&&strlen(c)) {
		for (i=0;i<VDP_CHIPTYPE_MAX;i++) if (stricmp(c,vdp_get_chiptype_name(i))==0) break;
		if (i==VDP_CHIPTYPE_MAX) i=VDP_CHIPTYPE_DEFAULT;
	}
	MEM_CLEAN(c);
	vdp_set_chiptype(i);
}

void vdp_clean(void)
{
	int i;
	for (i=0;i<UPLOAD_MAX;i++) { MEM_CLEAN(upload_data[i]); }
}

void vdp_poweron(void)
{
	int i=8;
	while (i--) regs[i]=0;
	status=statuslow=latch=address=read=last_vram_access=0;

	/* power-on VRAM contents: $ff on even and 0 on uneven address on my MSX1s, $ff on my MSX2 */
	/* MSX software known to be affected:
		Universe: Unknown (final) (expects 0, small glitch on stage start otherwise)
	*/
	i=0x4000;
	while (i--) ram[i]=(i&1)?0:0xff;
}

void vdp_reset(void)
{
	regs[0]=regs[1]=0;
	status=statuslow;
}

int vdp_upload(u32 a,u8* data,u32 size)
{
	if ((a&(VDP_UPLOAD_REG-1))>0x3fff||data==NULL||size==0||(a&VDP_UPLOAD_REG&&size!=1)||size>(0x4000-a)||upload_pending&2) return FALSE;

	if (msx_get_paused()) {
		/* only copy/invalidate when it differs */
		if (a&VDP_UPLOAD_REG) {
			/* reg */
			if (data[0]!=regs[a&7]) {
				regs[a&7]=data[0];
				tool_copy_locals(); reverse_invalidate();
			}
		}
		else if (memcmp(ram+a,data,size)) {
			/* ram */
			memcpy(ram+a,data,size);
			tool_copy_locals(); reverse_invalidate();
		}
	}
	else {
		if (upload_count==UPLOAD_MAX) return FALSE;

		upload_pending|=1;
		MEM_CREATE_N(upload_data[upload_count],size);
		memcpy(upload_data[upload_count],data,size);
		upload_size[upload_count]=size;
		upload_address[upload_count]=a;
		upload_count++;
	}

	return TRUE;
}

void vdp_new_frame(void)
{
	/* upload data */
	if (upload_pending) {
		int i;
		int c=FALSE;

		upload_pending|=2;

		for (i=0;i<UPLOAD_MAX;i++) {
			if (upload_data[i]==NULL) break;

			if (upload_address[i]&VDP_UPLOAD_REG) {
				/* reg */
				if (upload_data[i][0]!=regs[upload_address[i]&7]) {
					regs[upload_address[i]&7]=upload_data[i][0];
					c=TRUE;
				}
			}
			else if (memcmp(ram+upload_address[i],upload_data[i],upload_size[i])) {
				/* ram */
				memcpy(ram+upload_address[i],upload_data[i],upload_size[i]);
				c=TRUE;
			}

			MEM_CLEAN(upload_data[i]);
		}

		if (c) reverse_invalidate();
		upload_count=0;
		upload_pending=FALSE;
	}

	vblank=FALSE;

	if (last_vram_access<(2*crystal->frame)) last_vram_access+=crystal->frame;
}

void vdp_end_frame(void)
{
	vdp_luminoise_end_frame();
}

void __fastcall vdp_vblank(void)
{
	if (vblank) return;

	vblank=TRUE;
	bgc=regs[7]&0xf; /* remember background colour */

	if (!vblank_suppress) {
		status|=0x80;
		if (regs[1]&0x20) z80_irq_next(2*crystal->frame);
	}
	else vblank_suppress=FALSE;
}


/* data read */
u8 __fastcall vdp_read_data(void)
{
	u8 ret=read;
	read=ram[address];
	address=(address+1)&0x3fff;
	latch=0;
#if VDP_FASTACCESS
	vdp_z80toofastvram();
#endif
	return ret;
}

/* data write */
void __fastcall vdp_write_data(u8 v)
{
	read=ram[address]=v;
	address=(address+1)&0x3fff;
	latch=0;
#if VDP_FASTACCESS
	vdp_z80toofastvram();
#endif
}

/* address/reg write */
void __fastcall vdp_write_address(u8 v)
{
	if (latch) {
		address=(v<<8|(address&0xff))&0x3fff;

		/* reg */
		if (v&0x80) {

			/* EXTVID (register 0 bit 0): causes static/sync problems on my MSX1s, not emulatable */
			/* if (!(v&7)&&address&1) printf("X"); */

			/* reg 1 special cases */
			if ((v&7)==1) {

				/* interrupt */
				if (status&0x80) {
					if (address&0x20) { if (!z80_irq_pending()) z80_irq_next(z80_get_cycles()); }
					else if (z80_irq_pending()) z80_irq_ack();
				}

				/* 4K/16K mode swaps addresslines on TMS99xxA. */
				/* On (most?) real chips, unmapped RAM in 4K mode will degrade in time to its
				initial power-on contents (maybe faster with heat), this effect is unemulated. */
				if ((regs[1]^address)&0x80&&(vdp_chiptype&~1)==VDP_CHIPTYPE_TMS9918) {
					u8 buf[0x4000];
					int i=0x4000;

					if (address&0x80) while (i--) buf[i]=ram[(i&0x203f)|(i>>1&0xfc0)|(i<<6&0x1000)];
					else while (i--) buf[i]=ram[(i&0x203f)|(i<<1&0x1f80)|(i>>6&0x40)];

					memcpy(ram,buf,0x4000);
				}
			}

			regs[v&7]=address&0xff;
		}

		/* read */
		else if (~v&0x40) {
			read=ram[address];
			address=(address+1)&0x3fff;
#if VDP_FASTACCESS
			vdp_z80toofastvram();
#endif
		}
	}
	else address=(address&0x3f00)|v;

	latch^=1;
}

/* status read */
u8 __fastcall vdp_read_status(void)
{
	int cc;

	/* in reality, the lower bits are filled with the internal horizontal/sprite counter */
	u8 ret=status;
	latch=0;
	status=statuslow;

	/* If the status register is read just before vblank, vblank interrupt is suppressed and vblank status is not set
	(or cleared at the same time it's read+set). If it's read exactly at vblank, only the interrupt is suppressed */
	if (!vblank&&(cc=*z80_get_cycles_ptr())>=crystal->vblank_trigger&&cc<(crystal->vblank_trigger+3*crystal->vdp_cycle)) {
		if (cc<(crystal->vblank_trigger+crystal->vdp_cycle)) ret|=0x80;
		vblank_suppress=TRUE;
	}

	if (z80_irq_pending()) z80_irq_ack();
	return ret;
}


/* render 1 scanline */
void __fastcall vdp_line(int line)
{
	u8* screen=draw_get_screen_ptr()+256*line;
	int mode=(regs[1]>>4&1)|(regs[1]>>1&4)|(regs[0]&2)|viewbg; /* bit 0: M1, bit 1: M3, bit 2: M2, bit 3: bg disabled */
	int pn=regs[2]<<10&0x3fff;	/* pattern nametable */
	int pg=regs[4]<<11&0x3fff;	/* pattern generator table */
	int ct=regs[3]<<6;			/* colour table */
	int bd=regs[7]&0xf;			/* background colour */
	const int toshiba=(vdp_chiptype&~1)==VDP_CHIPTYPE_T7937A;

	/* Example software using M3 pg/ct mirroring:
	- Can of Worms (some of the minigames)
	- Ice King

	- demos by L!T/Bandwagon
	- Dr. Archie (sprite cloning of the boat in the intro happens on real MSX too)
	- Lotus F3 (with MSX1 non-reduced palette, + causes sprite clones)
	- Moon Over Arba Minch (game in development)
	- Universe: Unknown (intro, not in-game)

	undocumented "mixed" screenmodes aren't used often, just in dvik/joyrex's "scr5.rom" and Illusions demo */
	if (~regs[1]&0x40) { mode=9; statuslow=0x1f; } /* blank screen */
	else if (mode&2) {
		/* no M3 pg/ct mirroring on Toshiba VDPs */
		if (toshiba) { pg|=0x1800; ct|=0x1fc0; }
		pg=(line<<5&pg)|(pg&0x2000);

		/* M3 combination */
		if (mode&5) mode^=2;
	}

	switch (mode) {

		/* screen 1 */
		case 0: {
			int x,pd,p,cd;

			pn|=(line<<2&0xfe0);
			pg|=(line&7);

			/* 32 tiles, 8 pixels per tile */
			for (x=0;x<32;x++) {
				cd=ram[pn|x];
				pd=ram[cd<<3|pg];
				cd=ram[cd>>3|ct];

				#define UPD1() *screen++=(p=((pd<<=1)&0x100)?cd>>4:cd&0xf)?p:bd
				UPD1(); UPD1(); UPD1(); UPD1(); UPD1(); UPD1(); UPD1(); UPD1();
				/* (screen 2 uses it too) */
			}

			break;
		}

		/* screen 0 */
		case 1: case 5: {
			int x,tc=regs[7]>>4;
			if (!tc) tc=bd;

			/* left border, 6 pixels (9 on MSX2 and up) */
			*screen++=bd; *screen++=bd; *screen++=bd; *screen++=bd;
			*screen++=bd; *screen++=bd;

			if (mode&4) {
				/* bogus mode: M2 (or +3) is set too, this causes a glitched,
				but consistent static screen with vertical bars, unaffected by
				VRAM contents */

				/* 40 times 4 pixels text colour, 2 pixels background colour */
				for (x=0;x<40;x++) {
					*screen++=tc; *screen++=tc; *screen++=tc; *screen++=tc;
					*screen++=bd; *screen++=bd;
				}
			}
			else {
				/* standard M1 */
				int pd;

				pn|=((line>>3)*40);
				pg|=(line&7);

				/* 40 tiles, 6 pixels per tile */
				for (x=0;x<40;x++) {
					pd=ram[ram[pn+x]<<3|pg];

					#define UPD0() *screen++=((pd<<=1)&0x100)?tc:bd;
					UPD0(); UPD0(); UPD0(); UPD0(); UPD0(); UPD0();
					#undef UPD0
				}
			}

			/* right border, 10 pixels (7 on MSX2 and up) */
			*screen++=bd; *screen++=bd; *screen++=bd; *screen++=bd;
			*screen++=bd; *screen++=bd; *screen++=bd; *screen++=bd;
			*screen++=bd; *screen++=bd;

			statuslow=0x1f;

			break;
		}

		/* screen 2, M3 aka high res */
		case 2: {
			int x,pd,p,cd,ct_mask=ct|0x3f;

			pn|=(line<<2&0xfe0);
			pg|=(line&7);
			ct=(ct&0x2000)|(line&7)|(line<<5&0x1800);

			/* 32 tiles, 8 pixels per tile */
			for (x=0;x<32;x++) {
				cd=ram[pn|x]<<3;
				pd=ram[cd|pg];
				cd=ram[(cd|ct)&ct_mask];

				UPD1(); UPD1(); UPD1(); UPD1(); UPD1(); UPD1(); UPD1(); UPD1();
				#undef UPD1
			}

			break;
		}

		/* screen 3, M2 aka multicolor */
		case 4: {
			int x,pd,p;

			pn|=(line<<2&0xfe0);
			pg|=(line>>2&7);

			/* 32 tiles, 8 pixels per 'tile' */
			for (x=0;x<32;x++) {
				pd=ram[ram[pn|x]<<3|pg];

				/* left 4 pixels */
				p=pd>>4; p=p?p:bd;
				*screen++=p; *screen++=p; *screen++=p; *screen++=p;

				/* right 4 pixels */
				p=pd&0xf; p=p?p:bd;
				*screen++=p; *screen++=p; *screen++=p; *screen++=p;
			}

			break;
		}

		/* blank screen */
		default: {
			memset(screen,bd,256);
			screen+=256;

			break;
		}
	}

	/* sprites */
	if (~mode&1) {
		int size=8<<(regs[1]>>1&1),mag=regs[1]&1;
		int smax=(size<<mag)-1;
		int smask=(size&16)?0xfe0:0xfff;
		int y,f=0,o=0x20,clone=0x20,slmask=0xff;
		int clonemask[6]={0xff,0,0,0xff,0,0};	/* line AND, y XOR, y OR */
		u8* sa=ram+(regs[5]<<7&0x3fff);			/* sprite attribute table */
		u8* sg=ram+(regs[6]<<11&0x3fff);		/* sprite generator table */
		u8 sl[0x20+0x100+0x20];					/* sprite line */
		u8 yc;

		statuslow=0;

		line=(line-1)&0xff; /* sprite preprocessing is actually done one line in advance */

		/* invalidate out of bounds area */
		memset(sl,0x80,0x20);
		memset(sl+0x20,0,0x100);
		memset(sl+0x120,0x80,0x20);

		/* init sprite cloning */
		if (regs[0]&2&&(regs[4]&3)!=3&&!toshiba) {
			/* On TMS9xxx(/A) M3, sprites 8-31 Y position(s) is influenced by the least two
			bits of the pattern generator table offset, and somewhat by bits 5 and 6 of the
			colour table register. Sprite locations, mostly those on Y 0-63, will become
			glitchy, and cause a cloning effect similar to tiles with M3 table mirroring.
			This is not just a visual effect, as sprite collision and overflow behave as if
			sprites are normal. If the VDP is running hot, this effect will deteriorate
			(confirmed by flyguille with a blow dryer :) ), starting with block 1, and
			block 2 shortly after (effect deterioration from heat is unemulated). I assume
			the reason for the first 8 sprites not being affected is due to them being
			preprocessed in hblank. This glitch is briefly mentioned in the official
			TMS91xx programming manual btw, meaning that it's not undocumented.

			Known software affected by this (not counting tests or small glitches):
			- Alankomaat by Bandwagon, at the fire part (intended)
			- Lotus F3, unintended problems in MSX1 palette mode (black screen on my MSX1s)

			The calculations below are done to get the correct result, I can't think of
			a way to solve this logically. This implementation has been tested side by side
			to my MSX1, with all possible sprite Y and addressmasks. */

			/* 0-255, 4 blocks of 64 lines */
			if (line>191) {
				/* block 3 (only visible at the far top of the screen)
				Sprites are cloned from block borders. The colour block
				erase effect is the same as on block 2 below. */
				clonemask[0]=0xff; clonemask[2]=~regs[4]<<6&0xc0;
				if (~regs[3]&0x40&&clonemask[2]&0x80) {
					clonemask[0]&=0x7f;
					clonemask[2]&=0x7f;
				}

				clonemask[3]=clonemask[0]; clonemask[5]=clonemask[2];

				clone=7;
			}
			else if (line>127) {
				/* block 2, sprites are cloned from block 0.
				The bottom part of sprites on block 1+2 is always
				invisible. If this colour block mask is reset, real
				sprites in block 2 will be erased. */

				/* only has effect if this pattern block mask is reset */
				if (~regs[4]&2) {
					clonemask[1]=clonemask[4]=0x80;
					clonemask[5]=regs[3]<<1&0x80;

					clone=7;
				}
			}
			else if (line>63) {
				/* block 1, sprites are cloned from block 0.
				If this colour block mask is reset, the bottom part of
				sprites on block 0+1 will be invisible. */

				/* only has effect if this pattern block mask is reset */
				if (~regs[4]&1) {
					clonemask[0]=0x3f;
					clonemask[5]=~regs[3]<<1&0x40;

					clone=7;
				}
			}
			else {
				/* block 0, no effect */
				;
			}
		}

		for (;;) {
			/* forced break */
			if ((y=*sa++)==208) break;

			/* calculate offset */
			if (statuslow>clone) {
				yc=(line&clonemask[0])-((y^clonemask[1])|clonemask[2]);
				if (yc>smax) yc=(line&clonemask[3])-((y^clonemask[4])|clonemask[5]);
			}
			else yc=line-y;

			/* found sprite */
			if (yc<=smax) {
				u8* slp; int cd,g,x=*sa+++32;
				int pd=sg[g=yc>>mag|(*sa++<<3&smask)];			/* pixel data */
				if (f++&4) {
					o|=((status>>1^0x40)&0x40);					/* overflow */
					if (unlim) {								/* unlimited sprites? continue */
						if (slmask&0x80) { o&=0x40; slmask=statuslow; }
					}
					else break;
				}
				if ((cd=*sa++)&0x80) x-=32;
				cd=(cd&0xf)|0x20;	/* colour data */

				slp=sl+x;

				/* normal 8 pixel update */
				#define UPDS()									\
				if ((pd<<=1)&0x100) {							\
					status|=(*slp&0x20&o); /* collision */		\
					if (!(*slp&0x8f)) *slp=cd;					\
				} slp++

				/* magnified (16 pixels) update */
				#define UPDSM()									\
				if ((pd<<=1)&0x100) {							\
					status|=((*slp|(*slp+1))&0x20&o);			\
					if (!(*slp&0x8f)) *slp=cd;                  \
					slp++;			                            \
					if (!(*slp&0x8f)) *slp=cd;                  \
					slp++;			                            \
				} else slp+=2

				if (mag) { UPDSM(); UPDSM(); UPDSM(); UPDSM(); UPDSM(); UPDSM(); UPDSM(); UPDSM(); }
				else { UPDS(); UPDS(); UPDS(); UPDS(); UPDS(); UPDS(); UPDS(); UPDS(); }

				/* 16*16 */
				if (size&16) {
					pd=sg[g|16];

					if (mag) { UPDSM(); UPDSM(); UPDSM(); UPDSM(); UPDSM(); UPDSM(); UPDSM(); UPDSM(); }
					else { UPDS(); UPDS(); UPDS(); UPDS(); UPDS(); UPDS(); UPDS(); UPDS(); }
				}

				#undef UPDS
				#undef UPDSM
			}
			else sa+=3;

			/* all sprites checked */
			if (statuslow==31) break;
			else statuslow++;
		}

		statuslow&=slmask;
		if (~status&0x40) status=(status&0xe0)|statuslow|(o&0x40);

		/* update screen */
		if (f&viewspr) {
			int x=256; u8* slp=sl+32;
			screen-=256;

			while (x--) {
				if (*slp&0xf) *screen=*slp&0xf;
				screen++; slp++;
			}
		}
	}
}


/* lumi noise */
/* simulation of VDP brightness changes interference on the MSX audio line, basically
PCM with a range of 5.4MHz, it's pretty accurate, except that (most?) tvs seem to
have a highpass filter, diminishing the 50/60Hz hum */
#define LUMI_VOLUME		600.0

static int* luminoise_dac=NULL;

#define O_LUMI_BLANK	16
#define O_LUMI_SYNC		17
#define O_LUMI_MAX		18

static int lut_lumi[O_LUMI_MAX];

/* known types luminance */
static const float lut_lumi_t[][O_LUMI_MAX]={
/*                0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F      blank  sync */
/* 9x18      */ { 0.00, 0.00, 0.53, 0.67, 0.40, 0.53, 0.47, 0.67, 0.53, 0.67, 0.73, 0.80, 0.46, 0.53, 0.80, 1.00,  0.00,  -0.40 },
/* 9x28/9929 */ { 0.00, 0.00, 0.53, 0.67, 0.40, 0.53, 0.47, 0.73, 0.53, 0.67, 0.73, 0.80, 0.47, 0.53, 0.80, 1.00,  0.00,  -0.46 },
/* 9129      */ { 0.00, 0.00, 0.60, 0.73, 0.47, 0.60, 0.53, 0.73, 0.60, 0.73, 0.77, 0.83, 0.53, 0.60, 0.80, 1.00,  0.00,  -0.43 }
};

void vdp_luminoise_set(int l)
{
	if (!msx_is_running()||l==luminoise) return;

	luminoise=l;

	main_menu_check(IDM_LUMINOISE,l);
	if (l) luminoise_dac=sound_create_dac();
	else sound_clean_dac(luminoise_dac);
}

static void vdp_luminoise_recalc(void)
{
	float f,min=0,max=0;
	int i,t=2;

	switch (vdp_chiptype) {
		case VDP_CHIPTYPE_TMS9118: t=0; break;
		case VDP_CHIPTYPE_TMS9129: t=2; break;
		case VDP_CHIPTYPE_TMS9918: t=0; break;
		case VDP_CHIPTYPE_TMS9929: t=1; break;
		case VDP_CHIPTYPE_T7937A:  t=1; break;
		case VDP_CHIPTYPE_T6950:   t=1; break;
		default: break;
	}

	/* init normalisation (knowing that min<0) */
	for (i=0;i<O_LUMI_MAX;i++) {
		f=lut_lumi_t[t][i];
		if (f<min) min=f;
		if (f>max) max=f;
	}
	max-=min;

	/* fill lut */
	for (i=0;i<O_LUMI_MAX;i++) {
		f=lut_lumi_t[t][i]+min;
		if (f<0) f=0;
		lut_lumi[i]=(f/max)*LUMI_VOLUME*(lumivol/100.0);
	}
}

static void vdp_luminoise_end_frame(void)
{
	int b,c,d,i,j;
	u8* screen;
	int* dac=luminoise_dac;

	if (!luminoise||!sound_get_enabled()) return;

	screen=draw_get_screen_ptr();

	/* hblank shortcut (56 z80 clocks) */
	#define L_HBLANK()																			\
		c=lut_lumi[b];            j=8;  while (j--) *dac++=c; /* right border */				\
		c=lut_lumi[O_LUMI_BLANK]; j=6;  while (j--) *dac++=c; /* right blanking */				\
		c=lut_lumi[O_LUMI_SYNC];  j=18; while (j--) *dac++=c; /* horizontal sync */				\
		c=lut_lumi[O_LUMI_BLANK]; j=16; while (j--) *dac++=c; /* left blanking + color burst */	\
		c=lut_lumi[b];            j=8;  while (j--) *dac++=c  /* left border */

	/* vblank shortcut (172 z80 clocks) */
	#define L_VBLANK(x,y)																		\
		b=x; d=lut_lumi[b]; i=y;																\
		while (i--) {																			\
			j=172; while (j--) *dac++=d;														\
			L_HBLANK();																			\
		}

	/* active display */
	b=bgc; i=192;
	while (i--) {
		/* 255 pixels (170 z80 clocks) */
		j=85; while (j--) {
			*dac++=(lut_lumi[*screen]+lut_lumi[*screen]+lut_lumi[*(screen+1)])/3; screen++;
			*dac++=(lut_lumi[*screen]+lut_lumi[*(screen+1)]+lut_lumi[*(screen+1)])/3; screen++;
			screen++;
		}

		/* pixel 256 + 2 border pixels (2 z80 clocks) */
		*dac++=(lut_lumi[*screen]+lut_lumi[*screen]+lut_lumi[b])/3; screen++;
		*dac++=lut_lumi[b];

		L_HBLANK();
	}

	L_VBLANK(bgc,24)                          /* bottom border */
	L_VBLANK(O_LUMI_BLANK,crystal->mode?54:3) /* bottom blanking */

	/* vertical sync (should contain a pulse too) */
	c=lut_lumi[O_LUMI_SYNC]; i=3*228;
	while (i--) *dac++=c;

	L_VBLANK(O_LUMI_BLANK,13)                 /* top blanking */
	L_VBLANK(bgc,28)                          /* top border (+1 padding) */

	#undef L_HBLANK
	#undef L_VBLANK
}


/* state				size
ram						0x4000
regs					8
vblank					1
bgc						1
status					1
statuslow				1
latch					1
address					2
read					1

==						0x4000+16
*/
#define STATE_VERSION	1
#define STATE_SIZE		(0x4000+16)

int __fastcall vdp_state_get_version(void)
{
	return STATE_VERSION;
}

int __fastcall vdp_state_get_size(void)
{
	return STATE_SIZE;
}

/* save */
void __fastcall vdp_state_save(u8** s)
{
	STATE_SAVE_C(ram,0x4000);

	STATE_SAVE_1(regs[0]);	STATE_SAVE_1(regs[1]);	STATE_SAVE_1(regs[2]);	STATE_SAVE_1(regs[3]);
	STATE_SAVE_1(regs[4]);	STATE_SAVE_1(regs[5]);	STATE_SAVE_1(regs[6]);	STATE_SAVE_1(regs[7]);

	STATE_SAVE_1(vblank);
	STATE_SAVE_1(bgc);
	STATE_SAVE_1(status);	STATE_SAVE_1(statuslow);
	STATE_SAVE_1(latch);
	STATE_SAVE_2(address);
	STATE_SAVE_1(read);
}

/* load */
void __fastcall vdp_state_load_cur(u8** s)
{
	STATE_LOAD_C(ram,0x4000);

	STATE_LOAD_1(regs[0]);	STATE_LOAD_1(regs[1]);	STATE_LOAD_1(regs[2]);	STATE_LOAD_1(regs[3]);
	STATE_LOAD_1(regs[4]);	STATE_LOAD_1(regs[5]);	STATE_LOAD_1(regs[6]);	STATE_LOAD_1(regs[7]);

	STATE_LOAD_1(vblank);
	STATE_LOAD_1(bgc);
	STATE_LOAD_1(status);	STATE_LOAD_1(statuslow);
	STATE_LOAD_1(latch);
	STATE_LOAD_2(address);
	STATE_LOAD_1(read);
}

int __fastcall vdp_state_load(int v,u8** s)
{
	switch (v) {
		case STATE_VERSION:
			vdp_state_load_cur(s);
			break;

		default: return FALSE;
	}

	return TRUE;
}
