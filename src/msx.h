/******************************************************************************
 *                                                                            *
 *                                 "msx.h"                                    *
 *                                                                            *
 ******************************************************************************/

#ifndef MSX_H
#define MSX_H

enum {
	MSX_PRIORITY_NORMAL=0,
	MSX_PRIORITY_ABOVENORMAL,
	MSX_PRIORITY_HIGHEST,

	MSX_PRIORITY_AUTO,

	MSX_PRIORITY_MAX
};

typedef struct {
	u32 bios_crc;
	u32 cart1_crc;
	u32 cart2_crc;
	u8 cart1_uid;
	u8 cart2_uid;
	u16 cpu_speed;			/* 25-9999 */
	u8 region;
	u8 vdpchip_uid;
	u8 psgchip_uid;
	u32 flags;

} _msx_state;

/* flags */
#define MSX_FLAG_VF			1	/* 0: NTSC, 1: PAL */
#define MSX_FLAG_5060		2	/* 50/60hz hack, 0: no, 1: yes */
#define MSX_FLAG_5060A		4	/* 50/60hz auto, 0: no, 1: yes */

/* ram slot (inverted) */
#define MSX_FSHIFT_RAMSLOT	3
#define MSX_FMASK_RAMSLOT	(3<<MSX_FSHIFT_RAMSLOT)


void msx_init(void);
void msx_clean(void);
int msx_start(void);
void msx_stop(int);
void __fastcall msx_frame(int);
int __fastcall msx_is_running(void);
void __fastcall msx_set_main_interrupt(void);
void __fastcall msx_set_priority(int);
int msx_get_priority(void);
int msx_get_priority_auto(void);
const char* msx_get_priority_name(u32);
int msx_get_ignore_loadstate_crc(void);
void __fastcall msx_set_paused(int);
int __fastcall msx_get_paused(void);
int msx_get_frame_advance(void);
void msx_check_frame_advance(void);
void msx_wait(void);
void msx_wait_done(void);
void msx_reset(int);

void __fastcall msx_get_state(_msx_state*);
int __fastcall msx_state_get_version(void);
int __fastcall msx_state_get_size(void);
void __fastcall msx_state_save(u8**);
int __fastcall msx_state_load(int,u8**,_msx_state*,char*);

#endif /* MSX_H */
