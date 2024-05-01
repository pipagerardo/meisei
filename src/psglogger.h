/******************************************************************************
 *                                                                            *
 *                              "psglogger.h"                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef PSGLOGGER_H
#define PSGLOGGER_H

enum {
	PSGLOGGER_TYPE_BIN=1,
	PSGLOGGER_TYPE_MID,
	PSGLOGGER_TYPE_YM
};

int __fastcall psglogger_is_active(void);
int psglogger_get_interleaved(void);
int psglogger_get_trimsilence(void);
int psglogger_get_fi(void);

int psglogger_start(HWND,HWND*);
int psglogger_stop(int);

void __fastcall psglogger_frame(void);
void __fastcall psglogger_reverse_2frames(void);

void psglogger_init(void);
void psglogger_clean(void);

#endif /* PSGLOGGER_H */
