/******************************************************************************
 *                                                                            *
 *                                "input.c"                                   *
 *                                                                            *
 ******************************************************************************/

#define DIRECTINPUT_VERSION 0x500
#include <dinput.h>

#include "global.h"
#include "main.h"
#include "resource.h"
#include "input.h"
#include "msx.h"
#include "cont.h"
#include "z80.h"
#include "crystal.h"
#include "draw.h"
#include "sound.h"
#include "settings.h"
#include "version.h"
#include "netplay.h"


#define INPUT_T_MAX				(CONT_T_MAX+INPUT_T_END)
#define INPUT_A_MAX				(CONT_A_MAX+INPUT_A_END)
#define INPUT_DA_EM				CONT_DA_MAX

/* flags */
#define _FT(x) (input.num_triggers-(INPUT_MAX_ABOVE-x))
#define _FA(x) (input.num_axes-(INPUT_MAX_ABOVE-x))
#define INPUT_TT_FOCUS			_FT(0)
#define INPUT_TA_FOCUS			_FA(0)
#define INPUT_TT_ACTIVE			_FT(1)
#define INPUT_TA_ACTIVE			_FA(1)
#define INPUT_TT_NOTHING		_FT(2)
#define INPUT_TA_NOTHING		_FA(2)
#define INPUT_TT_CAPSLED		_FT(3)
#define INPUT_TA_CAPSLED		_FA(3)
#define INPUT_TT_NUMLED			_FT(4)
#define INPUT_TA_NUMLED			_FA(4)
#define INPUT_TT_SCROLLLED		_FT(5)
#define INPUT_TA_SCROLLLED		_FA(5)
#define INPUT_TT_MOUSEGRAB		_FT(6)
#define INPUT_TA_MOUSEGRAB		_FA(6)
/* 7 is empty */
#define INPUT_MAX_ABOVE			8

#define T_KEYBOARD_SIZE			256	/* number of keyboard triggers */
#define A_KEYBOARD_SIZE			0	/* axes, etc */
#define T_MOUSE_SIZE			10
#define A_MOUSE_SIZE			3
#define T_JOYSTICK_SIZE			64
#define A_JOYSTICK_SIZE			8

#define T_KEYBOARD_A_ALT		(T_KEYBOARD_SIZE)
#define T_KEYBOARD_A_CTRL		(T_KEYBOARD_SIZE+1)
#define T_KEYBOARD_A_SHIFT		(T_KEYBOARD_SIZE+2)
#define T_KEYBOARD_A_ENTER		(T_KEYBOARD_SIZE+3)
#define T_KEYBOARD_A_CIRCUMFLEX	(T_KEYBOARD_SIZE+4)
#define T_KEYBOARD_SIZE_TOTAL	(T_KEYBOARD_SIZE+5)

static struct {
	LPDIRECTINPUT dinput;
	int semaphore;
	int tick;
	int background;
	int menukey;
	int skey_set[CONT_DA_MAX+2]; /* -2:emulator shortcuts, -1: empty */

	int* trigger_rule[INPUT_T_MAX];
	int* trigger; /* $80 or 0 */
	int* trigger_old;
	char** trigger_info;
	int num_triggers;

	int axis_rule[INPUT_A_MAX];
	long* axis;
	long* axis_old;
	char** axis_info;
	int num_axes;

	/* keyboard */
	int num_keyboards;
	int leds[3];

	/* mouse */
	int num_mouses; /* .. I mean mice */
	DWORD mouse_cursor_ticks;
	int mouse_cursor_dc;
	int mouse_cursor_visible;
	int mouse_cursor_grabbed;
	int doubleclick;
	DWORD doubleclick_ticks;
	float mouse_sensitivity;

	/* joystick */
	int num_joysticks;
	int joystick_isenabled;
	float analog_sensitivity;

	/* shortcuts */
	int* emshort;
	int* emshort_old;
	DWORD cpuspeed_ticks;
	DWORD cpuspeed_ticks_next;

} input;

typedef struct input_device {
	LPDIRECTINPUTDEVICE pd;
	char* name;
	struct input_device* next;
	struct input_device* prev;
} input_device;

static input_device* input_device_keyboard_cursor;
static input_device* input_device_mouse_cursor;
static input_device* input_device_joystick_cursor;
static input_device* input_device_keyboard_begin;
static input_device* input_device_mouse_begin;
static input_device* input_device_joystick_begin;


static void enumerate_devices(void);
static void input_fill_info(void);

static BOOL CALLBACK keyboard_create(LPCDIDEVICEINSTANCE,LPVOID);
static void keyboard_clean(void);
static void keyboard_update(void);

static BOOL CALLBACK mouse_create(LPCDIDEVICEINSTANCE,LPVOID);
static void mouse_clean(void);
static void mouse_update(void);

static BOOL CALLBACK joystick_create(LPCDIDEVICEINSTANCE,LPVOID);
static void joystick_clean(void);
static void joystick_update(void);

int input_get_joystick_analog_max(void) { return 0x80*input.analog_sensitivity+0.5; }

/* trigger/axis info */

static void info_string(int type,int n,const char* c)
{
	const int sc=strlen(c)+1;
	const int trigger_keyboard_size=T_KEYBOARD_SIZE_TOTAL;
	const int trigger_keyboard_start=0;
	const int trigger_mouse_size=T_MOUSE_SIZE;
	const int trigger_mouse_start=input.num_keyboards*trigger_keyboard_size;
	const int axis_mouse_size=A_MOUSE_SIZE;
	const int axis_mouse_start=0;
	const int trigger_joystick_size=T_JOYSTICK_SIZE;
	const int trigger_joystick_start=trigger_mouse_start+(input.num_mouses*trigger_mouse_size);
	const int axis_joystick_size=A_JOYSTICK_SIZE;
	const int axis_joystick_start=input.num_mouses*axis_mouse_size;
	int i;

	/* create info string */
	#define INFO_CREATE(dev,str,ta)														\
		MEM_CREATE(input.ta##_info[n+ta##_##dev##_start+(i*ta##_##dev##_size)],4+sc);	\
		sprintf(input.ta##_info[n+ta##_##dev##_start+(i*ta##_##dev##_size)],"%s%d %s",str,i+1,c)
	#define INFO_LOOP(dev,str,ta)														\
		i=input.num_##dev##s;															\
		while (i--) { INFO_CREATE(dev,str,ta); }										\
		break

	switch (type) {
		/* keyboard_trigger_string */
		case 0:
			i=input.num_keyboards;
			while (i--) {
				if (input.trigger_info[n+(i*trigger_keyboard_size)]==NULL) {
					if (i) { INFO_CREATE(keyboard,"K",trigger); }
					else {
						MEM_CREATE(input.trigger_info[n],sc);
						sprintf(input.trigger_info[n],"%s",c);
					}
				}
			}
			break;

		case 1: INFO_LOOP(mouse,"M",trigger); /* mouse_trigger_string */
		case 2: INFO_LOOP(mouse,"M",axis); /* mouse_axis_string */
		case 3: INFO_LOOP(joystick,"J",trigger); /* joystick_trigger_string */
		case 4: INFO_LOOP(joystick,"J",axis); /* joystick_axis_string */

		default: break;
	}
}

/* keyboard */
static void keyboard_fill_info(void)
{
	int ku;
	char kc[12];

	/* std */
	info_string(0,DIK_A,			"A");
	info_string(0,DIK_B,			"B");
	info_string(0,DIK_C,			"C");
	info_string(0,DIK_D,			"D");
	info_string(0,DIK_E,			"E");
	info_string(0,DIK_F,			"F");
	info_string(0,DIK_G,			"G");
	info_string(0,DIK_H,			"H");
	info_string(0,DIK_I,			"I");
	info_string(0,DIK_J,			"J");
	info_string(0,DIK_K,			"K");
	info_string(0,DIK_L,			"L");
	info_string(0,DIK_M,			"M");
	info_string(0,DIK_N,			"N");
	info_string(0,DIK_O,			"O");
	info_string(0,DIK_P,			"P");
	info_string(0,DIK_Q,			"Q");
	info_string(0,DIK_R,			"R");
	info_string(0,DIK_S,			"S");
	info_string(0,DIK_T,			"T");
	info_string(0,DIK_U,			"U");
	info_string(0,DIK_V,			"V");
	info_string(0,DIK_W,			"W");
	info_string(0,DIK_X,			"X");
	info_string(0,DIK_Y,			"Y");
	info_string(0,DIK_Z,			"Z");

	info_string(0,DIK_0,			"0");
	info_string(0,DIK_1,			"1");
	info_string(0,DIK_2,			"2");
	info_string(0,DIK_3,			"3");
	info_string(0,DIK_4,			"4");
	info_string(0,DIK_5,			"5");
	info_string(0,DIK_6,			"6");
	info_string(0,DIK_7,			"7");
	info_string(0,DIK_8,			"8");
	info_string(0,DIK_9,			"9");

	info_string(0,DIK_ESCAPE,		"Escape");
	info_string(0,DIK_F1,			"F1");
	info_string(0,DIK_F2,			"F2");
	info_string(0,DIK_F3,			"F3");
	info_string(0,DIK_F4,			"F4");
	info_string(0,DIK_F5,			"F5");
	info_string(0,DIK_F6,			"F6");
	info_string(0,DIK_F7,			"F7");
	info_string(0,DIK_F8,			"F8");
	info_string(0,DIK_F9,			"F9");
	info_string(0,DIK_F10,			"F10");
	info_string(0,DIK_F11,			"F11");
	info_string(0,DIK_F12,			"F12");

	info_string(0,DIK_GRAVE,		"`");
	info_string(0,DIK_MINUS,		"-");
	info_string(0,DIK_EQUALS,		"=");
	info_string(0,DIK_BACK,			"Backspace");

	info_string(0,DIK_TAB,			"Tab");
	info_string(0,DIK_LBRACKET,		"[");
	info_string(0,DIK_RBRACKET,		"]");
	info_string(0,DIK_BACKSLASH,	"\\");

	info_string(0,DIK_CAPITAL,		"Caps Lock");
	info_string(0,DIK_SEMICOLON,	";");
	info_string(0,DIK_APOSTROPHE,	"'");
	info_string(0,DIK_RETURN,		"Main Enter");

	info_string(0,DIK_LSHIFT,		"lShift");
	info_string(0,DIK_COMMA,		",");
	info_string(0,DIK_PERIOD,		".");
	info_string(0,DIK_SLASH,		"/");
	info_string(0,DIK_RSHIFT,		"rShift");

	info_string(0,DIK_LCONTROL,		"lCtrl");
	info_string(0,DIK_LWIN,			"lWindows");
	info_string(0,DIK_LMENU,		"lAlt");
	info_string(0,DIK_SPACE,		"Space");
	info_string(0,DIK_RMENU,		"rAlt");
	info_string(0,DIK_RWIN,			"rWindows");
	info_string(0,DIK_APPS,			"AppMenu");
	info_string(0,DIK_RCONTROL,		"rCtrl");

	info_string(0,DIK_SYSRQ,		"PrtScr"); /* Print Screen */
	info_string(0,DIK_SCROLL,		"Scroll Lock");
	info_string(0,DIK_PAUSE,		"Pause"); /* single pulse, can't seem to get held state */

	info_string(0,DIK_INSERT,		"Insert");
	info_string(0,DIK_DELETE,		"Delete");
	info_string(0,DIK_HOME,			"Home");
	info_string(0,DIK_END,			"End");
	info_string(0,DIK_PRIOR,		"Page Up");
	info_string(0,DIK_NEXT,			"Page Down");

	info_string(0,DIK_UP,			"Up");
	info_string(0,DIK_DOWN,			"Down");
	info_string(0,DIK_LEFT,			"Left");
	info_string(0,DIK_RIGHT,		"Right");

	info_string(0,DIK_NUMPAD0,		"Numpad 0");
	info_string(0,DIK_NUMPAD1,		"Numpad 1");
	info_string(0,DIK_NUMPAD2,		"Numpad 2");
	info_string(0,DIK_NUMPAD3,		"Numpad 3");
	info_string(0,DIK_NUMPAD4,		"Numpad 4");
	info_string(0,DIK_NUMPAD5,		"Numpad 5");
	info_string(0,DIK_NUMPAD6,		"Numpad 6");
	info_string(0,DIK_NUMPAD7,		"Numpad 7");
	info_string(0,DIK_NUMPAD8,		"Numpad 8");
	info_string(0,DIK_NUMPAD9,		"Numpad 9");

	info_string(0,DIK_NUMLOCK,		"Num Lock");
	info_string(0,DIK_DIVIDE,		"Numpad /");
	info_string(0,DIK_MULTIPLY,		"Numpad *");
	info_string(0,DIK_SUBTRACT,		"Numpad -");
	info_string(0,DIK_ADD,			"Numpad +");
	info_string(0,DIK_DECIMAL,		"Numpad .");
	info_string(0,DIK_NUMPADENTER,	"Numpad Enter");

	info_string(0,DIK_POWER,		"Power");
	info_string(0,DIK_SLEEP,		"Sleep");
	info_string(0,DIK_WAKE,			"Wake");

	/* non-standard keyboards */
	info_string(0,DIK_OEM_102,		"<"); /* or \ */
	info_string(0,DIK_ABNT_C1,		"/ [Brazil]");
	info_string(0,DIK_ABNT_C2,		"Numpad . [Brazil]");
	info_string(0,DIK_F13,			"F13");
	info_string(0,DIK_F14,			"F14");
	info_string(0,DIK_F15,			"F15");
	info_string(0,DIK_YEN,			"Yen");
	info_string(0,DIK_AT,			"@");
	info_string(0,DIK_COLON,		":");
	info_string(0,DIK_UNDERLINE,	"_");
	info_string(0,DIK_STOP,			"Stop");
	info_string(0,DIK_AX,			"AX");
	info_string(0,DIK_UNLABELED,	"J3100");
	info_string(0,DIK_KANA,			"Kana");
	info_string(0,DIK_KANJI,		"Kanji");
	info_string(0,DIK_CONVERT,		"Convert");
	info_string(0,DIK_NOCONVERT,	"No Convert");
	info_string(0,DIK_NUMPADEQUALS,	"Numpad =");
	info_string(0,DIK_NUMPADCOMMA,	"Numpad ,");

	/* multimedia (Windows) keys */
	info_string(0,DIK_MYCOMPUTER,	"My Computer");
	info_string(0,DIK_MAIL,			"Mail");
	info_string(0,DIK_CALCULATOR,	"Calculator");
	info_string(0,DIK_PREVTRACK,	"Previous Track"); /* or ^ on Japanese keyboards; DIK_CIRCUMFLEX (as double underneath) */
	info_string(0,DIK_NEXTTRACK,	"Next Track");
	info_string(0,DIK_MEDIASELECT,	"Media Select");
	info_string(0,DIK_MEDIASTOP,	"Media Stop");
	info_string(0,DIK_PLAYPAUSE,	"Play");
	info_string(0,DIK_MUTE,			"Mute");
	info_string(0,DIK_VOLUMEDOWN,	"Volume -");
	info_string(0,DIK_VOLUMEUP,		"Volume +");
	info_string(0,DIK_WEBHOME,		"Web Home");
	info_string(0,DIK_WEBSTOP,		"Web Stop");
	info_string(0,DIK_WEBSEARCH,	"Search");
	info_string(0,DIK_WEBFAVORITES,	"Favourites");
	info_string(0,DIK_WEBBACK,		"Back");
	info_string(0,DIK_WEBFORWARD,	"Forward");
	info_string(0,DIK_WEBREFRESH,	"Refresh");

	/* custom double keys */
	info_string(0,T_KEYBOARD_A_ALT,	"Alt");
	info_string(0,T_KEYBOARD_A_CTRL,"Ctrl");
	info_string(0,T_KEYBOARD_A_SHIFT,"Shift");
	info_string(0,T_KEYBOARD_A_ENTER,"Enter");
	info_string(0,T_KEYBOARD_A_CIRCUMFLEX,"^");

	/* the rest is unknown */
	for (ku=0;ku<T_KEYBOARD_SIZE;ku++) {
		sprintf(kc,"Unknown $%02X",ku); /* 12 */
		info_string(0,ku,kc);
	}
}

/* mouse */
static void mouse_fill_info(void)
{
	int i,j;
	char c[3];

	/* triggers */
	i=0;
	info_string(1,i++,	"X-");
	info_string(1,i++,	"X+");
	info_string(1,i++,	"Y-");
	info_string(1,i++,	"Y+");
	info_string(1,i++,	"Z-");
	info_string(1,i++,	"Z+");

	for (j=1;j<5;j++) {
		sprintf(c,"B%d",j); /* 3 */
		info_string(1,i++,c);
	}

	/* axes */
	i=0;
	info_string(2,i++,	"X");
	info_string(2,i++,	"Y");
	info_string(2,i++,	"Z");
}

/* joystick */
static void joystick_fill_info(void)
{
	int i,j;
	char c[8];

	/* triggers */
	i=0;
	info_string(3,i++,	"X-");
	info_string(3,i++,	"X+");
	info_string(3,i++,	"Y-");
	info_string(3,i++,	"Y+");
	info_string(3,i++,	"Z-");
	info_string(3,i++,	"Z+");
	info_string(3,i++,	"RX-");
	info_string(3,i++,	"RX+");
	info_string(3,i++,	"RY-");
	info_string(3,i++,	"RY+");
	info_string(3,i++,	"RZ-");
	info_string(3,i++,	"RZ+");
	info_string(3,i++,	"U-");
	info_string(3,i++,	"U+");
	info_string(3,i++,	"V-");
	info_string(3,i++,	"V+");

	for (j=1;j<5;j++) {
		sprintf(c,"POV%d X-",j); info_string(3,i++,c); /* 8 */
		sprintf(c,"POV%d X+",j); info_string(3,i++,c);
		sprintf(c,"POV%d Y-",j); info_string(3,i++,c);
		sprintf(c,"POV%d Y+",j); info_string(3,i++,c);
	}

	for (j=1;j<33;j++) {
		sprintf(c,"B%d",j);
		info_string(3,i++,c);
	}

	/* axes */
	i=0;
	info_string(4,i++,	"X");
	info_string(4,i++,	"Y");
	info_string(4,i++,	"Z");
	info_string(4,i++,	"RX");
	info_string(4,i++,	"RY");
	info_string(4,i++,	"RZ");
	info_string(4,i++,	"U");
	info_string(4,i++,	"V");
}

static void input_fill_info(void)
{
	keyboard_fill_info();
	mouse_fill_info();
	joystick_fill_info();

	/* flags */
	#define F(t,a,s,l)																\
		MEM_CREATE(input.trigger_info[t],l);	sprintf(input.trigger_info[t],s);	\
		MEM_CREATE(input.axis_info[a],l);		sprintf(input.axis_info[a],s)

	F(INPUT_TT_FOCUS,		INPUT_TA_FOCUS,		"ISFOCUS",8);
	F(INPUT_TT_ACTIVE,		INPUT_TA_ACTIVE,	"ISACTIVE",9);
	F(INPUT_TT_NOTHING,		INPUT_TA_NOTHING,	"nothing",8);

	F(INPUT_TT_CAPSLED,		INPUT_TA_CAPSLED,	"CAPS_LED",9);
	F(INPUT_TT_NUMLED,		INPUT_TA_NUMLED,	"NUM_LED",8);
	F(INPUT_TT_SCROLLLED,	INPUT_TA_SCROLLLED,	"SCROLL_LED",11);

	F(INPUT_TT_MOUSEGRAB,	INPUT_TA_MOUSEGRAB,	"ISGRABBED",10);

	#undef F
}

/* trigger rules, same numbers as in cont.h/c */

/* shortcut names */
static const char* input_t_info[INPUT_T_END]={
	"hard reset",
	"soft reset",
	"media",
	"rewind tape",
	"grab mouse pointer",
	"exit",
	"full screen",
	"save screenshot",
	"pause",
	"switch video format",
	"paste text",
	"paste boot command",
	"select previous slot",
	"select next slot",
	"save state",
	"load state",
	"reverse  ",			/* spaces to prevent conflict with "reverse buffer size" */
	"continue",
	"speed up  ",			/* prevent conflict: "speed up percentage" */
	"slowdown  ",			/* prevent conflict: "slowdown percentage" */
	"DJ scratch hold",
	"decrease Z80 clock",
	"increase Z80 clock",
	"Z80 tracer",
	"help",
	"disable shortcuts"
};

const char* input_get_trigger_info(u32 i) { if (i>=INPUT_T_END) return NULL; else return input_t_info[i]; }

/* default */
static const char* input_t_default[INPUT_T_MAX]={
	/* joystick 1 */
	"J1 Y- | SCROLL_LED & Up",
	"J1 Y+ | SCROLL_LED & Down & !Up",
	"J1 X- | SCROLL_LED & Left",
	"J1 X+ | SCROLL_LED & Right & !Left",
	"J1 B1 | !lShift & SCROLL_LED & Z | lShift & SCROLL_LED & rapid(6,Z)",
	"J1 B2 | !lShift & SCROLL_LED & X | lShift & SCROLL_LED & rapid(6,X)",
	/* joystick 2 */
	"J2 Y-",
	"J2 Y+",
	"J2 X-",
	"J2 X+",
	"J2 B1",
	"J2 B2",

	/* mouse 1 */
	"M1 B1 | M1 B3",		/* left button */
	"M1 B2 | M1 B3",		/* right button */
	/* mouse 2 */
	"M1 B1 | M1 B3",
	"M1 B2 | M1 B3",

	/* trackball 1 */
	"M1 B1 | M1 B3",		/* lower button */
	"M1 B2 | M1 B3",		/* upper button */
	/* trackball 2 */
	"M1 B1 | M1 B3",
	"M1 B2 | M1 B3",

	/* hypershot 1 */
	"J1 B1 | SCROLL_LED & Z",
	"J1 B2 | !lShift & SCROLL_LED & X | lShift & SCROLL_LED & rapid(4,X)",
	/* hypershot 2 */
	"J2 B1",
	"J2 B2",

	/* arkanoid 1 */
	"M1 B1 | M1 B2",
	/* arkanoid 2 */
	"M1 B1 | M1 B2",

	/* keyboard */
	"0",
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"9",

	"A",
	"B",
	"C",
	"D",
	"E",
	"F",
	"G",
	"H",
	"I",
	"J",
	"K",
	"L",
	"M",
	"N",
	"O",
	"P",
	"Q",
	"R",
	"S",
	"T",
	"U",
	"V",
	"W",
	"X & !SCROLL_LED",
	"Y",
	"Z & !SCROLL_LED",

	"F1",
	"F2",
	"F3",
	"F4",
	"F5",

	"`",
	"-",
	"= | ^",
	"[ | @",
	"]",
	"\\ | Yen",
	";",
	"' | :",
	",",
	".",
	"/",
	"Page Down | _",		/* accent mark/_ */

	"Space",
	"Tab",
	"Enter",
	"Home",
	"Insert",
	"Delete",
	"Backspace",
	"Escape",
	"Page Up | Stop",		/* stop */
	"End",					/* select */

	"Up & !SCROLL_LED",
	"Down & !SCROLL_LED",
	"Left & !SCROLL_LED",
	"Right & !SCROLL_LED",

	"rShift | lShift & !SCROLL_LED",
	"Caps Lock",
	"Ctrl",
	"lAlt",					/* graph */
	"rAlt | Kana",			/* code */

	"Numpad 0",
	"Numpad 1",
	"Numpad 2",
	"Numpad 3",
	"Numpad 4",
	"Numpad 5",
	"Numpad 6",
	"Numpad 7",
	"Numpad 8",
	"Numpad 9",
	"Num Lock | Numpad ,",
	"Numpad .",
	"Numpad +",
	"Numpad -",
	"Numpad *",
	"Numpad /",

	"lWindows",				/* torikeshi (cancellation) */
	"rWindows",				/* jikkou (execution) */

	/* extra keyboard keys */
	"Shift & Ctrl & P",		/* sony pause */

	/* emulator shortcuts */
	"Ctrl & R",				/* hard reset */
	"Shift & Ctrl & R",		/* soft reset (no conflict with default hard reset) */
	"Ctrl & M",				/* media */
	"Ctrl & W",				/* rewind tape */
	"Shift & Ctrl & M",		/* grab mouse pointer (no conflict with default media) */
	"Alt & F4",				/* exit, though alt+f4 is standard */
	"Alt & Enter | dblclick() & !ISGRABBED", /* full screen */
	"Ctrl & PrtScr",		/* save screenshot */
	"Pause | !Shift & Ctrl & P | !ISACTIVE", /* pause */
	"Ctrl & F",				/* switch video format */
	"Ctrl & V",				/* paste text */
	"Shift & Ctrl & V",		/* paste boot command (no conflict with default paste text) */
	"F6",					/* prev slot */
	"F7",					/* next slot */
	"F8",					/* save state */
	"F9",					/* load state */
	"F11",					/* reverse */
	"nothing",				/* continue */
	"F12",					/* speed up */
	"F10",					/* slowdown */
	"F10 & F12",			/* DJ hold */
	"Ctrl & -",				/* decrease Z80 clock */
	"Ctrl & = | Ctrl & ^",	/* (+) increase Z80 clock */

	/* z80 tracer */
#if Z80_TRACER
	"Tab",
#else
	"nothing",
#endif

	"Ctrl & F1",			/* help */
	"AppMenu | !ISFOCUS"	/* disable shortcuts */
};
/* set */
static char* input_t_set[INPUT_T_MAX];
const char* input_get_trigger_set(u32 i) { if (i>=INPUT_T_MAX) i=0; return input_t_set[i]; }

/* axis */
/* shortcut names */
static const char* input_a_info[INPUT_A_END]={
	"DJ scratch spin"
};

const char* input_get_axis_info(u32 i) { if (i>=INPUT_A_END) return NULL; else return input_a_info[i]; }

static const char* input_a_default[INPUT_A_MAX]={
	/* mouse 1 */
	"M1 X",
	"M1 Y",
	/* mouse 2 */
	"M1 X",
	"M1 Y",

	/* trackball 1 */
	"M1 X",
	"M1 Y",
	/* trackball 2 */
	"M1 X",
	"M1 Y",

	/* arkanoid 1 */
	"M1 X",					/* dial */
	/* arkanoid 2 */
	"M1 X",

	/* emulator shortcuts */
	"invert(M1 Y)"			/* DJ spin axis */
};
static char* input_a_set[INPUT_A_MAX];
const char* input_get_axis_set(u32 i) { if (i>=INPUT_A_MAX) i=0; return input_a_set[i]; }


/* init */
/* std */

static void input_menucaption(int flags,int idm,int idi)
{
	/* flags:
		1=addition
	*/

	int i,j;
	char c[STRING_SIZE]={0};
	idi+=(INPUT_T_MAX-INPUT_T_END);

	main_menu_caption_get(idm,c);
	i=strlen(c);

	if (stricmp(input_t_set[idi],"nothing")!=0) {

		if (flags&1) c[i++]='-';
		else c[i++]='\t';

		for (j=0;(size_t)j<strlen(input_t_set[idi]);j++) {
			/* cap at | */
			if (input_t_set[idi][j]=='|') {
				if (i&&c[i-1]==' ') c[i-1]=0;
				break;
			}

			/* convert & to + (or menu will interpret it as shortcut) */
			if (input_t_set[idi][j]=='&') {
				if (i&&c[i-1]==' ') c[i-1]='+';
				else c[i++]='+';
				if (input_t_set[idi][j+1]==' ') j++;
			}

			else c[i++]=input_t_set[idi][j];
		}

		/* optional truncate */
		if (strlen(c)>64) { c[61]=c[62]=c[63]='.'; c[64]='\0'; }
	}

	main_menu_caption_put(idm,c);
}

void input_init(void)
{
	int i;
	float f;

	memset(&input,0,sizeof(input));
	input_device_keyboard_cursor=input_device_mouse_cursor=input_device_joystick_cursor=input_device_keyboard_begin=input_device_mouse_begin=input_device_joystick_begin=NULL;
	input.mouse_cursor_visible=TRUE;

	/* std settings */
	input.joystick_isenabled=TRUE; i=settings_get_yesnoauto(SETTINGS_JOYSTICK); if (i==FALSE||i==TRUE) { input.joystick_isenabled=i; }
	f=0.1; if (SETTINGS_GET_FLOAT(settings_info(SETTINGS_ANALOGSENSE),&f)) { CLAMP(f,0.01,1.0); } input.analog_sensitivity=f;
	f=0.5; if (SETTINGS_GET_FLOAT(settings_info(SETTINGS_MOUSESENSE),&f)) { CLAMP(f,0.01,1.0); } input.mouse_sensitivity=f;
	input.background=FALSE; i=settings_get_yesnoauto(SETTINGS_INPBG); if (i==FALSE||i==TRUE) { input.background=i; }

	/* create dinput */
	if ((DirectInputCreate(MAIN->module,DIRECTINPUT_VERSION,&input.dinput,NULL))!=DI_OK) {
		LOG(LOG_MISC|LOG_ERROR,"Can't create DirectInput!\n");
		input_clean(); exit(1);
	}

	/* enumerate devices */
	enumerate_devices();

	input.num_triggers=(input.num_keyboards*T_KEYBOARD_SIZE_TOTAL)+(input.num_mouses*T_MOUSE_SIZE)+(input.num_joysticks*T_JOYSTICK_SIZE)+INPUT_MAX_ABOVE;
	if (input.num_triggers>0x7fff) { LOG(LOG_MISC|LOG_ERROR,"Input trigger overflow!\n"); input_clean(); exit(1); }
	MEM_CREATE(input.trigger,input.num_triggers*sizeof(int));
	MEM_CREATE(input.trigger_old,input.num_triggers*sizeof(int));
	MEM_CREATE(input.trigger_info,input.num_triggers*sizeof(char*));

	input.num_axes=(input.num_keyboards*A_KEYBOARD_SIZE)+(input.num_mouses*A_MOUSE_SIZE)+(input.num_joysticks*A_JOYSTICK_SIZE)+INPUT_MAX_ABOVE;
	MEM_CREATE(input.axis,input.num_axes*sizeof(long));
	MEM_CREATE(input.axis_old,input.num_axes*sizeof(long));
	MEM_CREATE(input.axis_info,input.num_axes*sizeof(char*));

	MEM_CREATE(input.emshort,INPUT_T_END*sizeof(int));
	MEM_CREATE(input.emshort_old,INPUT_T_END*sizeof(int));

	input_fill_info();

	/* map trigger settings */
	#define IGET_MAP(x,y,z)															\
	input.skey_set[x]=0;															\
	for (i=y;i<z;i++) {																\
		input_t_set[i]=NULL;														\
		SETTINGS_GET_STRING(cont_get_trigger_info(i),&input_t_set[i]);				\
		if (!input_set_trigger(i,input_t_set[i],x)) {								\
			/* set setting to default */											\
			MEM_CLEAN(input_t_set[i]);												\
			MEM_CREATE(input_t_set[i],strlen(input_t_default[i])+1);				\
			memcpy(input_t_set[i],input_t_default[i],strlen(input_t_default[i]));	\
		}																			\
	}

	IGET_MAP(CONT_DA_J1,CONT_T_J1_BEGIN,CONT_T_J1_END)
	IGET_MAP(CONT_DA_J2,CONT_T_J2_BEGIN,CONT_T_J2_END)
	IGET_MAP(CONT_DA_M1,CONT_T_J1_BEGIN,CONT_T_M1_END)
	IGET_MAP(CONT_DA_M2,CONT_T_J2_BEGIN,CONT_T_M2_END)
	IGET_MAP(CONT_DA_T1,CONT_T_J1_BEGIN,CONT_T_T1_END)
	IGET_MAP(CONT_DA_T2,CONT_T_J2_BEGIN,CONT_T_T2_END)
	IGET_MAP(CONT_DA_HS1,CONT_T_HS1_BEGIN,CONT_T_HS1_END)
	IGET_MAP(CONT_DA_HS2,CONT_T_HS2_BEGIN,CONT_T_HS2_END)
	IGET_MAP(CONT_DA_ARK1,CONT_T_ARK1_BEGIN,CONT_T_ARK1_END)
	IGET_MAP(CONT_DA_ARK2,CONT_T_ARK2_BEGIN,CONT_T_ARK2_END)
	IGET_MAP(CONT_DA_KEY,CONT_T_K_BEGIN,CONT_T_K_END)
	IGET_MAP(INPUT_DA_EM,(INPUT_T_MAX-INPUT_T_END),INPUT_T_MAX)

	/* axis */
	#define IGETA_MAP(y,z)															\
	for (i=y;i<z;i++) {																\
		input_a_set[i]=NULL;														\
		SETTINGS_GET_STRING(cont_get_axis_info(i),&input_a_set[i]);					\
		if (!input_set_axis(i,input_a_set[i])) {									\
			/* set setting to default */											\
			MEM_CLEAN(input_a_set[i]);												\
			MEM_CREATE(input_a_set[i],strlen(input_a_default[i])+1);				\
			memcpy(input_a_set[i],input_a_default[i],strlen(input_a_default[i]));	\
			input_set_axis(i,input_a_set[i]);										\
		}																			\
	}

	IGETA_MAP(CONT_A_M1_BEGIN,CONT_A_M1_END)
	IGETA_MAP(CONT_A_M2_BEGIN,CONT_A_M2_END)
	IGETA_MAP(CONT_A_T1_BEGIN,CONT_A_T1_END)
	IGETA_MAP(CONT_A_T2_BEGIN,CONT_A_T2_END)
	IGETA_MAP(CONT_A_ARK1_BEGIN,CONT_A_ARK1_END)
	IGETA_MAP(CONT_A_ARK2_BEGIN,CONT_A_ARK2_END)
	IGETA_MAP((INPUT_A_MAX-INPUT_A_END),INPUT_A_MAX)


	input_affirm_ports();
	input_update_leds();

	/* set menu keys */
	input_menucaption(0,IDM_HARDRESET,INPUT_T_HARDRESET);
	input_menucaption(0,IDM_SOFTRESET,INPUT_T_SOFTRESET);
	input_menucaption(0,IDM_MEDIA,INPUT_T_MEDIA);
	input_menucaption(0,IDM_EXITEMU,INPUT_T_EXIT);
	input_menucaption(0,IDM_FULLSCREEN,INPUT_T_FULLSCREEN);
	input_menucaption(0,IDM_SCREENSHOT,INPUT_T_SCREENSHOT);
	input_menucaption(0,IDM_PAUSEEMU,INPUT_T_PAUSE);
	input_menucaption(0,IDM_VIDEOFORMAT,INPUT_T_VIDEOFORMAT);
	input_menucaption(0,IDM_PASTE,INPUT_T_PASTE);
	input_menucaption(0,IDM_STATE_CHANGE,INPUT_T_PREVSLOT);
	input_menucaption(1,IDM_STATE_CHANGE,INPUT_T_NEXTSLOT);
	input_menucaption(0,IDM_STATE_SAVE,INPUT_T_SAVESTATE);
	input_menucaption(0,IDM_STATE_LOAD,INPUT_T_LOADSTATE);
	input_menucaption(0,IDM_HELP,INPUT_T_HELP);

	LOG(LOG_VERBOSE,"input initialised\n");
}

static void enumerate_devices(void)
{
	HRESULT result;

	/* on winxp, dinput only enumerates 1 keyboard and 1 mouse */

	/* keyboard */
	result=IDirectInput_EnumDevices(input.dinput,DIDEVTYPE_KEYBOARD,keyboard_create,0,DIEDFL_ATTACHEDONLY);
	if ((result!=DI_OK)|(input.num_keyboards==0)) {
		LOG(LOG_MISC|LOG_ERROR,"Can't create DirectInput keyboard!\n"); input_clean(); exit(1);
	}

	/* mouse */
	result=IDirectInput_EnumDevices(input.dinput,DIDEVTYPE_MOUSE,mouse_create,0,DIEDFL_ATTACHEDONLY);
	if ((result!=DI_OK)|(input.num_mouses==0)) {
		LOG(LOG_MISC|LOG_ERROR,"Can't create DirectInput mouse!\n"); input_clean(); exit(1);
	}

	/* joystick */
	if (input.joystick_isenabled) {
		IDirectInput_EnumDevices(input.dinput,DIDEVTYPE_JOYSTICK,joystick_create,0,DIEDFL_ATTACHEDONLY);
		if (result!=DI_OK) { LOG(LOG_MISC|LOG_WARNING,"can't create DirectInput joystick\n"); joystick_clean(); } /* not mandatory */
	}
}

/* create device */
#define DEVICE_WARNING(nam,war,str)													\
LOG(LOG_MISC|LOG_WARNING,"%s for DI %s%d",str,war,input.num_##nam##s+1);			\
	if (pd) {																		\
		IDirectInputDevice_Unacquire(pd);											\
		IDirectInputDevice_Release(pd);												\
	} return DIENUM_CONTINUE

#define _CREATE_DEVICE(nam,war,dfm)													\
static BOOL CALLBACK nam##_create(LPCDIDEVICEINSTANCE instance,LPVOID v)			\
{																					\
    v = v;                                                                          \
	int i;																			\
	LPDIRECTINPUTDEVICE pd;															\
	input_device* prev=input_device_##nam##_cursor;									\
																					\
	/* create it */																	\
	if ((IDirectInput_CreateDevice(input.dinput,&instance->guidInstance,&pd,NULL))!=DI_OK) { DEVICE_WARNING(nam,war,"can't create device"); } \
	/* set dataformat */															\
	if ((IDirectInputDevice_SetDataFormat(pd,&dfm))!=DI_OK) { DEVICE_WARNING(nam,war,"can't set dataformat"); } \
	/* set cooperative level */														\
	if ((IDirectInputDevice_SetCooperativeLevel(pd,MAIN->window,DISCL_NONEXCLUSIVE|DISCL_BACKGROUND))!=DI_OK) { DEVICE_WARNING(nam,war,"can't set cooperative level"); } \
	/* acquire */																	\
	for (i=10;(IDirectInputDevice_Acquire(pd)!=DI_OK)&(i>0);i--) Sleep(5);			\
	if (i==0) { DEVICE_WARNING(nam,war,"can't acquire"); }							\
																					\
	/* root */																		\
	if (input_device_##nam##_begin==NULL) {											\
		MEM_CREATE_T(input_device_##nam##_begin,sizeof(input_device),input_device*);\
		input_device_##nam##_cursor=input_device_##nam##_begin;						\
	}																				\
	/* next */																		\
	else {																			\
		MEM_CREATE_T(input_device_##nam##_cursor->next,sizeof(input_device),input_device*); \
		input_device_##nam##_cursor=input_device_##nam##_cursor->next;				\
		input_device_##nam##_cursor->prev=prev;										\
	}																				\
																					\
	input_device_##nam##_cursor->pd=pd;												\
	if (instance->tszInstanceName&&strlen(instance->tszInstanceName)) {				\
		i=strlen(instance->tszInstanceName);										\
		MEM_CREATE(input_device_##nam##_cursor->name,i+1);							\
		strcpy(input_device_##nam##_cursor->name,instance->tszInstanceName);		\
	}																				\
	else { /* nameless, dunno if that can happen */									\
		MEM_CREATE(input_device_##nam##_cursor->name,strlen("nameless device")+1);	\
		sprintf(input_device_##nam##_cursor->name,"nameless device");				\
	}																				\
	input.num_##nam##s++;															\
	LOG(LOG_VERBOSE,"enumerated [%s] as [%s%d]\n",input_device_##nam##_cursor->name,war,input.num_##nam##s); \
	return DIENUM_CONTINUE;															\
}
_CREATE_DEVICE(keyboard,"K",c_dfDIKeyboard)
_CREATE_DEVICE(mouse,"M",c_dfDIMouse)
_CREATE_DEVICE(joystick,"J",c_dfDIJoystick)


void input_affirm_ports(void)
{
	int a=input.skey_set[INPUT_DA_EM];

	if (msx_is_running()) {
		u32 da;

		#define s(p) da=cont_get_da(p); a=(a)|(input.skey_set[da&0xff])|(input.skey_set[da>>8&0xff])|(input.skey_set[da>>16&0xff])|(input.skey_set[da>>24&0xff])
		s(CONT_P1); s(CONT_P2); s(CONT_PKEY);
		#undef s
	}

	input.menukey=a&3;

	if (a&4) sound_create_capture();
	else sound_clean_capture();
}



/* clean */
/* clean std */
void input_clean(void)
{
	int i;

	for (i=0;i<INPUT_T_MAX;i++) {
		MEM_CLEAN(input_t_set[i]);
		MEM_CLEAN(input.trigger_rule[i]);
	}

	for (i=0;i<INPUT_A_MAX;i++) { MEM_CLEAN(input_a_set[i]); }

	if (input.num_triggers) while (input.num_triggers--) { MEM_CLEAN(input.trigger_info[input.num_triggers]); }
	MEM_CLEAN(input.trigger); MEM_CLEAN(input.trigger_old); MEM_CLEAN(input.trigger_info);

	if (input.num_axes) while (input.num_axes--) { MEM_CLEAN(input.axis_info[input.num_axes]); }
	MEM_CLEAN(input.axis); MEM_CLEAN(input.axis_old); MEM_CLEAN(input.axis_info);

	MEM_CLEAN(input.emshort); MEM_CLEAN(input.emshort_old);

	mouse_clean();
	keyboard_clean();
	joystick_clean();

	if (input.dinput) { IDirectInput_Release(input.dinput); input.dinput=NULL; }

	LOG(LOG_VERBOSE,"input cleaned\n");
}

/* clean device */
#define _CLEAN_DEVICE(nam) static void nam##_clean(void)							\
{																					\
	if (input.num_##nam##s) {														\
		for (;;) {																	\
			if (input_device_##nam##_cursor->pd) {									\
				IDirectInputDevice_Unacquire(input_device_##nam##_cursor->pd);		\
				IDirectInputDevice_Release(input_device_##nam##_cursor->pd);		\
				MEM_CLEAN(input_device_##nam##_cursor->name);						\
			}																		\
			if (input_device_##nam##_cursor->prev) {								\
				input_device_##nam##_cursor=input_device_##nam##_cursor->prev;		\
				MEM_CLEAN(input_device_##nam##_cursor->next);						\
			}																		\
			else break;																\
		}																			\
		MEM_CLEAN(input_device_##nam##_begin);										\
		input_device_##nam##_cursor=NULL;											\
		input.num_##nam##s=0;														\
	}																				\
}
_CLEAN_DEVICE(keyboard)
_CLEAN_DEVICE(mouse)
_CLEAN_DEVICE(joystick)

/* save settings */
void input_settings_save(void)
{
	int i,j;
	input_device* d;

	/* std */
	SETTINGS_PUT_STRING(NULL,"; *** Input settings ***\n\n");
	settings_put_yesnoauto(SETTINGS_INPBG,input.background);
	settings_put_yesnoauto(SETTINGS_KEYNETPLAY,netplay_keyboard_enabled());
	settings_put_yesnoauto(SETTINGS_JOYSTICK,input.joystick_isenabled);
	SETTINGS_PUT_FLOAT(settings_info(SETTINGS_ANALOGSENSE),input.analog_sensitivity);
	SETTINGS_PUT_FLOAT(settings_info(SETTINGS_MOUSESENSE),input.mouse_sensitivity);

	cont_settings_save();

	SETTINGS_PUT_STRING(NULL,"\n; Use \"nothing\" to unmap. Check the first half of the input.c module of the\n");
	SETTINGS_PUT_STRING(NULL,"; meisei source code for a full list of possible triggers.\n");
	SETTINGS_PUT_STRING(NULL,"; Combine with | for OR, & for AND, ! for NOT. Priorities are ! > & > |,\n");
	SETTINGS_PUT_STRING(NULL,"; special rules or combinations are not allowed with axes.\n");

	/* devices */
	SETTINGS_PUT_STRING(NULL,";\n; Previously detected devices:");

	#define ISET_DETDEV(x,y)														\
	if (input.num_##x##s) {															\
		d=input_device_##x##_begin;													\
		for (i=1;;i++) {															\
			SETTINGS_PUT_STRING(NULL,"\n; ");										\
			SETTINGS_PUT_STRING(NULL,y);											\
			SETTINGS_PUT_INT(NULL,i);												\
			SETTINGS_PUT_STRING(NULL," as ");										\
			SETTINGS_PUT_STRING(NULL,d->name);										\
			if (i==1&&!j) { SETTINGS_PUT_STRING(NULL," (identifier is optional)"); j=1; } \
			if (d->next) d=d->next; else break;										\
		}																			\
	}

	j=0; ISET_DETDEV(keyboard,"K")
	ISET_DETDEV(mouse,"M")
	ISET_DETDEV(joystick,"J")
	else SETTINGS_PUT_STRING(NULL,"\n; (no joysticks)");

	SETTINGS_PUT_STRING(NULL,"\n;\n; Valid flags:\n");
	SETTINGS_PUT_STRING(NULL,"; ISACTIVE  - TRUE when inputs are currently enabled\n");
	SETTINGS_PUT_STRING(NULL,"; ISFOCUS   - TRUE when the main window is in the foreground\n");
	SETTINGS_PUT_STRING(NULL,"; ISGRABBED - TRUE when the mouse pointer is grabbed\n");
	SETTINGS_PUT_STRING(NULL,"; CAPS_LED, NUM_LED, SCROLL_LED for keyboard led status\n");
	SETTINGS_PUT_STRING(NULL,";\n; Valid functions:\n");
	SETTINGS_PUT_STRING(NULL,"; dblclick(void) - reacts as a single pulse when doubleclicking the client area\n");
	SETTINGS_PUT_STRING(NULL,"; invert(const char* axis) - inverts axis direction (won't work on digital rules)\n");
	SETTINGS_PUT_STRING(NULL,"; rapid(int rate,const char* key) - rapid fire at a rate of 1 to 63\n");
	SETTINGS_PUT_STRING(NULL,"; toggle(const char* key) - press once to enable, and again to disable\n");
	SETTINGS_PUT_STRING(NULL,"; sound(int threshold) - captures the current sound input source, usually the\n");
	SETTINGS_PUT_STRING(NULL,"; -- PC microphone, and triggers when the amplitude is over the given threshold\n");
	SETTINGS_PUT_STRING(NULL,"; -- percentage (optional, 75 by default)\n");
	SETTINGS_PUT_STRING(NULL,";\n; Examples:\n");
	SETTINGS_PUT_STRING(NULL,"; trigger when key \"A\" is pressed: A\n");
	SETTINGS_PUT_STRING(NULL,"; either \"A\" or \"B\": A | B\n");
	SETTINGS_PUT_STRING(NULL,"; \"A\" and \"B\" at the same time: A & B\n");
	SETTINGS_PUT_STRING(NULL,"; \"A\" and \"B\", or \"C\": A & B | C\n");
	SETTINGS_PUT_STRING(NULL,"; \"A\" pressed, but not \"B\": A & !B\n");

	/* mappings */
	#define ISET_MAP(x,y,z,n)						\
	if (n) SETTINGS_PUT_STRING(NULL,"\n; * ");		\
	else SETTINGS_PUT_STRING(NULL,"; * ");			\
	SETTINGS_PUT_STRING(NULL,cont_get_a_name(x));	\
	SETTINGS_PUT_STRING(NULL," *\n");				\
	for (i=y;i<z;i++) SETTINGS_PUT_STRING(cont_get_trigger_info(i),input_t_set[i])

	ISET_MAP(CONT_DA_J1,CONT_T_J1_BEGIN,CONT_T_J1_END,TRUE);
	ISET_MAP(CONT_DA_J2,CONT_T_J2_BEGIN,CONT_T_J2_END,FALSE);
	ISET_MAP(CONT_DA_M1,CONT_T_M1_BEGIN,CONT_T_M1_END,TRUE);		SETTINGS_PUT_STRING(cont_get_axis_info(CONT_A_M1_X),input_a_set[CONT_A_M1_X]);	SETTINGS_PUT_STRING(cont_get_axis_info(CONT_A_M1_Y),input_a_set[CONT_A_M1_Y]);
	ISET_MAP(CONT_DA_M2,CONT_T_M2_BEGIN,CONT_T_M2_END,FALSE);		SETTINGS_PUT_STRING(cont_get_axis_info(CONT_A_M2_X),input_a_set[CONT_A_M2_X]);	SETTINGS_PUT_STRING(cont_get_axis_info(CONT_A_M2_Y),input_a_set[CONT_A_M2_Y]);
	ISET_MAP(CONT_DA_T1,CONT_T_T1_BEGIN,CONT_T_T1_END,TRUE);		SETTINGS_PUT_STRING(cont_get_axis_info(CONT_A_T1_X),input_a_set[CONT_A_T1_X]);	SETTINGS_PUT_STRING(cont_get_axis_info(CONT_A_T1_Y),input_a_set[CONT_A_T1_Y]);
	ISET_MAP(CONT_DA_T2,CONT_T_T2_BEGIN,CONT_T_T2_END,FALSE);		SETTINGS_PUT_STRING(cont_get_axis_info(CONT_A_T2_X),input_a_set[CONT_A_T2_X]);	SETTINGS_PUT_STRING(cont_get_axis_info(CONT_A_T2_Y),input_a_set[CONT_A_T2_Y]);
	ISET_MAP(CONT_DA_HS1,CONT_T_HS1_BEGIN,CONT_T_HS1_END,TRUE);
	ISET_MAP(CONT_DA_HS2,CONT_T_HS2_BEGIN,CONT_T_HS2_END,FALSE);
	ISET_MAP(CONT_DA_ARK1,CONT_T_ARK1_BEGIN,CONT_T_ARK1_END,TRUE);	SETTINGS_PUT_STRING(cont_get_axis_info(CONT_A_ARK1_DIAL),input_a_set[CONT_A_ARK1_DIAL]);
	ISET_MAP(CONT_DA_ARK2,CONT_T_ARK2_BEGIN,CONT_T_ARK2_END,FALSE);	SETTINGS_PUT_STRING(cont_get_axis_info(CONT_A_ARK2_DIAL),input_a_set[CONT_A_ARK2_DIAL]);
	ISET_MAP(CONT_DA_KEY,CONT_T_K_BEGIN,CONT_T_K_END,TRUE);

	SETTINGS_PUT_STRING(NULL,"\n; * emulator shortcuts *\n");
	#define SETEM(x) SETTINGS_PUT_STRING(input_get_trigger_info(x),input_t_set[(INPUT_T_MAX-INPUT_T_END)+x]);
	#define SEAEM(x) SETTINGS_PUT_STRING(input_get_axis_info(x),input_a_set[(INPUT_A_MAX-INPUT_A_END)+x]);
	SETEM(INPUT_T_MEDIA);
	SETEM(INPUT_T_TAPEREWIND);
	SETEM(INPUT_T_PREVSLOT);
	SETEM(INPUT_T_NEXTSLOT);
	SETEM(INPUT_T_SAVESTATE);
	SETEM(INPUT_T_LOADSTATE);
	SETEM(INPUT_T_SCREENSHOT);
	SETEM(INPUT_T_EXIT);
	SETEM(INPUT_T_REVERSE);
	SETEM(INPUT_T_CONTINUE);
	SETEM(INPUT_T_SLOWDOWN);
	SETEM(INPUT_T_SPEEDUP);
	SETEM(INPUT_T_DJHOLD);
	SEAEM(INPUT_A_DJSPIN);
	SETEM(INPUT_T_CPUSPEEDMIN);
	SETEM(INPUT_T_CPUSPEEDPLUS);
	SETEM(INPUT_T_Z80TRACER);
	SETEM(INPUT_T_HARDRESET);
	SETEM(INPUT_T_SOFTRESET);
	SETEM(INPUT_T_GRABMOUSE);
	SETEM(INPUT_T_PASTE);
	SETEM(INPUT_T_PASTEBOOT);
	SETEM(INPUT_T_PAUSE);
	SETEM(INPUT_T_FULLSCREEN);
	SETEM(INPUT_T_VIDEOFORMAT);
	SETEM(INPUT_T_HELP);
	SETEM(INPUT_T_DISABLE);
	#undef SETEM
	#undef SEAEM
}




/* update device */
/* std */

void input_tick(void) { input.tick=TRUE; }

void input_update(void)
{
	input_read();
	input_read_shortcuts();

	/* every 20 seconds or so, fool possible screensaver mousecursor moved */
	if ((crystal->fc&0x7ff)==0x400&&((MAIN->foreground&(MAIN->dialog^1))|input.background)) {
		mouse_event(MOUSEEVENTF_MOVE,0,0,0,0);
	}
}

void input_read(void)
{
	int* p=input.trigger; long* a=input.axis;
	int fg=(MAIN->foreground&(MAIN->dialog^1));

	if (input.semaphore) return;
	input.semaphore=TRUE;

	if (fg|input.background) {
		if (input.tick) {
			input.tick=FALSE;

			input.trigger=input.trigger_old; input.trigger_old=p;
			input.axis=input.axis_old; input.axis_old=a;

			sound_capt_read();
			keyboard_update();
			mouse_update();
			joystick_update();
		}
		input.trigger[INPUT_TT_ACTIVE]=input.axis[INPUT_TA_ACTIVE]=0x80;
		input.trigger[INPUT_TT_FOCUS]=input.axis[INPUT_TA_FOCUS]=fg<<7;
		input.trigger[INPUT_TT_MOUSEGRAB]=input.axis[INPUT_TA_MOUSEGRAB]=input.mouse_cursor_grabbed<<7;

		input.trigger[INPUT_TT_CAPSLED]=input.axis[INPUT_TA_CAPSLED]=input.leds[0];
		input.trigger[INPUT_TT_NUMLED]=input.axis[INPUT_TA_NUMLED]=input.leds[1];
		input.trigger[INPUT_TT_SCROLLLED]=input.axis[INPUT_TA_SCROLLLED]=input.leds[2];
	}
	else {
		input.trigger=input.trigger_old; input.trigger_old=p;
		input.axis=input.axis_old; input.axis_old=a;

		memset(input.trigger,0,input.num_triggers*sizeof(int));
		memset(input.axis,0,input.num_axes*sizeof(long));
		input_mouse_cursor_reset_ticks();
	}

	/* debug: show held triggers */
	/*
	int ii;
	printf("\r                                                               \r");
	for (ii=0;ii<input.num_triggers;ii++) if (input.trigger[ii]) printf("%s ",input.trigger_info[ii]);
	*/

	/* debug: hack to do whatever if key pressed */
	/*
	if (input.trigger[DIK_RCONTROL]) {
		printf("LOG START\n");
		log_set_frame_start(0);
	}
	else if (input.trigger[DIK_RSHIFT]) {
		log_set_frame_start(~0);
		printf("LOG END\n");
	}
	*/

	input.semaphore=FALSE;
}

/* shortcuts */
void input_cpuspeed_ticks_stall(void) { input.cpuspeed_ticks_next=input.cpuspeed_ticks+400; }

#define pm(x,y) PostMessage(MAIN->window,WM_COMMAND,x,y)

static void input_shortcut_cpuspeed(void)
{
	/* cpuspeed: hold key */
	#define p input.emshort[INPUT_T_CPUSPEEDPLUS]
	#define m input.emshort[INPUT_T_CPUSPEEDMIN]
	if (crystal->overclock==100&&p&&m) input_cpuspeed_ticks_stall();
	else if (input.cpuspeed_ticks>input.cpuspeed_ticks_next) {
		input.cpuspeed_ticks_next=input.cpuspeed_ticks+40;
		if (p&&m) pm(IDM_CPUSPEEDNORMAL,0); /* both keys held for a while: reset to 100% */
		else if (m) pm(IDM_CPUSPEEDMIN,0);
		else pm(IDM_CPUSPEEDPLUS,0);
	}
	#undef p
	#undef m
}

#define o(x)	input.emshort_old[x]
#define p(x)	input.emshort[x]
static void input_shortcut_hardreset(void) { if (!o(INPUT_T_HARDRESET)) pm(IDM_HARDRESET,0); }
static void input_shortcut_softreset(void) { if (!o(INPUT_T_SOFTRESET)) pm(IDM_SOFTRESET,0); }
static void input_shortcut_media(void) { if (!o(INPUT_T_MEDIA)) pm(IDM_MEDIA,0); }
static void input_shortcut_taperewind(void) { if (!o(INPUT_T_TAPEREWIND)) pm(IDM_TAPEREWIND,0); }
static void input_shortcut_grabmouse(void) { if (!o(INPUT_T_GRABMOUSE)) pm(IDM_GRABMOUSE,0); }
static void input_shortcut_exit(void) { if (!o(INPUT_T_EXIT)) pm(IDM_EXITEMU,0); }
static void input_shortcut_fullscreen(void) { if (!o(INPUT_T_FULLSCREEN)) pm(IDM_FULLSCREEN,0); }
static void input_shortcut_screenshot(void) { if (!o(INPUT_T_SCREENSHOT)) pm(IDM_SCREENSHOT,0); }
static void input_shortcut_pause(void) { if (!o(INPUT_T_PAUSE)) pm(IDM_PAUSEEMU,0); }
static void input_shortcut_videoformat(void) { if (!o(INPUT_T_VIDEOFORMAT)) pm(IDM_VIDEOFORMAT,0); }
static void input_shortcut_paste(void) { if (!o(INPUT_T_PASTE)) pm(IDM_PASTE,0); }
static void input_shortcut_pasteboot(void) { if (!o(INPUT_T_PASTEBOOT)) pm(IDM_PASTEBOOT,0); }
static void input_shortcut_prevslot(void) { if (!o(INPUT_T_PREVSLOT)) pm(IDM_PREVSLOT,0); }
static void input_shortcut_nextslot(void) { if (!o(INPUT_T_NEXTSLOT)) pm(IDM_NEXTSLOT,0); }
static void input_shortcut_savestate(void) { if (!o(INPUT_T_SAVESTATE)) pm(IDM_STATE_SAVE,0); }
static void input_shortcut_loadstate(void) { if (!o(INPUT_T_LOADSTATE)) pm(IDM_STATE_LOAD,0); }
static void input_shortcut_cpuspeedmin(void) { if (!o(INPUT_T_CPUSPEEDMIN)) { pm(IDM_CPUSPEEDMIN,0); input_cpuspeed_ticks_stall(); } else input_shortcut_cpuspeed(); }
static void input_shortcut_cpuspeedplus(void) { if (!o(INPUT_T_CPUSPEEDPLUS)) { if (!p(INPUT_T_CPUSPEEDMIN)) pm(IDM_CPUSPEEDPLUS,0); input_cpuspeed_ticks_stall(); } else { if (!o(INPUT_T_CPUSPEEDMIN)&&p(INPUT_T_CPUSPEEDMIN)) input_cpuspeed_ticks_stall(); else input_shortcut_cpuspeed(); } }
static void input_shortcut_help(void) { if (!o(INPUT_T_HELP)) pm(IDM_HELP,0); }
static void input_shortcut_nothing(void) { return; }
#undef pm
#undef o
#undef p

typedef void(*fpt_input_shortcut)(void);
static const fpt_input_shortcut input_shortcut[INPUT_T_END]= {
	input_shortcut_hardreset,
	input_shortcut_softreset,
	input_shortcut_media,
	input_shortcut_taperewind,
	input_shortcut_grabmouse,
	input_shortcut_exit,
	input_shortcut_fullscreen,
	input_shortcut_screenshot,
	input_shortcut_pause,
	input_shortcut_videoformat,
	input_shortcut_paste,
	input_shortcut_pasteboot,
	input_shortcut_prevslot,
	input_shortcut_nextslot,
	input_shortcut_savestate,
	input_shortcut_loadstate,
	input_shortcut_nothing,			/* reverse, handled in reverse.c */
	input_shortcut_nothing,			/* continue, handled in reverse.c */
	input_shortcut_nothing,			/* speed up, handled in crystal.c */
	input_shortcut_nothing,			/* slowdown, handled in crystal.c */
	input_shortcut_nothing,			/* dj hold, handled in crystal.c */
	input_shortcut_cpuspeedmin,
	input_shortcut_cpuspeedplus,
	input_shortcut_nothing,			/* z80 tracer, handled in z80.c */
	input_shortcut_help,
	input_shortcut_nothing			/* disable, handled below */
};

void input_read_shortcuts(void)
{
	int i;

	/* remember previous state */
	int* swap=input.emshort_old; input.emshort_old=input.emshort; input.emshort=swap;

	input.cpuspeed_ticks=GetTickCount();

	/* read */
	input.emshort[INPUT_T_REVERSE]=input.emshort[INPUT_T_SPEEDUP]=input.emshort[INPUT_T_SLOWDOWN]=input.emshort[INPUT_T_DJHOLD]=input.emshort[INPUT_T_CONTINUE]=0; /* handled elsewhere */
	#define READ_SHORT(x) input.emshort[x]=input_trigger_held((INPUT_T_MAX-INPUT_T_END)+x)
	READ_SHORT(INPUT_T_DISABLE);
	READ_SHORT(INPUT_T_HELP);
	READ_SHORT(INPUT_T_CPUSPEEDPLUS);
	READ_SHORT(INPUT_T_CPUSPEEDMIN);
	READ_SHORT(INPUT_T_LOADSTATE);
	READ_SHORT(INPUT_T_SAVESTATE);
	READ_SHORT(INPUT_T_NEXTSLOT);
	READ_SHORT(INPUT_T_PREVSLOT);
	READ_SHORT(INPUT_T_PASTEBOOT);
	READ_SHORT(INPUT_T_PASTE);
	READ_SHORT(INPUT_T_VIDEOFORMAT);
	READ_SHORT(INPUT_T_PAUSE);
	READ_SHORT(INPUT_T_FULLSCREEN);
	READ_SHORT(INPUT_T_SCREENSHOT);
	READ_SHORT(INPUT_T_EXIT);
	READ_SHORT(INPUT_T_GRABMOUSE);
	READ_SHORT(INPUT_T_MEDIA);
	READ_SHORT(INPUT_T_TAPEREWIND);
	READ_SHORT(INPUT_T_SOFTRESET);
	READ_SHORT(INPUT_T_HARDRESET);

	/* pause is a special case: if you manage to 'press' the trigger while input is inactive (eg. !ISACTIVE, or a rapid function with !nothing), it will be interpreted as secondary hold key instead of the standard toggle */
	if (input.emshort[INPUT_T_PAUSE]&&!input.trigger[INPUT_TT_ACTIVE]&&!netplay_is_active()) msx_set_paused(msx_get_paused()|2); else msx_set_paused(msx_get_paused()&~2);

	if (!input.trigger_old[INPUT_TT_ACTIVE]||input.emshort[INPUT_T_DISABLE]) {
		input_cpuspeed_ticks_stall();
		return;
	}

	/* run shortcut (if it's there, bottom to top) */
	i=INPUT_T_END; while (i--) { if (input.emshort[i]) { input_shortcut[i](); break; } }
}


#define GETDEVICESTATE(s) IDirectInputDevice_GetDeviceState(d->pd,s,&state)
#define DEVICE_GETSTATE_ERROR(ts,as) t+=ts; a+=as; d=d->next; continue /* error, use previous state */
#define DEVICE_GETSTATE(s,ts,as)											\
	switch (GETDEVICESTATE(s)) {											\
		case DI_OK: break;													\
		case DIERR_NOTACQUIRED: case DIERR_INPUTLOST:						\
			IDirectInputDevice_Acquire(d->pd);								\
			if (GETDEVICESTATE(s)!=DI_OK) { DEVICE_GETSTATE_ERROR(ts,as); }	\
			else break;														\
		default: DEVICE_GETSTATE_ERROR(ts,as);								\
	}

/* keyboard */
static void keyboard_update(void)
{
	int t=0,a=0;
	UCHAR state[T_KEYBOARD_SIZE];
	input_device* d=input_device_keyboard_begin;

	while (d) {
		/* get keyboard state */
		DEVICE_GETSTATE(T_KEYBOARD_SIZE,T_KEYBOARD_SIZE,A_KEYBOARD_SIZE)

		/* copy state */
		for (a=0;a<T_KEYBOARD_SIZE;a++) input.trigger[t++]=state[a]&0x80;
		input.trigger[t++]=(state[DIK_LMENU]|state[DIK_RMENU])&0x80; /* alt=lalt|ralt */
		input.trigger[t++]=(state[DIK_LCONTROL]|state[DIK_RCONTROL])&0x80; /* ctrl=lctrl|rctrl */
		input.trigger[t++]=(state[DIK_LSHIFT]|state[DIK_RSHIFT])&0x80; /* shift=lshift|rshift */
		input.trigger[t++]=(state[DIK_RETURN]|state[DIK_NUMPADENTER])&0x80; /* enter=main enter|numpad enter */
		input.trigger[t++]=state[DIK_PREVTRACK]&0x80; /* circumflex=previous track */

		d=d->next;
	}
}

int input_menukey(int key)
{
	switch (key) {
		case VK_MENU: return input.menukey&1;
		case -1: return input.menukey<<7&input.trigger[T_KEYBOARD_A_ALT];
		case VK_F10: return input.menukey&2;

		default: break;
	}

	return FALSE;
}

void __fastcall input_update_leds(void)
{
	input.leds[0]=GetKeyState(VK_CAPITAL)<<7&0x80;
	input.leds[1]=GetKeyState(VK_NUMLOCK)<<7&0x80;
	input.leds[2]=GetKeyState(VK_SCROLL)<<7&0x80;
}

/* mouse */
static void mouse_update(void)
{
	int t=input.num_keyboards*T_KEYBOARD_SIZE_TOTAL;
	int a0,a1,a=0,c=TRUE,ao,to;
	static int fault=0;
	DIMOUSESTATE state;
	input_device* d=input_device_mouse_begin;

	a0=crystal->fast&~fault;
	fault=0;

	while (d) {
		to=t; ao=a;

		/* get mouse state */
		DEVICE_GETSTATE(sizeof(DIMOUSESTATE),T_MOUSE_SIZE,A_MOUSE_SIZE)
		a1=a0&(state.lX==0)&(state.lY==0)&(state.lZ==0);

		/* copy state */
		/* axes, relative */
		#define GET_M_AXIS(x)															\
			if (sound_get_ticks_prev()>SOUND_TICKS_LEN) state.x=state.x*(SOUND_TICKS_LEN/(float)sound_get_ticks_prev())+((state.x<0)?-0.999:0.999); \
			if (a1&&input.axis_old[a]!=0) { fault|=1; state.x=input.axis_old[a]; }		\
			input.trigger[t++]=(state.x<0)<<7; input.trigger[t++]=(state.x>0)<<7; input.axis[a++]=state.x*input.mouse_sensitivity;

		GET_M_AXIS(lX);		GET_M_AXIS(lY);		GET_M_AXIS(lZ);

		/* buttons */
		#define GET_M_BUT(x) input.trigger[t++]=state.rgbButtons[x]&0x80
		GET_M_BUT(0);		GET_M_BUT(1);		GET_M_BUT(2);		GET_M_BUT(3);

		/* cursor movement? */
		if (c) {
			if ((MAIN->foreground&(MAIN->dialog^1))&&(memcmp(&input.axis[ao],&input.axis_old[ao],A_MOUSE_SIZE*sizeof(long))==0)&&(memcmp(&input.trigger[to],&input.trigger_old[to],T_MOUSE_SIZE*sizeof(int))==0)) {
				if (input.mouse_cursor_ticks<GetTickCount()) { input.mouse_cursor_visible=FALSE; }
			}
			else { c=FALSE; input_mouse_cursor_reset_ticks(); }
		}

		d=d->next;
	}
}

/* joystick */
static void joystick_update(void)
{
	int i,t=(input.num_keyboards*T_KEYBOARD_SIZE_TOTAL)+(input.num_mouses*T_MOUSE_SIZE);
	int a=input.num_mouses*A_MOUSE_SIZE;
	DIJOYSTATE state;
	input_device* d=input_device_joystick_begin;

	if (input.num_joysticks==0) return;

	while (d) {
		/* get joystick state */
		DEVICE_GETSTATE(sizeof(DIJOYSTATE),T_JOYSTICK_SIZE,A_JOYSTICK_SIZE)

		/* copy state */
		/* axes, absolute; 0..65535 ($FFFF), convert to relative */
		#define GET_J_AXIS(x) input.trigger[t++]=(state.x<0x6000)<<7; input.trigger[t++]=(state.x>=0xa000)<<7; input.axis[a++]=((state.x-32767)>>8)*input.analog_sensitivity;
		GET_J_AXIS(lX);				GET_J_AXIS(lY);				GET_J_AXIS(lZ);
		GET_J_AXIS(lRx);			GET_J_AXIS(lRy);			GET_J_AXIS(lRz);
		GET_J_AXIS(rglSlider[0]);	GET_J_AXIS(rglSlider[1]);

		/* pov, in degrees*100.. digital interpretation only */
		for (i=0;i<4;i++) {
			input.trigger[t++]=((state.rgdwPOV[i]<=31500)&(state.rgdwPOV[i]>=22500))<<7;
			input.trigger[t++]=((state.rgdwPOV[i]<=13500)&(state.rgdwPOV[i]>=4500))<<7;
			input.trigger[t++]=((state.rgdwPOV[i]<=4500)|((state.rgdwPOV[i]>=31500)&(state.rgdwPOV[i]<36000)))<<7;
			input.trigger[t++]=((state.rgdwPOV[i]<=22500)&(state.rgdwPOV[i]>=13500))<<7;
		}

		/* buttons */
		for (i=0;i<32;i++) input.trigger[t++]=state.rgbButtons[i]&0x80;

		d=d->next;
	}
}


/* mouse cursor */
void __fastcall input_mouse_cursor_update(void)
{
	POINT p;

	/* visibility */
	/* known bug (don't know how to fix it): ShowCursor doesn't work properly when opening a ROM by holding the left mouse
		button after doubleclicking, or very quick doubleclicking, from Windows explorer/desktop, however, the cursor
		will hide for a split second if you move it afterwards (so it did have effect, but didn't 'show' it).
	*/

	/* grab/ungrab cursor */
	if (input.mouse_cursor_grabbed&&MAIN->foreground&&input_mouse_in_client(MAIN->window,NULL,&p,FALSE)) {
		RECT r;
		p.x=p.y=0;

		ClientToScreen(MAIN->window,&p);
		GetClientRect(MAIN->window,&r);
		OffsetRect(&r,p.x,p.y);
		ClipCursor(&r);

		input.mouse_cursor_visible=FALSE;
	}
	else {
		if (input.mouse_cursor_grabbed) {
			/* auto ungrab */
			input.mouse_cursor_grabbed=FALSE;
			ClipCursor(NULL);
		}
	}

	/* show cursor */
	if (input.mouse_cursor_visible) while (input.mouse_cursor_dc<0) input.mouse_cursor_dc=ShowCursor(TRUE);

	/* possibly hide cursor */
	else {
		if (msx_is_running()) {
			while (input.mouse_cursor_dc>=0) {
				if (input.mouse_cursor_dc==0) {
					if (!input_mouse_in_client(MAIN->window,NULL,&p,FALSE)) {
						input.mouse_cursor_visible=TRUE;
						break;
					}
				}
				input.mouse_cursor_dc=ShowCursor(FALSE);
			}
		}
		else input.mouse_cursor_visible=TRUE;
	}

	/* full screen menu */
	if (draw_get_fullscreen()) {
		if (!GetCursorPos(&p)) p.y=GetSystemMetrics(SM_CYMENU);
		if (!MAIN->dialog) {
			if (p.y<GetSystemMetrics(SM_CYMENU)&&!input.mouse_cursor_grabbed) draw_show_menu(TRUE);
			else draw_show_menu(FALSE);
		}
	}
}

/* get/set grab */
int input_get_mouse_cursor_grabbed(void) { return input.mouse_cursor_grabbed; }
void input_set_mouse_cursor_grabbed(int i)
{
	if (i&&!input.mouse_cursor_grabbed&&MAIN->foreground) {
		/* move cursor to client area */
		POINT p={0,0};
		ClientToScreen(MAIN->window,&p);
		SetCursorPos(p.x,p.y);
	}
	else ClipCursor(NULL);

	input.mouse_cursor_grabbed=i;
}

void input_mouse_cursor_reset_ticks(void) { input.mouse_cursor_visible=TRUE; input.mouse_cursor_ticks=GetTickCount()+2500; }

/* doubleclick */
void input_doubleclick(void) { input.doubleclick=TRUE; input.doubleclick_ticks=GetTickCount(); }

/* mouse position in client area? */
int input_mouse_in_client(HWND wnd,HWND parent,POINT* p,int quick)
{
	RECT r;

	p->x=p->y=0;
	ClientToScreen(wnd,p);
	GetClientRect(wnd,&r);
	OffsetRect(&r,p->x,p->y);

	/* in client area? */
	if (GetCursorPos(p)) {
		if (PtInRect(&r,*p)) {
			if (!quick) {
				HWND wnd_prev;
				RECT rc;

				if (parent) wnd=parent;

				/* check windows above this one */
				for (;;) {
					wnd_prev=GetNextWindow(wnd,GW_HWNDPREV);
					if (wnd_prev==NULL) break; /* done */
					wnd=wnd_prev;

					GetWindowRect(wnd,&rc);
					if (IsWindowVisible(wnd)&&PtInRect(&rc,*p)) {
						p->x-=r.left; p->y-=r.top;
						return FALSE;
					}
				}
			}

			p->x-=r.left; p->y-=r.top;
			return TRUE;
		}
		else {
			/* set to edge */
			if (p->x<r.left) p->x=0;
			else if (p->x>=r.right) p->x=r.right-(r.left+1);
			else p->x-=r.left;

			if (p->y<r.top) p->y=0;
			else if (p->y>=r.bottom) p->y=r.bottom-(r.top+1);
			else p->y-=r.top;
		}
	}

	return FALSE;
}



/* set/get trigger (rules) */

int input_set_trigger(const u32 trig,const char* set,u32 type)
{
	char *c=NULL,*t,*to,f[20];
	int i,def=TRUE,don=TRUE,off,sep,n,cur,len,end,rule_size,trc,ret=TRUE,rc=0,*trule,*rule=NULL;

	if (trig>=INPUT_T_MAX) return -1;

	if ((set!=NULL)&&(strlen(set)>0)) {

		MEM_CREATE(c,strlen(set)+1);
		memcpy(c,set,strlen(set));
retry:
		off=sep=def=trc=rule_size=0;

		/* find number of separators */
		for (i=0;;i++) {
			if (c[i]=='\0') break;
			if ((c[i]=='|')|(c[i]=='&')) rule_size++;
		}
		rule_size+=2; rule_size*=2;
		MEM_CREATE(trule,rule_size*sizeof(int));

		/* convert "key1 & key2 | key3" to -1 1 -2 2 -1 3 -3, a [not](!) is keynum|$8000 */
		trule[trc++]=-1;
		for (;;) {
			/* find new */
			for (;;off++) {
				if (c[off]=='\0') { off=-1; break; }
				else if ((c[off]!=' ')&(c[off]!='\t')&(c[off]!='&')&(c[off]!='|')) { break; }
			}
			if (off==-1) { trule[trc++]=-3; break; }

			/* find separator */
			if (sep<0) trule[trc++]=sep;
			sep=0; cur=off;
			for (;;cur++) {
				switch (c[cur]) {
					case '\0': break;
					case '|': sep=-1; break;
					case '&': sep=-2; break;
					default: continue;
				}
				break;
			}

			/* backtrack to calculate length */
			end=cur;
			for (;;end--) if ((c[end-1]!=' ')&(c[end-1]!='\t')) break;
			len=end-off;

			/* convert trigger string to int */
			/*
				bit 15: is not/invert

				key:
				bits 0-14: key number

				toggle:
				bit 16: is toggle
				bit 17: toggle is valid
				+key

				doubleclick:
				bit 17: is doubleclick

				rapid:
				bit 30: rapid is valid
				bits 18-23: rapid count
				bits 24-29: rapid max/is rapid
				+key

				sound:
				bit 30: is sound
				bits 0-7: sound threshold

				bit 31: unused (int sign bit)
			*/
			t=to=NULL;
			MEM_CREATE(t,len+2);
			memcpy(t,&c[off],len);
			to=t; n=0;

			#define T_FIND()																\
				if (t[0]=='!') {															\
					t++; n^=0x8000; /* not */												\
					while ((t[0]==' ')|(t[0]=='\t')) t++; /* ignore optional whitespace */	\
				}																			\
				if (strlen(t)>3) {															\
					memset(f,0,10);															\
					memcpy(f,t,3);															\
					if (stricmp(f,"K1 ")==0) t+=3; /* ignore optional "K1 " */				\
				}																			\
				for (i=0;i<(input.num_triggers-1);i++)										\
					if (stricmp(t,input.trigger_info[i])==0) break

			T_FIND();

			/* not found? check if it's a function */
			if (i==(input.num_triggers-1)) {

				int notfound=TRUE;

				len=strlen(t);

				/* doubleclick? */
				if (notfound&&len==10) {
					memset(f,0,20);
					memcpy(f,t,10);
					if (stricmp(f,"dblclick()")==0) {
						n|=0x20000; i=0;
						notfound=FALSE;
					}
				}

				/* rapid? */
				if (notfound&&len>9) {
					memset(f,0,20);
					memcpy(f,t,5);
					if ((stricmp(f,"rapid")==0)||(stricmp(f,"turbo")==0)) {
						int rapid=-1; t+=5;
						while ((t[0]=='(')|(t[0]==' ')|(t[0]=='\t')) t++;
						len=strlen(t);
						if (len) {
							sscanf(t,"%d",&rapid);
							while ((t[0]!='\0')&(t[0]!=',')) t++;
							t++; len=strlen(t);
							if (len) while (len--) if (t[len]==')') { t[len]='\0'; break; }
							if (len) while (len--) if ((t[len]!=' ')&(t[len]!='\t')) { t[++len]='\0'; break; }
						}
						if (len&&rapid!=-1) {
							CLAMP(rapid,1,0x3f);
							rapid<<=24;
							T_FIND(); n|=rapid;
							notfound=FALSE;
						}
					}
				}

				/* toggle? */
				if (notfound&&len>8) {
					memset(f,0,20);
					memcpy(f,t,6);
					if (stricmp(f,"toggle")==0) {
						t+=6;
						while ((t[0]=='(')|(t[0]==' ')|(t[0]=='\t')) t++;
						len=strlen(t);
						if (len) while (len--) if (t[len]==')') { t[len]='\0'; break; }
						if (len) while (len--) if ((t[len]!=' ')&(t[len]!='\t')) { t[++len]='\0'; break; }
						if (len) { T_FIND(); n|=0x10000; notfound=FALSE; }
					}
				}

				/* sound? */
				if (notfound&&len>6) {
					memset(f,0,20);
					memcpy(f,t,5);
					if (stricmp(f,"sound")==0) {
						int threshold=75; t+=5;
						while ((t[0]=='(')|(t[0]==' ')|(t[0]=='\t')) t++;
						len=strlen(t);
						if (len) {
							sscanf(t,"%d",&threshold);
							CLAMP(threshold,0,100);
							n|=0x40000000;
							n|=threshold;
							i=0;
							input.skey_set[type]|=4;
							notfound=FALSE;
						}
					}
				}
			}

			MEM_CLEAN(to);
			if (i==(input.num_triggers-1)) n=0;
			trule[trc++]=i|n;

			off=cur;
		}

		/* convert to [or_numkey(s)] [and_numkey(s)] [key] ([key], etc) [and_numkey(s)] [key] ([key], etc) etc */
		MEM_CREATE(rule,rule_size*sizeof(int));
		trc=end=0; rc=1;
		for (;;) {
			/* find next or */
			for (;;trc++) {
				if (trule[trc]==-1) break;
				else if (trule[trc]==-3) { end=TRUE; break; }
			}
			if (end) break;

			/* look if next key(s) = invalid */
			trc++; cur=trc; i=FALSE;
			for (;;trc++) {
				if ((trule[trc]&0x7fff)==(input.num_triggers-1)) { i=TRUE; }
				else if ((trule[trc]==-1)|(trule[trc]==-3)) break;
			}
			if (i) continue;

			/* copy next key(s) */
			off=0; rule[0]++;
			for (;;cur++) {
				if (trule[cur]<0) { if ((trule[cur]==-1)|(trule[cur]==-3)) break; }
				else { rule[rc]++; off++; rule[rc+off]=trule[cur]; }
			}
			rc=rc+off+1;
		}

		def=rule[0]==0;

		MEM_CLEAN(trule);
		MEM_CLEAN(c);
	}

	/* invalid; use default */
	if (def&don) {
		don=ret=FALSE;

		MEM_CLEAN(rule);
		MEM_CREATE(c,strlen(input_t_default[trig])+1);
		memcpy(c,input_t_default[trig],strlen(input_t_default[trig]));
		goto retry;
	}

	/* valid */
	if (rc) {
		int orc=rule[0];
		int andc; i=0;

		/* find single ALT or F10 (lalt&ralt situation would still pass though). "sound()" is handled above. */
		/* I'd want to trap LWIN and RWIN too, but that's only possible with a global hook, it seems (SetWindowsHookEx(WH_KEYBOARD,x,x,0)) */
		while (orc--) {
			i++; andc=rule[i];
			if (andc==1) {
				i++;
				switch (rule[i]&0x40007fff) { /* bit 30=sound, and uses lower bits */
					case DIK_LMENU: case DIK_RMENU: case T_KEYBOARD_A_ALT: input.skey_set[type]|=1; break;
					case DIK_F10: input.skey_set[type]|=2; break;
					default: break;
				}
			}
			else i+=andc;
		}

		/* copy */
		MEM_CREATE(input.trigger_rule[trig],rc*sizeof(int));
		memcpy(input.trigger_rule[trig],rule,rc*sizeof(int));
	}

	MEM_CLEAN(rule);

	return ret;
}


int __fastcall input_trigger_held(int t)
{
	int orc=input.trigger_rule[t][0];
	int andc,i=0,or=0,and=0;

	while (orc--) {
		i++; andc=input.trigger_rule[t][i]; and|=0x80;
		while (andc--) {
			i++;

			/* function */
			if (input.trigger_rule[t][i]&0x7f030000) {

				/* rapid */
				if (input.trigger_rule[t][i]&0x3f000000) {
					input.trigger_rule[t][i]&=0xbfffffff;
					input.trigger_rule[t][i]+=0x40000;
					if (input.trigger_old[input.trigger_rule[t][i]&0x7fff]^input.trigger[input.trigger_rule[t][i]&0x7fff]||(input.trigger_rule[t][i]&0xfc0000)==(input.trigger_rule[t][i]>>6&0xfc0000)) input.trigger_rule[t][i]&=0xbf03ffff;
					if ((input.trigger_rule[t][i]&0xfc0000)<(input.trigger_rule[t][i]>>7&0xfc0000)) input.trigger_rule[t][i]|=0x40000000;

					and&=(((input.trigger[input.trigger_rule[t][i]&0x7fff])^(input.trigger_rule[t][i]>>8))&(input.trigger_rule[t][i]>>23));
				}

				/* toggle */
				else if (input.trigger_rule[t][i]&0x10000) {
					input.trigger_rule[t][i]^=((((input.trigger_old[input.trigger_rule[t][i]&0x7fff])^0x80)&((input.trigger[input.trigger_rule[t][i]&0x7fff])))<<10);
					and&=(input.trigger_rule[t][i]>>10^(input.trigger_rule[t][i]>>8));
				}

				/* doubleclick */
				else if (input.trigger_rule[t][i]&0x20000) {
					and&=((input.doubleclick&((input.doubleclick_ticks+200)>GetTickCount()))<<7^(input.trigger_rule[t][i]>>8));
					input.doubleclick=FALSE;
				}

				/* sound */
				else if (input.trigger_rule[t][i]&0x40000000) {
					and&=((sound_get_capt_threshold()>(input.trigger_rule[t][i]&0x7f))<<7^(input.trigger_rule[t][i]>>8));
				}
			}

			/* normal */
			else and&=((input.trigger[input.trigger_rule[t][i]&0x7fff])^(input.trigger_rule[t][i]>>8));
		}
		or|=and;
	}

	return or>>7&1;
}


/* set/get axis (rules, simpler than trigger) */

int input_set_axis(const u32 axis,const char* set)
{
	int i,len,f=0;
	char c[STRING_SIZE];

	if (set==NULL||(len=strlen(set))==0) return FALSE;

	/* check for function */
	if (len>8) {
		memset(c,0,7); memcpy(c,set,6);
		if (stricmp(c,"invert")==0) {
			/* invert (0x8000) */
			i=6; while ((set[i]=='(')|(set[i]==' ')|(set[i]=='\t')) i++;
			len=strlen(set+i); if (len) memcpy(c,set+i,len+1);
			if (len) while (len--) if ((c[len]!=' ')&(c[len]!='\t')&(c[len]!=')')) { c[++len]='\0'; break; }
			if (len) f=0x8000;
			else strcpy(c,set);
		}
		else strcpy(c,set);
	}
	else strcpy(c,set);

	for (i=0;i<input.num_axes-INPUT_MAX_ABOVE;i++) {
		if (stricmp(c,input.axis_info[i])==0) { input.axis_rule[axis]=i|f; return TRUE; }
	}

	if (stricmp(c,input.axis_info[INPUT_TA_NOTHING])==0) { input.axis_rule[axis]=INPUT_TA_NOTHING; return TRUE; }

	return FALSE;
}

long __fastcall input_get_axis(int a)
{
	if (input.axis_rule[a]&0x8000) return -input.axis[input.axis_rule[a]&0x7fff]; /* invert */
	else return input.axis[input.axis_rule[a]&0x7fff];
}
