/******************************************************************************
 *                                                                            *
 *                                 "cont.c"                                   *
 *                                                                            *
 ******************************************************************************/

#include "global.h"
#include "cont.h"
#include "input.h"
#include "settings.h"
#include "netplay.h"
#include "paste.h"
#include "state.h"
#include "reverse.h"
#include "draw.h"
#include "resource.h"
#include "main.h"
#include "msx.h"
#include "movie.h"
#include "crystal.h"
#include "z80.h"
#include "io.h"

/* controller names */
static const char* cont_name[CONT_D_MAX]={
	"joystick",
	"Taito Arkanoid pad",
	"Konami HyperShot",
	"Sony Magic Key",
	"mouse",
	"trackball",

	"keyboard",

	"nothing"
};
const char* cont_get_name(u32 i) { if (i>=CONT_D_MAX) return NULL; else return cont_name[i]; }

static const char* cont_a_name[CONT_DA_MAX]={
	"joystick (port 1)",
	"joystick (port 2)",
	"mouse (port 1)",
	"mouse (port 2)",
	"trackball (port 1)",
	"trackball (port 2)",
	"Taito Arkanoid pad (port 1)",
	"Taito Arkanoid pad (port 2)",
	"Konami HyperShot controller (port 1)",
	"Konami HyperShot controller (port 2)",

	"keyboard"
};
const char* cont_get_a_name(u32 i) { if (i>=CONT_DA_MAX) return NULL; else return cont_a_name[i]; }

/* keyboard regions */
static int cont_auto_region=TRUE;
static int cont_region=CONT_REGION_INTERNATIONAL;
static const char* cont_region_name[CONT_REGION_MAX]={
	"International",	"Japanese",	"Korean",	"United Kingdom",	"auto"
};
static const char* cont_region_shortname[CONT_REGION_MAX]={
	"Int",				"JP",		"KR",		"UK",				"auto"
};
const char* cont_get_region_name(u32 i) { if (i>=CONT_REGION_MAX) return NULL; else return cont_region_name[i]; }
const char* cont_get_region_shortname(u32 i) { if (i>=CONT_REGION_MAX) return NULL; else return cont_region_shortname[i]; }
int cont_get_auto_region(void) { return cont_auto_region; }
int cont_get_region(void) { return cont_region; }
void cont_set_region(u32 r) { if (r>=CONT_REGION_MAX) r=0; cont_region=r; }

/* ports */
static const int cont_port_default[CONT_P_MAX]={CONT_D_J,CONT_D_J,CONT_D_KEY};
static int cont_port[CONT_P_MAX];
u32 cont_get_port(u32 p) { if (p<CONT_P_MAX) return cont_port[p]; else return CONT_D_NOTHING; }

/* misc controller settings */
static int cont_mousejoyemu=0;
static int cont_arkdialrange[2];

/* trigger info */
static const char* cont_t_info[CONT_T_MAX]={
	/* joystick 1 */
	"joystick 1 Up",
	"joystick 1 Down",
	"joystick 1 Left",
	"joystick 1 Right",
	"joystick 1 Button A",
	"joystick 1 Button B",
	/* joystick 2 */
	"joystick 2 Up",
	"joystick 2 Down",
	"joystick 2 Left",
	"joystick 2 Right",
	"joystick 2 Button A",
	"joystick 2 Button B",

	/* mouse 1 */
	"mouse 1 Left Button",
	"mouse 1 Right Button",
	/* mouse 2 */
	"mouse 2 Left Button",
	"mouse 2 Right Button",

	/* trackball 1 */
	"trackball 1 Lower Button",
	"trackball 1 Upper Button",
	/* trackball 2 */
	"trackball 2 Lower Button",
	"trackball 2 Upper Button",

	/* hypershot 1 */
	"HyperShot 1 Jump",
	"HyperShot 1 Run",
	/* hypershot 2 */
	"HyperShot 2 Jump",
	"HyperShot 2 Run",

	/* arkanoid */
	"Arkanoid pad 1 Button",
	"Arkanoid pad 2 Button",

	/* keyboard */
	"keyboard 0",
	"keyboard 1",
	"keyboard 2",
	"keyboard 3",
	"keyboard 4",
	"keyboard 5",
	"keyboard 6",
	"keyboard 7",
	"keyboard 8",
	"keyboard 9",

	"keyboard A ",
	"keyboard B ",
	"keyboard C ",
	"keyboard D ",
	"keyboard E ",
	"keyboard F ",
	"keyboard G ",
	"keyboard H ",
	"keyboard I ",
	"keyboard J ",
	"keyboard K ",
	"keyboard L ",
	"keyboard M ",
	"keyboard N ",
	"keyboard O ",
	"keyboard P ",
	"keyboard Q ",
	"keyboard R ",
	"keyboard S ",
	"keyboard T ",
	"keyboard U ",
	"keyboard V ",
	"keyboard W ",
	"keyboard X ",
	"keyboard Y ",
	"keyboard Z ",

	"keyboard F1",
	"keyboard F2",
	"keyboard F3",
	"keyboard F4",
	"keyboard F5",

	"keyboard `",
	"keyboard -",
	"keyboard =",
	"keyboard [",
	"keyboard ]",
	"keyboard \\",
	"keyboard ;",
	"keyboard '",
	"keyboard ,",
	"keyboard .",
	"keyboard /",
	"keyboard Accent Mark/_",

	"keyboard Spacebar",
	"keyboard Tab",
	"keyboard Return",
	"keyboard Home",
	"keyboard Insert",
	"keyboard Delete",
	"keyboard Backspace",
	"keyboard Escape",
	"keyboard Stop",
	"keyboard Select",

	"keyboard Up",
	"keyboard Down",
	"keyboard Left",
	"keyboard Right",

	"keyboard Shift",
	"keyboard Caps Lock",
	"keyboard Control",
	"keyboard Graph",
	"keyboard Code/Kana Lock",

	"keyboard Numpad 0",
	"keyboard Numpad 1",
	"keyboard Numpad 2",
	"keyboard Numpad 3",
	"keyboard Numpad 4",
	"keyboard Numpad 5",
	"keyboard Numpad 6",
	"keyboard Numpad 7",
	"keyboard Numpad 8",
	"keyboard Numpad 9",
	"keyboard Numpad ,",
	"keyboard Numpad .",
	"keyboard Numpad +",
	"keyboard Numpad -",
	"keyboard Numpad *",
	"keyboard Numpad /",

	"keyboard Torikeshi", /* "cancellation" */
	"keyboard Jikkou", /* "execute" */

	/* extra keyboard keys */
	"keyboard Pause"
};
const char* cont_get_trigger_info(u32 i) { if (i>=CONT_T_MAX) return input_get_trigger_info(i-CONT_T_MAX); else return cont_t_info[i]; }

/* axis info */
static const char* cont_a_info[CONT_A_MAX]={
	/* mouse 1 */
	"mouse 1 X Axis",
	"mouse 1 Y Axis",
	/* mouse 2 */
	"mouse 2 X Axis",
	"mouse 2 Y Axis",

	/* trackball 1 */
	"trackball 1 X Axis",
	"trackball 1 Y Axis",
	/* trackball 2 */
	"trackball 2 X Axis",
	"trackball 2 Y Axis",

	/* arkanoid */
	"Arkanoid pad 1 Dial",
	"Arkanoid pad 2 Dial"
};
const char* cont_get_axis_info(u32 i) { if (i>=CONT_A_MAX) return input_get_axis_info(i-CONT_A_MAX); else return cont_a_info[i]; }


u32 cont_get_da(u32 p)
{
	u32 ret=(CONT_DA_EMPTY)|(CONT_DA_EMPTY<<8)|(CONT_DA_EMPTY<<16)|(CONT_DA_EMPTY<<24);

	/* no need to add user-inputless devices like magic key */
	switch (p) {

		/* port 1 */
		case CONT_P1:
			switch (cont_get_port(CONT_P1)) {
				case CONT_D_J: ret=(ret&0xffffff00)|CONT_DA_J1; break;		/* joystick */
				case CONT_D_M: ret=(ret&0xffffff00)|CONT_DA_M1; break;		/* mouse */
				case CONT_D_T: ret=(ret&0xffffff00)|CONT_DA_T1; break;		/* trackball */
				case CONT_D_HS: ret=(ret&0xffffff00)|CONT_DA_HS1; break;	/* hypershot */
				case CONT_D_ARK: ret=(ret&0xffffff00)|CONT_DA_ARK1; break;	/* arkanoid */
				default: break;
			}
			break;

		/* port 2 */
		case CONT_P2:
			switch (cont_get_port(CONT_P2)) {
				case CONT_D_J: ret=(ret&0xffffff00)|CONT_DA_J2; break;		/* joystick */
				case CONT_D_M: ret=(ret&0xffffff00)|CONT_DA_M2; break;		/* mouse */
				case CONT_D_T: ret=(ret&0xffffff00)|CONT_DA_T2; break;		/* trackball */
				case CONT_D_HS: ret=(ret&0xffffff00)|CONT_DA_HS2; break;	/* hypershot */
				case CONT_D_ARK: ret=(ret&0xffffff00)|CONT_DA_ARK2; break;	/* arkanoid */
				default: break;
			}
			break;

		/* keyboard */
		case CONT_PKEY:
			switch (cont_get_port(CONT_PKEY)) {
				case CONT_D_KEY: ret=(ret&0xffffff00)|CONT_DA_KEY; break;	/* keyboard */
				default: break;
			}
			break;

		default: break;
	}

	return ret;
}


void cont_init(void)
{
	char* c=NULL;
	int i;

	/* keyboard region */
	i=cont_region;
	SETTINGS_GET_STRING(settings_info(SETTINGS_REGION),&c);
	if (c&&strlen(c)) {
		for (i=0;i<CONT_REGION_MAX;i++) if (stricmp(c,cont_get_region_name(i))==0) break;
		if (i==CONT_REGION_MAX||i==CONT_REGION_AUTO) i=cont_region;
		else cont_auto_region=FALSE;
	}
	MEM_CLEAN(c);
	cont_region=i;

	/* mouse joystick emulation */
	cont_mousejoyemu=FALSE; i=settings_get_yesnoauto(SETTINGS_MOUSEJOYEMU); if (i==FALSE||i==TRUE) { cont_mousejoyemu=i; }

	/* Arkanoid dial range, the defaults (and minimum) are the most user friendly for Arkanoid */
	/* the actual hardware range is ~55-325, but that makes the paddle (mouse) handling kind of awkward */
	cont_arkdialrange[0]=163; if (SETTINGS_GET_INT(settings_info(SETTINGS_ARKDIALMIN),&i)) { CLAMP(i,50,163); cont_arkdialrange[0]=i; }
	cont_arkdialrange[1]=309; if (SETTINGS_GET_INT(settings_info(SETTINGS_ARKDIALMAX),&i)) { CLAMP(i,309,400); cont_arkdialrange[1]=i; }

	/* get inserted controllers settings (no custom keyboard yet) */
	cont_port[CONT_PKEY]=CONT_D_KEY;

	for (i=CONT_P1;i<(CONT_P_MAX-1);i++) {
		int setport=0;

		switch (i) {
			case CONT_P1: setport=SETTINGS_PORT1; break;
			case CONT_P2: setport=SETTINGS_PORT2; break;
			default: break;
		}

		c=NULL;
		SETTINGS_GET_STRING(settings_info(setport),&c);
		cont_port[i]=cont_port_default[i];

		if (c&&strlen(c)) {
			int cont;
			for (cont=0;cont<CONT_D_MAX;cont++) if (stricmp(c,cont_name[cont])==0) break;
			if (cont==CONT_D_KEY||cont==CONT_D_MAX) cont=cont_port_default[i];
			cont_port[i]=cont;
		}
		MEM_CLEAN(c);

		cont_insert(i,cont_port[i],TRUE,TRUE);
	}
}

void cont_clean(void)
{
	;
}

void cont_settings_save(void)
{
	/* called from input.c input_settings_save */
	SETTINGS_PUT_STRING(NULL,"\n");
	settings_put_yesnoauto(SETTINGS_MOUSEJOYEMU,cont_mousejoyemu);
	SETTINGS_PUT_INT(settings_info(SETTINGS_ARKDIALMIN),cont_arkdialrange[0]);
	SETTINGS_PUT_INT(settings_info(SETTINGS_ARKDIALMAX),cont_arkdialrange[1]);

	SETTINGS_PUT_STRING(NULL,"\n; Valid port options: ");
	SETTINGS_PUT_STRING(NULL,cont_get_name(CONT_D_J)); SETTINGS_PUT_STRING(NULL,", ");
	SETTINGS_PUT_STRING(NULL,cont_get_name(CONT_D_M)); SETTINGS_PUT_STRING(NULL,", ");
	SETTINGS_PUT_STRING(NULL,cont_get_name(CONT_D_T)); SETTINGS_PUT_STRING(NULL,", ");
	SETTINGS_PUT_STRING(NULL,cont_get_name(CONT_D_HS)); SETTINGS_PUT_STRING(NULL,",\n; -- ");
	SETTINGS_PUT_STRING(NULL,cont_get_name(CONT_D_MAGIC)); SETTINGS_PUT_STRING(NULL,", ");
	SETTINGS_PUT_STRING(NULL,cont_get_name(CONT_D_ARK)); SETTINGS_PUT_STRING(NULL,", or ");
	SETTINGS_PUT_STRING(NULL,cont_get_name(CONT_D_NOTHING)); SETTINGS_PUT_STRING(NULL,".\n\n");

	SETTINGS_PUT_STRING(settings_info(SETTINGS_PORT1),cont_name[cont_port[CONT_P1]]);
	SETTINGS_PUT_STRING(settings_info(SETTINGS_PORT2),cont_name[cont_port[CONT_P2]]);

	SETTINGS_PUT_STRING(NULL,"\n; Valid "); SETTINGS_PUT_STRING(NULL,settings_info(SETTINGS_REGION)); SETTINGS_PUT_STRING(NULL," options: ");
	SETTINGS_PUT_STRING(NULL,cont_get_region_name(CONT_REGION_INTERNATIONAL)); SETTINGS_PUT_STRING(NULL,", ");
	SETTINGS_PUT_STRING(NULL,cont_get_region_name(CONT_REGION_JAPANESE)); SETTINGS_PUT_STRING(NULL,", ");
	SETTINGS_PUT_STRING(NULL,cont_get_region_name(CONT_REGION_KOREAN)); SETTINGS_PUT_STRING(NULL,",\n; -- ");
	SETTINGS_PUT_STRING(NULL,cont_get_region_name(CONT_REGION_UK)); SETTINGS_PUT_STRING(NULL,", or ");
	SETTINGS_PUT_STRING(NULL,cont_get_region_name(CONT_REGION_AUTO)); SETTINGS_PUT_STRING(NULL,".\n\n");

	SETTINGS_PUT_STRING(settings_info(SETTINGS_REGION),cont_get_region_name(cont_auto_region?CONT_REGION_AUTO:cont_region));
}


/* emu */
static u8 keyrow[16];
static int keyextra;		/* extra keys not on matrix, like hardware pause key */
static int stateextra;		/* extra state, like hardware pause state */
static int sonypause_prev;

static int lastmove[2];
static int curport=0;
static int gen_output[2]={0,0};
static u8 joy_state[2];
static u8 hs_state[2];

static int mouse_reset[2];
static int mouse_joystick[2];
static int mouse_cc[2];
static u16 mouse_shift[2];
static u8 mouse_rot_x_upd[2];
static u8 mouse_rot_y_upd[2];
static u8 mouse_rot_x[2];
static u8 mouse_rot_y[2];
static u8 mouse_state[2];

static int tb_reset[2];
static s8 tb_adder_x[2];
static s8 tb_adder_y[2];
static s8 tb_adder_x_is[2];
static s8 tb_adder_y_is[2];
static u8 tb_state[2];

static int ark_shift_upd[2];
static int ark_shift[2];
static int ark_button[2];
static int ark_bitcount[2];

u8 __fastcall cont_get_joystate(u8 j) { return joy_state[j]; }
u8 __fastcall cont_get_keyrow(u8 row) { return keyrow[row]; }
int __fastcall cont_get_keyextra(void) { return keyextra; }
int __fastcall cont_get_stateextra(void) { return stateextra; }
void __fastcall cont_set_keyextra(int e) { keyextra=e; }
void __fastcall cont_set_stateextra(int e){ stateextra=e; }

int __fastcall cont_check_keypress(void)
{
	/* ret:
		bit 0:	read trigger pressed (has priority over bit 1)
		bit 1:	raw axis moved
	*/

	/* keyboard */
	int i=12;
	while (i--) if (keyrow[i]!=0xff) return 1;
	if (keyextra) return 1;

	/* ports */
	i=2;
	while (i--) {
		switch (cont_port[i]) {

			/* joystick */
			case CONT_D_J:
				if (joy_state[i]!=0x3f) return 1;
				break;

			/* mouse */
			case CONT_D_M:
				if (mouse_joystick[i]) {
					if (mouse_state[i]!=0x3f) return 1;
				}
				else {
					if ((mouse_state[i]&0x30)!=0x30) return 1;
					if (mouse_rot_x_upd[i]||mouse_rot_y_upd[i]) return 2;
				}
				break;
			/* trackball */
			case CONT_D_T:
				if ((tb_state[i]&0x30)!=0x30) return 1;
				if (lastmove[i]) return 2;
				break;

			/* hypershot */
			case CONT_D_HS:
				if (hs_state[i]!=0x3f) return 1;
				break;

			/* arkanoid */
			case CONT_D_ARK:
				if (ark_button[i]&2) return 1;
				if (lastmove[i]) return 2;
				break;

			default: break;
		}
	}

	return 0;
}

/* POWERON */
/* joystick */
static void contpower_joystick(int port)
{
	joy_state[port]=0x3f;
}

/* mouse */
static void contpower_mouse(int port)
{
	mouse_rot_x_upd[port]=mouse_rot_y_upd[port]=0;
	mouse_rot_x[port]=mouse_rot_y[port]=0;

	mouse_shift[port]=0;
	mouse_cc[port]=z80_get_rel_cycles();
	mouse_state[port]=0x30;

	mouse_reset[port]=0xff;
	mouse_joystick[port]=0; /* undefined, will be done at initial update */
}

/* trackball */
static void contpower_trackball(int port)
{
	tb_adder_x[port]=tb_adder_y[port]=0;
	tb_adder_x_is[port]=tb_adder_y_is[port]=0;
	tb_state[port]=0x38;

	tb_reset[port]=(gen_output[port]&4)?0:0xff;
}

/* hypershot */
static void contpower_hypershot(int port)
{
	hs_state[port]=0x3f;
}

/* arkanoid */
static void contpower_arkanoid(int port)
{
	ark_shift_upd[port]=0xec;
	ark_shift[port]=0;
	ark_button[port]=2;
	ark_bitcount[port]=8;
}

static void cont_poweron_port(int port)
{
	/* call above */
	contpower_joystick(port);
	contpower_mouse(port);
	contpower_trackball(port);
	contpower_hypershot(port);
	contpower_arkanoid(port);

	lastmove[port]=FALSE;
}

void cont_poweron(void)
{
	memset(keyrow,0xff,16);
	keyextra=stateextra=0;

	/* not all MSXes turn off joystick port power on reset though */
	curport=gen_output[0]=gen_output[1]=0;

	cont_reset_sonypause();

	cont_poweron_port(0);
	cont_poweron_port(1);
}

/* UPDATE */
typedef void __fastcall (*_contupdate)(int);
static _contupdate contupdate[2];
static void __fastcall contupdate_nothing(int port) { port = port; } /* nothing */

/* joystick: 4 directions, 1 or 2 buttons (many different models) */
static void __fastcall contupdate_joystick(int port)
{
	const int size=port*CONT_T_J_SIZE;
	joy_state[port]=(input_trigger_held(CONT_T_J1_UP+size)|(input_trigger_held(CONT_T_J1_DOWN+size)<<1)|(input_trigger_held(CONT_T_J1_LEFT+size)<<2)|(input_trigger_held(CONT_T_J1_RIGHT+size)<<3)|(input_trigger_held(CONT_T_J1_A+size)<<4)|(input_trigger_held(CONT_T_J1_B+size)<<5))^0x3f;
	if (draw_is_flip_x()) joy_state[port]=(joy_state[port]&0xf3)|(joy_state[port]<<1&8)|(joy_state[port]>>1&4); /* switch left/right */
}

/* mouse: 2 buttons, several models, mice with a Mitsumi chip (Sony, Philips) support joystick emulation */
/* software support: Music Studio G7, ..? (used more often on MSX2 and up) */
static int __fastcall cont_mouse_update_rotary(int port)
{
	/* Once pin 8 is low, rotary encoders take some time to sense movement again,
	 useful to prevent the shift register being updated during a program reading it.
	 This delay is hard to measure accurately and may differ on other mice. */
	const int cc_ready=8000;

	if (~gen_output[port]&4&&((mouse_cc[port]-z80_get_rel_cycles())/crystal->rel_cycle)>cc_ready) {
		/* update if movement was detected */
		if (mouse_rot_x[port]==0) mouse_rot_x[port]=mouse_rot_x_upd[port];
		if (mouse_rot_y[port]==0) mouse_rot_y[port]=mouse_rot_y_upd[port];

		return TRUE;
	}

	return FALSE;
}
static void __fastcall contupdate_mouse(int port)
{
	const int tsize=port*CONT_T_M_SIZE;
	const int asize=port*CONT_A_M_SIZE;
	long lx=-input_get_axis(CONT_A_M1_X+asize);
	long ly=-input_get_axis(CONT_A_M1_Y+asize);

	if (draw_is_flip_x()) lx=-lx;
	CLAMP(lx,-128,127); mouse_rot_x_upd[port]=lx;
	CLAMP(ly,-128,127); mouse_rot_y_upd[port]=ly;
	cont_mouse_update_rotary(port);

	mouse_state[port]=((input_trigger_held(CONT_T_M1_LEFT+tsize)<<4)|(input_trigger_held(CONT_T_M1_RIGHT+tsize)<<5))^0x30;

	/* joystick emulation */
	/* enable if left button is held down at power-on */
	if (mouse_reset[port]) {
		if (cont_mousejoyemu&&~mouse_state[port]&0x10) {
			mouse_shift[port]=0;
			mouse_joystick[port]=TRUE;
			LOG(LOG_MISC|LOG_COLOUR(LC_GREEN),"mouse %d joystick emulation activated\n",port+1);
		}
		mouse_reset[port]=FALSE;
	}

	if (mouse_joystick[port]) mouse_state[port]|=(((ly>2)|(ly<-2)<<1|(lx>2)<<2|(lx<-2)<<3)^0xf);
}

/* trackball, known models are:
 - HAL HTC-001 "CAT" - red lower button, white upper button
 - Sony GB-5/GB-6/GB-7 - red lower button, orange upper button (GB-7 has a 3rd button acting like red+orange)
 I don't have a trackball myself, implementation is mostly based on logical assumptions. */
/* software support: HAL: Eddy II, Fruit Search, Hole in One(+Pro), Space Trouble, Super Billiards, etc.
   other: Music Studio G7, ..? */
static void __fastcall contupdate_trackball(int port)
{
	const int tsize=port*CONT_T_T_SIZE;
	const int asize=port*CONT_A_T_SIZE;
	long lx=input_get_axis(CONT_A_T1_X+asize);
	long ly=input_get_axis(CONT_A_T1_Y+asize);
	lastmove[port]=(lx!=0)|(ly!=0);

	/* two 8 bit rotary encoders connected to adders */
	if (draw_is_flip_x()) lx=-lx;
	lx+=tb_adder_x[port]; CLAMP(lx,-128,127); tb_adder_x[port]=lx;
	ly+=tb_adder_y[port]; CLAMP(ly,-128,127); tb_adder_y[port]=ly;

	/* remember frame start initial state for proper movie playback */
	tb_adder_x_is[port]=tb_adder_x[port];
	tb_adder_y_is[port]=tb_adder_y[port];

	tb_state[port]=(tb_state[port]&~0x30)|(((input_trigger_held(CONT_T_T1_LOWER+tsize)<<4)|(input_trigger_held(CONT_T_T1_UPPER+tsize)<<5))^0x30);
}

/* hypershot: jump button on the left, run button on the right (device can be turned around to 'swap' buttons) */
/* software support: Konami Hyper Sports/Olympic series */
static void __fastcall contupdate_hypershot(int port)
{
	const int size=port*CONT_T_HS_SIZE;
	hs_state[port]=(input_trigger_held(CONT_T_HS1_JUMP+size)<<4^0x30)|(input_trigger_held(CONT_T_HS1_RUN+size)?5:0xf); /* same as joystick button 1 and right+down */
}

/* magic key (simple cheat device by Sony) doesn't need update */
/* software support: Gall Force, and several MSX2 games */

/* arkanoid: 180 degrees dial on the bottom, button above (only usable on ROM release, CAS release works with joystick)
 The one released with Arkanoid is black, with red buttons, the one released with Arkanoid 2 is red, with black buttons. */
/* software support: Arkanoid and Arkanoid 2(MSX2) games only */
static void __fastcall contupdate_arkanoid(int port)
{
	const int tsize=port*CONT_T_ARK_SIZE;
	const int asize=port*CONT_A_ARK_SIZE;
	long l=input_get_axis(CONT_A_ARK1_DIAL+asize);
	lastmove[port]=l!=0;

	if (draw_is_flip_x()) l=-l;
	l+=ark_shift_upd[port];
	CLAMP(l,cont_arkdialrange[0],cont_arkdialrange[1]);

	ark_shift_upd[port]=l;
	ark_button[port]=input_trigger_held(CONT_T_ARK1_BUTTON+tsize)<<1^2;
}

/* keyboard + general */
void __fastcall cont_update(void)
{
	int i=11; /* matrix */

	/* update plugged-in devices */
	contupdate[0](0); contupdate[1](1);

	/* keyboard.. the matrix reloaded ;p */
	#define k(row,k7,k6,k5,k4,k3,k2,k1,k0) keyrow[row]=~(input_trigger_held(CONT_T_K_##k7)<<7|input_trigger_held(CONT_T_K_##k6)<<6|input_trigger_held(CONT_T_K_##k5)<<5|input_trigger_held(CONT_T_K_##k4)<<4|input_trigger_held(CONT_T_K_##k3)<<3|input_trigger_held(CONT_T_K_##k2)<<2|input_trigger_held(CONT_T_K_##k1)<<1|input_trigger_held(CONT_T_K_##k0))
	k(0,  7,         6,        5,         4,         3,      2,      1,     0);
	k(1,  SEMICOLON, RBRACKET, LBRACKET,  BACKSLASH, EQUALS, MINUS,  9,     8);
	k(2,  B,         A,        ACCENT,    SLASH,     PERIOD, COMMA,  TILDE, QUOTE);
	k(3,  J,         I,        H,         G,         F,      E,      D,     C);
	k(4,  R,         Q,        P,         O,         N,      M,      L,     K);
	k(5,  Z,         Y,        X,         W,         V,      U,      T,     S);
	k(6,  F3,        F2,       F1,        CODE,      CAPS,   GRAPH,  CTRL,  SHIFT);
	k(7,  RETURN,    SELECT,   BACKSPACE, STOP,      TAB,    ESC,    F5,    F4);
	k(8,  RIGHT,     DOWN,     UP,        LEFT,      DEL,    INS,    HOME,  SPACE);
	k(9,  N4,        N3,       N2,        N1,        N0,     NSLASH, NPLUS, NASTERISK);
	k(10, NPERIOD,   NCOMMA,   NMINUS,    N9,        N8,     N7,     N6,    N5);
	#undef k
	keyrow[11]=~((input_trigger_held(CONT_T_K_TORIKESHI)<<3)|(input_trigger_held(CONT_T_K_JIKKOU)<<1)); /* row 12, only 2 keys */

	/* extra keys, only hardware pause for now */
	keyextra=input_trigger_held(CONT_T_K_SONYPAUSE)&CONT_EKEY_SONYPAUSE;

	/* switch keyboard left/right */
	if (draw_is_flip_x()) keyrow[8]=(keyrow[8]&0x6f)|(keyrow[8]<<3&0x80)|(keyrow[8]>>3&0x10);

	reverse_cont_update();
	paste_frame(keyrow);
	netplay_frame(keyrow,&keyextra,joy_state);

	/* matrix quirk (after netplay) */
	while (i--) {

		if (keyrow[i]==0xff) continue;

		if (i==6) {
			/* modifier keys, this differs per MSX: some still have the ghosting effect with the
			modifier keys, some protect SHIFT/GRAPH/CODE by using diodes (and maybe even some
			protect row 6 completely, or differently?) */
			#define MKMASK 0x15

			int k=keyrow[6]&MKMASK; keyrow[6]|=MKMASK;
			if (keyrow[6]!=0xff) {
				int j=12;
				while (j--) {
					if ((keyrow[6]|keyrow[j])!=0xff) keyrow[6]=keyrow[j]=keyrow[6]&keyrow[j];
				}
			}
			keyrow[6]=k|(keyrow[6]&~MKMASK);
		}

		else {
			int j=12;
			while (j--) {
				if ((keyrow[i]|keyrow[j])!=0xff) keyrow[i]=keyrow[j]=keyrow[i]&keyrow[j];
			}
		}
	}

	movie_frame();
}

/* READ/WRITE */
typedef u8 __fastcall (*_contread)(int);
static _contread contread[2];
u8 __fastcall cont_read(void)
{
	/* PSG R14 bit 0-5 read, pins 6/7 input ANDed with pins 6/7 output */
#if COMPLY_STANDARDS_TEST
	if ((gen_output[curport]&3)!=3) printf("eeio R $A2 - PC $%04X (pin 6/7 low)\n",z80_get_pc());
#endif
	return contread[curport](curport)&((gen_output[curport]<<4&0x30)|0xf);
}

typedef void __fastcall (*_contwrite)(int,u8);
static _contwrite contwrite[2];
void __fastcall cont_write(u8 v)
{
	/* PSG R15 write, output pins 6,7,8 */
	int v0=(v&3)|(v>>2&4);
	int v1=(v>>2&3)|(v>>3&4);

	curport=v>>6&1;

	contwrite[0](0,v0);
	contwrite[1](1,v1);

	gen_output[0]=v0;
	gen_output[1]=v1;
}

/* nothing */
static u8 __fastcall contread_nothing(int port) { port = port; return 0x3f; }
static void __fastcall contwrite_nothing(int port,u8 v) { port = port; v = v; }

/* joystick
 Some MSX joysticks/pads have a similar quirk to the hypershot controller, where pins 6/8 influence direction pins.
 Since there are so many joystick models, it's left unemulated */
static u8 __fastcall contread_joystick(int port) { return joy_state[port]; }

/* mouse
 Like with the Arkanoid pad, reading bits 0-3 in mouse mode is timing critical. Bits may be discarded when reading/strobing
 very slowly or very quickly, this (annoying) behaviour is unemulated, stomme stinkmuis. */
static u8 __fastcall contread_mouse(int port)
{
	/* bits 0-3: movement, bits 4,5: buttons */
	return mouse_state[port]|(mouse_shift[port]&0xf);
}
static void __fastcall contwrite_mouse(int port,u8 v)
{
	/* no effect in joystick emulation mode */
	if (mouse_joystick[port]) return;

	/* pin 8 flipped */
	if ((v^gen_output[port])&4) {
		/* latch on rising edge if rotary encoders have updated */
		if (v&4&&cont_mouse_update_rotary(port)) {
			/* fedc ba98 7654 3210 - ylow yhigh xlow xhigh >> */
			mouse_shift[port]=(mouse_rot_x[port]>>4&0xf)|(mouse_rot_x[port]<<4&0xf0)|(mouse_rot_y[port]<<4&0xf00)|(mouse_rot_y[port]<<12&0xf000);
		}

		else {
			/* shift */
			mouse_shift[port]>>=4;

			/* reset rotary encoders on falling edge */
			if (~v&4) {
				mouse_cc[port]=z80_get_rel_cycles();
				mouse_rot_x[port]=mouse_rot_y[port]=0;
			}
		}
	}
}

/* trackball */
static u8 __fastcall contread_trackball(int port)
{
	return tb_state[port];
}
static void __fastcall contwrite_trackball(int port,u8 v)
{
	/* reset adders on first rising edge */
	if (v&4&tb_reset[port]) {
		tb_adder_x[port]=tb_adder_y[port]=0;
		tb_reset[port]=FALSE;
	}

	/* pin 8 flipped */
	if ((v^gen_output[port])&4) {
		/* latch to d0-2 + d7 and update adder x on fall, y on rise */
		s8* a=(v&4)?&tb_adder_y[port]:&tb_adder_x[port];
		s8 sb=*a; CLAMP(sb,-8,7); *a-=sb;

		tb_state[port]=(tb_state[port]&~0xf)|(sb+8);
	}
}

/* hypershot */
static u8 __fastcall contread_hypershot(int port)
{
	/* if output pin 8 is high and output pin 6 is low, the run button will only be detected if the jump button is pressed */
	if (gen_output[port]&4&&!(gen_output[port]&1)&&hs_state[port]&0x10) return 0x3f;
	else return hs_state[port];
}

/* magic key */
static u8 __fastcall contread_magickey(int port) { port = port; return 0x3c; }

/* arkanoid
 The hardware works with a 556 dual timer that's unemulated here, it requires short delays at each shift,
 and a long delay at latching. Too short delays will cause garbage reads, and too long delays at shifting
 will eventually cause the shift register bits to return to 0. */
static u8 __fastcall contread_arkanoid(int port)
{
	/* bit 0: dial position (serial read), bit 1: button */
	return (ark_shift[port]>>ark_bitcount[port]&1)|ark_button[port]|0x3c;
}
static void __fastcall contwrite_arkanoid(int port,u8 v)
{
	/* rising edge pin 6, shift */
	if (v&1&~gen_output[port]) ark_bitcount[port]-=(ark_bitcount[port]!=0);

	/* rising edge pin 8, latch */
	if (v&4&~gen_output[port]) {
		ark_bitcount[port]=8;
		ark_shift[port]=ark_shift_upd[port];
	}
}

/* NEW FRAME */
typedef void __fastcall (*_contnf)(int);
static _contnf contnf[2];
void __fastcall cont_new_frame(void) { contnf[0](0); contnf[1](1); }

/* nothing */
static void __fastcall contnf_nothing(int port) { port = port; }

/* mouse */
static void __fastcall contnf_mouse(int port)
{
	if (mouse_cc[port]<(2*crystal->frame)) mouse_cc[port]+=crystal->frame;
}

/* MISC */
/* hardware pause state */
/* Sony MSX1 pause key is officially documented to use Z80 bus request, and
then internally mute the sound. BiFi measured it on a tR, and it looks like
it's using BUSRQ too (not WAIT). I assume this is the same on MSX2+.
*/
void cont_mute_sonypause(signed short* stream,int len)
{
	if (!(stateextra&CONT_ESTATE_SONYPAUSE)) return;

	/* mute internal audio signal, eg. shouldn't mute moonsound (not that meisei supports the moonie) */
	while (len--) stream[len]=0;
}
void cont_set_sonypause(int p)
{
	if (z80_set_busrq(p)) /* halt/unhalt z80 */
		stateextra=(stateextra&~CONT_ESTATE_SONYPAUSE)|p;
}
void cont_reset_sonypause(void)
{
	sonypause_prev=0xff;
	cont_set_sonypause(FALSE);
}
void cont_check_sonypause(void)
{
	int prev=(keyextra&CONT_EKEY_SONYPAUSE)|(stateextra&CONT_ESTATE_SONYPAUSE)<<1;
	if (~sonypause_prev&keyextra&CONT_EKEY_SONYPAUSE) cont_set_sonypause(~sonypause_prev>>1&1); /* rising edge */
	sonypause_prev=prev;
}

/* INSERT */
void cont_insert(int port,int d,int init,int poweron)
{
	UINT id=IDM_PORT_FIRST+port*IDM_PORT_SIZE;

	if (!init&&cont_port[port]==d) return;
	cont_port[port]=d;
	if (poweron) cont_poweron_port(port);

	/* menu:
		Joystick			+0
		Mouse				+1
		Trackball			+2
		--------------
		Konami HyperShot	+3
		Sony Magic Key		+4
		Taito Arkanoid Pad	+5
		--------------
		Nothing				+6
	*/

	contupdate[port]=contupdate_nothing;
	contread[port]=contread_nothing;
	contwrite[port]=contwrite_nothing;
	contnf[port]=contnf_nothing;

	switch (d) {
		/* joystick */
		case CONT_D_J:
			contupdate[port]=contupdate_joystick;
			contread[port]=contread_joystick;
			id+=0;
			break;
		/* mouse */
		case CONT_D_M:
			contupdate[port]=contupdate_mouse;
			contread[port]=contread_mouse;
			contwrite[port]=contwrite_mouse;
			contnf[port]=contnf_mouse;
			id+=1;
			break;
		/* trackball */
		case CONT_D_T:
			contupdate[port]=contupdate_trackball;
			contread[port]=contread_trackball;
			contwrite[port]=contwrite_trackball;
			id+=2;
			break;

		/* hypershot */
		case CONT_D_HS:
			contupdate[port]=contupdate_hypershot;
			contread[port]=contread_hypershot;
			id+=3;
			break;
		/* magic key */
		case CONT_D_MAGIC:
			contread[port]=contread_magickey;
			id+=4;
			break;
		/* arkanoid */
		case CONT_D_ARK:
			contupdate[port]=contupdate_arkanoid;
			contread[port]=contread_arkanoid;
			contwrite[port]=contwrite_arkanoid;
			id+=5;
			break;

		/* nothing */
		default:
			id+=6;
			break;
	}

	main_menu_radio(id,IDM_PORT_FIRST+port*IDM_PORT_SIZE,(IDM_PORT_FIRST+port*IDM_PORT_SIZE)+IDM_PORT_SIZE-1);

	input_affirm_ports();
}

void cont_user_insert(UINT id)
{
	int port=0,device=0;

	if (netplay_is_active()||movie_get_active_state()) return;

	switch (id) {
		case IDM_PORT1_J:		port=0; device=CONT_D_J;		break;
		case IDM_PORT1_M:		port=0; device=CONT_D_M;		break;
		case IDM_PORT1_T:		port=0; device=CONT_D_T;		break;
		case IDM_PORT1_HS:		port=0; device=CONT_D_HS;		break;
		case IDM_PORT1_MAGIC:	port=0; device=CONT_D_MAGIC;	break;
		case IDM_PORT1_ARK:		port=0; device=CONT_D_ARK;		break;
		case IDM_PORT1_NO:		port=0; device=CONT_D_NOTHING;	break;

		case IDM_PORT2_J:		port=1; device=CONT_D_J;		break;
		case IDM_PORT2_M:		port=1; device=CONT_D_M;		break;
		case IDM_PORT2_T:		port=1; device=CONT_D_T;		break;
		case IDM_PORT2_HS:		port=1; device=CONT_D_HS;		break;
		case IDM_PORT2_MAGIC:	port=1; device=CONT_D_MAGIC;	break;
		case IDM_PORT2_ARK:		port=1; device=CONT_D_ARK;		break;
		case IDM_PORT2_NO:		port=1; device=CONT_D_NOTHING;	break;

		default: return;
	}

	msx_wait();
	cont_insert(port,device,FALSE,TRUE);
	msx_wait_done();
}


/* state				size
port 1					1
port 2					1

curport					1
keyrow					12
keyextra				1
stateextra				1
sonypause_prev			1

gen_output				1/1
joy_state				1/1
hs_state				1/1

ark_shift_upd			2/2
ark_shift				2/2
ark_button				1/1
ark_bitcount			1/1

mouse_reset				1/1
mouse_joystick			1/1
mouse_cc				4/4
mouse_shift				2/2
mouse_rot_x_upd			1/1
mouse_rot_y_upd			1/1
mouse_rot_x				1/1
mouse_rot_y				1/1
mouse_state				1/1

tb_reset				1/1
tb_adder_x				1/1
tb_adder_y				1/1
tb_adder_x_is			1/1
tb_adder_y_is			1/1
tb_state				1/1

==						74
*/
#define STATE_VERSION	3
/* version history:
1: initial
2: improved gen_output (was gen_strobe) + added hypershot + added keyextra/stateextra/sonypause + added torikeshi/jikkou keys
3: added mouse + added trackball
*/
#define STATE_SIZE		74

int __fastcall cont_state_get_version(void)
{
	return STATE_VERSION;
}

int __fastcall cont_state_get_size(void)
{
	return STATE_SIZE;
}

/* save */
void __fastcall cont_state_save(u8** s)
{
	/* internal */
	STATE_SAVE_1(cont_port[CONT_P1]);	STATE_SAVE_1(cont_port[CONT_P2]);
	STATE_SAVE_1(curport);

	STATE_SAVE_C(keyrow,12);
	STATE_SAVE_1(keyextra);				STATE_SAVE_1(stateextra);
	STATE_SAVE_1(sonypause_prev);

	/* external */
	STATE_SAVE_1(gen_output[0]);		STATE_SAVE_1(gen_output[1]);
	STATE_SAVE_1(joy_state[0]);			STATE_SAVE_1(joy_state[1]);
	STATE_SAVE_1(hs_state[0]);			STATE_SAVE_1(hs_state[1]);

	STATE_SAVE_2(ark_shift_upd[0]);		STATE_SAVE_2(ark_shift_upd[1]);
	STATE_SAVE_2(ark_shift[0]);			STATE_SAVE_2(ark_shift[1]);
	STATE_SAVE_1(ark_button[0]);		STATE_SAVE_1(ark_button[1]);
	STATE_SAVE_1(ark_bitcount[0]);		STATE_SAVE_1(ark_bitcount[1]);

	STATE_SAVE_1(mouse_reset[0]);		STATE_SAVE_1(mouse_reset[1]);
	STATE_SAVE_1(mouse_joystick[0]);	STATE_SAVE_1(mouse_joystick[1]);
	STATE_SAVE_4(mouse_cc[0]);			STATE_SAVE_4(mouse_cc[1]);
	STATE_SAVE_2(mouse_shift[0]);		STATE_SAVE_2(mouse_shift[1]);
	STATE_SAVE_1(mouse_rot_x_upd[0]);	STATE_SAVE_1(mouse_rot_x_upd[1]);
	STATE_SAVE_1(mouse_rot_y_upd[0]);	STATE_SAVE_1(mouse_rot_y_upd[1]);
	STATE_SAVE_1(mouse_rot_x[0]);		STATE_SAVE_1(mouse_rot_x[1]);
	STATE_SAVE_1(mouse_rot_y[0]);		STATE_SAVE_1(mouse_rot_y[1]);
	STATE_SAVE_1(mouse_state[0]);		STATE_SAVE_1(mouse_state[1]);

	STATE_SAVE_1(tb_reset[0]);			STATE_SAVE_1(tb_reset[1]);
	STATE_SAVE_1(tb_adder_x[0]);		STATE_SAVE_1(tb_adder_x[1]);
	STATE_SAVE_1(tb_adder_y[0]);		STATE_SAVE_1(tb_adder_y[1]);
	STATE_SAVE_1(tb_adder_x_is[0]);		STATE_SAVE_1(tb_adder_x_is[1]);
	STATE_SAVE_1(tb_adder_y_is[0]);		STATE_SAVE_1(tb_adder_y_is[1]);
	STATE_SAVE_1(tb_state[0]);			STATE_SAVE_1(tb_state[1]);
}

/* load */
void __fastcall cont_state_load_cur(u8** s)
{
	/* internal */
	STATE_LOAD_1(cont_port[CONT_P1]);	STATE_LOAD_1(cont_port[CONT_P2]);
	STATE_LOAD_1(curport);

	STATE_LOAD_C(keyrow,12);
	STATE_LOAD_1(keyextra);				STATE_LOAD_1(stateextra);
	STATE_LOAD_1(sonypause_prev);

	/* external */
	STATE_LOAD_1(gen_output[0]);		STATE_LOAD_1(gen_output[1]);
	STATE_LOAD_1(joy_state[0]);			STATE_LOAD_1(joy_state[1]);
	STATE_LOAD_1(hs_state[0]);			STATE_LOAD_1(hs_state[1]);

	STATE_LOAD_2(ark_shift_upd[0]);		STATE_LOAD_2(ark_shift_upd[1]);
	STATE_LOAD_2(ark_shift[0]);			STATE_LOAD_2(ark_shift[1]);
	STATE_LOAD_1(ark_button[0]);		STATE_LOAD_1(ark_button[1]);
	STATE_LOAD_1(ark_bitcount[0]);		STATE_LOAD_1(ark_bitcount[1]);

	STATE_LOAD_1(mouse_reset[0]);		STATE_LOAD_1(mouse_reset[1]);
	STATE_LOAD_1(mouse_joystick[0]);	STATE_LOAD_1(mouse_joystick[1]);
	STATE_LOAD_4(mouse_cc[0]);			STATE_LOAD_4(mouse_cc[1]);
	STATE_LOAD_2(mouse_shift[0]);		STATE_LOAD_2(mouse_shift[1]);
	STATE_LOAD_1(mouse_rot_x_upd[0]);	STATE_LOAD_1(mouse_rot_x_upd[1]);
	STATE_LOAD_1(mouse_rot_y_upd[0]);	STATE_LOAD_1(mouse_rot_y_upd[1]);
	STATE_LOAD_1(mouse_rot_x[0]);		STATE_LOAD_1(mouse_rot_x[1]);
	STATE_LOAD_1(mouse_rot_y[0]);		STATE_LOAD_1(mouse_rot_y[1]);
	STATE_LOAD_1(mouse_state[0]);		STATE_LOAD_1(mouse_state[1]);

	STATE_LOAD_1(tb_reset[0]);			STATE_LOAD_1(tb_reset[1]);
	STATE_LOAD_1(tb_adder_x[0]);		STATE_LOAD_1(tb_adder_x[1]);
	STATE_LOAD_1(tb_adder_y[0]);		STATE_LOAD_1(tb_adder_y[1]);
	STATE_LOAD_1(tb_adder_x_is[0]);		STATE_LOAD_1(tb_adder_x_is[1]);
	STATE_LOAD_1(tb_adder_y_is[0]);		STATE_LOAD_1(tb_adder_y_is[1]);
	STATE_LOAD_1(tb_state[0]);			STATE_LOAD_1(tb_state[1]);
}

void __fastcall cont_state_load_2(u8** s)
{
	/* version 2 */
	int i;

	/* internal */
	STATE_LOAD_1(cont_port[CONT_P1]);	STATE_LOAD_1(cont_port[CONT_P2]);
	STATE_LOAD_1(curport);

	STATE_LOAD_C(keyrow,12);
	STATE_LOAD_1(keyextra);				STATE_LOAD_1(stateextra);
	STATE_LOAD_1(sonypause_prev);

	/* external */
	STATE_LOAD_1(gen_output[0]);		STATE_LOAD_1(gen_output[1]);
	STATE_LOAD_1(joy_state[0]);			STATE_LOAD_1(joy_state[1]);
	STATE_LOAD_1(hs_state[0]);			STATE_LOAD_1(hs_state[1]);

	STATE_LOAD_2(ark_shift_upd[0]);		STATE_LOAD_2(ark_shift_upd[1]);
	STATE_LOAD_2(ark_shift[0]);			STATE_LOAD_2(ark_shift[1]);
	STATE_LOAD_1(ark_button[0]);		STATE_LOAD_1(ark_button[1]);
	STATE_LOAD_1(ark_bitcount[0]);		STATE_LOAD_1(ark_bitcount[1]);

	/* post-version */
	/* external */
	for (i=0;i<2;i++) {
		contpower_mouse(i); mouse_reset[i]=0;
		contpower_trackball(i);
	}
}

void __fastcall cont_state_load_1(u8** s)
{
	/* version 1 */
	int i;

	/* internal */
	STATE_LOAD_1(cont_port[CONT_P1]);	STATE_LOAD_1(cont_port[CONT_P2]);
	STATE_LOAD_1(curport);

	STATE_LOAD_C(keyrow,11);

	/* external */
	STATE_LOAD_1(gen_output[0]);		STATE_LOAD_1(gen_output[1]);
	STATE_LOAD_1(joy_state[0]);			STATE_LOAD_1(joy_state[1]);

	STATE_LOAD_2(ark_shift_upd[0]);		STATE_LOAD_2(ark_shift_upd[1]);
	STATE_LOAD_2(ark_shift[0]);			STATE_LOAD_2(ark_shift[1]);
	STATE_LOAD_1(ark_button[0]);		STATE_LOAD_1(ark_button[1]);
	STATE_LOAD_1(ark_bitcount[0]);		STATE_LOAD_1(ark_bitcount[1]);

	/* post-version */
	/* gen_output */
	gen_output[0]=(gen_output[0]&3)|(gen_output[0]>>2&4);
	gen_output[1]=(gen_output[1]>>2&3)|(gen_output[1]>>3&4);

	/* extra key/state */
	keyextra=stateextra=0;
	sonypause_prev=0xff;

	/* torikeshi/jikkou */
	keyrow[11]=0xff;

	/* external */
	for (i=0;i<2;i++) {
		contpower_mouse(i); mouse_reset[i]=0;
		contpower_trackball(i);
		contpower_hypershot(i);
	}
}

int __fastcall cont_state_load(int v,u8** s)
{
	switch (v) {
		case 1:
			cont_state_load_1(s);
			break;
		case 2:
			cont_state_load_2(s);
			break;

		case STATE_VERSION:
			cont_state_load_cur(s);
			break;

		default: return FALSE;
	}

	return TRUE;
}

/* movie	size

keyboard	12 (must)
keyextra	1  (must)
joystick	1+1
hypershot	1+1
arkanoid	2+2
mouse		3+3
trackball	3+3

MAX			19

*/
int __fastcall cont_movie_get_size(void)
{
	int port=2,size=13; /* keyboard+extra */

	/* ports */
	while (port--) {
		switch (cont_port[port]) {
			case CONT_D_J: case CONT_D_HS:	size+=1; break;
			case CONT_D_ARK:				size+=2; break;
			case CONT_D_M: case CONT_D_T:	size+=3; break;

			default: break;
		}
	}

	return size;
}

void __fastcall cont_movie_put(u8* c)
{
	int port=2,i=12; /* keyboard size */

	/* keyboard */
	memcpy(keyrow,c,12);
	keyextra=c[i++];

	/* ports */
	while (port--) {
		switch (cont_port[port]) {
			case CONT_D_J:
				joy_state[port]=c[i++]; break;
			case CONT_D_HS:
				hs_state[port]=c[i++]; break;

			case CONT_D_ARK:
				ark_button[port]=c[i]&2;
				ark_shift_upd[port]=(c[i]<<8|c[i+1])&0x1ff;
				i+=2; break;

			case CONT_D_M:
				mouse_rot_x_upd[port]=c[i++];
				mouse_rot_y_upd[port]=c[i++];
				mouse_state[port]=c[i]&0x3f;
				mouse_joystick[port]=c[i]>>6&1;
				i++; break;

			case CONT_D_T:
				tb_adder_x_is[port]=tb_adder_x[port]=c[i++];
				tb_adder_y_is[port]=tb_adder_y[port]=c[i++];
				tb_state[port]=c[i++];
				break;

			default: break;
		}
	}
}

void __fastcall cont_movie_get(u8* c)
{
	int port=2,i=12; /* keyboard size */

	/* keyboard */
	memcpy(c,keyrow,12);
	c[i++]=keyextra;

	/* ports */
	while (port--) {
		switch (cont_port[port]) {
			case CONT_D_J:
				c[i++]=joy_state[port]; break;
			case CONT_D_HS:
				c[i++]=hs_state[port]; break;

			case CONT_D_ARK:
				c[i++]=ark_button[port]|(ark_shift_upd[port]>>8&1);
				c[i++]=ark_shift_upd[port]&0xff;
				break;

			case CONT_D_M:
				c[i++]=mouse_rot_x_upd[port];
				c[i++]=mouse_rot_y_upd[port];
				c[i++]=mouse_state[port]|mouse_joystick[port]<<6;
				break;

			case CONT_D_T:
				c[i++]=tb_adder_x_is[port];
				c[i++]=tb_adder_y_is[port];
				c[i++]=tb_state[port];
				break;

			default: break;
		}
	}
}
