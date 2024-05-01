/******************************************************************************
 *                                                                            *
 *                                 "psg.h"                                    *
 *                                                                            *
 ******************************************************************************/

#ifndef PSG_H
#define PSG_H

enum {
	PSG_CHIPTYPE_AY8910=0,
	PSG_CHIPTYPE_YM2149,

	PSG_CHIPTYPE_MAX
};

#define PSG_CHIPTYPE_DEFAULT	PSG_CHIPTYPE_YM2149

const char* psg_get_chiptype_name(u32);
void psg_set_chiptype(u32);
int psg_get_chiptype(void);
int psg_get_chiptype_uid(u32);
int psg_get_uid_chiptype(u32);
int __fastcall psg_get_reg(u32);
int __fastcall psg_get_address(void);
int __fastcall psg_get_e_vol(void);

void __fastcall psg_write_address(u8);
u8 __fastcall psg_read_data(void);
void __fastcall psg_write_data(u8);
void psg_set_buffer_counter(int);
int psg_get_buffer_counter(void);
int psg_get_cc(void);

void psg_custom_enable(int);

void psg_new_frame(void);
void psg_end_frame(void);
void psg_init_sound(void);

void psg_init(void);
void psg_init_amp(void);
void psg_clean(void);
void psg_poweron(void);

int __fastcall psg_state_get_version(void);
int __fastcall psg_state_get_size(void);
void __fastcall psg_state_save(u8**);
void __fastcall psg_state_load_cur(u8**);
void __fastcall psg_state_load_1(u8**);
int __fastcall psg_state_load(int,u8**);

#endif /* PSG_H */
