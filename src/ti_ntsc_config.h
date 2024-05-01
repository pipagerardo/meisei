/* Configure library by modifying this file */

#ifndef TI_NTSC_CONFIG_H
#define TI_NTSC_CONFIG_H

/* The following affect the built-in blitter only; a custom blitter can
handle things however it wants. */

/* Bits per pixel of output. Can be 15, 16, 32, or 24 (same as 32). */
#define TI_NTSC_OUT_DEPTH 32

/* Type of input pixel values */
#define TI_NTSC_IN_T unsigned char

/* Each raw pixel input value is passed through this. You might want to mask
the pixel index if you use the high bits as flags, etc. */
#define TI_NTSC_ADJ_IN( in ) in

/* For each pixel, this is the basic operation:
output_color = color_palette [TI_NTSC_ADJ_IN( TI_NTSC_IN_T )] */

#endif
