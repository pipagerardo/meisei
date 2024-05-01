/******************************************************************************
 *                                                                            *
 *                                  "io.h"                                    *
 *                                                                            *
 ******************************************************************************/

#ifndef IO_H
#define IO_H

/* test I/O complies to MSX standard (debug printf), almost all Japanese ROMs from the 80s pass the test */
#define COMPLY_STANDARDS_TEST	0
#define COMPLY_STANDARDS_VDPR	0x9c	/* VDP rd port in BIOS 06 (normally $98) */
#define COMPLY_STANDARDS_VDPW	0x9e	/* VDP wr port in BIOS 07 (normally $98) */

typedef __fastcall u8(*fp_ioread)(void);
extern fp_ioread ioread[0x100]; /* 256 z80 io ports */

typedef __fastcall void(*fp_iowrite)(u8);
extern fp_iowrite iowrite[0x100];

void __fastcall io_setreadport(u8,fp_ioread);
void __fastcall io_setwriteport(u8,fp_iowrite);

u8 __fastcall ioreadnull(void);
void __fastcall iowritenull(u8);

u8 __fastcall io_read_key(void);
u8 __fastcall io_read_ppic(void);
void __fastcall io_write_ppic(u8);
void __fastcall io_write_ppicontrol(u8);

int io_get_click_buffer_counter(void);
void io_set_click_buffer_counter(int);
void io_new_frame(void);
void io_end_frame(void);
void io_init_click_sound(void);

void io_init(void);
void io_clean(void);
void io_poweron(void);

int __fastcall io_state_get_version(void);
int __fastcall io_state_get_size(void);
void __fastcall io_state_save(u8**);
void __fastcall io_state_load_cur(u8**);
int __fastcall io_state_load(int,u8**);

#endif /* IO_H */
