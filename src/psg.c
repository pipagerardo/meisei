/******************************************************************************
 *                                                                            *
 *                                 "psg.c"                                    *
 *                                                                            *
 ******************************************************************************/

/*

Yamaha YM2149 soundchip, runs as tested on my MSX1.
(also includes General Instruments AY-3-8910 emulation)

Known models on MSXes and their differences:
* General Instruments AY-3-8910 (the original, call it AY8910 in short):
 - mask on PSG regs when reading, this can be 1f instead of 0f on register 1,3,5
   (would need more testing, only seen on 1 MSX so far: Quibus's Philips VG8010)
* Yamaha YM2149: -- also included in Yamaha MSX engines S3527(MSX1/2) and S1985
 - all PSG regs read back as 8 bit
 - envelope slopes are twice smoother (faster) than on the AY8910
* Toshiba T9769B/C (T7937A, T9769A behave same?): -- MSX engine including PSG
 - read masks are like AY8910
 - envelope slopes are like YM2149

All chips contain a quirk where (joystick) R14 input pins 6/7 are ANDed with R15 output pins 6/7. (cont.c)
There's another quirk regarding bits 6(must be 0) and 7(must be 1) of the mixer being able to cause
a short circuit on some MSXes, but that's too risky to test.

*/

#include <math.h>

#include "global.h"
#include "cont.h"
#include "sound.h"
#include "z80.h"
#include "crystal.h"
#include "state.h"
#include "psg.h"
#include "psgtoy.h"
#include "settings.h"
#include "reverse.h"
#include "io.h"

#define PSG_VOL_MAX	6000
#define PSG_VOLUME	(6000+28) /* 28=min vol for 6028 */

static const int read_mask[2][0x10]={
	{0xff,0x0f,0xff,0x0f,0xff,0x0f,0x1f,0xff,0x1f,0x1f,0x1f,0xff,0xff,0x0f,0xff,0xff},	/* AY8910 */
	{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}	/* YM2149 */
};

static int psg_chiptype=0;

static int lut_volume_std[0x80];			/* 00000vvvvvaa: v=volume, a=00:low, 01/11:high, 10:ultrasonic causing lower volume */
static int lut_volume_custom_a[0x8000];		/* sssssssdvvvvvaa: s=sample, d=disabled/noise, v=volume, a=as above */
static int lut_volume_custom_b[0x8000];
static int lut_volume_custom_c[0x8000];
static int lut_volume_custom_l[3][0x2000];	/* levels */
static int* psg_dac;
static int psg_bc=0;
static int psg_cc=0;
static int remain;

static int psg_address;
static int psg_regs[0x10];

/* tone channels */
static int a_period;		static int b_period;		static int c_period;
static int a_period_raw;	static int b_period_raw;	static int c_period_raw;
static int a_disable;		static int b_disable;		static int c_disable;
static int a_noise_disable;	static int b_noise_disable;	static int c_noise_disable;
static int a_envelope;		static int b_envelope;		static int c_envelope;
static int a_sample;		static int b_sample;		static int c_sample;
static int a_vol;			static int b_vol;			static int c_vol;
static int a_output;		static int b_output;		static int c_output;

/* noise */
static int noise_period;
static int noise_period_raw;
static int noise_random;
static int noise_sample;

/* envelope */
static int e_period;
static int e_period_raw;
static int e_count;
static int e_invert;
static int e_hold;
static int e_alt;
static int e_disabled;
static int e_vol;

/* custom */
static int custom_amplitude[3];
static u8* custom_wave[3];
static int custom_enabled=0;
static int custom_enabled_set=0;
static int custom_amp_minus=0;

static int a_cur_sample;	static int b_cur_sample;	static int c_cur_sample;
static int a_step_base;		static int b_step_base;		static int c_step_base;
static int a_step_base_raw;	static int b_step_base_raw;	static int c_step_base_raw;
static int a_step_sub;		static int b_step_sub;		static int c_step_sub;
static int a_step_sub_raw;	static int b_step_sub_raw;	static int c_step_sub_raw;
static int a_step_add;		static int b_step_add;		static int c_step_add;
static int a_step_add_raw;	static int b_step_add_raw;	static int c_step_add_raw;
static int a_step_rest;		static int b_step_rest;		static int c_step_rest;
static int a_fill_remain;	static int b_fill_remain;	static int c_fill_remain;
static int a_noise_xor;		static int b_noise_xor;		static int c_noise_xor;

static const char* psg_chiptype_name[PSG_CHIPTYPE_MAX]={
	"GI AY8910",	"Yamaha YM2149"
};
static const int psg_chiptype_uid[PSG_CHIPTYPE_MAX]={
	0,				1
};

const char* psg_get_chiptype_name(u32 i) { if (i>=PSG_CHIPTYPE_MAX) return NULL; else return psg_chiptype_name[i]; }
void psg_set_chiptype(u32 i) { if (i>=PSG_CHIPTYPE_MAX) i=0; psg_chiptype=i; }
int psg_get_chiptype(void) { return psg_chiptype; }
int __fastcall psg_get_reg(u32 r) { return psg_regs[r&0xf]; }
int __fastcall psg_get_address(void) { return psg_address; }
int __fastcall psg_get_e_vol(void) { return e_vol; }

int psg_get_chiptype_uid(u32 type)
{
	if (type>=PSG_CHIPTYPE_MAX) type=0;
	return psg_chiptype_uid[type];
}

int psg_get_uid_chiptype(u32 id)
{
	int type;

	for (type=0;type<PSG_CHIPTYPE_MAX;type++) {
		if ((u32)psg_chiptype_uid[type]==id) return type;
	}

	return 0;
}

/* psg standard update */
/* recalculate channel output volume */
#define RECALC_STD(x) x##_output=lut_volume_std[((x##_sample|x##_disable)&(noise_sample|x##_noise_disable))|x##_vol]

/* clock tone channel */
#define CLOCK_STD(x)				\
	if (!--x##_period) {			\
		if (x##_period_raw>7) { x##_period=x##_period_raw; x##_sample=(x##_sample^1)&1; } \
		else { x##_period=x##_period_raw?x##_period_raw:1; x##_sample=2; /* ultrasonic: disable possible artifacts, and half volume */ } \
		RECALC_STD(x);				\
	}

/* output sample into buffer */
#define FILL_STD() psg_dac[psg_bc++]=output

static void __fastcall psg_update_std(void)
{
	int i=(psg_cc-z80_get_rel_cycles())/crystal->rel_cycle;
	int ir,output=a_output+b_output+c_output;

	if (i<=0) return;
	psg_cc=z80_get_rel_cycles();

	/* finish previous remainder */
	if (remain)
	for (;;) {
		remain--; i--;
		FILL_STD();
		if (!i&&remain) return;
		if (!remain) break;
	}

	ir=i&0xf; i>>=4; /* minimum osc resolution of 16 Z80 cycles */

	for(;;) {

		if (!--e_period) {
			/* clock envelope */
			if (e_disabled) {
				e_vol=e_invert;
				e_period=0xffff00; /* optimisation hack, doesn't affect accuracy */
			}
			else {
				e_vol=e_count<<2^e_invert;

				if (!e_count--) {
					if (e_alt) e_invert^=0x7c;
					e_disabled=e_hold;
					e_count=0x1f;
				}

				e_period=e_period_raw?e_period_raw:1;
			}

			if (a_envelope) { a_vol=e_vol; RECALC_STD(a); }
			if (b_envelope) { b_vol=e_vol; RECALC_STD(b); }
			if (c_envelope) { c_vol=e_vol; RECALC_STD(c); }
		}

		if (!--noise_period) {
			/* clock noise */
			if ((noise_random+1)&2) {
				noise_sample^=3;
				RECALC_STD(a); RECALC_STD(b); RECALC_STD(c);
			}

			noise_random=noise_random>>1|((noise_random>>3^noise_random)<<16&0x10000);

			noise_period=noise_period_raw;
		}

		/* clock tone channels */
		CLOCK_STD(a) CLOCK_STD(b) CLOCK_STD(c)

		output=a_output+b_output+c_output;
		if (!i--) break;

		/* 16 samples into buffer */
		FILL_STD(); FILL_STD(); FILL_STD(); FILL_STD(); FILL_STD(); FILL_STD(); FILL_STD(); FILL_STD();
		FILL_STD(); FILL_STD(); FILL_STD(); FILL_STD(); FILL_STD(); FILL_STD(); FILL_STD(); FILL_STD();
	}

	/* remainder */
	remain=0x10-ir;
	while (ir--) FILL_STD();
}


/* psg custom waveform */
/* recalculate channel output volume */
#define RECALC_CUSTOM(x) x##_output=lut_volume_custom_##x[((x##_sample|x##_disable)&(noise_sample|x##_noise_disable))|x##_vol|x##_cur_sample]

/* recalculate noise enabled */
#define RECALC_NOISE_CUSTOM(x)														\
	if (x##_noise_disable) {														\
		x##_sample&=3;																\
		x##_noise_xor=0;															\
	}																				\
	else {																			\
		if (!x##_disable) {															\
			if (x##_sample) x##_sample|=0x80;										\
			x##_noise_xor=0x80;														\
		}																			\
	}

/* recalculate steps */
#define RECALC_STEP_CUSTOM(x,y)														\
	/* current period */															\
	if ((x##_step_base=(x##_period<<12)/(0x8000-x##_cur_sample))) {					\
		if (x##_fill_remain>x##_step_base) x##_fill_remain=x##_step_base;			\
		x##_step_sub=(x##_period<<4)%(0x80-(x##_cur_sample>>8));					\
		x##_step_add=x##_step_rest=0x80;											\
	}																				\
	else {																			\
		x##_step_add=x##_step_rest=x##_fill_remain=x##_step_base=1;					\
		x##_step_sub=0;																\
	}																				\
	/* raw period */																\
	if ((x##_step_base_raw=y>>3)) {													\
		x##_step_add_raw=8;															\
		x##_step_sub_raw=y&7;														\
	}																				\
	else {																			\
		x##_step_add_raw=x##_step_base_raw=1;										\
		x##_step_sub_raw=0;															\
	}

/* clock tone channel */
#define CLOCK_CUSTOM(x)																\
	if (!--x##_period) {															\
		if (x##_period_raw>7) { x##_period=x##_period_raw; x##_sample=(x##_sample^(1|x##_noise_xor))&0x81; } \
		else { x##_period=x##_period_raw?x##_period_raw:1; x##_sample=2; /* ultrasonic: disable possible artifacts, and half volume */ } \
		/* reset steps */															\
		x##_fill_remain=x##_step_base=x##_step_base_raw;							\
		x##_step_rest=x##_step_add=x##_step_add_raw;								\
		x##_step_sub=x##_step_sub_raw;												\
		x##_cur_sample=0;															\
		RECALC_CUSTOM(x);															\
	}

/* fill */
/* once */
#define FILL_CUSTOM_ONCE(x,y)														\
	y=x##_output;																	\
	if (!--x##_fill_remain) {														\
		x##_fill_remain=x##_step_base;												\
		x##_step_rest-=x##_step_sub;												\
		if (x##_step_rest<=0) { x##_step_rest+=x##_step_add; x##_fill_remain++; }	\
		x##_cur_sample+=0x100; if (x##_cur_sample==0x8000) x##_cur_sample=0x7f00;	\
		RECALC_CUSTOM(x);															\
	}
/* 16/once */
#define FILL_CUSTOM_C(x)															\
	if (x##_fill_remain>0x10) {														\
		/* write 16 samples */														\
		j=0; o=x##_output;															\
		b##x[j++]=o; b##x[j++]=o; b##x[j++]=o; b##x[j++]=o; b##x[j++]=o; b##x[j++]=o; b##x[j++]=o; b##x[j++]=o; \
		b##x[j++]=o; b##x[j++]=o; b##x[j++]=o; b##x[j++]=o; b##x[j++]=o; b##x[j++]=o; b##x[j++]=o; b##x[j]=o; \
		x##_fill_remain-=0x10;														\
	}																				\
	else {																			\
		j=0x10;																		\
		while (j--) { FILL_CUSTOM_ONCE(x,b##x[j]) }									\
	}
/* dac */
#define FILL_CUSTOM_B() psg_dac[psg_bc++]=ba[j]+bb[j]+bc[j]; j++

static void __fastcall psg_update_custom(void)
{
	int i=(psg_cc-z80_get_rel_cycles())/crystal->rel_cycle;
	int j,ir,o;
	int ba[0x10],bb[0x10],bc[0x10];

	if (i<=0) return;
	psg_cc=z80_get_rel_cycles();

	/* finish previous remainder */
	if (remain)
	for (;;) {
		remain--; i--;
		o=0;
		FILL_CUSTOM_ONCE(a,j) o+=j;
		FILL_CUSTOM_ONCE(b,j) o+=j;
		FILL_CUSTOM_ONCE(c,j) o+=j;
		psg_dac[psg_bc++]=o;
		if (!i&&remain) return;
		if (!remain) break;
	}

	ir=i&0xf; i>>=4; /* minimum osc resolution of 16 Z80 cycles */

	for(;;) {

		if (!--e_period) {
			/* clock envelope (same as standard update) */
			if (e_disabled) {
				e_vol=e_invert;
				e_period=0xffff00; /* optimisation hack, doesn't affect accuracy */
			}
			else {
				e_vol=e_count<<2^e_invert;

				if (!e_count--) {
					if (e_alt) e_invert^=0x7c;
					e_disabled=e_hold;
					e_count=0x1f;
				}

				e_period=e_period_raw?e_period_raw:1;
			}

			if (a_envelope) { a_vol=e_vol; RECALC_CUSTOM(a); }
			if (b_envelope) { b_vol=e_vol; RECALC_CUSTOM(b); }
			if (c_envelope) { c_vol=e_vol; RECALC_CUSTOM(c); }
		}

		if (!--noise_period) {
			/* clock noise (same as standard update) */
			if ((noise_random+1)&2) {
				noise_sample^=3;
				RECALC_CUSTOM(a); RECALC_CUSTOM(b); RECALC_CUSTOM(c);
			}

			noise_random=noise_random>>1|((noise_random>>3^noise_random)<<16&0x10000);

			noise_period=noise_period_raw;
		}

		/* clock tone channels */
		CLOCK_CUSTOM(a) CLOCK_CUSTOM(b) CLOCK_CUSTOM(c)

		if (!i--) break;

		/* 16 samples into buffer */
		FILL_CUSTOM_C(a) FILL_CUSTOM_C(b) FILL_CUSTOM_C(c) j=0;
		FILL_CUSTOM_B(); FILL_CUSTOM_B(); FILL_CUSTOM_B(); FILL_CUSTOM_B(); FILL_CUSTOM_B(); FILL_CUSTOM_B(); FILL_CUSTOM_B(); FILL_CUSTOM_B();
		FILL_CUSTOM_B(); FILL_CUSTOM_B(); FILL_CUSTOM_B(); FILL_CUSTOM_B(); FILL_CUSTOM_B(); FILL_CUSTOM_B(); FILL_CUSTOM_B(); FILL_CUSTOM_B();
	}

	/* remainder */
	remain=0x10-ir;
	while (ir--) {
		o=0;
		FILL_CUSTOM_ONCE(a,j) o+=j;
		FILL_CUSTOM_ONCE(b,j) o+=j;
		FILL_CUSTOM_ONCE(c,j) o+=j;
		psg_dac[psg_bc++]=o;
	}
}


/* shared */
void __fastcall (*psg_update)(void);
/* recalculate mixer */
static __fastcall void psg_recalc_mixer(void)
{
	if (custom_enabled) {
		if (a_disable) a_disable=0x83;
		if (b_disable) b_disable=0x83;
		if (c_disable) c_disable=0x83;

		if (a_noise_disable) a_noise_disable=0x83;
		if (b_noise_disable) b_noise_disable=0x83;
		if (c_noise_disable) c_noise_disable=0x83;

		a_cur_sample=b_cur_sample=c_cur_sample=0;
		noise_sample|=0x80;
		RECALC_NOISE_CUSTOM(a) RECALC_NOISE_CUSTOM(b) RECALC_NOISE_CUSTOM(c)

		RECALC_STEP_CUSTOM(a,a_period_raw) RECALC_STEP_CUSTOM(b,b_period_raw) RECALC_STEP_CUSTOM(c,c_period_raw)
		a_fill_remain=a_step_base;
		b_fill_remain=b_step_base;
		c_fill_remain=c_step_base;
	}
	else {
		a_disable&=1; b_disable&=1; c_disable&=1;
		a_noise_disable&=3; b_noise_disable&=3; c_noise_disable&=3;
		a_sample&=3; b_sample&=3; c_sample&=3;
		noise_sample&=3;
	}
}

/* recalculate channel output volume */
static __inline__ void psg_recalc_output(void)
{
	if (custom_enabled) { RECALC_CUSTOM(a); RECALC_CUSTOM(b); RECALC_CUSTOM(c); }
	else { RECALC_STD(a); RECALC_STD(b); RECALC_STD(c); }
}


/* custom init */
static void __fastcall psg_custom_fill_volume_channel_square_all(void)
{
	int i,j,k;

	for (i=0;i<0x100;i++) {
		for (j=0;j<0x20;j++) {
			for (k=0;k<4;k++) {
				lut_volume_custom_a[i<<7|j<<2|k]=lut_volume_custom_b[i<<7|j<<2|k]=lut_volume_custom_c[i<<7|j<<2|k]=lut_volume_std[j<<2|k]-custom_amp_minus;
			}
		}
	}
}

static void __fastcall psg_custom_fill_volume_levels(int c)
{
	double a,vol;
	int i,j;

	a=custom_amplitude[c]/100.0;

	for (i=0;i<0x100;i++) {
		vol=(i/255.0)*a;
		for (j=0;j<0x20;j++) {
			lut_volume_custom_l[c][i<<5|j]=vol*lut_volume_std[j<<2|1]+0.5;
			lut_volume_custom_l[c][i<<5|j]-=custom_amp_minus;
		}
	}
}

static void __fastcall psg_custom_fill_volume_channel(int c)
{
	int i,j;

	#define VFILL(x,y)				\
	for (i=0;i<0x80;i++) {			\
		for (j=0;j<0x20;j++) {		\
			lut_volume_custom_##y[i<<8|j<<2|1]=lut_volume_custom_##y[i<<8|j<<2|3]=lut_volume_custom_l[x][custom_wave[x][i]<<5|j]; \
			lut_volume_custom_##y[i<<8|j<<2]=lut_volume_custom_l[x][custom_wave[x][i|0x80]<<5|j]; \
		}							\
	}

	if (c==0) { VFILL(0,a) }
	else if (c==1) { VFILL(1,b) }
	else { VFILL(2,c) }

	#undef VFILL
}

static int __fastcall psg_custom_recalc_amp_minus(void)
{
	int prev=custom_amp_minus;
	int i,j,k=0,l=1;

	for (i=0;i<3;i++) {
		j=psgtoy_get_custom_amplitude(i);
		if (j<=100) j=0;
		else { j-=100; l=0; }
		k+=j;
	}

	if (l) custom_amp_minus=0;
	else custom_amp_minus=PSG_VOL_MAX*(float)k/300.0;

	if (prev!=custom_amp_minus) {
		psg_custom_fill_volume_channel_square_all();
		return TRUE;
	}

	return FALSE;
}

static void psg_custom_init(void)
{
	int i;

	for (i=0;i<3;i++) {
		custom_amplitude[i]=psgtoy_get_custom_amplitude(i);
		psg_custom_fill_volume_levels(i);
		psgtoy_reset_custom_wave_changed(i);
		psg_custom_fill_volume_channel(i);
	}

	psg_update=&psg_update_custom;
	psg_recalc_mixer();
}

void psg_custom_enable(int e)
{
	custom_enabled_set=e;
}


/* psg registers handling (octal numbers) */

/* R0-5: 12 bit tone period, clock divider of 16 (/2 for duty cycle)
 anything below 8 is inaudible, some games, eg. Eggerland, rely on that.
 Related to all:
  - initial clock divider of 2 (~3.5mhz->1.7mhz)
  - also immediately affects period
  - period actually counts up, not down
	reg:	bits:
	R0,2,4	all	tone period fine, channel a,b,c
	R1,3,5	0-3	tone period coarse, channel a,b,c */
#define _R_PERIOD(x)																\
	(*psg_update)(); x##_period+=period_raw-x##_period_raw;							\
	if (x##_period<=0) x##_period=1;												\
	if (custom_enabled) { RECALC_STEP_CUSTOM(x,period_raw) }						\
	x##_period_raw=period_raw
#define R_PERIOD_FINE(x) int period_raw=v|(x##_period_raw&0xf00); _R_PERIOD(x)
#define R_PERIOD_COARSE(x) int period_raw=(v<<8&0xf00)|(x##_period_raw&0xff); _R_PERIOD(x)

/* R6: noise period, bits 0-4, clock divider of 16 */
#define R_NOISE()																	\
	int period,period_raw; (*psg_update)();											\
	period_raw=(v&0x1f)?v<<1&0x3e:2; /* 1 reg, so converting 0 to 1 done here */	\
	period=noise_period; noise_period+=period_raw-noise_period_raw;					\
	if (noise_period<=0) noise_period=(period&1)?1:2;								\
	noise_period_raw=period_raw

/* R7: enable/disable channels
	bits:
	0,1,2	disable tone channel a,b,c
	3,4,5	disable noise on channel a,b,c
	6,7		io mode, 0=input, 1=output. 6=port a, 7=port b (don't care here) */
#define R_MIXER()																	\
	(*psg_update)();																\
	if (custom_enabled) {															\
		a_disable=(v&1)?0x83:0; b_disable=(v&2)?0x83:0; c_disable=(v&4)?0x83:0;		\
		a_noise_disable=(v&8)?0x83:0; b_noise_disable=(v&0x10)?0x83:0; c_noise_disable=(v&0x20)?0x83:0; \
		RECALC_NOISE_CUSTOM(a) RECALC_NOISE_CUSTOM(b) RECALC_NOISE_CUSTOM(c)		\
		RECALC_CUSTOM(a); RECALC_CUSTOM(b); RECALC_CUSTOM(c);						\
	}																				\
	else {																			\
		a_disable=v&1; b_disable=v>>1&1; c_disable=v>>2&1;							\
		a_noise_disable=(v&8)?3:0; b_noise_disable=(v&0x10)?3:0; c_noise_disable=(v&0x20)?3:0; \
		RECALC_STD(a); RECALC_STD(b); RECALC_STD(c);								\
	}

/* R10-12 (8-10): amplitude (channel a,b,c)
	bits:
	0,1,2,3	amplitude
	4		enable envelope */
#define R_AMP(x)																	\
	(*psg_update)();																\
	if ((x##_envelope=v&0x10)) x##_vol=e_vol;										\
	else { v&=0xf; x##_vol=v?v<<3|4:0; }											\
	psg_recalc_output()

/* R13,14 (11,12): 16 bit envelope period, clock divider of 8 (16 on AY8910) (*32/16 for complete envelope cycle)
 (low periods can be used to create triangle and saw waves, though deformed due to nonlinear volume)
	reg:	bits:
	R13		all	envelope period fine
	R14		all	envelope period coarse */
#define _R_EPERIOD()																\
	(*psg_update)(); e_period+=period_raw-e_period_raw;								\
	if (e_period<=0) e_period=1;                                                    \
	e_period_raw=period_raw
#define R_EPERIOD_FINE() int period_raw=v|(e_period_raw&0xff00); _R_EPERIOD()
#define R_EPERIOD_COARSE() int period_raw=v<<8|(e_period_raw&0xff); _R_EPERIOD()

/* R15 (13): envelope shape
	bits:
	0	hold
	1	alternate
	2	attack
	3	continue */
#define R_ESHAPE()																	\
	(*psg_update)();																\
	if (v&8) { e_alt=v&2; e_hold=v&1; }												\
	else { e_alt=v&4; e_hold=1; }													\
	e_invert=v&4?0x7c:0;															\
	e_count=0x1f; e_disabled=0; e_period=1 /* clocks immediately */

/* R16: IO port a data, R17: IO port b data */
/* (no handling needed) */

/* write to psg register (see above) */
void __fastcall psg_write_data(u8 v)
{
	psg_regs[psg_address]=v;

	switch (psg_address) {

		/* tone period */
		case 0: { R_PERIOD_FINE(a); break; }
		case 1: { R_PERIOD_COARSE(a); break; }
		case 2: { R_PERIOD_FINE(b); break; }
		case 3: { R_PERIOD_COARSE(b); break; }
		case 4: { R_PERIOD_FINE(c); break; }
		case 5: { R_PERIOD_COARSE(c); break; }

		/* noise period */
		case 6: { R_NOISE(); break; }

		/* mixer */
		case 7:
#if COMPLY_STANDARDS_TEST
			if ((v&0xc0)!=0x80) printf("eeio W $A1 - PC $%04X (mixer $%02X!=$80)\n",z80_get_pc(),v&0xc0);
#endif
			R_MIXER()
			break;

		/* amplitude */
		case 8: R_AMP(a); break;
		case 9: R_AMP(b); break;
		case 10: R_AMP(c); break;

		/* envelope */
		case 11: { R_EPERIOD_FINE(); break; }
		case 12: { R_EPERIOD_COARSE(); break; }
		case 13: R_ESHAPE(); break;

		/* i/o */
		case 14: break; /* is input on msx */
		case 15: cont_write(v); break;

		default: break;
	}
}

u8 __fastcall psg_read_data(void)
{
	/* bit 0-5: controller state, bit 6: ansi/jis, bit 7: cassette input (unemulated) */
	/* bit 6 should only be read as set on tR, and some Japanese MSX2 and 2+, with JIS
	keyboard layout. Known exceptions: Canon V-20 (claims it's JIS) */
	if (psg_address==14) return cont_read();

	else return psg_regs[psg_address]&read_mask[psg_chiptype][psg_address];
}

void __fastcall psg_write_address(u8 v)
{
	psg_address=v&0xf;
}


/* --- */
void psg_set_buffer_counter(int bc)
{
	if (bc!=0&&bc<psg_bc) {
		/* copy leftover to start */
		int i=bc,j=psg_bc;
		while (i--) psg_dac[i]=psg_dac[--j];
	}

	psg_bc=bc;
}

int psg_get_buffer_counter(void) { return psg_bc; }
int psg_get_cc(void) { return psg_cc; }
void psg_end_frame(void) { (*psg_update)(); }
void psg_init_sound(void) { psg_dac=sound_create_dac(); }

void psg_new_frame(void)
{
	psg_cc+=crystal->frame;

	/* custom enabled changed */
	if (custom_enabled^custom_enabled_set) {
		if ((custom_enabled=custom_enabled_set)) psg_custom_init();
		else {
			psg_update=&psg_update_std;
			psg_recalc_mixer();
		}

		psg_recalc_output();

		/* (pops and clicks if no invalidate, but nothing really bad) */
		/* reverse_invalidate(); */

		return;
	}

	if (custom_enabled) {
		int i,a,c;

		/* update volume lookups if changed */
		c=psg_custom_recalc_amp_minus();
		for (i=0;i<3;i++) {
			a=psgtoy_get_custom_amplitude(i);
			if (a!=custom_amplitude[i]||c) {
				custom_amplitude[i]=a;
				psg_custom_fill_volume_levels(i);
				psgtoy_reset_custom_wave_changed(i);
				psg_custom_fill_volume_channel(i);
			}
			else if (psgtoy_reset_custom_wave_changed(i)) {
				psg_custom_fill_volume_channel(i);
			}
		}

		psg_recalc_output();
	}
}

static void psg_init_regs(void)
{
	int i;

	i=0x10; while (i--) psg_regs[i]=0;

	psg_cc=psg_address=0;
	remain=0x10;

	/* tone channels */
	a_period=			b_period=			c_period=1;
	a_period_raw=		b_period_raw=		c_period_raw=0;
	a_disable=			b_disable=			c_disable=1;
	a_noise_disable=	b_noise_disable=	c_noise_disable=3;
	a_envelope=			b_envelope=			c_envelope=0;
	a_sample=			b_sample=			c_sample=1;
	a_vol=				b_vol=				c_vol=0;
	a_output=			b_output=			c_output=0;

	/* noise */
	noise_period=noise_period_raw=2;
	noise_random=1;
	noise_sample=0;

	/* envelope */
	e_period=1;
	e_period_raw=0;
	e_count=0x1e;
	e_invert=0;
	e_hold=1;
	e_alt=0;
	e_disabled=0;
	e_vol=0x7c;

	/* custom */
	a_noise_xor=		b_noise_xor=		c_noise_xor=0;
}

void psg_poweron(void)
{
	psg_address=15; psg_write_data(0); /* init R15 (cont) */
	psg_init_regs();
	psg_recalc_mixer();
	psg_recalc_output();
}

void psg_init_amp(void)
{
	int i,min;
	double vol=PSG_VOLUME;
	double d=sqrt(sqrt(2));

	i=32; while (i--) lut_volume_std[i<<2]=0; /* silence for 00 */

	/* steps of ~1.5dB (envelope, ~3dB for normal amp) */
	i=32; while (i--) {
		lut_volume_std[i<<2|1]=lut_volume_std[i<<2|2]=lut_volume_std[i<<2|3]=vol+0.5;
		vol/=d;
	}

	/* normalize */
	min=lut_volume_std[1];
	i=32; while (i--) {
		lut_volume_std[i<<2|1]-=min;
		lut_volume_std[i<<2|2]-=min;
		lut_volume_std[i<<2|3]-=min;
	}

	if (psg_chiptype==PSG_CHIPTYPE_AY8910) {
		/* 16 volume levels */
		i=16; while (i--) lut_volume_std[i<<3|1]=lut_volume_std[i<<3|2]=lut_volume_std[i<<3|3]=lut_volume_std[i<<3|7];
	}

	i=32; while (i--) lut_volume_std[i<<2|2]>>=1; /* half volume for 02 */

	/* normalize 02 */
	min=lut_volume_std[2];
	i=32; while (i--) lut_volume_std[i<<2|2]-=min;

	psg_custom_fill_volume_channel_square_all();

	if (custom_enabled) psg_custom_init();

	psg_recalc_mixer();
	psg_recalc_output();
}

void psg_init(void)
{
	char* c=NULL;
	int i;

	psg_update=&psg_update_std;

	psg_init_regs();

	i=PSG_CHIPTYPE_DEFAULT;
	SETTINGS_GET_STRING(settings_info(SETTINGS_PSGCHIPTYPE),&c);
	if (c&&strlen(c)) {
		for (i=0;i<PSG_CHIPTYPE_MAX;i++) if (stricmp(c,psg_get_chiptype_name(i))==0) break;
		if (i==PSG_CHIPTYPE_MAX) i=PSG_CHIPTYPE_DEFAULT;
	}
	MEM_CLEAN(c);
	psg_chiptype=i;

	for (i=0;i<3;i++) {
		custom_wave[i]=psgtoy_get_custom_wave_ptr(i);
	}

	custom_enabled_set=custom_enabled=psgtoy_get_custom_enabled();

	psg_init_amp();
}

void psg_clean(void)
{
	;
}


/* state				size
psg_cc					4
remain					1
address					1
regs					16

a/b/c period			2/2/2
a/b/c period raw		2/2/2
a/b/c disable			1/1/1
a/b/c noise disable		1/1/1
a/b/c envelope			1/1/1
a/b/c sample			1/1/1
a/b/c vol				1/1/1

noise period			1
noise period raw		1
noise random			3
noise sample			1

env period				4
env period raw			2
env count				1
env invert				1
env hold				1
env alt					1
env disabled			1
env vol					1

 custom, optional
a/b/c step sub			1/1/1
a/b/c step add			1/1/1
a/b/c step rest			1/1/1
a/b/c step base			2/2/2
a/b/c fill remain		2/2/2
a/b/c cur sample		2/2/2

==						94
*/
#define STATE_VERSION	2
/* version history:
1: initial
2: removed a/b/c output (recalculated on state_load), added custom waveform
*/
#define STATE_SIZE		94

int __fastcall psg_state_get_version(void)
{
	return STATE_VERSION;
}

int __fastcall psg_state_get_size(void)
{
	return STATE_SIZE;
}

/* save */
void __fastcall psg_state_save(u8** s)
{
	STATE_SAVE_4(psg_cc);
	STATE_SAVE_1(remain);
	STATE_SAVE_1(psg_address);

	STATE_SAVE_1(psg_regs[0]);			STATE_SAVE_1(psg_regs[1]);		STATE_SAVE_1(psg_regs[2]);	STATE_SAVE_1(psg_regs[3]);
	STATE_SAVE_1(psg_regs[4]);			STATE_SAVE_1(psg_regs[5]);		STATE_SAVE_1(psg_regs[6]);	STATE_SAVE_1(psg_regs[7]);
	STATE_SAVE_1(psg_regs[8]);			STATE_SAVE_1(psg_regs[9]);		STATE_SAVE_1(psg_regs[10]);	STATE_SAVE_1(psg_regs[11]);
	STATE_SAVE_1(psg_regs[12]);			STATE_SAVE_1(psg_regs[13]);		STATE_SAVE_1(psg_regs[14]);	STATE_SAVE_1(psg_regs[15]);

	STATE_SAVE_2(a_period);				STATE_SAVE_2(b_period);			STATE_SAVE_2(c_period);
	STATE_SAVE_2(a_period_raw);			STATE_SAVE_2(b_period_raw);		STATE_SAVE_2(c_period_raw);
	STATE_SAVE_1(a_disable);			STATE_SAVE_1(b_disable);		STATE_SAVE_1(c_disable);
	STATE_SAVE_1(a_noise_disable);		STATE_SAVE_1(b_noise_disable);	STATE_SAVE_1(c_noise_disable);
	STATE_SAVE_1(a_envelope);			STATE_SAVE_1(b_envelope);		STATE_SAVE_1(c_envelope);
	STATE_SAVE_1(a_sample);				STATE_SAVE_1(b_sample);			STATE_SAVE_1(c_sample);
	STATE_SAVE_1(a_vol);				STATE_SAVE_1(b_vol);			STATE_SAVE_1(c_vol);

	STATE_SAVE_1(noise_period);
	STATE_SAVE_1(noise_period_raw);
	STATE_SAVE_3(noise_random);
	STATE_SAVE_1(noise_sample);

	STATE_SAVE_4(e_period);
	STATE_SAVE_2(e_period_raw);
	STATE_SAVE_1(e_count);
	STATE_SAVE_1(e_invert);
	STATE_SAVE_1(e_hold);
	STATE_SAVE_1(e_alt);
	STATE_SAVE_1(e_disabled);
	STATE_SAVE_1(e_vol);

	/* custom, optional */
	if (custom_enabled) {
		STATE_SAVE_1(a_step_sub);		STATE_SAVE_1(b_step_sub);		STATE_SAVE_1(c_step_sub);
		STATE_SAVE_1(a_step_add);		STATE_SAVE_1(b_step_add);		STATE_SAVE_1(c_step_add);
		STATE_SAVE_1(a_step_rest);		STATE_SAVE_1(b_step_rest);		STATE_SAVE_1(c_step_rest);
		STATE_SAVE_2(a_step_base);		STATE_SAVE_2(b_step_base);		STATE_SAVE_2(c_step_base);
		STATE_SAVE_2(a_fill_remain);	STATE_SAVE_2(b_fill_remain);	STATE_SAVE_2(c_fill_remain);
		STATE_SAVE_2(a_cur_sample);		STATE_SAVE_2(b_cur_sample);		STATE_SAVE_2(c_cur_sample);
	}
	else (*s)+=27;
}

/* load */
void __fastcall psg_state_load_cur(u8** s)
{
	STATE_LOAD_4(psg_cc);
	STATE_LOAD_1(remain);
	STATE_LOAD_1(psg_address);

	STATE_LOAD_1(psg_regs[0]);			STATE_LOAD_1(psg_regs[1]);		STATE_LOAD_1(psg_regs[2]);	STATE_LOAD_1(psg_regs[3]);
	STATE_LOAD_1(psg_regs[4]);			STATE_LOAD_1(psg_regs[5]);		STATE_LOAD_1(psg_regs[6]);	STATE_LOAD_1(psg_regs[7]);
	STATE_LOAD_1(psg_regs[8]);			STATE_LOAD_1(psg_regs[9]);		STATE_LOAD_1(psg_regs[10]);	STATE_LOAD_1(psg_regs[11]);
	STATE_LOAD_1(psg_regs[12]);			STATE_LOAD_1(psg_regs[13]);		STATE_LOAD_1(psg_regs[14]);	STATE_LOAD_1(psg_regs[15]);

	STATE_LOAD_2(a_period);				STATE_LOAD_2(b_period);			STATE_LOAD_2(c_period);
	STATE_LOAD_2(a_period_raw);			STATE_LOAD_2(b_period_raw);		STATE_LOAD_2(c_period_raw);
	STATE_LOAD_1(a_disable);			STATE_LOAD_1(b_disable);		STATE_LOAD_1(c_disable);
	STATE_LOAD_1(a_noise_disable);		STATE_LOAD_1(b_noise_disable);	STATE_LOAD_1(c_noise_disable);
	STATE_LOAD_1(a_envelope);			STATE_LOAD_1(b_envelope);		STATE_LOAD_1(c_envelope);
	STATE_LOAD_1(a_sample);				STATE_LOAD_1(b_sample);			STATE_LOAD_1(c_sample);
	STATE_LOAD_1(a_vol);				STATE_LOAD_1(b_vol);			STATE_LOAD_1(c_vol);

	STATE_LOAD_1(noise_period);
	STATE_LOAD_1(noise_period_raw);
	STATE_LOAD_3(noise_random);
	STATE_LOAD_1(noise_sample);

	STATE_LOAD_4(e_period);
	STATE_LOAD_2(e_period_raw);
	STATE_LOAD_1(e_count);
	STATE_LOAD_1(e_invert);
	STATE_LOAD_1(e_hold);
	STATE_LOAD_1(e_alt);
	STATE_LOAD_1(e_disabled);
	STATE_LOAD_1(e_vol);

	/* custom, optional. only load if custom was enabled at save */
	if (custom_enabled&&noise_sample&0x80) {
		psg_recalc_mixer();

		STATE_LOAD_1(a_step_sub);		STATE_LOAD_1(b_step_sub);		STATE_LOAD_1(c_step_sub);
		STATE_LOAD_1(a_step_add);		STATE_LOAD_1(b_step_add);		STATE_LOAD_1(c_step_add);
		STATE_LOAD_1(a_step_rest);		STATE_LOAD_1(b_step_rest);		STATE_LOAD_1(c_step_rest);
		STATE_LOAD_2(a_step_base);		STATE_LOAD_2(b_step_base);		STATE_LOAD_2(c_step_base);
		STATE_LOAD_2(a_fill_remain);	STATE_LOAD_2(b_fill_remain);	STATE_LOAD_2(c_fill_remain);
		STATE_LOAD_2(a_cur_sample);		STATE_LOAD_2(b_cur_sample);		STATE_LOAD_2(c_cur_sample);
	}
	else {
		psg_recalc_mixer();
		(*s)+=27;
	}

	psg_recalc_output();
}

void __fastcall psg_state_load_1(u8** s)
{
	/* version 1 */
	STATE_LOAD_4(psg_cc);
	STATE_LOAD_1(remain);
	STATE_LOAD_1(psg_address);

	STATE_LOAD_1(psg_regs[0]);			STATE_LOAD_1(psg_regs[1]);		STATE_LOAD_1(psg_regs[2]);	STATE_LOAD_1(psg_regs[3]);
	STATE_LOAD_1(psg_regs[4]);			STATE_LOAD_1(psg_regs[5]);		STATE_LOAD_1(psg_regs[6]);	STATE_LOAD_1(psg_regs[7]);
	STATE_LOAD_1(psg_regs[8]);			STATE_LOAD_1(psg_regs[9]);		STATE_LOAD_1(psg_regs[10]);	STATE_LOAD_1(psg_regs[11]);
	STATE_LOAD_1(psg_regs[12]);			STATE_LOAD_1(psg_regs[13]);		STATE_LOAD_1(psg_regs[14]);	STATE_LOAD_1(psg_regs[15]);

	STATE_LOAD_2(a_period);				STATE_LOAD_2(b_period);			STATE_LOAD_2(c_period);
	STATE_LOAD_2(a_period_raw);			STATE_LOAD_2(b_period_raw);		STATE_LOAD_2(c_period_raw);
	STATE_LOAD_1(a_disable);			STATE_LOAD_1(b_disable);		STATE_LOAD_1(c_disable);
	STATE_LOAD_1(a_noise_disable);		STATE_LOAD_1(b_noise_disable);	STATE_LOAD_1(c_noise_disable);
	STATE_LOAD_1(a_envelope);			STATE_LOAD_1(b_envelope);		STATE_LOAD_1(c_envelope);
	STATE_LOAD_1(a_sample);				STATE_LOAD_1(b_sample);			STATE_LOAD_1(c_sample);
	STATE_LOAD_1(a_vol);				STATE_LOAD_1(b_vol);			STATE_LOAD_1(c_vol);
	STATE_LOAD_4(a_output);				STATE_LOAD_4(b_output);			STATE_LOAD_4(c_output); /* dummy */

	STATE_LOAD_1(noise_period);
	STATE_LOAD_1(noise_period_raw);
	STATE_LOAD_3(noise_random);
	STATE_LOAD_1(noise_sample);

	STATE_LOAD_4(e_period);
	STATE_LOAD_2(e_period_raw);
	STATE_LOAD_1(e_count);
	STATE_LOAD_1(e_invert);
	STATE_LOAD_1(e_hold);
	STATE_LOAD_1(e_alt);
	STATE_LOAD_1(e_disabled);
	STATE_LOAD_1(e_vol);

	psg_recalc_mixer();
	psg_recalc_output();
}

int __fastcall psg_state_load(int v,u8** s)
{
	switch (v) {
		case 1:
			psg_state_load_1(s);
			break;
		case STATE_VERSION:
			psg_state_load_cur(s);
			break;

		default: return FALSE;
	}

	return TRUE;
}
