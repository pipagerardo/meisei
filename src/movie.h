/******************************************************************************
 *                                                                            *
 *                                "movie.h"                                   *
 *                                                                            *
 ******************************************************************************/

#ifndef MOVIE_H
#define MOVIE_H

int __fastcall movie_get_active_state(void);
int __fastcall movie_is_playing(void);
int __fastcall movie_is_recording(void);
int movie_get_timecode(void);
void movie_set_timecode(int);

void movie_reset_custom_name(void);
int movie_get_filterindex(void);
int movie_set_custom_name_open(void);
int movie_confirm_exit(void);
int movie_set_custom_name_save(int);

void movie_play(int);
void movie_record(void);
void movie_stop(int);
void __fastcall movie_frame(void);
void __fastcall movie_reverse_frame(void);

void movie_init(void);
void movie_clean(void);

#endif /* MOVIE_H */
