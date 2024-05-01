/******************************************************************************
 *                                                                            *
 *                                 "scc.h"                                    *
 *                                                                            *
 ******************************************************************************/

#ifndef SCC_H
#define SCC_H

/* volume factors, measured on my MSX1 (it's the same on my MSX2) */
#define SCC_VF_DEFAULT	15.32	/* official SCC/SCC-I (Nemesis 2, King's Valley 2, SD Snatcher): ~61.23% of max psg */
#define SCC_VF_MFR		10.31	/* Mega Flash ROM SCC: ~41.2% */

#define SCC_MODEL		0xc0
#define SCC_MASK		0x80
#define SCCI_MODEL		0x140
#define SCCI_MASK		0x100

typedef struct {
	u8 ram[0x20];
	int enable;
	int amp;
	int output;
	int period_raw;
	int period_upd;
	int period;
	int pos;
} _scc_chan;

typedef struct {
	int lut_volume[0x1000];
	int* dac;
	int bc;
	int cc;
	int output;

	int model;
	int test;

	_scc_chan c[5];
} _scc;

void __fastcall scc_write(_scc*,u16,u8);
u8 __fastcall scc_read(_scc*,u16);
void __fastcall scci_write(_scc*,u16,u8);
u8 __fastcall scci_read(_scc*,u16);

void scc_set_buffer_counter(_scc*,int);
void scc_new_frame(_scc*);
void scc_end_frame(_scc*);

void scc_poweron(_scc*);
_scc* scc_init(float,int);
void scc_clean(_scc*);

int scc_get_volume(void);
void scc_init_volume(void);

int __fastcall scc_state_get_size(void);
void __fastcall scc_state_save(_scc*,u8**);
void __fastcall scc_state_load_cur(_scc*,u8**);
int __fastcall scc_state_load(_scc*,int,u8**);

#endif /* SCC_H */
