/*
    jpeg_decoder.cpp
    This file is part of the SunDog engine.
    Copyright (C) 2005 - 2022 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"
#include "jpeg_decoder.h"

void* load_jpeg( const char* filename, sfs_file f, int* width, int* height, int* num_components, jd_pixel_format pixel_format )
{
    void* rv = 0;
    
    if( filename && f == 0 )
	f = sfs_open( filename, "rb" );
    if( f == 0 ) return 0;
	
    while( 1 )
    {
	jpeg_decoder jd;
	jd_init( f, &jd );
	if( jd.m_error_code != JPGD_SUCCESS )
	{
	    slog( "JPEG loading: jd_init() error %d\n", jd.m_error_code );
	    break;
	}
	
	int image_width = jd.m_image_x_size;
	int image_height = jd.m_image_y_size;
	int image_num_components = jd.m_comps_in_frame;
	if( width ) *width = image_width;
	if( height ) *height = image_height;
	if( num_components ) *num_components = image_num_components;
    
	if( jd_begin_decoding( &jd ) != JPGD_SUCCESS )
	{
	    slog( "JPEG loading: jd_begin_decoding() error %d\n", jd.m_error_code );
	    break;
	}
	
	int dest_bpp = 0;
	switch( pixel_format )
	{
	    case JD_GRAYSCALE: dest_bpp = 1; break;
	    case JD_RGB: dest_bpp = 3; break;
	    case JD_SUNDOG_COLOR: dest_bpp = COLORLEN; break;
	}
	const int dest_bpl = image_width * dest_bpp;

        uint8_t* dest = (uint8_t*)smem_new( dest_bpl * image_height );
	if( !dest )
	    break;
	
	for( int y = 0; y < image_height; y++ )
	{
	    const uint8_t* scan_line;
	    uint scan_line_len;
	    if( jd_decode( (const void**)&scan_line, &scan_line_len, &jd ) != JPGD_SUCCESS )
	    {
		slog( "JPEG loading: jd_decode() error %d\n", jd.m_error_code );
    		smem_free( dest );
    		dest = 0;
    		break;
	    }

	    uint8_t* dest_line = dest + y * dest_bpl;
	
	    if( pixel_format == JD_GRAYSCALE && image_num_components == 1 )
	    {
		memcpy( dest_line, scan_line, dest_bpl );
	    }
	    else
	    {
		switch( pixel_format )
		{
    		    case JD_GRAYSCALE:
    			if( image_num_components == 3 )
    			{
    			    int YR = 19595, YG = 38470, YB = 7471;
    			    for( int x = 0; x < image_width * 4; x += 4 )
    			    {
        			int r = scan_line[ x + 0 ];
        			int g = scan_line[ x + 1 ];
        			int b = scan_line[ x + 2 ];
        			*dest_line++ = static_cast<uint8_t>( ( r * YR + g * YG + b * YB + 32768 ) >> 16 );
    			    }
    			}
    			break;
    		    case JD_RGB:
    			if( image_num_components == 3 )
    			{
    			    for( int x = 0; x < image_width * 4; x += 4 )
    			    {
        			*dest_line++ = scan_line[ x + 0 ];
        			*dest_line++ = scan_line[ x + 1 ];
        			*dest_line++ = scan_line[ x + 2 ];
    			    }
    			}
    			if( image_num_components == 1 )
    			{
    			    for( int x = 0; x < image_width; x++ )
    			    {
        			uint8_t luma = scan_line[ x ];
        			*dest_line++ = luma;
        			*dest_line++ = luma;
        			*dest_line++ = luma;
    			    }
    			}
    			break;
    		    case JD_SUNDOG_COLOR:
    			if( image_num_components == 3 )
    			{
    			    COLORPTR p = (COLORPTR)dest_line;
    			    for( int x = 0; x < image_width * 4; x += 4 )
    			    {
        			int r = scan_line[ x + 0 ];
        			int g = scan_line[ x + 1 ];
        			int b = scan_line[ x + 2 ];
        			*p++ = get_color( r, g, b );
    			    }
    			}
    			if( image_num_components == 1 )
    			{
    			    COLORPTR p = (COLORPTR)dest_line;
    			    for( int x = 0; x < image_width; x++ )
    			    {
        			uint8_t luma = scan_line[ x ];
        			*p++ = get_color( luma, luma, luma );
    			    }
    			}
    			break;
    		}
	    }
	}

	jd_deinit( &jd );
	
	rv = (void*)dest;
	break;
    }

    if( f && filename )
    {
        sfs_close( f );
    }
    
    return rv;
}
