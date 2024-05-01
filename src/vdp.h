/******************************************************************************
 *                                                                            *
 *                                 "vdp.h"                                    *
 *                                                                            *
 ******************************************************************************/

#ifndef VDP_H
#define VDP_H

/* debug printf too fast mid-screen VRAM accesses */
#define VDP_FASTACCESS 0

enum {
	/* don't change order */
	VDP_CHIPTYPE_TMS9118=0,	/* or 9128 */
	VDP_CHIPTYPE_TMS9129,
	VDP_CHIPTYPE_TMS9918,	/* or 9928 */
	VDP_CHIPTYPE_TMS9929,
	VDP_CHIPTYPE_T7937A,
	VDP_CHIPTYPE_T6950,

	VDP_CHIPTYPE_MAX
};

#define VDP_CHIPTYPE_DEFAULT VDP_CHIPTYPE_TMS9129

const char* vdp_get_chiptype_name(u32);
int __fastcall vdp_get_chiptype(void);
void vdp_set_chiptype(u32);
void vdp_set_chiptype_vf(int);
int vdp_get_chiptype_vf(int);
int vdp_get_chiptype_uid(u32);
int vdp_get_uid_chiptype(u32);

int vdp_get_bgc(void);
void vdp_set_bgc(int);
int vdp_get_bg_enabled(void);
void vdp_set_bg_enabled(int);
int vdp_get_spr_enabled(void);
void vdp_set_spr_enabled(int);
int vdp_get_spr_unlim(void);
void vdp_set_spr_unlim(int);

void __fastcall vdp_whereami(char*);
void __fastcall vdp_z80toofastvram(void);

void vdp_init(void);
void vdp_clean(void);
#define VDP_UPLOAD_REG 0x40000000
int vdp_upload(u32,u8*,u32);
void __fastcall vdp_vblank(void);
void __fastcall vdp_line(int);
void vdp_reset(void);
void vdp_poweron(void);
void vdp_new_frame(void);
void vdp_end_frame(void);

int vdp_luminoise_get(void);
int vdp_luminoise_get_volume(void);
void vdp_luminoise_set(int);

int __fastcall vdp_get_address(void);
int __fastcall vdp_get_reg(u32);
int __fastcall vdp_get_status(void);
int __fastcall vdp_get_statuslow(void);
u8* __fastcall vdp_get_ram(void);
void __fastcall vdp_reset_vblank(void);

void __fastcall vdp_write_data(u8);
void __fastcall vdp_write_address(u8);
u8 __fastcall vdp_read_data(void);
u8 __fastcall vdp_read_status(void);

int __fastcall vdp_state_get_version(void);
int __fastcall vdp_state_get_size(void);
void __fastcall vdp_state_save(u8**);
void __fastcall vdp_state_load_cur(u8**);
int __fastcall vdp_state_load(int,u8**);

#endif /* VDP_H */
