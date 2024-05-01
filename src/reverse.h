/******************************************************************************
 *                                                                            *
 *                               "reverse.h"                                  *
 *                                                                            *
 ******************************************************************************/

#ifndef REVERSE_H
#define REVERSE_H

#define REVERSE_BUFFER_SIZE_MAX 6

void reverse_init(void);
void reverse_clean(void);
void __fastcall reverse_invalidate(void);
void reverse_frame(void);
void reverse_cont_update(void);

int reverse_is_enabled(void);
int reverse_get_state_size(void);
int reverse_get_active_state(void);
int reverse_get_buffer_size(void);
int reverse_get_buffer_size_size(u32);
int reverse_get_nolimit(void);
void reverse_set_enable(u32);
void reverse_set_buffer_size(u32);

#endif /* REVERSE_H */
