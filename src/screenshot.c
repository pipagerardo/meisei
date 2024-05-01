/******************************************************************************
 *                                                                            *
 *                              "screenshot.c"                                *
 *                                                                            *
 ******************************************************************************/

#include "global.h"
#include "screenshot.h"
#include "zlib.h"
#include "draw.h"
#include "mapper.h"
#include "file.h"

/* save PNG screenshot */
int screenshot_save(const int width,const int height,int input_type,const void* input_data,const void* input_data2,const char* fn)
{
	static int semaphore=0;
	FILE* fd=NULL;
	u8 *data,*screen,*s,*d;
	int i,x,y,ret=FALSE;
	const int png_sig[8]={137,80,78,71,13,10,26,10};
	const int png_iend[12]={0,0,0,0,(int)'I',(int)'E',(int)'N',(int)'D',0xae,0x42,0x60,0x82}; /* is always the same, no need to calculate crc32 */
	const int png_len=8+(12+13)+(12+0)+12; /* sig, IHDR+meta, IDAT meta (size unknown yet), IEND meta */
	const int sourcelen=(width*3+1)*height;
	uLong bound,crc;
	uLongf destlen;

	if (semaphore||!input_data||!fn||!strlen(fn)) return FALSE;
	semaphore=1;

	/* IDAT data, stream of rr gg bb per scanline, 1 byte filter type at start */
	MEM_CREATE_N(screen,sourcelen);

	switch (input_type&0xff) {

		/* 4 bytes per pixel standard bmp */
		case SCREENSHOT_TYPE_32BPP: {
			u8* input=(u8*)input_data;
			s=screen;

			for (y=0;y<height;y++) {
				*s++=0;
				for (x=0;x<width;x++) {
					*s++=input[2];
					*s++=input[1];
					*s++=input[0];
					input+=4;
				}
			}

			break;
		}

		/* 1 byte per pixel, palette in data2 */
		case SCREENSHOT_TYPE_8BPP_INDEXED: {
			u8* input=(u8*)input_data;
			int* pal=(int*)input_data2;
			s=screen;

			for (y=0;y<height;y++) {
				*s++=0;
				for (x=0;x<width;x++) {
					*s++=pal[*input]>>16&0xff;
					*s++=pal[*input]>>8&0xff;
					*s++=pal[*input++]&0xff;
				}
			}

			break;
		}

		/* 1 bit per pixel, palette in data2 */
		case SCREENSHOT_TYPE_1BPP_INDEXED: {
			u8* input=(u8*)input_data;
			u8* pal=(u8*)input_data2;
			const int xm=width>>3;
			int b;
			s=screen;

			for (y=0;y<height;y++) {
				*s++=0;
				for (x=0;x<xm;x++) {
					/* 8 pixels */
					b=*input++;
					i=8;
					while (i--) {
						if (b>>i&1) { *s++=pal[6]; *s++=pal[5]; *s++=pal[4]; }
						else { *s++=pal[2]; *s++=pal[1]; *s++=pal[0]; }
					}
				}
			}

			break;
		}

		default:
			MEM_CLEAN(screen);
			semaphore=0;
			return FALSE;
	}

	#define SET32(o,v) d[o]=v>>24&0xff; d[(o)+1]=v>>16&0xff; d[(o)+2]=v>>8&0xff; d[(o)+3]=v&0xff

	/* png image data buffer */
	bound=compressBound(sourcelen);
	MEM_CREATE_N(data,bound+png_len);
	d=data;

	/* png signature, 8 bytes */
	i=8; while (i--) d[i]=png_sig[i];
	d+=8;

	/* IHDR chunk, 13 bytes (not counting length/name/crc) */
	SET32(0,13); d+=4; /* size */
	d[0]='I'; d[1]='H'; d[2]='D'; d[3]='R'; /* chunk type */
	SET32(4,width);
	SET32(8,height);
	d[12]=8; /* bit depth */
	d[13]=2; /* color type */
	d[14]=d[15]=d[16]=0; /* compression, filter, interlacing (no) */
	crc=crc32(0,d,17); SET32(17,crc);
	d+=21;

	/* IDAT chunk */
	destlen=bound;
	if (compress(d+8,&destlen,screen,sourcelen)!=Z_OK) {
		MEM_CLEAN(screen);
		MEM_CLEAN(data);
		semaphore=0;
		return FALSE;
	}

	MEM_CLEAN(screen);
	SET32(0,destlen); d+=4;
	d[0]='I'; d[1]='D'; d[2]='A'; d[3]='T';
	crc=crc32(0,d,destlen+4); SET32(destlen+4,crc);
	d+=(destlen+8);

	/* IEND chunk */
	i=12; while (i--) d[i]=png_iend[i];


	/* write to file */
	if (file_save_custom(&fd,fn)&&file_write_custom(fd,data,destlen+png_len)) ret=TRUE;

	file_close_custom(fd);
	MEM_CLEAN(data);

	semaphore=0;
	return ret;
}
