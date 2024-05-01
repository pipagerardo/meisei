/******************************************************************************
 *                                                                            *
 *                               "netplay.h"                                  *
 *                                                                            *
 ******************************************************************************/

#ifndef NETPLAY_H
#define NETPLAY_H

void netplay_init( void );
void netplay_clean( void );
int  __fastcall netplay_is_active( void );
int  netplay_keyboard_enabled( void );
void netplay_open( void );
void netplay_frame( u8* key, int* keyextra, u8* joy );
int  WINAPI netplay_starts( char* name, int player, int numplayers );
void WINAPI netplay_drop( char* name, int player );

#endif /* NETPLAY_H */
