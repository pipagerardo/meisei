/******************************************************************************
 *                                                                            *
 *                               "sample.c"                                   *
 *                                                                            *
 ******************************************************************************/

#include "global.h"
#include "file.h"
#include "sample.h"
#include "state.h"

/*
games using samples, simulated to play one on first request:
(correct behaviour wouldn't work with samples that have unexpected length/
different length than the original ones)

Playball:

	0	strike
	1	ball
	2	foul
	3	safe
	4	out
	5	game starts
	6	bat hits ball
	7	catcher obtains the ball
	8	game
	9	batter can walk after getting hit by ball
	10	batter gets hit by ball
	11	? (it's used)
	12	? (it's used)
	13	? (it's used)
	14	? (it's used)

*/

/*
sample loading errors:

00: wrong filename name: last 1 or 2 characters must be a valid (and unused before) number
01: general error reading file
02: unexpected end of file (file size too small)
03: not a RIFF WAVE file
04: chunk 1 (format) size too small
05: wrong sound format: only PCM (uncompressed) is allowed
06: wrong number of channels: should be 1 or 2 (mono/stereo)
07: wrong samplerate: should be 11025,22050,or 44100hz
08: wrong bits: should be 8 or 16
09: no "data" field
10: wrong amount of bytes in data (0, or uneven with 16 bit/stereo)

*/


_sample* sample_init(void)
{
	_sample* sample;

	MEM_CREATE(sample,sizeof(_sample));
	return sample;
}

void sample_clean(_sample* sample)
{
	int i;

	if (sample==NULL) return;

	for (i=0;i<SAMPLE_MAX;i++) { MEM_CLEAN(sample->sam[i]); }
	MEM_CLEAN(sample);
}

void sample_load(_sample* sample,int slot,const char* name)
{
	/* can't use mmio* */

	char* fn;
	int i;

	if (sample==NULL) return;

	sample->invalid=TRUE;

	if (name==NULL||(i=strlen(name))==0) return;

	MEM_CREATE(fn,i+1);
	memcpy(fn,name,i);

	while (i--) {
		file_setfile(&file->sampledir,fn,"zip","wav");
		if (file_accessible()) { i=-2; break; }
		fn[i]=0;
	}

	MEM_CLEAN(fn);

	if (i!=-2) {
		LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_SLOT1FAIL+slot),"slot %d: couldn't open samples",slot+1);
		return;
	}

	/* open samples */
	for (;;) {

		if (file_open()) {
			int len=strlen(file->filename_in_zip_post);

			if (file_get_zn()==-1) break; /* end of zipfile */

			/* .wav file? */
			if ((len>4)&&(stricmp((file->filename_in_zip_post+len-4),".wav")==0)) {

				#define SAMPLE_WARN_N(x) LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_SLOT1FAIL+slot),"slot %d: error %02d reading %s",slot+1,x,file->filename_in_zip_post)
				#define SAMPLE_WARN(x) SAMPLE_WARN_N(x); MEM_CLEAN(sf); continue
				#define SAMPLE_SEEK(x) o+=(x); if ((u32)o>=file->size) { SAMPLE_WARN(2); }

				/* get sample number */
				int sn,inv;
				int o=len-6; if (o<0) o=0;
				o+=((file->filename_in_zip_post[o]<48)|(file->filename_in_zip_post[o]>57)); /* 0-9 */

				sn=0; sscanf(file->filename_in_zip_post+o,"%d",&sn); inv=sn==0;
				sn=1; sscanf(file->filename_in_zip_post+o,"%d",&sn); inv&=(sn==1);
				inv|=(sn>=SAMPLE_MAX);
				if (!inv) inv|=(sample->len[sn]>0);

				/* found number */
				if (!inv) {
					u8* sf;
					o=0;

					MEM_CREATE(sf,file->size+32);
					if (file_read(sf,file->size)) {
						int oo,step;
						u16* sb;
						u32 l,sr;
						u16 chans,bits;
						o=0;

						/* examine header */
						memset(sf+4,'x',4); if (memcmp(sf,"RIFFxxxxWAVEfmt ",16)!=0) { SAMPLE_WARN(3); } SAMPLE_SEEK(16) /* header */
						memcpy(&l,sf+o,4); if (l<16) { SAMPLE_WARN(4); } SAMPLE_SEEK(4) /* chunk 1 size */
						if (memcmp(sf+o,"\1\0",2)!=0) { SAMPLE_WARN(5); } /* PCM (uncompressed) */
						memcpy(&chans,sf+o+2,2); if (chans!=1&&chans!=2) { SAMPLE_WARN(6); } /* mono/stereo */
						memcpy(&sr,sf+o+4,4); if (sr!=44100&&sr!=22050&&sr!=11025) { SAMPLE_WARN(7); } /* samplerate */
						memcpy(&bits,sf+o+14,2); if (bits!=8&&bits!=16) { SAMPLE_WARN(8); } /* 8/16 bit */
						SAMPLE_SEEK(l)

						if (memcmp(sf+o,"data",4)!=0) { SAMPLE_WARN(9); } SAMPLE_SEEK(4)
						memcpy(&l,sf+o,4); if ((l<((bits>>3)*chans))||(((bits==16)|(chans==2))&l)||((l>>1)&(bits==16)&(chans==2))) { SAMPLE_WARN(10); } SAMPLE_SEEK(4) /* data size */


						/* copy data */

						oo=o;
						step=44100/sr;

						SAMPLE_SEEK(l-1);

						MEM_CREATE(sb,l+2);
						memcpy(sb,sf+oo,l);
						l>>=1;

						/* convert 8 to 16 bits */
						if (bits==8) {
							u16* sb2;
							l<<=1; i=l;
							MEM_CREATE(sb2,(l<<1)+2);
							while (i--) sb2[i]=(sb[i>>1]>>(i<<3&8)^0x80)<<8&0xff00;
							MEM_CLEAN(sb); sb=sb2;
						}

						/* convert stereo to mono */
						if (chans==2) {
							l>>=1;
							for (i=0;(u32)i<l;i++) sb[i]=((s16)sb[i<<1]+(s16)sb[i<<1|1])/2;
						}

						MEM_CREATE(sample->sam[sn],l*step*2); sample->len[sn]=l*step;

						oo=0;

						/* convert samplerate (interpolate) */
						if (sr!=44100) {
							int sc,sd;

							for (i=0;(u32)i<l;i++) {
								sample->sam[sn][oo++]=sb[i];

								sc=step-1; sd=((s16)sb[i+1]-(s16)sb[i])/step;
								while (sc--) {
									sample->sam[sn][oo]=sample->sam[sn][oo-1]+sd;
									oo++;
								}
							}
						}

						else memcpy(sample->sam[sn],sb,l<<1);

						MEM_CLEAN(sb);


						/* adjust volume */
						i=sample->len[sn];
						while (i--) { sample->sam[sn][i]/=4; }

					}
					else { SAMPLE_WARN(1); }
					MEM_CLEAN(sf);
				}
				else { SAMPLE_WARN_N(0); }
			}
		}

		else {
			/* fopen error */
			LOG(LOG_MISC|LOG_WARNING|LOG_TYPE(LT_SLOT1FAIL+slot),"slot %d: couldn't open samples",slot+1);
			break;
		}

		file_close();
		file_zn_next();
	}

	file_close();
	file_zn_reset();

	for (i=0;i<SAMPLE_MAX;i++) sample->invalid&=(sample->len[i]==0);
}

void sample_stream(_sample* sample,int slot,signed short* stream,int len)
{
	if (sample==NULL) return;

	if ( (len!=0) & sample->playing & (sample->len[sample->cur] > (u32)0) & ((u32)sample->bc < sample->len[sample->cur]) ) {
		int i;

		for ( i=0; (i<len) & ((u32)sample->bc < sample->len[sample->cur] ); i++ ) { stream[i]+=sample->sam[sample->cur][sample->bc++]; }

		sample->playing=sample->bc != (int)sample->len[sample->cur];
		if (!sample->playing&&sample->next!=SAMPLE_MAX) sample_play(sample,slot,sample->next);
	}
}

void __fastcall sample_play(_sample* sample,int slot,int sn)
{
	if (sample==NULL) return;

	if (sample->playing) { sample->next=sn; return; }

	sample->playing=TRUE;
	sample->cur=sn;
	sample->bc=0;
	sample->next=SAMPLE_MAX;

	if (!sample->invalid&&sample->len[sample->cur]==0) {
		LOG(LOG_MISC|LOG_WARNING,"slot %d: sample %d unavailable\n",slot+1,sample->cur);
	}
}

void __fastcall sample_stop(_sample* sample)
{
	if (sample==NULL) return;

	sample->playing=FALSE;
}


/* state				size
bc						4
playing					1
cur						1
next					1

==						7
*/
#define STATE_VERSION	3 /* mapper.c */
/* version history:
1: initial
2: no changes (mapper.c)
3: no changes (mapper.c)
*/
#define STATE_SIZE		7

int __fastcall sample_state_get_size(void)
{
	return STATE_SIZE;
}

/* save */
void __fastcall sample_state_save(_sample* sample,u8** s)
{
	if (sample==NULL) return;

	STATE_SAVE_4(sample->bc);
	STATE_SAVE_1(sample->playing);
	STATE_SAVE_1(sample->cur);
	STATE_SAVE_1(sample->next);
}

/* load */
void __fastcall sample_state_load_cur(_sample* sample,u8** s)
{
	if (sample==NULL) return;

	STATE_LOAD_4(sample->bc);
	STATE_LOAD_1(sample->playing);
	STATE_LOAD_1(sample->cur);
	STATE_LOAD_1(sample->next);
}

int __fastcall sample_state_load(_sample* sample,int v,u8** s)
{
	if (sample==NULL) return FALSE;

	switch (v) {
		case 1: case 2: case STATE_VERSION:
			sample_state_load_cur(sample,s);
			break;

		default: return FALSE;
	}

	return TRUE;
}
