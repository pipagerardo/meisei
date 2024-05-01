/******************************************************************************
 *                                                                            *
 *                               "ti_ntsc.c"                                  *
 *                                                                            *
 ******************************************************************************/

/* ti_ntsc 0.1.0. http://www.slack.net/~ant/ */

#include "ti_ntsc.h"

/* Copyright (C) 2006-2007 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

ti_ntsc_setup_t const ti_ntsc_monochrome = { 0,-1, 0, 0,.2,  0, .2,-.2,-.2,-1, 0,  0, 0 };
ti_ntsc_setup_t const ti_ntsc_composite  = { 0, 0, 0, 0, 0,  0,.15,  0,  0, 0, 0,  0, 0 };
ti_ntsc_setup_t const ti_ntsc_svideo     = { 0, 0, 0, 0, 0,  0,.25, -1, -1, 0, 0,  0, 0 };
ti_ntsc_setup_t const ti_ntsc_rgb        = { 0, 0, 0, 0,.2,  0,.70, -1, -1,-1, 0,  0, 0 };

#define alignment_count 3
#define burst_count     1
#define rescale_in      8
#define rescale_out     7

#define artifacts_mid   0.4f
#define artifacts_max   1.5f
#define fringing_mid    0.8f
#define std_decoder_hue 0

#define gamma_size 256
#define default_palette_contrast 1.2f

#include "ti_ntsc_impl.h"

/* 3 input pixels -> 8 composite samples */
pixel_info_t const ti_ntsc_pixels [alignment_count] = {
	{ PIXEL_OFFSET( -4, -9 ), { 1, 1, .6667f, 0 } },
	{ PIXEL_OFFSET( -2, -7 ), {       .3333f, 1, 1, .3333f } },
	{ PIXEL_OFFSET(  0, -5 ), {                  0, .6667f, 1, 1 } },
};

/* Based on Sean Young's TMS9918A doc, rotated 15 degrees to look correct */
static unsigned char const default_palette [ti_ntsc_palette_size] [3] = {
	{  0,  0,  0}, {  0,  0,  0}, { 16,164, 91}, { 67,183,129},
	{ 82, 54,209}, {115, 84,221}, {172, 73, 19}, { 59,188,254},
	{209, 79, 14}, {245,111, 45}, {166,171, 55}, {186,182, 92},
	{ 16,141, 78}, {175, 73,126}, {173,173,173}, {222,222,222},
};

static void correct_errors( ti_ntsc_rgb_t color, ti_ntsc_rgb_t* out )
{
	unsigned i;
	for ( i = 0; i < rgb_kernel_size / 2; i++ )
	{
		ti_ntsc_rgb_t error = color -
				out [i    ] - out [(i+12)%14+14] - out [(i+10)%14+28] -
				out [i + 7] - out [i + 5    +14] - out [i + 3    +28];
		CORRECT_ERROR( i + 3 + 28 );
	}
}

void ti_ntsc_init( ti_ntsc_t* ntsc, ti_ntsc_setup_t const* setup )
{
	init_t impl;
	if ( !setup )
		setup = &ti_ntsc_composite;
	init( &impl, setup );

	{
		unsigned char const* palette = (setup->palette ? setup->palette : default_palette [0]);
		unsigned char* palette_out = setup->palette_out;
		ti_ntsc_rgb_t* kernel_out = (ntsc ? ntsc->table [0] : 0);
		int n = ti_ntsc_palette_size;

		do
		{
			float r = impl.to_float [*palette++];
			float g = impl.to_float [*palette++];
			float b = impl.to_float [*palette++];

			float y, i, q = RGB_TO_YIQ( r, g, b, y, i );

			int ir, ig, ib = YIQ_TO_RGB( y, i, q, impl.to_rgb, int, ir, ig );
			ti_ntsc_rgb_t rgb = PACK_RGB( ir, ig, ib );

			if ( palette_out )
			{
				RGB_PALETTE_OUT( rgb, palette_out );
				palette_out += 3;
			}

			if ( kernel_out )
			{
				gen_kernel( &impl, y, i, q, kernel_out );
				correct_errors( rgb, kernel_out );
				kernel_out += burst_size;
			}
		}
		while ( --n );
	}
}

#ifndef TI_NTSC_NO_BLITTERS

void ti_ntsc_blit_32( ti_ntsc_t const* ntsc, TI_NTSC_IN_T const* input, long in_row_width,
		int in_width, int height, void* rgb_out, long out_pitch, int bgc )
{
	int const chunk_count = (in_width - 1) / ti_ntsc_in_chunk;
	while ( height-- )
	{
		TI_NTSC_IN_T const* line_in = input;
		TI_NTSC_BEGIN_ROW( ntsc, bgc, bgc, TI_NTSC_ADJ_IN( *line_in ) );
		ti_ntsc_out_t* restrict line_out = (ti_ntsc_out_t*) rgb_out;
		int n;
		++line_in;

		for ( n = chunk_count; n; --n )
		{
			/* order of input and output pixels must not be altered */
			TI_NTSC_COLOR_IN( 0, ntsc, TI_NTSC_ADJ_IN( line_in [0] ) );
			TI_NTSC_RGB_OUT( 0, line_out [0], TI_NTSC_OUT_DEPTH );
			TI_NTSC_RGB_OUT( 1, line_out [1], TI_NTSC_OUT_DEPTH );

			TI_NTSC_COLOR_IN( 1, ntsc, TI_NTSC_ADJ_IN( line_in [1] ) );
			TI_NTSC_RGB_OUT( 2, line_out [2], TI_NTSC_OUT_DEPTH );
			TI_NTSC_RGB_OUT( 3, line_out [3], TI_NTSC_OUT_DEPTH );

			TI_NTSC_COLOR_IN( 2, ntsc, TI_NTSC_ADJ_IN( line_in [2] ) );
			TI_NTSC_RGB_OUT( 4, line_out [4], TI_NTSC_OUT_DEPTH );
			TI_NTSC_RGB_OUT( 5, line_out [5], TI_NTSC_OUT_DEPTH );
			TI_NTSC_RGB_OUT( 6, line_out [6], TI_NTSC_OUT_DEPTH );

			line_in  += 3;
			line_out += 7;
		}

		/* finish final pixels */
		TI_NTSC_COLOR_IN( 0, ntsc, bgc );
		TI_NTSC_RGB_OUT( 0, line_out [0], TI_NTSC_OUT_DEPTH );
		TI_NTSC_RGB_OUT( 1, line_out [1], TI_NTSC_OUT_DEPTH );

		TI_NTSC_COLOR_IN( 1, ntsc, bgc );
		TI_NTSC_RGB_OUT( 2, line_out [2], TI_NTSC_OUT_DEPTH );
		TI_NTSC_RGB_OUT( 3, line_out [3], TI_NTSC_OUT_DEPTH );

		TI_NTSC_COLOR_IN( 2, ntsc, bgc );
		TI_NTSC_RGB_OUT( 4, line_out [4], TI_NTSC_OUT_DEPTH );
		TI_NTSC_RGB_OUT( 5, line_out [5], TI_NTSC_OUT_DEPTH );
		TI_NTSC_RGB_OUT( 6, line_out [6], TI_NTSC_OUT_DEPTH );

		input += in_row_width;
		rgb_out = (char*) rgb_out + out_pitch;
	}
}

void ti_ntsc_blit_16( ti_ntsc_t const* ntsc, TI_NTSC_IN_T const* input, long in_row_width,
		int in_width, int height, void* rgb_out, long out_pitch, int bgc )
{
	int const chunk_count = (in_width - 1) / ti_ntsc_in_chunk;
	while ( height-- )
	{
		TI_NTSC_IN_T const* line_in = input;
		TI_NTSC_BEGIN_ROW( ntsc, bgc, bgc, TI_NTSC_ADJ_IN( *line_in ) );
		unsigned short* restrict line_out = (unsigned short*) rgb_out;
		int n;
		++line_in;

		for ( n = chunk_count; n; --n )
		{
			/* order of input and output pixels must not be altered */
			TI_NTSC_COLOR_IN( 0, ntsc, TI_NTSC_ADJ_IN( line_in [0] ) );
			TI_NTSC_RGB_OUT( 0, line_out [0], 16 );
			TI_NTSC_RGB_OUT( 1, line_out [1], 16 );

			TI_NTSC_COLOR_IN( 1, ntsc, TI_NTSC_ADJ_IN( line_in [1] ) );
			TI_NTSC_RGB_OUT( 2, line_out [2], 16 );
			TI_NTSC_RGB_OUT( 3, line_out [3], 16 );

			TI_NTSC_COLOR_IN( 2, ntsc, TI_NTSC_ADJ_IN( line_in [2] ) );
			TI_NTSC_RGB_OUT( 4, line_out [4], 16 );
			TI_NTSC_RGB_OUT( 5, line_out [5], 16 );
			TI_NTSC_RGB_OUT( 6, line_out [6], 16 );

			line_in  += 3;
			line_out += 7;
		}

		/* finish final pixels */
		TI_NTSC_COLOR_IN( 0, ntsc, bgc );
		TI_NTSC_RGB_OUT( 0, line_out [0], 16 );
		TI_NTSC_RGB_OUT( 1, line_out [1], 16 );

		TI_NTSC_COLOR_IN( 1, ntsc, bgc );
		TI_NTSC_RGB_OUT( 2, line_out [2], 16 );
		TI_NTSC_RGB_OUT( 3, line_out [3], 16 );

		TI_NTSC_COLOR_IN( 2, ntsc, bgc );
		TI_NTSC_RGB_OUT( 4, line_out [4], 16 );
		TI_NTSC_RGB_OUT( 5, line_out [5], 16 );
		TI_NTSC_RGB_OUT( 6, line_out [6], 16 );

		input += in_row_width;
		rgb_out = (char*) rgb_out + out_pitch;
	}
}

#endif
