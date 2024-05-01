/******************************************************************************
 *                                                                            *
 *                               "reverse.c"                                  *
 *                                                                            *
 ******************************************************************************/

#include "global.h"
#include "z80.h"
#include "vdp.h"
#include "io.h"
#include "psg.h"
#include "mapper.h"
#include "msx.h"
#include "cont.h"
#include "input.h"
#include "sound.h"
#include "draw.h"
#include "netplay.h"
#include "settings.h"
#include "reverse.h"
#include "movie.h"
#include "crystal.h"
#include "psglogger.h"
#include "tape.h"

/*

Reverse playback/realtime rewind, idea from a discussion we had on nesdev.
Data is uncompressed, memory usage is about 16MB + 1KB per frame.

*/

#define REV_LIMIT		0x80000 /* 0.5MB */

#define A_MASK			7
#define A_STATE_BUFFER	1
#define A_STATE_REVERSE	2
#define A_STATE_REPLAY	3
#define A_STATE_SWITCH	8

#define REV_STEP		100
#define FAR_MAX			200
static const int lut_size[REVERSE_BUFFER_SIZE_MAX]={5,25,50,100,150,FAR_MAX};

static _msx_state msxstate_prev;
static int cont_prev;
static int init_done=0;
static int reverse_enabled=0;
static int state_size;
static int near_index;
static int revbuf_index;
static int revbuf_size;
static int far_index;
static int far_index2;
static int far_max;
static int framecount;
static int frame_max;
static int reverse_canbuffer;
static int replay;
static int replay_frame;
static int active_state;
static int nolimit;

static u8* rev_near[REV_STEP*2];

static struct {
	u8 btn[REV_STEP][CONT_MAX_MOVIE_SIZE];
	u8* state;
} rev_far[FAR_MAX];

int reverse_get_active_state(void) { return active_state&A_MASK; }
int reverse_get_buffer_size(void) { return revbuf_size; }
int reverse_get_buffer_size_size(u32 i) { if (i>REVERSE_BUFFER_SIZE_MAX) i=0; return lut_size[i]; }
int reverse_get_nolimit(void) { return nolimit; }
int reverse_is_enabled(void) { return reverse_enabled; }
void reverse_set_enable(u32 i) { reverse_enabled=i&1; }
void reverse_set_buffer_size(u32 i) { revbuf_size=i; }

int reverse_get_state_size(void)
{
	return \
	 z80_state_get_size()	+
	 vdp_state_get_size()	+
	 cont_state_get_size()	+
	 io_state_get_size()	+
	 psg_state_get_size()	+
	 mapper_state_get_size()+
	 tape_state_get_size();
}

static __inline__ void state_load_dependencies(void)
{
	mapper_refresh_cb(0);
	mapper_refresh_cb(1);
	mapper_refresh_pslot_read();
	mapper_refresh_pslot_write();
}

void __fastcall reverse_invalidate(void)
{
	static int semaphore=FALSE;
	int i;

	if (semaphore||!init_done) return;
	semaphore=TRUE;

	for (i=0;i<REV_STEP*2;i++) { MEM_CLEAN(rev_near[i]); }
	for (i=0;i<FAR_MAX;i++) { MEM_CLEAN(rev_far[i].state); }

	if (!reverse_enabled) { semaphore=FALSE; return; }

	if (active_state) LOG(LOG_MISC|LOG_COLOUR(LC_GREEN)|LOG_TYPE(LT_REVERSE),"reverse invalidated ");

	far_max=lut_size[revbuf_size];
	frame_max=far_max*REV_STEP-REV_STEP;

	state_size=reverse_get_state_size();
	near_index=REV_STEP-1; far_index=1;
	revbuf_index=far_index2=framecount=reverse_canbuffer=replay=replay_frame=active_state=0;

	semaphore=FALSE;
}

void reverse_cont_update(void)
{
	switch (active_state&A_MASK) {
		case A_STATE_BUFFER:
			cont_movie_put(rev_far[(revbuf_index%REV_STEP)?far_index2:far_index].btn[revbuf_index%REV_STEP]);
			break;

		case A_STATE_REPLAY: {
			int check;

			if (stricmp(input_get_trigger_set(CONT_T_MAX+INPUT_T_CONTINUE),"nothing")==0) check=cont_check_keypress(); /* check for pressed keys */
			else check=input_trigger_held(CONT_T_MAX+INPUT_T_CONTINUE);

			if (check&&!movie_is_playing()) active_state=A_STATE_SWITCH;
			else

			/* rest same as reverse */
			cont_movie_put(rev_far[((near_index+1)%REV_STEP)?far_index:(far_index+1)%far_max].btn[(near_index+1)%REV_STEP]);
			break;
		}

		case A_STATE_REVERSE:
			/* (in case of reverse, only needed for possible sound effects (most notably, reverse keyclick would be silent otherwise), graphics are unaffected) */
			cont_movie_put(rev_far[((near_index+1)%REV_STEP)?far_index:(far_index+1)%far_max].btn[(near_index+1)%REV_STEP]);
			break;

		default: break;
	}
}

void reverse_frame(void)
{
	u8* state=NULL;
	_msx_state msxstate;
	int reversekey;

	if (!reverse_enabled||netplay_is_active()) return;

	/* invalidate needed? */
	msx_get_state(&msxstate);
	msxstate.region=0; msxstate.flags&=(MSX_FLAG_VF|MSX_FMASK_RAMSLOT); /* don't care */
	if (
        (u32)tape_get_runtime_block_inserted() || (u32)cont_prev != ( cont_get_port(CONT_P2) << 8u | cont_get_port(CONT_P1) ) ||
        (u32)memcmp( &msxstate_prev, &msxstate, sizeof(_msx_state) ) != 0u
     ) {
		reverse_invalidate();
		tape_set_runtime_block_inserted(FALSE);
		cont_prev=cont_get_port(CONT_P2)<<8|cont_get_port(CONT_P1);
		memcpy(&msxstate_prev,&msxstate,sizeof(_msx_state));
	}

	#define REV_SAVELOAD(x)			\
		z80_state_##x(&state);		\
		vdp_state_##x(&state);		\
		cont_state_##x(&state);		\
		io_state_##x(&state);		\
		psg_state_##x(&state);		\
		mapper_state_##x(&state);	\
		tape_state_##x(&state)

	reversekey=(crystal->dj_reverse&1)|(~crystal->dj_reverse>>1&input_trigger_held(CONT_T_MAX+INPUT_T_REVERSE));

	if (nolimit^1&&state_size>REV_LIMIT) {
		if (reversekey) LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_REVLIMIT),"no reverse, memory limit reached");
		return;
	}

	if (reversekey&&framecount>2) {
		/* backward */
		movie_reverse_frame();
		movie_reverse_frame();
		psglogger_reverse_2frames();

		framecount--;

		/* load far state */
		if (!(near_index%REV_STEP)) {
			far_index=far_index2;
			far_index2=(far_index2+(far_max-1))%far_max;

			state=rev_far[far_index2].state;
			revbuf_index=near_index;
			reverse_canbuffer=TRUE;

			/* copy possible lost state 1 */
			if (!rev_near[near_index]&&rev_far[far_index].state) {
				MEM_CREATE_N(rev_near[near_index],state_size)
				memcpy(rev_near[near_index],rev_far[far_index].state,state_size);
			}
		}

		/* copy possible lost state 2 */
		if (rev_far[far_index2].state) {
			int i=(near_index<REV_STEP)?REV_STEP:0;
			if (!rev_near[i]) {
				MEM_CREATE_N(rev_near[i],state_size)
				memcpy(rev_near[i],rev_far[far_index2].state,state_size);
			}
		}

		/* fill reverse buffer */
		if (reverse_canbuffer) {
			if (!state) state=rev_near[revbuf_index];
			revbuf_index=(revbuf_index+1)%(REV_STEP*2);
			if (state) {
				REV_SAVELOAD(load_cur);
				state_load_dependencies();
				active_state=A_STATE_BUFFER; msx_frame(TRUE);
				if ((state=rev_near[revbuf_index])) { REV_SAVELOAD(save); }
			}
		}

		near_index=(near_index+(REV_STEP*2-1))%(REV_STEP*2);

		/* load near state */
		state=rev_near[near_index];
		REV_SAVELOAD(load_cur);
		state_load_dependencies();

		psg_set_buffer_counter(0);
		io_set_click_buffer_counter(0);
		mapper_set_buffer_counter(0);

		/* reverse sound */
		if (sound_get_enabled()) {
			int i,j,k;
			int size=sound_get_num_samples()>>1;
			s16* buffer=sound_get_buffer();
			s16* buffer2;

			MEM_CREATE_N(buffer2,size+4);
			memcpy(buffer2,buffer,size+4);

			j=0; k=size>>1;
			for (i=size-1;i>k;i--) buffer[j++]=buffer[i];
			j=size-1; k=(size+4)>>1;
			for (i=0;i<k;i++) buffer[j--]=buffer2[i];

			MEM_CLEAN(buffer2);
		}

		/* show status */
		LOG(LOG_MISC|LOG_COLOUR(LC_GREEN)|LOG_TYPE(LT_REVERSE),"%#05.2f%% reverse      ",((framecount-2)/(float)(replay_frame-1))*100.0);
		active_state=A_STATE_REVERSE; replay=TRUE;
	}

	else {
		/* forward */
		int i;

		if (framecount<frame_max) framecount++;

		near_index=(near_index+1)%(REV_STEP*2);
		revbuf_index=(revbuf_index+(REV_STEP*2-1))%(REV_STEP*2);

		/* save near state */
		if (!rev_near[near_index]) { MEM_CREATE_N(rev_near[near_index],state_size) }
		state=rev_near[near_index];
		REV_SAVELOAD(save);

		/* save far state */
		if (!(near_index%REV_STEP)&&rev_near[i=(near_index+REV_STEP)%(REV_STEP*2)]) {
			if (!reverse_canbuffer) {
				MEM_CLEAN(rev_far[far_index].state);
				rev_far[far_index].state=rev_near[i];
				rev_near[i]=NULL;
			}

			far_index2=far_index;
			far_index=(far_index+1)%far_max;

			reverse_canbuffer=FALSE;
		}

		cont_movie_get(rev_far[far_index].btn[near_index%REV_STEP]); /* button state */

		/* show status */
		if (replay) {
			float f=((framecount-2)/(float)(replay_frame-2))*100.0;
			CLAMP(f,0.0,100.0);
			if (replay_frame<=framecount||active_state&A_STATE_SWITCH) {
				active_state=replay=0;
				LOG(LOG_MISC|LOG_COLOUR(LC_GREEN)|LOG_TYPE(LT_REVERSE),"%#05.*f%% continue     ",1+(f<100.0),f);
				replay_frame=framecount-1;
			}
			else {
				active_state=A_STATE_REPLAY;
				LOG(LOG_MISC|LOG_COLOUR(LC_GREEN)|LOG_TYPE(LT_REVERSE),"%#05.2f%% replay       ",f);
			}
		}
		else replay_frame=framecount-1;
	}
}

void reverse_init(void)
{
	int i;

	for (i=0;i<REV_STEP*2;i++) rev_near[i]=NULL;
	for (i=0;i<FAR_MAX;i++) rev_far[i].state=NULL;

	memset(&msxstate_prev,0,sizeof(_msx_state));

	revbuf_size=3; if (SETTINGS_GET_INT(settings_info(SETTINGS_REVERSESIZE),&i)) { CLAMP(i,0,(REVERSE_BUFFER_SIZE_MAX-1)); revbuf_size=i; }
	nolimit=FALSE; i=settings_get_yesnoauto(SETTINGS_REVNOLIMIT); if (i==FALSE||i==TRUE) nolimit=i;

	init_done=TRUE;
	cont_prev=-1;
	active_state=0;
	reverse_enabled=TRUE;
	reverse_invalidate();

	if (revbuf_size==0) reverse_clean();
}

void reverse_clean(void)
{
	int i;

	for (i=0;i<REV_STEP*2;i++) { MEM_CLEAN(rev_near[i]); }
	for (i=0;i<FAR_MAX;i++) { MEM_CLEAN(rev_far[i].state); }

	memset(&msxstate_prev,0,sizeof(_msx_state));

	cont_prev=-1;
	active_state=0;
	reverse_enabled=FALSE;
}
