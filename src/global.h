/******************************************************************************
 *                                                                            *
 *                               "global.h"                                   *
 *                                                                            *
 ******************************************************************************/

#ifndef GLOBAL_H
#define GLOBAL_H

#define WIN32_LEAN_AND_MEAN

/* memleak debug */
#ifdef MSS
#include "mss/mss.h"
#endif

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
// int stricmp (const char*, const char*); /* case insensitive string compare */

#define TRUE			1
#define FALSE			0

#define STRING_SIZE		4096

/* typedefs */
#include <stdint.h>
typedef uint8_t	        u8;		/* 8 bit */
typedef int8_t		    s8;
typedef uint16_t    	u16;	/* 16 bit */
typedef int16_t     	s16;
typedef uint32_t        u32;	/* 32 bit */
typedef int32_t 		s32;
typedef uint64_t        u64;
typedef int64_t         s64;


/* binary; original approach by Peter Ammon, modified */
#define BINARY(b)		(b&1)|(b>>2&2)|(b>>4&4)|(b>>6&8)|(b>>8&0x10)|(b>>0xa&0x20)|(b>>0xc&0x40)|(b>>0xe&0x80)
#define	BIN8(b)			(BINARY(0##b))
#define BIN16(b,c)		((BINARY(0##b))<<8|(BINARY(0##c)))
#define BIN32(b,c,d,e)	(((BINARY(0##b))<<24)|((BINARY(0##c))<<16)|((BINARY(0##d))<<8)|(BINARY(0##e)))
#define BIT(b)			(1<<(b))

/* math */
#define PI				3.14159265358979323846
#define RAD(a)			(PI*(a)/180.0)

/* etc */
#define CLAMP(x,y,z) if (x<y) x=y; else if (x>z) x=z

#include "log.h"

/* memory (t=+typecast, n=no memset 0) */
#define MEM_CREATE_N(x,y) x=NULL; if ((x=malloc(y))==NULL) { LOG(LOG_MISC|LOG_ERROR,"Out of memory in %s line %d!\n",__FILE__,__LINE__); exit(1); }
#define MEM_CREATE(x,y) MEM_CREATE_N(x,y) memset(x,0,y)
#define MEM_CREATE_T_N(x,y,t) x=NULL; if ((x=(t)malloc(y))==NULL) { LOG(LOG_MISC|LOG_ERROR,"Out of memory in %s line %d!\n",__FILE__,__LINE__); exit(1); }
#define MEM_CREATE_T(x,y,t) MEM_CREATE_T_N(x,y,t) memset(x,0,y)
#define MEM_CLEAN(x) if (x) free(x); x=NULL

/* profiler */
#if DEBUG_PROFILER
LARGE_INTEGER prof_start[100];
LARGE_INTEGER prof_end[100];
int prof_result[100];
int prof_approx[0x80];
int prof_a_count;
#endif

#define PROFILER_INIT()																				\
	/* counts per second */																			\
	if (QueryPerformanceFrequency(&prof_start[0])) {												\
		prof_result[0]=prof_start[0].QuadPart;														\
		LOG(LOG_PROFILER,"PROFILER frequency: %d [NTSC:%d] [PAL:%d]\n",prof_result[0],(int)(prof_result[0]/59.92),(int)(prof_result[0]/50.16)); \
	}																								\
	else LOG(LOG_PROFILER,"PROFILER broken!\n");													\
	memset(prof_start,0,100*sizeof(LARGE_INTEGER)); memset(prof_end,0,100*sizeof(LARGE_INTEGER));	\
	memset(prof_result,0,100*sizeof(int)); memset(prof_approx,0,0x80*sizeof(int));					\
	prof_a_count=0

#define PROFILER_START(x) QueryPerformanceCounter(&prof_start[x])
#define PROFILER_END(x) QueryPerformanceCounter(&prof_end[x]); prof_result[x]+=(prof_end[x].QuadPart-prof_start[x].QuadPart)
#define PROFILER_RESULT(x) LOG(LOG_PROFILER,"[%2d %6d]\t",x,prof_result[x]); prof_result[x]=0

#define PROFILER_APPROX_C(x,y)																		\
	/* multiple APPROX will screw up */																\
	prof_approx[prof_a_count]=prof_result[x];														\
	prof_a_count++;																					\
	prof_a_count&=0x7f;																				\
	prof_result[x]=0;																				\
	prof_approx[prof_a_count]=-1;																	\
	for (;;) {																						\
		prof_a_count++;																				\
		prof_a_count&=0x7f;																			\
		if (prof_approx[prof_a_count]==-1) break;													\
		else prof_result[x]+=prof_approx[prof_a_count];												\
	}																								\
	LOG(LOG_PROFILER,"\r[%2d %10d]",x,prof_result[x]>>y); /* >>7=same as normal */					\
	prof_result[x]=0
#define PROFILER_APPROX(x) PROFILER_APPROX_C(x,7)

#endif /* GLOBAL_H */
