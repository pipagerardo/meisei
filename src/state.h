/******************************************************************************
 *                                                                            *
 *                                "state.h"                                   *
 *                                                                            *
 ******************************************************************************/

#ifndef STATE_H
#define STATE_H

#define STATE_HEADER			"meisei state"

#define STATE_MIN_SIZE			82199
#define STATE_MAX_SIZE			(1131424+0x10000) /* 2*Mega Flash ROM SCC + 64KB overhead */
#define STATE_SLOT_CUSTOM		16
#define STATE_SLOT_MEMORY		17

#define STATE_ID_STATE			0
#define STATE_ID_MOVIE			1

#define STATE_ID_Z80			1
#define STATE_ID_PSG			2
#define STATE_ID_IO				3
#define STATE_ID_MSX			4
#define STATE_ID_VDP			5
#define STATE_ID_CONT			6
#define STATE_ID_MAPPER			7
#define STATE_ID_TAPE			8
#define STATE_ID_END			0xfe

/* 1/2/3/4 bytes */
#define STATE_SAVE_1(x)			**s=(x); (*s)++
#define STATE_LOAD_1(x)			(x)=**s; (*s)++
#define STATE_SAVE_2(x)			**s=(x)&0xff; (*s)++; **s=(x)>>8&0xff; (*s)++
#define STATE_LOAD_2(x)			STATE_LOAD_1(x); (x)|=((**s)<<8); (*s)++
#define STATE_SAVE_3(x)			STATE_SAVE_2(x); **s=(x)>>16&0xff; (*s)++
#define STATE_LOAD_3(x)			STATE_LOAD_2(x); (x)|=((**s)<<16); (*s)++
#define STATE_SAVE_4(x)			STATE_SAVE_3(x); **s=(x)>>24&0xff; (*s)++
#define STATE_LOAD_4(x)			STATE_LOAD_3(x); (x)|=((**s)<<24); (*s)++

/* custom */
#define STATE_SAVE_C(x,size)	memcpy(*s,x,size); (*s)+=(size)
#define STATE_LOAD_C(x,size)	memcpy(x,*s,size); (*s)+=(size)


void state_reset_custom_name(void);
int state_get_filterindex(void);
int state_set_custom_name_save(void);
int state_set_custom_name_open(void);

void state_save(void);
void state_load(void);
void state_init(void);
void state_clean(void);

char* state_get_slotdesc(int,char*);
int state_get_slot(void);
void state_set_slot(int,int);
void* state_get_msxstate(void);

#endif /* STATE_H */
