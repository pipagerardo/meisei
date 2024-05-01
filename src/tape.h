/******************************************************************************
 *                                                                            *
 *                                "tape.h"                                    *
 *                                                                            *
 ******************************************************************************/

#ifndef TAPE_H
#define TAPE_H

enum {
	CAS_TYPE_BIN=0,
	CAS_TYPE_BAS,
	CAS_TYPE_ASC,
	CAS_TYPE_CUS,
	CAS_TYPE_NOP,
	CAS_TYPE_END,

	CAS_TYPE_MAX
};

void tape_init(void);
void tape_clean(void);

int __fastcall tape_is_inserted(void);
int __fastcall tape_get_runtime_block_inserted(void);
void tape_set_runtime_block_inserted(int);
char* tape_get_fn_cur(void);
char* tape_get_fn_od(void);
u32 tape_get_crc_od(void);
u32 tape_get_crc_cur(void);
int tape_get_pos_cur(void);
int tape_get_cur_block_cur(void);
int tape_get_cur_block_od(void);
int tape_get_blocktype_cur(void);
int tape_get_updated_od(void);
void tape_set_updated_od(void);
int tape_cur_block_od_is_empty(void);

void tape_cur_to_od(void);
void tape_od_to_cur(void);
int tape_open_od(int);
int tape_save_od(void);
int tape_new_od(HWND);
void tape_clean_od(void);
void tape_rewind_cur(void);
void tape_set_cur_block_od(u32);
void tape_flush_cache_od(void);

void tape_init_gui(LONG_PTR);
void tape_draw_gui(HWND);
BOOL CALLBACK tape_sub_listbox( HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam );
void tape_draw_listbox(HWND);
void tape_draw_pos(HWND);
void tape_log_info(void);
void tape_set_popup_pos(void);
INT_PTR CALLBACK tape_blockinfo_dialog( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam );

void __fastcall tape_read_header(void);
void __fastcall tape_read_byte(void);
void __fastcall tape_write_header(void);
void __fastcall tape_write_byte(void);

int  __fastcall tape_state_get_version(void);
int  __fastcall tape_state_get_size(void);
void __fastcall tape_state_save(u8**);
void __fastcall tape_state_load_cur(u8**);
int  __fastcall tape_state_load(int,u8**);

#endif /* TAPE_H */
