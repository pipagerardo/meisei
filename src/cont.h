/******************************************************************************
 *                                                                            *
 *                                 "cont.h"                                   *
 *                                                                            *
 * Controles de teclado, ratón, joysticks, trackball, etc...                  *
 ******************************************************************************/

#ifndef CONT_H
#define CONT_H

#define CONT_MAX_MOVIE_SIZE	19

/* ports */
enum {
	CONT_P1=0,
	CONT_P2,
	CONT_PKEY,

	CONT_P_MAX
};

/* devices */
enum {
	CONT_D_J=0,		/* joystick */
	CONT_D_ARK,		/* Taito Arkanoid pad */
	CONT_D_HS,		/* Konami HyperShot */
	CONT_D_MAGIC,	/* Sony Magic Key */
	CONT_D_M,		/* mouse */
	CONT_D_T,		/* trackball */
	/* insert new one here, don't mess up order */

	CONT_D_KEY,		/* keyboard */

	CONT_D_NOTHING,

	CONT_D_MAX
};
/* alternate names */
enum {
	CONT_DA_J1,		/* joystick 1 */
	CONT_DA_J2,		/* joystick 2 */
	CONT_DA_M1,		/* mouse 1 */
	CONT_DA_M2,		/* mouse 2 */
	CONT_DA_T1,		/* trackball 1 */
	CONT_DA_T2,		/* trackball 2 */
	CONT_DA_ARK1,	/* Taito Arkanoid pad 1 */
	CONT_DA_ARK2,	/* Taito Arkanoid pad 2 */
	CONT_DA_HS1,	/* Konami HyperShot 1 */
	CONT_DA_HS2,	/* Konami HyperShot 2 */

	CONT_DA_KEY,	/* keyboard */

	CONT_DA_MAX,

	CONT_DA_EMPTY
};

/* keyboard regions */
enum {
	CONT_REGION_INTERNATIONAL=0,
	CONT_REGION_JAPANESE,
	CONT_REGION_KOREAN,
	CONT_REGION_UK,

	CONT_REGION_AUTO,

	CONT_REGION_MAX
};


/* triggers */
/* joystick */
#define CONT_T_J1_BEGIN 0
enum {
	CONT_T_J1_UP=CONT_T_J1_BEGIN,
	CONT_T_J1_DOWN,
	CONT_T_J1_LEFT,
	CONT_T_J1_RIGHT,
	CONT_T_J1_A,
	CONT_T_J1_B,

	CONT_T_J1_END
};
#define CONT_T_J2_BEGIN CONT_T_J1_END
enum {
	CONT_T_J2_UP=CONT_T_J2_BEGIN,
	CONT_T_J2_DOWN,
	CONT_T_J2_LEFT,
	CONT_T_J2_RIGHT,
	CONT_T_J2_A,
	CONT_T_J2_B,

	CONT_T_J2_END
};
#define CONT_T_J_SIZE (CONT_T_J1_END-CONT_T_J1_BEGIN)

/* mouse */
#define CONT_T_M1_BEGIN CONT_T_J2_END
enum {
	CONT_T_M1_LEFT=CONT_T_M1_BEGIN,
	CONT_T_M1_RIGHT,

	CONT_T_M1_END
};
#define CONT_T_M2_BEGIN CONT_T_M1_END
enum {
	CONT_T_M2_LEFT=CONT_T_M2_BEGIN,
	CONT_T_M2_RIGHT,

	CONT_T_M2_END
};
#define CONT_T_M_SIZE (CONT_T_M1_END-CONT_T_M1_BEGIN)

/* trackball */
#define CONT_T_T1_BEGIN CONT_T_M2_END
enum {
	CONT_T_T1_LOWER=CONT_T_T1_BEGIN,
	CONT_T_T1_UPPER,

	CONT_T_T1_END
};
#define CONT_T_T2_BEGIN CONT_T_T1_END
enum {
	CONT_T_T2_LOWER=CONT_T_T2_BEGIN,
	CONT_T_T2_UPPER,

	CONT_T_T2_END
};
#define CONT_T_T_SIZE (CONT_T_T1_END-CONT_T_T1_BEGIN)

/* hypershot */
#define CONT_T_HS1_BEGIN CONT_T_T2_END
enum {
	CONT_T_HS1_JUMP=CONT_T_HS1_BEGIN,
	CONT_T_HS1_RUN,

	CONT_T_HS1_END
};
#define CONT_T_HS2_BEGIN CONT_T_HS1_END
enum {
	CONT_T_HS2_JUMP=CONT_T_HS2_BEGIN,
	CONT_T_HS2_RUN,

	CONT_T_HS2_END
};
#define CONT_T_HS_SIZE (CONT_T_HS1_END-CONT_T_HS1_BEGIN)

/* arkanoid */
#define CONT_T_ARK1_BEGIN CONT_T_HS2_END
enum {
	CONT_T_ARK1_BUTTON=CONT_T_ARK1_BEGIN,

	CONT_T_ARK1_END
};
#define CONT_T_ARK2_BEGIN CONT_T_ARK1_END
enum {
	CONT_T_ARK2_BUTTON=CONT_T_ARK2_BEGIN,

	CONT_T_ARK2_END
};
#define CONT_T_ARK_SIZE (CONT_T_ARK1_END-CONT_T_ARK1_BEGIN)

/* keyboard */
#define CONT_T_K_BEGIN CONT_T_ARK2_END
enum {
	CONT_T_K_0=CONT_T_K_BEGIN,
	CONT_T_K_1,
	CONT_T_K_2,
	CONT_T_K_3,
	CONT_T_K_4,
	CONT_T_K_5,
	CONT_T_K_6,
	CONT_T_K_7,
	CONT_T_K_8,
	CONT_T_K_9,

	CONT_T_K_A,
	CONT_T_K_B,
	CONT_T_K_C,
	CONT_T_K_D,
	CONT_T_K_E,
	CONT_T_K_F,
	CONT_T_K_G,
	CONT_T_K_H,
	CONT_T_K_I,
	CONT_T_K_J,
	CONT_T_K_K,
	CONT_T_K_L,
	CONT_T_K_M,
	CONT_T_K_N,
	CONT_T_K_O,
	CONT_T_K_P,
	CONT_T_K_Q,
	CONT_T_K_R,
	CONT_T_K_S,
	CONT_T_K_T,
	CONT_T_K_U,
	CONT_T_K_V,
	CONT_T_K_W,
	CONT_T_K_X,
	CONT_T_K_Y,
	CONT_T_K_Z,

	CONT_T_K_F1,
	CONT_T_K_F2,
	CONT_T_K_F3,
	CONT_T_K_F4,
	CONT_T_K_F5,

	CONT_T_K_TILDE,
	CONT_T_K_MINUS,
	CONT_T_K_EQUALS,
	CONT_T_K_LBRACKET,
	CONT_T_K_RBRACKET,
	CONT_T_K_BACKSLASH,
	CONT_T_K_SEMICOLON,
	CONT_T_K_QUOTE,
	CONT_T_K_COMMA,
	CONT_T_K_PERIOD,
	CONT_T_K_SLASH,
	CONT_T_K_ACCENT,	/* right area, near enter */

	CONT_T_K_SPACE,
	CONT_T_K_TAB,
	CONT_T_K_RETURN,
	CONT_T_K_HOME,		/* upper right area */
	CONT_T_K_INS,		/* upper right area */
	CONT_T_K_DEL,		/* upper right area */
	CONT_T_K_BACKSPACE,
	CONT_T_K_ESC,
	CONT_T_K_STOP,		/* upper right area */
	CONT_T_K_SELECT,	/* right area, usually upper right */

	CONT_T_K_UP,
	CONT_T_K_DOWN,
	CONT_T_K_LEFT,
	CONT_T_K_RIGHT,

	CONT_T_K_SHIFT,
	CONT_T_K_CAPS,
	CONT_T_K_CTRL,
	CONT_T_K_GRAPH,		/* left of spacebar */
	CONT_T_K_CODE,		/* right of spacebar, some MSXes have 2 */

	CONT_T_K_N0,
	CONT_T_K_N1,
	CONT_T_K_N2,
	CONT_T_K_N3,
	CONT_T_K_N4,
	CONT_T_K_N5,
	CONT_T_K_N6,
	CONT_T_K_N7,
	CONT_T_K_N8,
	CONT_T_K_N9,
	CONT_T_K_NCOMMA,
	CONT_T_K_NPERIOD,
	CONT_T_K_NPLUS,
	CONT_T_K_NMINUS,
	CONT_T_K_NASTERISK,
	CONT_T_K_NSLASH,

	CONT_T_K_TORIKESHI,	/* left against spacebar */
	CONT_T_K_JIKKOU,	/* right against spacebar */

	/* extra keyboard keys */
	CONT_T_K_SONYPAUSE,	/* upper right area for sony, lower right area for tR */

	CONT_T_K_END
};
#define CONT_T_MAX CONT_T_K_END

/* axes */
/* mouse */
#define CONT_A_M1_BEGIN 0
enum {
	CONT_A_M1_X=CONT_A_M1_BEGIN,
	CONT_A_M1_Y,

	CONT_A_M1_END
};
#define CONT_A_M2_BEGIN CONT_A_M1_END
enum {
	CONT_A_M2_X=CONT_A_M2_BEGIN,
	CONT_A_M2_Y,

	CONT_A_M2_END
};
#define CONT_A_M_SIZE (CONT_A_M1_END-CONT_A_M1_BEGIN)

/* trackball */
#define CONT_A_T1_BEGIN CONT_A_M2_END
enum {
	CONT_A_T1_X=CONT_A_T1_BEGIN,
	CONT_A_T1_Y,

	CONT_A_T1_END
};
#define CONT_A_T2_BEGIN CONT_A_T1_END
enum {
	CONT_A_T2_X=CONT_A_T2_BEGIN,
	CONT_A_T2_Y,

	CONT_A_T2_END
};
#define CONT_A_T_SIZE (CONT_A_T1_END-CONT_A_T1_BEGIN)

/* arkanoid */
#define CONT_A_ARK1_BEGIN CONT_A_T2_END
enum {
	CONT_A_ARK1_DIAL=CONT_A_ARK1_BEGIN,

	CONT_A_ARK1_END
};
#define CONT_A_ARK2_BEGIN CONT_A_ARK1_END
enum {
	CONT_A_ARK2_DIAL=CONT_A_ARK2_BEGIN,

	CONT_A_ARK2_END
};
#define CONT_A_ARK_SIZE (CONT_A_ARK1_END-CONT_A_ARK1_BEGIN)
#define CONT_A_MAX CONT_A_ARK2_END

/* extra key/state */
#define CONT_EKEY_SONYPAUSE		1
#define CONT_ESTATE_SONYPAUSE	1
int __fastcall cont_get_keyextra(void);
int __fastcall cont_get_stateextra(void);
void __fastcall cont_set_keyextra(int);
void __fastcall cont_set_stateextra(int);

void cont_init(void);
void cont_settings_save(void);
void cont_clean(void);
void cont_poweron(void);
void __fastcall cont_update(void);
void __fastcall cont_new_frame(void);
u32 cont_get_port(u32);
u32 cont_get_da(u32);

const char* cont_get_name(u32);
const char* cont_get_a_name(u32);
const char* cont_get_trigger_info(u32);
const char* cont_get_axis_info(u32);
const char* cont_get_region_name(u32);
const char* cont_get_region_shortname(u32);
int cont_get_auto_region(void);
int cont_get_region(void);
void cont_set_region(u32);

u8 __fastcall cont_read(void);
void __fastcall cont_write(u8);

void cont_mute_sonypause(signed short*,int);
void cont_set_sonypause(int);
void cont_reset_sonypause(void);
void cont_check_sonypause(void);

void cont_insert(int,int,int,int);
void cont_user_insert(UINT);

u8 __fastcall cont_get_joystate(u8);
u8 __fastcall cont_get_keyrow(u8);
int __fastcall cont_check_keypress(void);

int __fastcall cont_state_get_version(void);
int __fastcall cont_state_get_size(void);
void __fastcall cont_state_save(u8**);
void __fastcall cont_state_load_cur(u8**);
int __fastcall cont_state_load(int,u8**);

int __fastcall cont_movie_get_size(void);
void __fastcall cont_movie_put(u8*);
void __fastcall cont_movie_get(u8*);

#endif /* CONT_H */
