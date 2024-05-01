/******************************************************************************
 *                                                                            *
 *                                "input.h"                                   *
 *                                                                            *
 ******************************************************************************/

#ifndef INPUT_H
#define INPUT_H

/* emulator shortcut triggers */
#define INPUT_T_BEGIN 0
enum {
	INPUT_T_HARDRESET=INPUT_T_BEGIN,
	INPUT_T_SOFTRESET,
	INPUT_T_MEDIA,
	INPUT_T_TAPEREWIND,
	INPUT_T_GRABMOUSE,
	INPUT_T_EXIT,
	INPUT_T_FULLSCREEN,
	INPUT_T_SCREENSHOT,
	INPUT_T_PAUSE,
	INPUT_T_VIDEOFORMAT,
	INPUT_T_PASTE,
	INPUT_T_PASTEBOOT,
	INPUT_T_PREVSLOT,
	INPUT_T_NEXTSLOT,
	INPUT_T_SAVESTATE,
	INPUT_T_LOADSTATE,
	INPUT_T_REVERSE,
	INPUT_T_CONTINUE,
	INPUT_T_SPEEDUP,
	INPUT_T_SLOWDOWN,
	INPUT_T_DJHOLD,
	INPUT_T_CPUSPEEDMIN,
	INPUT_T_CPUSPEEDPLUS,
	INPUT_T_Z80TRACER,
	INPUT_T_HELP,
	INPUT_T_DISABLE,

	INPUT_T_END
};

/* and axes */
#define INPUT_A_BEGIN 0
enum {
	INPUT_A_DJSPIN=INPUT_A_BEGIN,

	INPUT_A_END
};


void input_init(void);
void input_affirm_ports(void);
void input_clean(void);
void input_tick(void);
void input_read(void);
void input_read_shortcuts(void);
void input_update(void);
int input_menukey(int);
void __fastcall input_update_leds(void);
void input_settings_save(void);
int input_get_joystick_analog_max(void);
const char* input_get_trigger_info(u32);
const char* input_get_trigger_set(u32);
int input_set_trigger(const u32,const char*,u32);
int __fastcall input_trigger_held(int);
const char* input_get_axis_info(u32);
const char* input_get_axis_set(u32);
int input_set_axis(const u32,const char*);
long __fastcall input_get_axis(int);

void input_doubleclick(void);
int input_mouse_in_client(HWND,HWND,POINT*,int);
int input_get_mouse_cursor_grabbed(void);
void input_set_mouse_cursor_grabbed(int);
void __fastcall input_mouse_cursor_update(void);
void input_mouse_cursor_reset_ticks(void);
void input_cpuspeed_ticks_stall(void);


#endif /* INPUT_H */
