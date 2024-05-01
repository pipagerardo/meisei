/******************************************************************************
 *                                                                            *
 *                                 "scc.c"                                    *
 *                                                                            *
 ******************************************************************************/

#include "global.h"
#include "sound.h"
#include "state.h"
#include "scc.h"
#include "crystal.h"
#include "z80.h"
#include "psg.h"
#include "settings.h"

/*

Konami SCC soundchip

Unclear:
- test register emulation is correct? (no test programs for bits 0,1 behaviour, not tested well for SCC-I)
- in ch4&5, SCC sometimes sets wrong values in a wave shape, it changes when rewriting the frequency.
  Kings Valley 2 selection/intro screen music bells sound too high pitched when compared to the original,
  most likely related to this. problem was fixed in SCC-I hardware.
- writes to SCC-I RAM inside register area, currently ignored/write to RAM...
  Sean Young: "If the SCC control areas (these things above) are active, and you put the SCC+ into RAM mode
  and then start writing to the control areas, some really weird things happen in the RAM. The SCC is unaffected
  but whole areas of the RAM are messed up."
*/

static int sccvolume=100;

#define SCC_HALT	0xffffff

/* clock channel */
#define CLOCK(n)									\
	if (!--scc->c[n].period) {						\
		scc->output-=scc->c[n].output;				\
		if (scc->c[n].period_raw>8) {				\
			scc->c[n].pos=(scc->c[n].pos+1)&0x1f;	\
			scc->c[n].output=scc->lut_volume[(scc->c[n].enable&scc->c[n].amp)|scc->c[n].ram[scc->c[n].pos]]; \
		}											\
		else {										\
			/* not running */						\
			scc->c[n].output=0;						\
			scc->c[n].period_upd=SCC_HALT; /* optimisation hack, doesn't affect accuracy */ \
		}											\
		scc->output+=scc->c[n].output;				\
		scc->c[n].period=scc->c[n].period_upd+1;	\
	}

static void __fastcall scc_update(_scc* scc)
{
	int i=(scc->cc-z80_get_rel_cycles())/crystal->rel_cycle;

	if (i<=0) return;
	scc->cc=z80_get_rel_cycles();

	while (i--) {
		/* clock channels */
		CLOCK(0) CLOCK(1) CLOCK(2) CLOCK(3) CLOCK(4)

		/* output */
		scc->dac[scc->bc++]=scc->output;
	}
}


/* write/read */
void __fastcall scc_write(_scc* scc,u16 a,u8 v)
{
	switch (a&0xe0) {
		/* waveform */
		/* channels 1,2,3 */
		case 0x00: case 0x20: case 0x40:
			/* all channels read-only? */
			if (scc->test&0x40) break;

			scc_update(scc);

			scc->c[a>>5&3].ram[a&0x1f]=v;
			break;
		/* channels 4,5 */
		case 0x60:
			/* channels 4,5 read-only? */
			if (scc->test&scc->model) break;

			scc_update(scc);

			scc->c[3].ram[a&0x1f]=scc->c[4].ram[a&0x1f]=v;
			break;

		case 0x80:
			switch (a&0xf) {

				/* fine period */
				case 0: case 2: case 4: case 6: case 8: {
					_scc_chan* c=&scc->c[a>>1&7];
					scc_update(scc);

					c->period_raw=(c->period_raw&0xf00)|v;
					if (scc->test&0x20) { c->pos=0x1f; c->period=1; }
					else if (c->period_upd==SCC_HALT) c->period=1;
					c->period_upd=c->period_raw;

					break;
				}

				/* coarse period (4 bit) */
				case 1: case 3: case 5: case 7: case 9: {
					_scc_chan* c=&scc->c[a>>1&7];
					scc_update(scc);

					c->period_raw=(c->period_raw&0xff)|(v<<8&0xf00);
					if (scc->test&0x20) { c->pos=0x1f; c->period=1; }
					else if (c->period_upd==SCC_HALT) c->period=1;
					c->period_upd=c->period_raw;

					break;
				}

				/* volume (4 bit) */
				case 0xa: case 0xb: case 0xc: case 0xd: case 0xe:
					scc_update(scc);
					scc->c[(a-2)&7].amp=v<<8&0xf00;
					break;

				/* enable */
				case 0xf:
					scc_update(scc);

					scc->c[0].enable=(v&1)?0xf00:0;
					scc->c[1].enable=(v&2)?0xf00:0;
					scc->c[2].enable=(v&4)?0xf00:0;
					scc->c[3].enable=(v&8)?0xf00:0;
					scc->c[4].enable=(v&16)?0xf00:0;
					break;
				}
			break;

		/* test register */
		case 0xc0:
			/* SCC-I */
			if (scc->model&SCCI_MASK) scc->test=v;
			break;
		case 0xe0:
			/* SCC */
			if (scc->model&SCC_MASK) scc->test=v;
			break;

		default: break;
	}
}

u8 __fastcall scc_read(_scc* scc,u16 a)
{
	switch (a&0xe0) {
		/* waveform */
		/* channels 1,2,3 */
		case 0x00: case 0x20: case 0x40:
			/* all channels rotating? */
			if (scc->test&0x40) {
				int c=a>>5&3;
				scc_update(scc);
				return scc->c[c].ram[(a+scc->c[c].pos)&0x1f];
			}
			else return scc->c[a>>5&3].ram[a&0x1f];
		/* channels 4,5 */
		case 0x60:
			/* rotating? */
			if (scc->test&scc->model) {
				scc_update(scc);
				return scc->c[3].ram[(a+scc->c[3+((scc->test&0x40)==0x40)].pos)&0x1f];
			}
			else return scc->c[3].ram[a&0x1f]; /* channel 4 */
		/* channel 5 on SCC-I */
		case 0xa0:
			if (scc->model&SCC_MASK) break;
			/* rotating? */
			if (scc->test&scc->model) {
				scc_update(scc);
				return scc->c[4].ram[(a+scc->c[4].pos)&0x1f];
			}
			else return scc->c[4].ram[a&0x1f];

		/* test register */
		case 0xc0:
			/* SCC-I */
			if (scc->model&SCCI_MASK) scc_write(scc,a,0xff);
			break;
		case 0xe0:
			/* SCC */
			if (scc->model&SCC_MASK) scc_write(scc,a,0xff);
			break;

		default: break;
	}

	return 0xff;
}

/* write/read, SCC-I specific */
void __fastcall scci_write(_scc* scc,u16 a,u8 v)
{
	switch (a&0xe0) {
		/* waveform */
		case 0x00: case 0x20: case 0x40: case 0x60: case 0x80:
			/* all channels read-only? */
			if (scc->test&0x40) break;

			scc_update(scc);

			scc->c[(a&0x80)?4:a>>5&3].ram[a&0x1f]=v;
			break;

		/* like SCC */
		case 0xa0:
			a-=0x20;
			/* continue */
		default:
			scc_write(scc,a,v);
			break;
	}
}

u8 __fastcall scci_read(_scc* scc,u16 a)
{
	switch (a&0xe0) {
		/* waveform */
		case 0x00: case 0x20: case 0x40: case 0x60: case 0x80: {
			int c=(a&0x80)?4:a>>5&3;
			/* all channels rotating? */
			if (scc->test&0x40) {
				scc_update(scc);
				return scc->c[c].ram[(a+scc->c[c].pos)&0x1f];
			}
			else return scc->c[c].ram[a&0x1f];
			break;
		}

		/* test register */
		case 0xc0:
			scc_write(scc,a,0xff);
			break;

		default: break;
	}

	return 0xff;
}


/* --- */
void scc_set_buffer_counter(_scc* scc,int bc)
{
	if (bc!=0&&bc<scc->bc) {
		/* copy leftover to start */
		int i=bc,j=scc->bc;
		while (i--) scc->dac[i]=scc->dac[--j];
	}

	scc->bc=bc;
}

void scc_new_frame(_scc* scc) { scc->cc+=crystal->frame; }
void scc_end_frame(_scc* scc) { scc_update(scc); }

void scc_poweron(_scc* scc)
{
	int i;

	/* sync */
	scc->cc=psg_get_cc();
	scc->bc=psg_get_buffer_counter();

	scc->output=0;
	scc->test=0;

	i=5;
	while (i--) {
		/* some ram bits are cleared on MSX, seems random (different on the 2 games I tested) */
		memset(scc->c[i].ram,0xff,0x20);

		scc->c[i].enable=0;
		scc->c[i].amp=0xf00;
		scc->c[i].output=0;
		scc->c[i].period=1;
		scc->c[i].period_raw=1;
		scc->c[i].period_upd=SCC_HALT;
		scc->c[i].pos=0x1f;
	}
}

_scc* scc_init(float vol_factor,int model)
{
	_scc* scc;
	int i,v;

	MEM_CREATE(scc,sizeof(_scc));
	scc->dac=sound_create_dac();
	scc->model=model;

	for (i=0;i<0x100;i++) for (v=0;v<0x10;v++) scc->lut_volume[v<<8|i]=(((((s8)i)*v)>>4)*vol_factor+0.5)*(sccvolume/100.0);

	return scc;
}

void scc_clean(_scc* scc)
{
	if (!scc) return;

	sound_clean_dac(scc->dac);
	MEM_CLEAN(scc);
}


int scc_get_volume(void) { return sccvolume; }

void scc_init_volume(void)
{
	int i;

	/* get volume percentage */
	if (SETTINGS_GET_INT(settings_info(SETTINGS_SCC_VOLUME),&i)) { CLAMP(i,0,250); sccvolume=i; }
}


/* state				size
cc						4
output					4
test					1

enable					2
amp						2
output					4
period_upd				3
period_raw				2
period					3
pos						1
						*5

ram						32*5

==						254
*/
#define STATE_VERSION	3 /* mapper.c */
/* version history:
1: initial
2: SCC-I support (1 channel more)
3: no changes (mapper.c)
*/
#define STATE_SIZE		254

int __fastcall scc_state_get_size(void)
{
	return STATE_SIZE;
}

/* save */
void __fastcall scc_state_save(_scc* scc,u8** s)
{
	int i;

	STATE_SAVE_4(scc->cc);
	STATE_SAVE_4(scc->output);
	STATE_SAVE_1(scc->test);

	i=5;
	while (i--) {
		STATE_SAVE_2(scc->c[i].enable);
		STATE_SAVE_2(scc->c[i].amp);
		STATE_SAVE_4(scc->c[i].output);
		STATE_SAVE_3(scc->c[i].period_upd);
		STATE_SAVE_2(scc->c[i].period_raw);
		STATE_SAVE_3(scc->c[i].period);
		STATE_SAVE_1(scc->c[i].pos);
		STATE_SAVE_C(scc->c[i].ram,0x20);
	}
}

/* load */
void __fastcall scc_state_load_cur(_scc* scc,u8** s)
{
	int i;

	STATE_LOAD_4(scc->cc);
	STATE_LOAD_4(scc->output);
	STATE_LOAD_1(scc->test);

	i=5;
	while (i--) {
		STATE_LOAD_2(scc->c[i].enable);
		STATE_LOAD_2(scc->c[i].amp);
		STATE_LOAD_4(scc->c[i].output);
		STATE_LOAD_3(scc->c[i].period_upd);
		STATE_LOAD_2(scc->c[i].period_raw);
		STATE_LOAD_3(scc->c[i].period);
		STATE_LOAD_1(scc->c[i].pos);
		STATE_LOAD_C(scc->c[i].ram,0x20);
	}
}

void __fastcall scc_state_load_1(_scc* scc,u8** s)
{
	/* version 1 */
	int i;

	STATE_LOAD_4(scc->cc);
	STATE_LOAD_4(scc->output);
	STATE_LOAD_1(scc->test);

	i=5;
	while (i--) {
		STATE_LOAD_2(scc->c[i].enable);
		STATE_LOAD_2(scc->c[i].amp);
		STATE_LOAD_4(scc->c[i].output);
		STATE_LOAD_3(scc->c[i].period_upd);
		STATE_LOAD_2(scc->c[i].period_raw);
		STATE_LOAD_3(scc->c[i].period);
		STATE_LOAD_1(scc->c[i].pos);
	}

	i=4; while (i--) { STATE_LOAD_C(scc->c[i].ram,0x20); }

	/* channel 5 */
	memcpy(scc->c[4].ram,scc->c[3].ram,0x20);
}

int __fastcall scc_state_load(_scc* scc,int v,u8** s)
{
	switch (v) {
		case 1:
			scc_state_load_1(scc,s);
			break;
		case 2: case STATE_VERSION:
			scc_state_load_cur(scc,s);
			break;

		default: return FALSE;
	}

	return TRUE;
}
