/******************************************************************************
 *                                                                            *
 *                                 "log.h"                                    *
 *                                                                            *
 ******************************************************************************/

#ifndef LOG_H
#define LOG_H

#define LOG_MISC		0x1
#define LOG_CPU			0x2
#define LOG_PPU			0x3
#define LOG_CPU_MMAP	0x4
#define LOG_MAPPER		0x5
#define LOG_VERBOSE		0x6
#define LOG_PROFILER	0x7
#define LOG_WARNING		0x10
#define LOG_ERROR		0x20

#define LOG_COLOUR(x)	((x)<<8)
enum {
	/* (draw.c) */
	LC_CYAN=0,
	LC_PINK,
	LC_YELLOW,
	LC_GREEN,
	LC_ORANGE,
	LC_WHITE,
	LC_BLUE,
	LC_GRAY,
	LC_BLACK,

	LC_RED
};
#define LC_MAX			LC_RED

#define LOG_TYPE(x)		((x)<<16)
enum {
	LT_CPUSPEED=1,
	LT_RESET,
	LT_VIDFORMAT,
	LT_SCREENSHOT,
	LT_PASTEFAIL,
	LT_PAUSE,
	LT_DDRAWLOCKFAIL,
	LT_DSOUNDLOCKFAIL,
	LT_NETPLAYSRAM,
	LT_SLOT1FAIL,
	LT_SLOT2FAIL,
	LT_TAPERW,
	LT_STATE,
	LT_STATEFAIL,
	LT_REVERSE,
	LT_REVLIMIT,
	LT_MOVIETIME
};
#define LT_IGNORE		0x1000

#define LOG_NOCHOP		0x40000000

#define DEBUG_PROFILER	FALSE	/* optional profiler */
#define DEBUG_CPU		FALSE	/* cpu opcodes */
#define DEBUG_CPU_MMAP	FALSE	/* cpu memory accesses */
#define DEBUG_PPU		FALSE	/* ppu scanline/hblank/vblank */
#define DEBUG_MAPPER	FALSE	/* various mapper related */
#define DEBUG_VERBOSE	FALSE	/* quite useless info */
#define DEBUG_MISC		TRUE	/* quite useful info, false: show only errors/warnings */

#define LOG_DOS_BOX		FALSE	/* compiled with dosbox enabled */
#define LOG_MAX_WIDTH	45		/* maximum number of characters of 1 logged line */

#define LOG_ERROR_WINDOW(w,s)	MessageBox(w,s,"I am Error.",MB_ICONEXCLAMATION|MB_OK)

void __cdecl LOG(int,const char*,...);
void LOG_BIN(int,u32,int);
void log_init(void);
void log_set_frame_start(u32);
void log_enable(void);
void log_disable(void);

#endif /* LOG_H */
