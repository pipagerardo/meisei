/******************************************************************************
 *                                                                            *
 *                               "settings.h"                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef SETTINGS_H
#define SETTINGS_H

/* enums */
enum {
	/* draw.c */
	SETTINGS_VSYNC=0,
	SETTINGS_RENDERTYPE,
	SETTINGS_FLIPX,
	SETTINGS_BFILTER,
	SETTINGS_SOFTRENDER,
	SETTINGS_MESSAGES,
	SETTINGS_FORCE5060,
	SETTINGS_CLIPUP,	SETTINGS_CLIPDOWN,	SETTINGS_CLIPLEFT,	SETTINGS_CLIPRIGHT,
	SETTINGS_ASPECTRATIO,
	SETTINGS_STRETCH,
	SETTINGS_WINDOWZOOM,
	SETTINGS_FULLSCREEN,
	SETTINGS_VDPCHIP,				/* vdp.c */
	SETTINGS_VIDSIGNAL,
	SETTINGS_PALETTE,
	SETTINGS_DECODER,
	SETTINGS_RYANGLE,	SETTINGS_GYANGLE,	SETTINGS_BYANGLE,
	SETTINGS_RYGAIN,	SETTINGS_GYGAIN,	SETTINGS_BYGAIN,
	SETTINGS_HUE,
	SETTINGS_SATURATION,
	SETTINGS_CONTRAST,
	SETTINGS_BRIGHTNESS,
	SETTINGS_SHARPNESS,
	SETTINGS_GAMMA,
	SETTINGS_RESOLUTION,
	SETTINGS_ARTIFACTS,
	SETTINGS_FRINGING,
	SETTINGS_BLEED,
	SETTINGS_SAVEPALETTE,
	SETTINGS_FRAMESKIP,
	SETTINGS_BORDEROVERLAY,
	SETTINGS_TIMECODE,				/* movie.c */
	SETTINGS_LAYERB,				/* vdp.c */
	SETTINGS_LAYERS,				/* vdp.c */
	SETTINGS_LAYERUNL,				/* vdp.c */

	/* sound.c */
	SETTINGS_SOUND,
	SETTINGS_SLEEP,
	SETTINGS_BUFSIZE,
	SETTINGS_LUMINOISE,				/* vdp.c */
	SETTINGS_LUMINOISE_VOLUME,		/* vdp.c */
	SETTINGS_SCC_VOLUME,			/* scc.c */
	SETTINGS_PRIORITY,				/* msx.c */
	SETTINGS_CPUSPEED,				/* crystal.c */
	SETTINGS_SPEEDPERC,				/* crystal.c */
	SETTINGS_SLOWPERC,				/* crystal.c */
	SETTINGS_REVERSESIZE,			/* reverse.c */
	SETTINGS_REVNOLIMIT,			/* reverse.c */
	SETTINGS_PSGCHIPTYPE,			/* psg.c */
	SETTINGS_RAMSIZE,				/* mapper.c */
	SETTINGS_RAMSLOT,				/* mapper.c */

	/* tool.c */
	SETTINGS_YMLOGTRIMSILENCE,		/* psglogger.c */
	SETTINGS_YMLOGINTERLEAVED,		/* psglogger.c */
	SETTINGS_CORRECTCENTS,			/* psgtoy.c */
	SETTINGS_CUSTOMWAVE,			/* psgtoy.c */
	SETTINGS_CUSTOMWAVEAMP,			/* psgtoy.c */
	SETTINGS_CUSTOMWAVEA,			/* psgtoy.c */
	SETTINGS_CUSTOMWAVEB,			/* psgtoy.c */
	SETTINGS_CUSTOMWAVEC,			/* psgtoy.c */
	SETTINGS_TILEMAPSOURCE,			/* tileview.c */
	SETTINGS_TAPPLYTOALLBLOCKS,		/* tileview.c */
	SETTINGS_TIMPORT_NT,			/* tileview.c */
	SETTINGS_TIMPORT_PGT,			/* tileview.c */
	SETTINGS_TIMPORT_CT,			/* tileview.c */
	SETTINGS_TSIMULATEMODE,			/* tileview.c */
	SETTINGS_TILEBLOAD,				/* tileview.c */
	SETTINGS_TILESAVEMIMICMASKS,	/* tileview.c */

	/* input.c */
	SETTINGS_UPDFREQUENCY,
	SETTINGS_INPBG,
	SETTINGS_KEYNETPLAY,			/* netplay.c */
	SETTINGS_JOYSTICK,
	SETTINGS_ANALOGSENSE,
	SETTINGS_MOUSESENSE,
	SETTINGS_MOUSEJOYEMU,			/* cont.c */
	SETTINGS_ARKDIALMIN,			/* cont.c */
	SETTINGS_ARKDIALMAX,			/* cont.c */
	SETTINGS_PORT1,					/* cont.c */
	SETTINGS_PORT2,					/* cont.c */
	SETTINGS_REGION,				/* cont.c */

	/* file.c */
	SETTINGS_BIOS,					/* mapper.c */
	SETTINGS_AUTODETECTROM,			/* mapper.c */
	SETTINGS_CRCLOADSTATEIGNORE,	/* msx.c */
	SETTINGS_FILTERINDEX_BIOS,		/* media.c */
	SETTINGS_FILTERINDEX_CART,		/* media.c */
	SETTINGS_FILTERINDEX_TAPE,		/* media.c */
	SETTINGS_FILTERINDEX_STATE,		/* state.c */
	SETTINGS_FILTERINDEX_MOVIE,		/* movie.c */
	SETTINGS_FILTERINDEX_PALETTE,	/* draw.c */
	SETTINGS_FILTERINDEX_OPENTILES,	/* tileview.c */
	SETTINGS_FILTERINDEX_SAVETILES,	/* tileview.c */
	SETTINGS_FILTERINDEX_OPENSPRT,	/* spriteview.c */
	SETTINGS_FILTERINDEX_SAVESPRT,	/* spriteview.c */
	SETTINGS_FILTERINDEX_ZOOMPAT,	/* tool.c */
	SETTINGS_FILTERINDEX_OPENCWAVE,	/* psgtoy.c */
	SETTINGS_FILTERINDEX_SAVECWAVE,	/* psgtoy.c */
	SETTINGS_FILTERINDEX_PSGLOG,	/* psglogger.c */
	SETTINGS_ROMDIR,
	SETTINGS_CASDIR,
	SETTINGS_BIOSDIR,
	SETTINGS_TOOLDIR,
	SETTINGS_PALETTEDIR,
	SETTINGS_PATCHDIR,
	SETTINGS_SAMPLEDIR,
	SETTINGS_SHOTDIR,
	SETTINGS_SRAMDIR,
	SETTINGS_STATEDIR,

	SETTINGS_MAX
};

enum {
	SETTINGS_TYPE_STRING=0,
	SETTINGS_TYPE_INT,
	SETTINGS_TYPE_FLOAT,

	SETTINGS_TYPE_MAX
};

void settings_init(void);
void settings_clean(void);
void settings_save(void);

void settings_disable_warning(void);
int settings_notfound(void);
int settings_get_runtimes(void);

int settings_get(const char*,char**,int*,float*);
#define SETTINGS_GET_STRING(x,y)	settings_get(x,y,NULL,NULL)
#define SETTINGS_GET_INT(x,y)		settings_get(x,NULL,y,NULL)
#define SETTINGS_GET_FLOAT(x,y)		settings_get(x,NULL,NULL,y)
int settings_get_yesnoauto(int);

int settings_put(const int,const char*,const char*,const int,const float);
#define SETTINGS_PUT_STRING(x,y)	settings_put(SETTINGS_TYPE_STRING,x,y,0,0.0)
#define SETTINGS_PUT_INT(x,y)		settings_put(SETTINGS_TYPE_INT,x,NULL,y,0.0)
#define SETTINGS_PUT_FLOAT(x,y)		settings_put(SETTINGS_TYPE_FLOAT,x,NULL,0,y)
void settings_put_yesnoauto(int,int);

const char* settings_info(u32);

#endif /* SETTINGS_H */
