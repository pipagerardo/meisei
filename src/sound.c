/******************************************************************************
 *                                                                            *
 *                                "sound.c"                                   *
 *                                                                            *
 ******************************************************************************/

#include <dsound.h>

#include "global.h"
#include "sound.h"
#include "main.h"
#include "crystal.h"
#include "msx.h"
#include "settings.h"
#include "io.h"
#include "mapper.h"
#include "resource.h"
#include "psg.h"
#include "reverse.h"
#include "input.h"
#include "cont.h"
#include "vdp.h"
#include "scc.h"

static struct {
	LPDIRECTSOUND dsound;
	DSCAPS caps;
	LPDIRECTSOUNDBUFFER buffer_primary;
	LPDIRECTSOUNDBUFFER buffer_secondary;
	DWORD buffer_secondary_size;
	WAVEFORMATEX format;
	DSBUFFERDESC desc_primary;
	DSBUFFERDESC desc_secondary;

	LPDIRECTSOUNDCAPTURE dscapture;
	LPDIRECTSOUNDCAPTUREBUFFER buffer_capture;
	DSCBUFFERDESC desc_capture;
	int no_capture;

	DWORD write_cursor;
	DWORD capt_cursor;
	int capt_threshold;
	signed short* buffer;
	int base_count;
	int num_samples;
	int num_samples_prev;
	HANDLE sleepthread;
	int slept;
	int slept_prev;
	int sleep;
	int sleep_auto;
	u32 sleeptime;
	u32 sleeptime_raw;
	int lostframes;
	int buffer_secondary_size_set;
	int buffer_size;
	int enabled;
	int ticks;
	int ticks_prev;
} sound;

/* last one is empty */
typedef struct sound_channel {
	int* buffer;
	struct sound_channel* next;
	struct sound_channel* prev;
} sound_channel;
static sound_channel* sound_channel_begin;
static sound_channel* sound_channel_cursor;

/* last one is filled, and next points to the first */
typedef struct sound_dac {
	int* buffer;
	int* buffer_count;
	struct sound_dac* next;
	struct sound_dac* prev;
} sound_dac;
static sound_dac* sound_dac_begin;
static sound_dac* sound_dac_cursor;

/* samples */
typedef struct {
	LPDIRECTSOUNDBUFFER buffer;
	DSBUFFERDESC desc;
	int playing;
} sound_sample;

#define DS_CHECK_RESULT(x) if ((result=x)==DSERR_BUFFERLOST) { IDirectSoundBuffer_Restore(sound.buffer_secondary); result=x; }

static DWORD WINAPI sound_calc_sleeptime(void*);
static void sound_reset_sleeptime(int);

int sound_get_ticks_prev(void) { return sound.ticks_prev; }
void sound_set_ticks_prev(int i) { sound.ticks_prev=i; }
int sound_get_ticks(void) { return sound.ticks; }
void sound_set_ticks(int i) { sound.ticks=i; }
int sound_get_num_samples_prev(void) { return sound.num_samples_prev; }
void sound_set_num_samples_prev(int i) { sound.num_samples_prev=i; }
int sound_get_slept(void) { return sound.slept; }
int sound_get_slept_prev(void) { return sound.slept_prev; }
void sound_reset_slept(void) { sound.slept=sound.slept_prev=0; }
void sound_new_frame(void) { sound.slept_prev=sound.slept; sound.slept=0; }
u32 sound_get_sleeptime_raw(void) { return sound.sleeptime_raw; }
int sound_get_sleep_auto(void) { return sound.sleep_auto; }
void sound_sleep(u32 s) { sound.slept+=s; Sleep(s); }
s16* sound_get_buffer(void) { return sound.buffer; }
DWORD sound_get_buffer_secondary_size(void) { return sound.buffer_secondary_size; }
int sound_get_buffer_size(void) { return sound.buffer_size; }
int sound_get_lostframes(void) { return sound.lostframes; }
int sound_get_capt_threshold(void) { return sound.capt_threshold; }
int sound_get_num_samples(void) { return sound.num_samples; }
DWORD sound_get_playposition(void) { DWORD p=0; IDirectSoundBuffer_GetCurrentPosition(sound.buffer_secondary,&p,0); return p; }
void sound_set_write_cursor(DWORD w) { sound.write_cursor=w; }
int sound_get_enabled(void) { return sound.enabled; }
void sound_set_enabled(int s) { sound.enabled=s; main_menu_check(IDM_SOUND,s); main_menu_enable(IDM_LUMINOISE,s); if (!s) sound_silence(FALSE); }

void sound_reset_write_cursor(void) { IDirectSoundBuffer_GetCurrentPosition(sound.buffer_secondary,&sound.write_cursor,0); }
void sound_reset_capt_cursor(void) { if (sound.no_capture) return; else IDirectSoundCaptureBuffer_GetCurrentPosition(sound.buffer_capture,0,&sound.capt_cursor); }
void sound_reset_lostframes(void) { sound.lostframes=0; }

/***********/

/* init */
int sound_init(void)
{
	int i;

	memset(&sound,0,sizeof(sound));

	/* settings */
	sound.sleep=TRUE; sound.sleep_auto=TRUE; i=settings_get_yesnoauto(SETTINGS_SLEEP); if (i==FALSE||i==TRUE) { sound.sleep=i; sound.sleep_auto=FALSE; } else if (i==2) { sound.sleep_auto=TRUE; }
	sound.enabled=TRUE; i=settings_get_yesnoauto(SETTINGS_SOUND); if (i==FALSE||i==TRUE) sound.enabled=i; sound_set_enabled(sound.enabled);

	sound.buffer_secondary_size_set=5; /* default: 5 frames */
	if (SETTINGS_GET_INT(settings_info(SETTINGS_BUFSIZE),&i)) {
		CLAMP(i,2,50);
		sound.buffer_secondary_size_set=i;
	}

	/* create it */
	if ((DirectSoundCreate(NULL,&sound.dsound,NULL))!=DS_OK) {
		LOG(LOG_MISC|LOG_ERROR,"Can't create DirectSound!\n");
		sound_clean(); exit(1);
	}

	/* get capabilities */
	sound.caps.dwSize=sizeof(sound.caps);
	if ((IDirectSound_GetCaps(sound.dsound,&sound.caps))!=DS_OK) {
		LOG(LOG_MISC|LOG_ERROR,"Can't get DirectSound capabilities!\n");
		sound_clean(); exit(1);
	}

	/* set cooperative level */
	if ((IDirectSound_SetCooperativeLevel(sound.dsound,MAIN->window,DSSCL_PRIORITY))!=DS_OK) {
		LOG(LOG_MISC|LOG_ERROR,"Can't set DirectSound cooperative level!\n");
		sound_clean(); exit(1);
	}

	/* create primary buffer */
	sound.desc_primary.dwSize=sizeof(DSBUFFERDESC);
	sound.desc_primary.dwFlags=DSBCAPS_PRIMARYBUFFER;

	if ((IDirectSound_CreateSoundBuffer(sound.dsound,&sound.desc_primary,&sound.buffer_primary,NULL))!=DS_OK) {
		LOG(LOG_MISC|LOG_ERROR,"Can't create DirectSound primary buffer!\n");
		sound_clean(); exit(1);
	}

	/* set format */
	sound.format.cbSize=sizeof(WAVEFORMATEX);
	sound.format.wFormatTag=WAVE_FORMAT_PCM;	/* waveform audio format type */
	sound.format.nChannels=1;					/* mono/stereo */
	sound.format.nSamplesPerSec=44100;			/* samplerate */
	sound.format.wBitsPerSample=16;				/* bits */
	sound.format.nBlockAlign=sound.format.wBitsPerSample*sound.format.nChannels/8;		/* bytes per sample */
	sound.format.nAvgBytesPerSec=sound.format.nBlockAlign*sound.format.nSamplesPerSec;	/* average datatransferrate */

	if ((IDirectSoundBuffer_SetFormat(sound.buffer_primary,&sound.format))!=DS_OK) {
		LOG(LOG_MISC|LOG_ERROR,"Can't set DirectSound format!\n");
		sound_clean(); exit(1);
	}

	/* get format */
	if ((IDirectSoundBuffer_GetFormat(sound.buffer_primary,&sound.format,sizeof(sound.format),NULL))!=DS_OK) {
		LOG(LOG_MISC|LOG_ERROR,"Can't get DirectSound format!\n");
		sound_clean(); exit(1);
	}

	sound.desc_secondary.dwSize=sizeof(DSBUFFERDESC);
	sound.desc_secondary.dwFlags=DSBCAPS_GLOBALFOCUS|DSBCAPS_GETCURRENTPOSITION2;
	sound.desc_secondary.lpwfxFormat=&sound.format;


	if (sound.sleep) {
		DWORD tid; /* unused */

		sound.sleepthread=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)sound_calc_sleeptime,NULL,0,&tid);
		SetThreadAffinityMask(sound.sleepthread,1);
		SetThreadPriority(sound.sleepthread,THREAD_PRIORITY_ABOVE_NORMAL);
	}
	else sound.sleeptime=sound.sleeptime_raw=~0;

	sound.ticks=SOUND_TICKS_LEN;

	LOG(LOG_VERBOSE,"sound initialised\n");

	return 0;
}

int sound_create_capture(void)
{
	if (sound.dscapture) return FALSE;

	sound.desc_capture.dwSize=sizeof(DSCBUFFERDESC);
	sound.desc_capture.lpwfxFormat=&sound.format;
	sound.desc_capture.dwBufferBytes=0x10000;

	/* create capture */
	if ((DirectSoundCaptureCreate(NULL,&sound.dscapture,NULL))!=DS_OK) {
		LOG(LOG_MISC|LOG_WARNING,"can't create DirectSoundCapture\n");
		sound.dscapture=NULL; sound.no_capture=TRUE; return FALSE;
	}

	/* create buffer */
	if ((IDirectSoundCapture_CreateCaptureBuffer(sound.dscapture,&sound.desc_capture,&sound.buffer_capture,NULL))!=DS_OK) {
		LOG(LOG_MISC|LOG_WARNING,"can't create DirectSound capture buffer\n");
		sound_clean_capture(); sound.no_capture=TRUE; return FALSE;
	}

	/* start */
	if ((IDirectSoundCaptureBuffer_Start(sound.buffer_capture,DSBPLAY_LOOPING))!=DS_OK) {
		LOG(LOG_MISC|LOG_WARNING,"can't start DirectSound capture buffer\n");
		sound_clean_capture(); sound.no_capture=TRUE; return FALSE;
	}

	sound.no_capture=FALSE;
	sound_reset_capt_cursor();

	return TRUE;
}

void sound_create_secondary_buffer(int c)
{
	HRESULT result;

	int mul,cspf=crystal->cspf*sound.buffer_secondary_size_set*sound.format.nBlockAlign;
	if (!c&&sound.buffer_secondary_size==(DWORD)cspf) return;

	/* clean previous buffer */
	sound_clean_secondary_buffer();

	/* (re)create secondary and inter buffer */
	sound.buffer_secondary_size=cspf;
	mul=(100.0/(float)crystal_get_slow_max())+1;
	sound.buffer_size=mul*sound.format.nBlockAlign*crystal_get_cspf_max();
	MEM_CREATE(sound.buffer,sound.buffer_size);

	sound.desc_secondary.dwBufferBytes=sound.buffer_secondary_size;

	if ((IDirectSound_CreateSoundBuffer(sound.dsound,&sound.desc_secondary,&sound.buffer_secondary,NULL))!=DS_OK) {
		LOG(LOG_MISC|LOG_ERROR,"Can't create DirectSound secondary buffer!\n");
		sound_clean(); exit(1);
	}

	/* initial silence */
	sound_silence(FALSE);

	/* play */
	DS_CHECK_RESULT(IDirectSoundBuffer_Play(
		sound.buffer_secondary,	/* buffer to play */
		0,						/* reserved */
		0,						/* priority */
		DSBPLAY_LOOPING			/* flags */
	))
	if (result!=DS_OK) {
		LOG(LOG_MISC|LOG_ERROR,"Can't play DirectSound!\n");
		sound_clean(); exit(1);
	}

	sound_reset_lostframes();
}


static void sound_reset_sleeptime(int t)
{
	sound.sleeptime_raw=t;
	sound.sleeptime=((sound.format.nBlockAlign/((1.0/sound.format.nSamplesPerSec)*1000.0))*t)+0.5;
}

static DWORD WINAPI sound_calc_sleeptime(void*)
{
	DWORD ticks;
	DWORD result;
	int i;

	sound_reset_sleeptime(5);

	/* calculate approximate time in ms of 1 sleep */
	ticks=GetTickCount();
	for (i=0;i<100;i++) Sleep(1);
	result=(DWORD)(GetTickCount()-ticks);

	i=((result+(result/10.0))/100.0)+0.999;
	sound_reset_sleeptime(i);

	return 0;
}

void __fastcall sound_close_sleepthread(void)
{
	DWORD ec;

	if (!sound.sleepthread) return;

	if (GetExitCodeThread(sound.sleepthread,&ec)&&ec!=STILL_ACTIVE) {
		CloseHandle(sound.sleepthread);
		sound.sleepthread=NULL;
	}
}


/* clean */
void sound_clean(void)
{
	/* close sleepthread */
	if (sound.sleepthread) {
		WaitForSingleObject(sound.sleepthread,INFINITE);
		CloseHandle(sound.sleepthread);
		sound.sleepthread=NULL;
	}

	sound_clean_secondary_buffer();
	sound_clean_capture();

	if (sound.buffer_primary) { IDirectSoundBuffer_Release(sound.buffer_primary); sound.buffer_primary=NULL; } /* free primary buffer */

	if (sound.dsound) { IDirectSound_Release(sound.dsound); sound.dsound=NULL; } /* free directsound */

	memset(&sound,0,sizeof(sound));

	LOG(LOG_VERBOSE,"sound cleaned\n");
}

void sound_clean_capture(void)
{
	sound.no_capture=TRUE;

	if (sound.buffer_capture) {
		IDirectSoundCaptureBuffer_Stop(sound.buffer_capture); /* stop recording */
		IDirectSoundCaptureBuffer_Release(sound.buffer_capture); sound.buffer_capture=NULL; /* free capture buffer */
	}
	if (sound.dscapture) { IDirectSoundCapture_Release(sound.dscapture); sound.dscapture=NULL; } /* free main capture */
}

void sound_clean_secondary_buffer(void)
{
	if (sound.buffer_secondary) {
		IDirectSoundBuffer_Stop(sound.buffer_secondary); /* stop playing */
		IDirectSoundBuffer_Release(sound.buffer_secondary); sound.buffer_secondary=NULL; /* free secondary buffer */
	}
	MEM_CLEAN(sound.buffer);
}

void sound_settings_save(void)
{
	SETTINGS_PUT_STRING(NULL,"; *** Sound/Misc settings ***\n\n");

	/* psg.c */
	SETTINGS_PUT_STRING(NULL,"; Valid "); SETTINGS_PUT_STRING(NULL,settings_info(SETTINGS_PSGCHIPTYPE)); SETTINGS_PUT_STRING(NULL," options: ");
	SETTINGS_PUT_STRING(NULL,psg_get_chiptype_name(PSG_CHIPTYPE_AY8910)); SETTINGS_PUT_STRING(NULL,", or ");
	SETTINGS_PUT_STRING(NULL,psg_get_chiptype_name(PSG_CHIPTYPE_YM2149)); SETTINGS_PUT_STRING(NULL,".\n\n");
	SETTINGS_PUT_STRING(settings_info(SETTINGS_PSGCHIPTYPE),psg_get_chiptype_name(psg_get_chiptype()));
	SETTINGS_PUT_STRING(NULL,"\n");

	settings_put_yesnoauto(SETTINGS_SOUND,sound.enabled);
	settings_put_yesnoauto(SETTINGS_LUMINOISE,vdp_luminoise_get());
	SETTINGS_PUT_INT(settings_info(SETTINGS_LUMINOISE_VOLUME),vdp_luminoise_get_volume());
	SETTINGS_PUT_INT(settings_info(SETTINGS_SCC_VOLUME),scc_get_volume());
	SETTINGS_PUT_INT(settings_info(SETTINGS_BUFSIZE),sound.buffer_secondary_size_set);
	settings_put_yesnoauto(SETTINGS_SLEEP,sound.sleep_auto?2:sound.sleep);
	SETTINGS_PUT_STRING(NULL,"\n");

	SETTINGS_PUT_INT(settings_info(SETTINGS_RAMSIZE),mapper_get_ramsize());
	SETTINGS_PUT_INT(settings_info(SETTINGS_RAMSLOT),mapper_get_ramslot());
	SETTINGS_PUT_STRING(NULL,"\n");

	settings_put_yesnoauto(SETTINGS_REVNOLIMIT,reverse_get_nolimit());
	SETTINGS_PUT_INT(settings_info(SETTINGS_REVERSESIZE),reverse_get_buffer_size());
	SETTINGS_PUT_INT(settings_info(SETTINGS_SPEEDPERC),crystal->speed_percentage);
	SETTINGS_PUT_INT(settings_info(SETTINGS_SLOWPERC),crystal->slow_percentage);
	SETTINGS_PUT_INT(settings_info(SETTINGS_CPUSPEED),crystal->overclock);

	/* thread priority (msx.c) */
	SETTINGS_PUT_STRING(NULL,"\n; Valid "); SETTINGS_PUT_STRING(NULL,settings_info(SETTINGS_PRIORITY)); SETTINGS_PUT_STRING(NULL," options: ");
	SETTINGS_PUT_STRING(NULL,msx_get_priority_name(MSX_PRIORITY_NORMAL)); SETTINGS_PUT_STRING(NULL,", ");
	SETTINGS_PUT_STRING(NULL,msx_get_priority_name(MSX_PRIORITY_ABOVENORMAL)); SETTINGS_PUT_STRING(NULL,", ");
	SETTINGS_PUT_STRING(NULL,msx_get_priority_name(MSX_PRIORITY_HIGHEST)); SETTINGS_PUT_STRING(NULL,", or ");
	SETTINGS_PUT_STRING(NULL,msx_get_priority_name(MSX_PRIORITY_AUTO)); SETTINGS_PUT_STRING(NULL,".\n\n");
	SETTINGS_PUT_STRING(settings_info(SETTINGS_PRIORITY),msx_get_priority_auto()?msx_get_priority_name(MSX_PRIORITY_AUTO):msx_get_priority_name(msx_get_priority()));
}

/***********/


void sound_create_ll(void)
{
	#define SOUND_CREATE_LL(x) MEM_CREATE_T(x##_begin,sizeof(x),x*); x##_cursor=x##_begin

	SOUND_CREATE_LL(sound_channel);
	SOUND_CREATE_LL(sound_dac);
}

void sound_clean_ll(void)
{
	#define SOUND_CLEAN_LL(x)					\
		for (;;) {								\
			MEM_CLEAN(x##_cursor->buffer);		\
			if (x##_cursor->prev) {				\
				x##_cursor=x##_cursor->prev;	\
				MEM_CLEAN(x##_cursor->next);	\
			}									\
			else break;							\
		}										\
		MEM_CLEAN(x##_begin);					\
		x##_cursor=NULL

	SOUND_CLEAN_LL(sound_channel);
	SOUND_CLEAN_LL(sound_dac);
}

int* sound_create_channel(void)
{
	sound_channel* prev=sound_channel_cursor;
	int* buffer;
	int bs=(crystal->max_cycles+1000)*sizeof(int);

	MEM_CREATE(buffer,bs);
	sound_channel_cursor->buffer=buffer;

	MEM_CREATE_T(sound_channel_cursor->next,sizeof(sound_channel),sound_channel*);
	sound_channel_cursor=sound_channel_cursor->next;
	sound_channel_cursor->prev=prev;

	return buffer;
}

int* sound_create_dac(void)
{
	sound_dac* prev=sound_dac_cursor;
	int* buffer;
	int bs=(crystal->max_cycles+1000)*sizeof(int);

	if (sound_dac_cursor->next!=NULL) {
		MEM_CREATE_T(sound_dac_cursor->next,sizeof(sound_dac),sound_dac*);
		sound_dac_cursor=sound_dac_cursor->next;
		sound_dac_cursor->prev=prev;
	}

	sound_dac_cursor->next=sound_dac_begin;

	MEM_CREATE(buffer,bs);
	sound_dac_cursor->buffer=buffer;

	return buffer;
}

void sound_clean_dac(int* buffer)
{
	sound_dac* dp=sound_dac_begin;

	if (!buffer||!dp) return;

	/* locate it */
	for (;;) {
		if (dp->buffer==buffer) break;
		if (dp==sound_dac_cursor) return;
		dp=dp->next;
	}

	/* restructure ll */
	if (dp==dp->prev->next) dp->prev->next=dp->next;
	if (dp==dp->next->prev) dp->next->prev=dp->prev;
	if (dp==sound_dac_cursor) sound_dac_cursor=dp->prev;

	/* clean entry */
	MEM_CLEAN(dp->buffer);
	MEM_CLEAN(dp);
}

/***********/

void sound_silence(int c)
{
	HRESULT result;
	void* buffer_ds_1;
	void* buffer_ds_2;
	DWORD buffer_ds_length_1=0;
	DWORD buffer_ds_length_2=0;
	signed short sample=0;
	int i;

	if (!sound.buffer_secondary) return;

	sound.base_count=0;
	if ((c)&&(sound.num_samples>1)&&sound.enabled) sample=sound.buffer[(sound.num_samples/sizeof(signed short))-1]&~0xff;
	for (i=0;(unsigned int)i<(sound.buffer_size/sizeof(signed short));i++) sound.buffer[i]=sample;

	/* lock all */
	DS_CHECK_RESULT(IDirectSoundBuffer_Lock(
		sound.buffer_secondary,			/* buffer to lock */
		0,								/* lock start offset */
		sound.buffer_secondary_size,	/* number of bytes to lock */
		&buffer_ds_1,&buffer_ds_length_1,&buffer_ds_2,&buffer_ds_length_2, /* locked part start position, locked part size, second locked part start position, second locked part size */
		0								/* flags */
	))
	if (result!=DS_OK) LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_DSOUNDLOCKFAIL),"DirectSound lock failed");

	else {
		/* empty */
		memcpy(buffer_ds_1,(u8*)sound.buffer,buffer_ds_length_1);
		if (buffer_ds_length_2) memcpy(buffer_ds_2,(u8*)sound.buffer+buffer_ds_length_1,buffer_ds_length_2);

		/* unlock */
		IDirectSoundBuffer_Unlock(sound.buffer_secondary,buffer_ds_1,buffer_ds_length_1,buffer_ds_2,buffer_ds_length_2);
	}
}

void sound_compress(void)
{
	sound_dac* dp=sound_dac_begin;
	int num_dacs=0;
	int num_samples=io_get_click_buffer_counter(); /* they're all the same */
	int num_samples_compressed=0;
	int slice,neg,j,sample,i=0;

	/* should not happen */
	#if 0
	if (num_samples!=psg_get_buffer_counter()) printf("%d %d\n",num_samples,psg_get_buffer_counter());
	#endif

	/* reset dac buffer counts */
	for (;;) {
		num_dacs++;
		dp->buffer_count=dp->buffer;
		if (dp==sound_dac_cursor) break;
		dp=dp->next;
	}

	/* compress down to 44Khz */
	for (;;) {
		neg=sound.base_count<0;
		slice=crystal->slice[neg];
		i+=slice;
		if (i>num_samples) break;
		sound.base_count+=(crystal->base*neg);
		sound.base_count-=crystal->sub;
		sample=0;
		j=slice*num_dacs;
		while(j--) {
			sample+=*dp->buffer_count++;
			dp=dp->next;
		}
		sound.buffer[num_samples_compressed++]=sample/slice;
	}

	mapper_sound_stream(sound.buffer,num_samples_compressed);
	cont_mute_sonypause(sound.buffer,num_samples_compressed);
	sound.num_samples_prev=sound.num_samples;
	sound.num_samples=num_samples_compressed*sizeof(signed short);

	i-=slice;

	if (i<num_samples) {
		/* write leftover to the start */
		for (sound_channel_cursor=sound_channel_begin;sound_channel_cursor->buffer!=NULL;sound_channel_cursor=sound_channel_cursor->next) {
			memmove(sound_channel_cursor->buffer,&sound_channel_cursor->buffer[i],(num_samples-i)*sizeof(int));
		}
	}

	psg_set_buffer_counter(num_samples-i);
	io_set_click_buffer_counter(num_samples-i);
	mapper_set_buffer_counter(num_samples-i);
}

void sound_write(void)
{
	HRESULT result;
	int offset=0;
	void* buffer_ds_1;
	void* buffer_ds_2;
	DWORD buffer_ds_length_1;
	DWORD buffer_ds_length_2;
	DWORD write_cursor,play_cursor;
	int available_samples=0,samples=0;
	int num_samples=sound.num_samples;

	sound.ticks+=num_samples;
	if (sound.ticks>=SOUND_TICKS_LEN) {
		sound.ticks_prev=sound.ticks;
		sound.ticks=0;
		input_tick();
	}

	while (num_samples>0) {

		buffer_ds_length_1=0;
		buffer_ds_length_2=0;
		play_cursor=sound.write_cursor;
		samples=num_samples;

		if (samples>((crystal->cspf<<1)+(crystal->cspf>>1))) {
			samples=num_samples%(crystal->cspf<<1);
			if (samples==0) samples=crystal->cspf<<1;
		}

		/* wait for room in the secondary sound buffer */
		for (;;) {
			DS_CHECK_RESULT(IDirectSoundBuffer_GetCurrentPosition(sound.buffer_secondary,&play_cursor,&write_cursor))
			available_samples=play_cursor-sound.write_cursor;
			if (available_samples<0) available_samples+=sound.buffer_secondary_size;
			if (available_samples>samples) break;
			if ((u32)(samples-available_samples)>sound.sleeptime||sound.sleep_auto^sound.sleep) sound_sleep(1);
		}

		if (((play_cursor<write_cursor)&(sound.write_cursor>play_cursor)&(sound.write_cursor<write_cursor))|((play_cursor>write_cursor)&((sound.write_cursor>play_cursor)|(sound.write_cursor<write_cursor)))) {
			LOG(LOG_VERBOSE,"soundbuffer underrun\n");
			sound.write_cursor=write_cursor;
		}

		if (!sound.enabled) sound.write_cursor=(sound.write_cursor+samples)%sound.buffer_secondary_size; /* timing problems otherwise */
		else {
			/* lock */
			DS_CHECK_RESULT(IDirectSoundBuffer_Lock(sound.buffer_secondary,sound.write_cursor,samples,&buffer_ds_1,&buffer_ds_length_1,&buffer_ds_2,&buffer_ds_length_2,0))
			sound.write_cursor=(sound.write_cursor+samples)%sound.buffer_secondary_size;
			if (result!=DS_OK) LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_DSOUNDLOCKFAIL),"DirectSound lock failed");

			else {
				/* write */
				memcpy(buffer_ds_1,(u8*)sound.buffer+offset,buffer_ds_length_1);
				if (buffer_ds_length_2) memcpy(buffer_ds_2,(u8*)sound.buffer+offset+buffer_ds_length_1,buffer_ds_length_2);

				/* unlock */
				IDirectSoundBuffer_Unlock(sound.buffer_secondary,buffer_ds_1,buffer_ds_length_1,buffer_ds_2,buffer_ds_length_2);
			}

			offset+=samples;
		}

		num_samples-=samples;
	}

	available_samples-=samples;
	sound.lostframes=available_samples/(samples+1);
}

void sound_capt_read(void)
{
	void* buffer_ds_1;
	void* buffer_ds_2;
	DWORD buffer_ds_length_1=0;
	DWORD buffer_ds_length_2=0;
	DWORD readpos=0,captpos=0;
	int len;

	sound.capt_threshold=0;

	if (sound.no_capture) return;

	IDirectSoundCaptureBuffer_GetCurrentPosition(sound.buffer_capture,&captpos,&readpos);

	if (((readpos<captpos)&(sound.capt_cursor<captpos)&(sound.capt_cursor>readpos))|((readpos>captpos)&((sound.capt_cursor<captpos)|(sound.capt_cursor>readpos)))) {
		LOG(LOG_VERBOSE,"capturebuffer underrun\n");
		sound.capt_cursor=captpos;
	}

	len=readpos-sound.capt_cursor;
	if (len<0) len+=sound.desc_capture.dwBufferBytes;

	if (len<=0) return;

	if ((IDirectSoundCaptureBuffer_Lock(sound.buffer_capture,sound.capt_cursor,len,&buffer_ds_1,&buffer_ds_length_1,&buffer_ds_2,&buffer_ds_length_2,0))==DS_OK) {
		signed short* buffer;
		int ub=-0x8000,lb=0x8000;
		int i=len/sizeof(signed short);

		MEM_CREATE(buffer,len);
		memcpy(buffer,buffer_ds_1,buffer_ds_length_1);
		if (buffer_ds_length_2) memcpy((u8*)buffer+buffer_ds_length_1,buffer_ds_2,buffer_ds_length_2);

		IDirectSoundCaptureBuffer_Unlock(sound.buffer_capture,buffer_ds_1,buffer_ds_length_1,buffer_ds_2,buffer_ds_length_2);

		while (i--) {
			if (buffer[i]<lb) lb=buffer[i];
			if (buffer[i]>ub) ub=buffer[i];
		}

		sound.capt_threshold=(ub-lb)/(0x10000/100.0);
	}

	sound.capt_cursor=readpos;
}


/* sample buffers */
void* sound_create_sample(const void* buffer,int size,int looped)
{
	#define SILENCE_SAMPLES 2500

	int silence=looped?0:SILENCE_SAMPLES;
	void* bl; DWORD bs;
	sound_sample* ss;
	if (size&1||buffer==NULL) return NULL;
	MEM_CREATE(ss,sizeof(sound_sample));

	ss->desc.dwSize=sizeof(DSBUFFERDESC);
	ss->desc.dwFlags=DSBCAPS_GLOBALFOCUS;
	ss->desc.lpwfxFormat=&sound.format;
	ss->desc.dwBufferBytes=size+silence*4;

	if (IDirectSound_CreateSoundBuffer(sound.dsound,&ss->desc,&ss->buffer,NULL)==DS_OK) {
		if (IDirectSoundBuffer_Lock(ss->buffer,0,size,&bl,&bs,NULL,NULL,0)==DS_OK) {
			int i=0;
			memcpy((u8*)bl+silence*2,buffer,bs);

			/* silence at start and end */
			while (i<silence) {
				((s16*)bl)[i]=((s16*)bl)[i+silence+(size>>1)]=0;
				i++;
			}

			IDirectSoundBuffer_Unlock(ss->buffer,bl,bs,NULL,0);
			IDirectSoundBuffer_SetCurrentPosition(ss->buffer,0);
			return (void*)ss;
		}
		else IDirectSoundBuffer_Release(ss->buffer);
	}

	MEM_CLEAN(ss);
	return NULL;
}

void sound_clean_sample(void* sample)
{
	sound_sample* ss;
	if (sample==NULL) return;
	ss=(sound_sample*)sample;

	sound_stop_sample(sample,FALSE);
	IDirectSoundBuffer_Release(ss->buffer);

	MEM_CLEAN(ss);
}

void sound_play_sample(void* sample,int loop)
{
	sound_sample* ss;
	if (sample==NULL||!sound.enabled) return;
	ss=(sound_sample*)sample;
	if (ss->playing) return;

	if (IDirectSoundBuffer_Play(ss->buffer,0,0,loop?DSBPLAY_LOOPING:0)==DSERR_BUFFERLOST) {
		IDirectSoundBuffer_Restore(ss->buffer);
		IDirectSoundBuffer_Play(ss->buffer,0,0,loop?DSBPLAY_LOOPING:0);
	}
	ss->playing=TRUE;
}

void sound_stop_sample(void* sample,int rewind)
{
	sound_sample* ss;
	if (sample==NULL) return;
	ss=(sound_sample*)sample;
	if (!ss->playing) return;

	if (IDirectSoundBuffer_Stop(ss->buffer)==DSERR_BUFFERLOST) {
		IDirectSoundBuffer_Restore(ss->buffer);
		IDirectSoundBuffer_Stop(ss->buffer);
	}
	if (rewind) IDirectSoundBuffer_SetCurrentPosition(ss->buffer,0);
	ss->playing=FALSE;
}
