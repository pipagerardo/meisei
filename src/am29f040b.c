/******************************************************************************
 *                                                                            *
 *                               "am29f040b.c"                                *
 *                                                                            *
 ******************************************************************************/

#include "global.h"
#include "state.h"
#include "am29f040b.h"

/* AMD (Spansion) Am29F040B Flash Memory emulation */
/* Write/Erase suspend, and status bits while busy aren't emulated */

static const int unlock_info[3]={0x55555,0x2aaaa,0};


void __fastcall am29f_write(_am29f* chip,u16 a,u8 v)
{
	switch (chip->mode) {
		case AM29F_UNLOCK1:
			if ((v<<11|(a&0x7ff))==unlock_info[chip->inc]) chip->inc++;
			else am29f_reset(chip);
			if (chip->inc&2) { chip->inc=0; chip->mode=AM29F_WAIT1; }
        break;

		case AM29F_WAIT1:
			if ((a&0x7ff)==(unlock_info[0]&0x7ff))
			switch (v) {
				case 0x90: chip->mode=AM29F_AUTOSELECT; break;	/* autoselect command */
				case 0xa0: chip->mode=AM29F_PROGRAM; break;		/* program command */
				case 0x80: chip->mode=AM29F_UNLOCK2; break;
				default: am29f_reset(chip); break;
			}
			else am29f_reset(chip);
        break;

		case AM29F_PROGRAM:
			if (chip->unprotect[chip->writesector>>16]) chip->data[a|chip->writesector]&=v;
			am29f_reset(chip);
        break;

		case AM29F_UNLOCK2:
			if ((v<<11|(a&0x7ff))==unlock_info[chip->inc]) chip->inc++;
			else am29f_reset(chip);
			if (chip->inc&2) { chip->inc=0; chip->mode=AM29F_WAIT2; }
        break;

		case AM29F_WAIT2:
			if (v==0x10&&(a&0x7ff)==(unlock_info[0]&0x7ff)) {
				/* erase chip command */
				int i=8;
				while (i--) if (chip->unprotect[i]) memset(chip->data+0x10000*i,0xff,0x10000);
			}
			else if (v==0x30) {
				/* erase sector command */
				if (chip->unprotect[chip->writesector>>16]) memset(chip->data+chip->writesector,0xff,0x10000);
			}

			am29f_reset(chip);
        break;

		default:
		    am29f_reset(chip);
        break;
	}
}

u8 __fastcall am29f_read(_am29f* chip,u16 a)
{
	if (chip->mode&AM29F_AUTOSELECT)
	switch (a&3) {
		case 0: return 0x01; /* manufacturer ID */
		case 1: return 0xa4; /* device ID */
		case 2: return chip->unprotect[chip->writesector>>16]^1;
		case 3: return 0x01; /* mirror of 0? */
	}

	return chip->data[a|chip->readsector];
}


void __fastcall am29f_reset(_am29f* chip)
{
	chip->inc=0;
	chip->mode=AM29F_UNLOCK1;
}

_am29f* am29f_init(int initmem)
{
	_am29f* chip;

	MEM_CREATE(chip,sizeof(_am29f));

	if (initmem) {
		MEM_CREATE_N(chip->data,0x80000); /* 0.5MB */
		memset(chip->data,0xff,0x80000);
	}

	return chip;
}

void am29f_clean(_am29f* chip)
{
	if (!chip) return;

	MEM_CLEAN(chip->data);
	MEM_CLEAN(chip);
}


/* state				size
mode					2
inc						1
readsector				3
writesector				3

data					0x80000 (but handled by SRAM)

==						9
*/
#define STATE_VERSION	3 /* mapper.c */
/* version history:
1: initial
2: SRAM crash bugfix
3: no changes (mapper.c)
*/
#define STATE_SIZE		9

int __fastcall am29f_state_get_size(void)
{
	return STATE_SIZE;
}

/* save */
void __fastcall am29f_state_save(_am29f* chip,u8** s)
{
	STATE_SAVE_2(chip->mode);
	STATE_SAVE_1(chip->inc);
	STATE_SAVE_3(chip->readsector);
	STATE_SAVE_3(chip->writesector);
}

/* load */
void __fastcall am29f_state_load_cur(_am29f* chip,u8** s)
{
	STATE_LOAD_2(chip->mode);
	STATE_LOAD_1(chip->inc);
	STATE_LOAD_3(chip->readsector);
	STATE_LOAD_3(chip->writesector);
}

int __fastcall am29f_state_load(_am29f* chip,int v,u8** s)
{
	switch (v) {
		case 1: case 2: case STATE_VERSION:
			am29f_state_load_cur(chip,s);
			break;

		default: return FALSE;
	}

	return TRUE;
}
