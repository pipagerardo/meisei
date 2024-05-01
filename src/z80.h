/******************************************************************************
 *                                                                            *
 *                                 "z80.h"                                    *
 *                                                                            *
 ******************************************************************************/

#ifndef Z80_H
#define Z80_H

/* enable tracer (default key: TAB) */
#define Z80_TRACER 0
void __fastcall z80_set_tracer(int);

void z80_init(void);
void z80_reset(void);
void z80_execute(int);
void z80_clean(void);
void z80_poweron(void);
void z80_new_frame(void);
void z80_end_frame(void);

void z80_fill_op_cycles_lookup(void);
void z80_adjust_rel_cycles(void);
int* __fastcall z80_get_cycles_ptr(void);
int __fastcall z80_get_rel_cycles(void);
void __fastcall z80_set_cycles(int);
int __fastcall z80_get_cycles(void);
void __fastcall z80_steal_cycles(int);

void __fastcall z80_irq_ack(void);
void __fastcall z80_irq_next(int);
int __fastcall z80_irq_pending(void);

int __fastcall z80_get_busack(void);
void __fastcall z80_set_busack(int);
int __fastcall z80_get_busrq(void);
int __fastcall z80_set_busrq(int);

void* __fastcall z80_get_tracer_fd(void);
int __fastcall z80_get_pc(void);
int __fastcall z80_get_f(void);
int __fastcall z80_get_a(void);
void __fastcall z80_set_f(int);
void __fastcall z80_set_a(int);
void __fastcall z80_di(void);

int __fastcall z80_state_get_version(void);
int __fastcall z80_state_get_size(void);
void __fastcall z80_state_save(u8**);
void __fastcall z80_state_load_cur(u8**);
int __fastcall z80_state_load(int,u8**);

#endif /* Z80_H */
