/******************************************************************************
 *                                                                            *
 *                                  "io.c"                                    *
 *                                                                            *
 ******************************************************************************/

#include "global.h"
#include "io.h"
#include "mapper.h"
#include "vdp.h"
#include "cont.h"
#include "psg.h"
#include "sound.h"
#include "z80.h"
#include "crystal.h"
#include "state.h"

/* EXTERN */
fp_ioread ioread[0x100]; /* 256 z80 io ports */
fp_iowrite iowrite[0x100];

/* 8255 PPI are made by different brands: Intel (original), NEC, Mitsubishi, Toshiba, inside MSX engine, ...? */
/* no known differences from the programmer's POV. Emulation of it is spread over to io.c, cont.c, mapper.c */

static u8 ppic=0;		/* ppi port c */

#define UNMAPPED		0xff /* unmapped reads (see mapper.c) */

/* keyclick sound, also used by some games (Beepertron, .. some Speccy ports, eg. Auf Wiedersehen Monty, Avenger, Jack the Nipper 2) */
#define CLICK_VOLUME	5567 /* max psg volume*~0.9278, measured on my Canon V-20 (i/o chip: Mitsubishi M5L8255AP-5) */

static int click_bc=0;
static int click_cc=0;
static int* click_dac;

static __inline__ void click_update(void)
{
	int i=(click_cc-z80_get_rel_cycles())/crystal->rel_cycle;
	const int sample=(ppic&0x80)?CLICK_VOLUME:0;

	if (i<=0) return;
	click_cc=z80_get_rel_cycles();

	while (i--) click_dac[click_bc++]=sample;
}

void io_set_click_buffer_counter(int bc)
{
	if (bc!=0&&bc<click_bc) {
		/* copy leftover to start */
		int i=bc,j=click_bc;
		while (i--) click_dac[i]=click_dac[--j];
	}

	click_bc=bc;
}

void io_init_click_sound(void) { click_dac=sound_create_dac(); }
int io_get_click_buffer_counter(void) { return click_bc; }
void io_new_frame(void) { click_cc+=crystal->frame; }
void io_end_frame(void) { click_update(); }


/* misc i/o port functions */
u8 __fastcall ioreadnull(void) { return UNMAPPED; }
void __fastcall iowritenull(u8 v) { v = v; }

u8 __fastcall io_read_key(void) { return cont_get_keyrow(ppic&0xf); }
u8 __fastcall io_read_ppic(void) { return ppic; }

void __fastcall io_write_ppic(u8 v)
{
	if ((v^ppic)&0x80) click_update(); /* click sound */

	ppic=v;
}

void __fastcall io_write_ppicontrol(u8 v)
{
	if (!(v&0x80)) {
		if (v&1) io_write_ppic(ppic|(1<<(v>>1&7)));
		else io_write_ppic(ppic&~(1<<(v>>1&7)));
	}
}

/* debug comply standards */
#if COMPLY_STANDARDS_TEST

static void __fastcall cs_check(int write,int port)
{
	int pc=z80_get_pc();
	int slot=mapper_read_pslot();

	/* exceptions */
	if (write&&port==0xa8&&pc==0xf381) return;
	if (write&&port==0xa8&&pc==0xf38d) return;
	if (write&&port==0xa8&&pc==0xf38a) return;
	if (write&&port==0xa8&&pc==0xf395) return;

	if (pc>0x7fff||(pc<0x4000&&slot&3)||(pc>0x3fff&&slot&0xc)) printf("csio %c $%02X - PC $%04X\n",write?'W':'R',port,pc);
}

u8 __fastcall vdp_read_data_c(void) {			cs_check(0,0x98); return vdp_read_data();		}
u8 __fastcall vdp_read_status_c(void) {			cs_check(0,0x99); return vdp_read_status();		}
u8 __fastcall psg_read_data_c(void) {			cs_check(0,0xa2); return psg_read_data();		}
u8 __fastcall mapper_read_pslot_c(void) {		cs_check(0,0xa8); return mapper_read_pslot();	}
u8 __fastcall io_read_key_c(void) {				cs_check(0,0xa9); return io_read_key();			}
u8 __fastcall io_read_ppic_c(void) {			cs_check(0,0xaa); return io_read_ppic();		}

void __fastcall vdp_write_data_c(u8 v) {		cs_check(1,0x98); vdp_write_data(v);			}
void __fastcall vdp_write_address_c(u8 v) {		cs_check(1,0x99); vdp_write_address(v);			}
void __fastcall psg_write_address_c(u8 v) {		cs_check(1,0xa0); psg_write_address(v);			}
void __fastcall psg_write_data_c(u8 v) {		cs_check(1,0xa1); psg_write_data(v);			}
void __fastcall mapper_write_pslot_c(u8 v) {	cs_check(1,0xa8); mapper_write_pslot(v);		}
void __fastcall io_write_ppic_c(u8 v) {			cs_check(1,0xaa); io_write_ppic(v);				}
void __fastcall io_write_ppicontrol_c(u8 v) {	cs_check(1,0xab); io_write_ppicontrol(v);		}

u8 __fastcall vdp_read_data_cw(void) {			printf("csio R $%02X - PC $%04X ($98 07 W)\n",COMPLY_STANDARDS_VDPW  ,z80_get_pc()); return vdp_read_data();	}
void __fastcall vdp_write_data_cr(u8 v) {		printf("csio W $%02X - PC $%04X ($99 06 R)\n",COMPLY_STANDARDS_VDPR  ,z80_get_pc()); vdp_write_data(v);			}
u8 __fastcall vdp_read_status_cr(void) {		printf("csio R $%02X - PC $%04X ($99 06+1)\n",COMPLY_STANDARDS_VDPR+1,z80_get_pc()); return vdp_read_status();	}
u8 __fastcall vdp_read_status_cw(void) {		printf("csio R $%02X - PC $%04X ($99 07+1)\n",COMPLY_STANDARDS_VDPW+1,z80_get_pc()); return vdp_read_status();	}
void __fastcall vdp_write_address_cr(u8 v) {	printf("csio W $%02X - PC $%04X ($99 06+1)\n",COMPLY_STANDARDS_VDPR+1,z80_get_pc()); vdp_write_address(v);		}
void __fastcall vdp_write_address_cw(u8 v) {	printf("csio W $%02X - PC $%04X ($99 07+1)\n",COMPLY_STANDARDS_VDPW+1,z80_get_pc()); vdp_write_address(v);		}

static void io_init_comply_standards(void)
{
	/* set standard read ports */
	io_setreadport(0x98,vdp_read_data_c);
	io_setreadport(0x99,vdp_read_status_c);
	io_setreadport(0xa2,psg_read_data_c);
	io_setreadport(0xa8,mapper_read_pslot_c);
	io_setreadport(0xa9,io_read_key_c);
	io_setreadport(0xaa,io_read_ppic_c);

	/* set standard write ports */
	io_setwriteport(0x98,vdp_write_data_c);
	io_setwriteport(0x99,vdp_write_address_c);
	io_setwriteport(0xa0,psg_write_address_c);
	io_setwriteport(0xa1,psg_write_data_c);
	io_setwriteport(0xa8,mapper_write_pslot_c);
	io_setwriteport(0xaa,io_write_ppic_c);
	io_setwriteport(0xab,io_write_ppicontrol_c);

	/* legal VRAM access */
	io_setreadport(COMPLY_STANDARDS_VDPR,vdp_read_data);
	io_setwriteport(COMPLY_STANDARDS_VDPW,vdp_write_data);

	/* illegal VRAM access */
	io_setreadport (COMPLY_STANDARDS_VDPW,  vdp_read_data_cw);		/* read from writeport		*/
	io_setwriteport(COMPLY_STANDARDS_VDPR,  vdp_write_data_cr);		/* write to readport		*/
	io_setreadport (COMPLY_STANDARDS_VDPR+1,vdp_read_status_cr);	/* read from readport +1	*/
	io_setreadport (COMPLY_STANDARDS_VDPW+1,vdp_read_status_cw);	/* read from writeport +1	*/
	io_setwriteport(COMPLY_STANDARDS_VDPR+1,vdp_write_address_cr);	/* write to readport +1		*/
	io_setwriteport(COMPLY_STANDARDS_VDPW+1,vdp_write_address_cw);	/* write to writeport +1	*/
}
#endif


/* init/clean */
void io_poweron(void)
{
	click_cc=ppic=0;
}

void __fastcall io_setreadport(u8 p,fp_ioread f) { ioread[p]=f?f:ioreadnull; }
void __fastcall io_setwriteport(u8 p,fp_iowrite f) { iowrite[p]=f?f:iowritenull; }

void io_init(void)
{
	int i;
	for (i=0;i<0x100;i++) {
		io_setreadport(i,NULL);
		io_setwriteport(i,NULL);
	}

	/* set standard read ports */
	io_setreadport(0x98,vdp_read_data);
	io_setreadport(0x99,vdp_read_status);
	io_setreadport(0xa2,psg_read_data);
	io_setreadport(0xa8,mapper_read_pslot);
	io_setreadport(0xa9,io_read_key);
	io_setreadport(0xaa,io_read_ppic);

	/* set standard write ports */
	io_setwriteport(0x98,vdp_write_data);
	io_setwriteport(0x99,vdp_write_address);
	io_setwriteport(0xa0,psg_write_address);
	io_setwriteport(0xa1,psg_write_data);
	io_setwriteport(0xa8,mapper_write_pslot);
	io_setwriteport(0xaa,io_write_ppic);
	io_setwriteport(0xab,io_write_ppicontrol);

#if COMPLY_STANDARDS_TEST
	io_init_comply_standards();
#endif
}

void io_clean(void)
{
	;
}


/* state				size
click_cc				4
ppic					1

==						5
*/
#define STATE_VERSION	2
/* version history:
1: initial
2: added tape (nothing changed here)
*/
#define STATE_SIZE		5

int __fastcall io_state_get_version(void)
{
	return STATE_VERSION;
}

int __fastcall io_state_get_size(void)
{
	return STATE_SIZE;
}

/* save */
void __fastcall io_state_save(u8** s)
{
	STATE_SAVE_4(click_cc);
	STATE_SAVE_1(ppic);
}

/* load */
void __fastcall io_state_load_cur(u8** s)
{
	STATE_LOAD_4(click_cc);
	STATE_LOAD_1(ppic);
}

int __fastcall io_state_load(int v,u8** s)
{
	switch (v) {
		case 1: case STATE_VERSION:
			io_state_load_cur(s);
			break;

		default: return FALSE;
	}

	return TRUE;
}
