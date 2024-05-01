/******************************************************************************
 *                                                                            *
 *                                "sample.h"                                  *
 *                                                                            *
 ******************************************************************************/

#ifndef SAMPLE_H
#define SAMPLE_H

#define SAMPLE_MAX 0x20

typedef struct {
	s16* sam[SAMPLE_MAX];
	u32 len[SAMPLE_MAX];

	int cur;
	int bc;
	int playing;
	int next;
	int invalid;
} _sample;


_sample* sample_init(void);
void sample_clean(_sample*);
void sample_load(_sample*,int,const char*);

void __fastcall sample_play(_sample*,int,int);
void __fastcall sample_stop(_sample*);
void sample_stream(_sample*,int,signed short*,int);

int __fastcall sample_state_get_size(void);
void __fastcall sample_state_save(_sample*,u8**);
void __fastcall sample_state_load_cur(_sample*,u8**);
int __fastcall sample_state_load(_sample*,int,u8**);

#endif /* SAMPLE_H */
