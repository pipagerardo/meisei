/******************************************************************************
 *                                                                            *
 *                                "sound.h"                                   *
 *                                                                            *
 ******************************************************************************/

#ifndef SOUND_H
#define SOUND_H

#define SOUND_TICKS_LEN 1460 /* about 60hz max */

int* sound_create_channel(void);
int* sound_create_dac(void);
void sound_clean_dac(int*);
void sound_compress(void);
void sound_write(void);
void sound_capt_read(void);
int sound_init(void);
int sound_create_capture(void);
void sound_clean_capture(void);
void sound_create_secondary_buffer(int);
void sound_clean_secondary_buffer(void);
void sound_clean(void);
void sound_settings_save(void);
void sound_set_write_cursor(DWORD);
void sound_reset_write_cursor(void);
void sound_reset_capt_cursor(void);
void sound_reset_lostframes(void);
void sound_silence(int);
void sound_create_ll(void);
void sound_clean_ll(void);
u32 sound_get_sleeptime_raw(void);
DWORD sound_get_buffer_secondary_size(void);
int sound_get_buffer_size(void);
s16* sound_get_buffer(void);
int sound_get_lostframes(void);
void __fastcall sound_close_sleepthread(void);
int sound_get_capt_threshold(void);
int sound_get_sleep_auto(void);
void sound_sleep(u32);
int sound_get_slept(void);
void sound_reset_slept(void);
void sound_new_frame(void);
int sound_get_slept_prev(void);
int sound_get_num_samples(void);
DWORD sound_get_playposition(void);
void* sound_create_sample(const void*,int,int);
void sound_clean_sample(void*);
void sound_play_sample(void*,int);
void sound_stop_sample(void*,int);
void sound_set_enabled(int);
int sound_get_enabled(void);
int sound_get_ticks(void);
void sound_set_ticks(int);
int sound_get_ticks_prev(void);
void sound_set_ticks_prev(int);
int sound_get_num_samples_prev(void);
void sound_set_num_samples_prev(int);

#endif /* SOUND_H */
