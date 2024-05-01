/******************************************************************************
 *                                                                            *
 *                               "mapper.h"                                   *
 *                                                                            *
 ******************************************************************************/

#ifndef MAPPER_H
#define MAPPER_H

#define ROM_MAXPAGES		0x200 /* max 8KB pages */
#define ROM_MAXSIZE			(ROM_MAXPAGES*0x2000)
#define ROMPAGE_DUMMY_U		(ROM_MAXPAGES+2)
#define ROMPAGE_DUMMY_FF	(ROM_MAXPAGES+3)

enum {
	CARTTYPE_QURAN=0,
	CARTTYPE_ASCII,
	CARTTYPE_BTL80,
	CARTTYPE_BTL90,
	CARTTYPE_BTL126,
	CARTTYPE_CROSSBLAIM,
	CARTTYPE_IREMTAMS1,
	CARTTYPE_GAMEMASTER2,
	CARTTYPE_KONAMISCC,
	CARTTYPE_KONAMISCCI,
	CARTTYPE_KONAMISYN,
	CARTTYPE_KONAMIVRC,
	CARTTYPE_MATRAINK,
	CARTTYPE_HARRYFOX,
	CARTTYPE_NOMAPPER,
	CARTTYPE_PAC,
	CARTTYPE_MFLASHSCC,
	CARTTYPE_PLAYBALL,
	CARTTYPE_DSK2ROM,

	CARTTYPE_MAX
};

#define CARTUID_MAX			28

#define MCF_SRAM			1
#define MCF_EMPTY			2
#define MCF_EXTRA			4

typedef __fastcall u8(*fp_mapread)(u16);
extern fp_mapread mapread[8];                   /* 64KB (8*8KB) Z80 memmap */
typedef __fastcall void(*fp_mapwrite)(u16,u8);
extern fp_mapwrite mapwrite[8];

void mapper_init(void);
void mapper_clean(void);
int mapper_init_ramslot(int,int);
void mapper_flush_ram(void);
void mapper_set_buffer_counter(int);
void mapper_sound_stream(signed short*,int);
void mapper_new_frame(void);
void mapper_end_frame(void);

void __fastcall mapper_write_pslot(u8);
u8 __fastcall mapper_read_pslot(void);

void __fastcall mapper_refresh_cb(int);
void __fastcall mapper_refresh_pslot_read(void);
void __fastcall mapper_refresh_pslot_write(void);

void mapper_open_bios(const char*);
void mapper_update_bios_hack(void);
int mapper_open_cart(int,const char*,int);
void mapper_close_cart(int);
void mapper_init_cart(int);
void mapper_set_carttype(int,u32);
int mapper_get_carttype(int);
u32 mapper_get_cartcrc(int);
u32 mapper_get_bioscrc(void);
int mapper_get_autodetect_rom(void);
int mapper_get_cartsize(int);
int mapper_get_ramsize(void);
int mapper_get_ramslot(void);
int mapper_get_default_ramsize(void);
int mapper_get_default_ramslot(void);
u32 mapper_autodetect_cart(int,const char*);

void mapper_extra_configs_reset(int);
void mapper_extra_configs_apply(int);
void mapper_extra_configs_revert(int);
int mapper_extra_configs_differ(int,int);
void mapper_get_extra_configs(int,int,u32*);
void mapper_extra_configs_dialog(HWND,int,int,int,int);

void mapper_log_type(int,int);
int mapper_get_type_uid(u32);
int mapper_get_uid_type(u32);
const char* mapper_get_type_longname(u32);
const char* mapper_get_type_shortname(u32);
u32 mapper_get_type_flags(u32);
char* mapper_get_current_name(char*);
const char* mapper_get_file(int);
const char* mapper_get_defaultbios_file(void);
void mapper_get_defaultbios_name(char*);
const char* mapper_get_bios_file(void);
const char* mapper_get_bios_name(void);

int mapper_get_mel_error(void);
int __fastcall mapper_state_get_version(void);
int __fastcall mapper_state_get_size(void);
void __fastcall mapper_state_save(u8**);
void __fastcall mapper_state_load_cur(u8**);
int __fastcall mapper_state_load(int,u8**);

#endif /* MAPPER_H */
