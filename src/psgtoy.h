/******************************************************************************
 *                                                                            *
 *                               "psgtoy.h"                                   *
 *                                                                            *
 ******************************************************************************/

#ifndef PSGVIEW_H
#define PSGVIEW_H

const char* psgtoy_get_preset_name(u32);
int __fastcall psgtoy_get_act_notepos(u32);
int psgtoy_get_open_fi(void);
int psgtoy_get_save_fi(void);
int psgtoy_get_centcorrection(void);
int psgtoy_get_custom_enabled(void);
int psgtoy_get_custom_preset(u32);
int __fastcall psgtoy_get_custom_amplitude(u32);
int __fastcall psgtoy_reset_custom_wave_changed(u32);
u8* psgtoy_get_custom_wave_ptr(u32);
void psgtoy_get_custom_wave(u32,u8*);
void psgtoy_get_custom_wave_string(u32,char*);
INT_PTR CALLBACK psgtoy_window( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam );
void psgtoy_init(void);
void psgtoy_clean(void);

#endif /* PSGVIEW_H */
