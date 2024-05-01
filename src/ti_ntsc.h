/******************************************************************************
 *                                                                            *
 *                                "ti_ntsc.h"                                 *
 *                                                                            *
 ******************************************************************************/

/*

minor changes for meisei:
- removed extra ext_decoder_hue 15 degrees shift
- added 32bpp blitter
- added support for non-black border colour

*/

/* TI 99/4A NTSC video filter */

/* ti_ntsc 0.1.0 */
#ifndef TI_NTSC_H
#define TI_NTSC_H

#include "ti_ntsc_config.h"

#ifdef __cplusplus
	extern "C" {
#endif

/* Image parameters, ranging from -1.0 to 1.0. Actual internal values shown
in parenthesis and should remain fairly stable in future versions. */
typedef struct ti_ntsc_setup_t
{
	/* Basic parameters */
	double hue;        /* -1 = -180 degrees     +1 = +180 degrees */
	double saturation; /* -1 = grayscale (0.0)  +1 = oversaturated colors (2.0) */
	double contrast;   /* -1 = dark (0.5)       +1 = light (1.5) */
	double brightness; /* -1 = dark (0.5)       +1 = light (1.5) */
	double sharpness;  /* edge contrast enhancement/blurring */

	/* Advanced parameters */
	double gamma;      /* -1 = dark (1.5)       +1 = light (0.5) */
	double resolution; /* image resolution */
	double artifacts;  /* artifacts caused by color changes */
	double fringing;   /* color artifacts caused by brightness changes */
	double bleed;      /* color bleed (color resolution reduction) */
	float const* decoder_matrix; /* optional RGB decoder matrix, 6 elements */

	/* You can replace the standard TI color generation with an RGB palette */
	unsigned char const* palette;/* optional RGB palette to use, 3 bytes per color */

	unsigned char* palette_out;  /* optional RGB palette out, 3 bytes per color */
} ti_ntsc_setup_t;

/* Video format presets */
extern ti_ntsc_setup_t const ti_ntsc_composite; /* color bleeding + artifacts */
extern ti_ntsc_setup_t const ti_ntsc_svideo;    /* color bleeding only */
extern ti_ntsc_setup_t const ti_ntsc_rgb;       /* crisp image */
extern ti_ntsc_setup_t const ti_ntsc_monochrome;/* desaturated + artifacts */

enum { ti_ntsc_palette_size = 16 };

/* Initialize and adjust parameters. Can be called multiple times on the same
ti_ntsc_t object. Can pass NULL for either parameter. */
typedef struct ti_ntsc_t ti_ntsc_t;
void ti_ntsc_init( ti_ntsc_t* ntsc, ti_ntsc_setup_t const* setup );

/* Filters one or more rows of pixels. Input pixels are 4-bit TI palette colors.
In_row_width is the number of pixels to get to the next input row. Out_pitch
is the number of *bytes* to get to the next output row. Output pixel format
is set by TI_NTSC_OUT_DEPTH (defaults to 16-bit RGB). */
void ti_ntsc_blit_16( ti_ntsc_t const* ntsc, TI_NTSC_IN_T const* input,
		long in_row_width, int in_width, int height, void* rgb_out, long out_pitch,int);
void ti_ntsc_blit_32( ti_ntsc_t const* ntsc, TI_NTSC_IN_T const* input,
		long in_row_width, int in_width, int height, void* rgb_out, long out_pitch,int);

/* Number of output pixels written by blitter for given input width. Width might
be rounded down slightly; use TI_NTSC_IN_WIDTH() on result to find rounded
value. Guaranteed not to round 256 down at all. */
#define TI_NTSC_OUT_WIDTH( in_width ) \
	((((in_width) - 1) / ti_ntsc_in_chunk + 1) * ti_ntsc_out_chunk)

/* Number of input pixels that will fit within given output width. Might be
rounded down slightly; use TI_NTSC_OUT_WIDTH() on result to find rounded
value. */
#define TI_NTSC_IN_WIDTH( out_width ) \
	(((out_width) / ti_ntsc_out_chunk - 1) * ti_ntsc_in_chunk + 1)


/* Interface for user-defined custom blitters */

enum { ti_ntsc_in_chunk  = 3 }; /* number of input pixels read per chunk */
enum { ti_ntsc_out_chunk = 7 }; /* number of output pixels generated per chunk */
enum { ti_ntsc_black     = 0 }; /* palette index for black */

/* Begins outputting row and start three pixels. First pixel will be cut off a bit.
Use ti_ntsc_black for unused pixels. Declares variables, so must be before first
statement in a block (unless you're using C++). */
#define TI_NTSC_BEGIN_ROW( ntsc, pixel0, pixel1, pixel2 ) \
	TI_NTSC_BEGIN_ROW_6_( pixel0, pixel1, pixel2, TI_NTSC_ENTRY_, ntsc )

/* Begins input pixel */
#define TI_NTSC_COLOR_IN( in_index, ntsc, color_in ) \
	TI_NTSC_COLOR_IN_( in_index, color_in, TI_NTSC_ENTRY_, ntsc )

/* Generates output pixel. Bits can be 24, 16, 15, 32 (treated as 24), or 0:
24:          RRRRRRRR GGGGGGGG BBBBBBBB (8-8-8 RGB)
16:                   RRRRRGGG GGGBBBBB (5-6-5 RGB)
15:                    RRRRRGG GGGBBBBB (5-5-5 RGB)
 0: xxxRRRRR RRRxxGGG GGGGGxxB BBBBBBBx (native internal format; x = junk bits) */
#define TI_NTSC_RGB_OUT( index, rgb_out, bits ) \
	TI_NTSC_RGB_OUT_14_( index, rgb_out, bits, 0 )


/* private */
enum { ti_ntsc_entry_size = 3 * 14 };
typedef unsigned long ti_ntsc_rgb_t;
struct ti_ntsc_t {
	ti_ntsc_rgb_t table [ti_ntsc_palette_size] [ti_ntsc_entry_size];
};

#define TI_NTSC_ENTRY_( ntsc, n ) (ntsc)->table [n & 0x0F]

/* common 3->7 ntsc macros */
#define TI_NTSC_BEGIN_ROW_6_( pixel0, pixel1, pixel2, ENTRY, table ) \
	unsigned const ti_ntsc_pixel0_ = (pixel0);\
	ti_ntsc_rgb_t const* kernel0  = ENTRY( table, ti_ntsc_pixel0_ );\
	unsigned const ti_ntsc_pixel1_ = (pixel1);\
	ti_ntsc_rgb_t const* kernel1  = ENTRY( table, ti_ntsc_pixel1_ );\
	unsigned const ti_ntsc_pixel2_ = (pixel2);\
	ti_ntsc_rgb_t const* kernel2  = ENTRY( table, ti_ntsc_pixel2_ );\
	ti_ntsc_rgb_t const* kernelx0;\
	ti_ntsc_rgb_t const* kernelx1 = kernel0;\
	ti_ntsc_rgb_t const* kernelx2 = kernel0

#define TI_NTSC_RGB_OUT_14_( x, rgb_out, bits, shift ) {\
	ti_ntsc_rgb_t raw_ =\
		kernel0  [x       ] + kernel1  [(x+12)%7+14] + kernel2  [(x+10)%7+28] +\
		kernelx0 [(x+7)%14] + kernelx1 [(x+ 5)%7+21] + kernelx2 [(x+ 3)%7+35];\
	TI_NTSC_CLAMP_( raw_, shift );\
	TI_NTSC_RGB_OUT_( rgb_out, bits, shift );\
}

/* common ntsc macros */
#define ti_ntsc_rgb_builder    ((1L << 21) | (1 << 11) | (1 << 1))
#define ti_ntsc_clamp_mask     (ti_ntsc_rgb_builder * 3 / 2)
#define ti_ntsc_clamp_add      (ti_ntsc_rgb_builder * 0x101)
#define TI_NTSC_CLAMP_( io, shift ) {\
	ti_ntsc_rgb_t sub = (io) >> (9-(shift)) & ti_ntsc_clamp_mask;\
	ti_ntsc_rgb_t clamp = ti_ntsc_clamp_add - sub;\
	io |= clamp;\
	clamp -= sub;\
	io &= clamp;\
}

#define TI_NTSC_COLOR_IN_( index, color, ENTRY, table ) {\
	unsigned color_;\
	kernelx##index = kernel##index;\
	kernel##index = (color_ = (color), ENTRY( table, color_ ));\
}

/* x is always zero except in snes_ntsc library */
#define TI_NTSC_RGB_OUT_( rgb_out, bits, x ) {\
	if ( bits == 16 )\
		rgb_out = (raw_>>(13-x)& 0xF800)|(raw_>>(8-x)&0x07E0)|(raw_>>(4-x)&0x001F);\
	if ( bits == 24 || bits == 32 )\
		rgb_out = (raw_>>(5-x)&0xFF0000)|(raw_>>(3-x)&0xFF00)|(raw_>>(1-x)&0xFF);\
	if ( bits == 15 )\
		rgb_out = (raw_>>(14-x)& 0x7C00)|(raw_>>(9-x)&0x03E0)|(raw_>>(4-x)&0x001F);\
	if ( bits == 0 )\
		rgb_out = raw_ << x;\
}

#ifdef __cplusplus
	}
#endif

#endif
