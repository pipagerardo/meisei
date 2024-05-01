/******************************************************************************
 *                                                                            *
 *                              "am29f040b.h"                                 *
 *                                                                            *
 * Usado por mapper.c para leer y escribir el chip am29f0b de los cartuchos.  *
 * 4 Megabit (512 K x 8-Bit) CMOS 5.0 Volt-only, Uniform Sector Flash Memory  *
 ******************************************************************************/

#ifndef AM29F040B_H
#define AM29F040B_H

#define AM29F_UNPROTECT		1

#define AM29F_AUTOSELECT	1
#define AM29F_UNLOCK1		2
#define AM29F_UNLOCK2		4
#define AM29F_WAIT1			8
#define AM29F_WAIT2			0x10
#define AM29F_PROGRAM		0x20

typedef struct {
	int mode;
	int inc;
	int readsector;
	int writesector;
	int unprotect[8];

	u8* data;

} _am29f;

void __fastcall am29f_write( _am29f* chip, u16 a, u8 v );
u8   __fastcall am29f_read(  _am29f* chip, u16 a );

void __fastcall am29f_reset( _am29f* chip );
_am29f*         am29f_init( int initmem );
void            am29f_clean( _am29f* chip );

int  __fastcall am29f_state_get_size( void );
void __fastcall am29f_state_save( _am29f* chip, u8** s );
void __fastcall am29f_state_load_cur( _am29f* chip, u8** s );
int  __fastcall am29f_state_load( _am29f* chip, int v, u8** s );

#endif /* AM29F040B_H */
