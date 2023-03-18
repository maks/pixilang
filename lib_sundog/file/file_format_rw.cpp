/*
    file_format_rw.cpp - helper functions for reading and writing various file formats
    This file is part of the SunDog engine.
    Copyright (C) 2022 - 2023 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

#ifndef NOIMAGEFORMATS

//
// JPEG
//

#ifndef NOJPEG

#include "jpgd.h"
#include "jpge.h"

int sfs_load_jpeg( const char* filename, sfs_file f, simage_desc* img )
{
    int rv = -1;
    
    if( filename && f == 0 )
	f = sfs_open( filename, "rb" );
    if( f == 0 ) return -1;
	
    while( 1 )
    {
	jpeg_decoder jd;
	jd_init( f, &jd );
	if( jd.m_error_code != JPGD_SUCCESS )
	{
	    slog( "JPEG loading: jd_init() error %d\n", jd.m_error_code );
	    break;
	}
	
	int width = jd.m_image_x_size;
	int height = jd.m_image_y_size;
	int channels = jd.m_comps_in_frame;
	int pixel_format = img->format;
	img->width = width;
	img->height = height;
    
	if( jd_begin_decoding( &jd ) != JPGD_SUCCESS )
	{
	    slog( "JPEG loading: jd_begin_decoding() error %d\n", jd.m_error_code );
	    break;
	}
	
	int dest_bpp = g_simage_pixel_format_size[ pixel_format ];
	const int dest_bpl = width * dest_bpp;

        uint8_t* dest = (uint8_t*)smem_new( dest_bpl * height );
	if( !dest )
	    break;
	
	for( int y = 0; y < height; y++ )
	{
	    const uint8_t* scan_line;
	    uint scan_line_len;
	    if( jd_decode( (const void**)&scan_line, &scan_line_len, &jd ) != JPGD_SUCCESS )
	    {
		slog( "JPEG loading: jd_decode() error %d\n", jd.m_error_code );
    		smem_free( dest );
    		dest = NULL;
    		break;
	    }

	    uint8_t* dest_line = dest + y * dest_bpl;
	
	    switch( pixel_format )
	    {
    	        case PFMT_GRAYSCALE_8:
    	    	    if( channels == 3 )
    		    {
    		        /*int YR = 19595, YG = 38470, YB = 7471;
    		        for( int x = 0; x < width * 4; x += 4 )
    		        {
        	    	    int r = scan_line[ x + 0 ];
        		    int g = scan_line[ x + 1 ];
        		    int b = scan_line[ x + 2 ];
        		    *dest_line++ = static_cast<uint8_t>( ( r * YR + g * YG + b * YB + 32768 ) >> 16 );
    			}*/
    		        for( int x = 0; x < width * 4; x += 4 )
    		        {
        	    	    int r = scan_line[ x + 0 ];
        		    int g = scan_line[ x + 1 ];
        		    int b = scan_line[ x + 2 ];
        		    *dest_line++ = ( r + g + b ) / 3;
    			}
    		    }
    		    if( channels == 1 )
    		    {
		        memcpy( dest_line, scan_line, dest_bpl );
    		    }
    		    break;
    		case PFMT_RGBA_8888:
    		    if( channels == 3 )
    		    {
    		        for( int x = 0; x < width * 4; x += 4 )
    		        {
        	    	    *dest_line++ = scan_line[ x + 0 ];
        		    *dest_line++ = scan_line[ x + 1 ];
        		    *dest_line++ = scan_line[ x + 2 ];
        		    *dest_line++ = 255;
    			}
    		    }
    		    if( channels == 1 )
    		    {
    		        for( int x = 0; x < width; x++ )
    		        {
        	    	    uint8_t luma = scan_line[ x ];
        		    *dest_line++ = luma;
        		    *dest_line++ = luma;
        		    *dest_line++ = luma;
        		    *dest_line++ = 255;
    			}
    		    }
    		    break;
    		case PFMT_SUNDOG_COLOR:
    		    if( channels == 3 )
    		    {
    		        COLORPTR p = (COLORPTR)dest_line;
    			for( int x = 0; x < width * 4; x += 4 )
    			{
        		    int r = scan_line[ x + 0 ];
        		    int g = scan_line[ x + 1 ];
        		    int b = scan_line[ x + 2 ];
        		    *p++ = get_color( r, g, b );
    			}
    		    }
    		    if( channels == 1 )
    		    {
    		        COLORPTR p = (COLORPTR)dest_line;
    		        for( int x = 0; x < width; x++ )
    		        {
        	    	    uint8_t luma = scan_line[ x ];
        		    *p++ = get_color( luma, luma, luma );
    			}
    		    }
    		    break;
	    }
	}

	jd_deinit( &jd );
	
	img->data = dest;
	rv = 0;
	break;
    }

    if( f && filename )
    {
        sfs_close( f );
    }
    
    return rv;
}

const int YR = 19595, YG = 38470, YB = 7471, CB_R = -11059, CB_G = -21709, CB_B = 32768, CR_R = 32768, CR_G = -27439, CR_B = -5329;
static inline uint8_t jpeg_clamp( int i ) { if( (uint)i > 255U ) { if( i < 0 ) i = 0; else if( i > 255 ) i = 255; } return (uint8_t)i; }

static void sundog_color_to_YCC( uint8_t* dst, const uint8_t* src, int num_pixels )
{
    const COLORPTR c = (const COLORPTR)src;
    for( ; num_pixels; dst += 3, c++, num_pixels-- )
    {
        int r = red( c[ 0 ] );
        int g = green( c[ 0 ] );
        int b = blue( c[ 0 ] );
        dst[ 0 ] = (uint8_t)( ( r * YR + g * YG + b * YB + 32768 ) >> 16 );
        dst[ 1 ] = jpeg_clamp( 128 + ( ( r * CB_R + g * CB_G + b * CB_B + 32768 ) >> 16 ) );
        dst[ 2 ] = jpeg_clamp( 128 + ( ( r * CR_R + g * CR_G + b * CR_B + 32768 ) >> 16 ) );
    }
}

static void sundog_color_to_Y( uint8_t* dst, const uint8_t* src, int num_pixels )
{
    const COLORPTR c = (const COLORPTR)src;
    for( ; num_pixels; dst++, c++, num_pixels-- )
    {
        int r = red( c[ 0 ] );
        int g = green( c[ 0 ] );
        int b = blue( c[ 0 ] );
        dst[ 0 ] = (uint8_t)( ( r * YR + g * YG + b * YB + 32768 ) >> 16 );
    }
}

int sfs_save_jpeg( const char* filename, sfs_file f, simage_desc* img, sfs_jpeg_enc_params* pars )
{
    int rv = 0;
    
    int num_channels = 0;
    int pixel_size = 0;
    
    je_comp_params encoder_pars;
    init_je_comp_params( &encoder_pars );
    encoder_pars.m_quality = pars->quality;
    encoder_pars.m_two_pass_flag = pars->two_pass_flag;
    switch( pars->subsampling )
    {
	case JE_Y_ONLY: encoder_pars.m_subsampling = Y_ONLY; break;
	case JE_H1V1: encoder_pars.m_subsampling = H1V1; break;
	case JE_H2V1: encoder_pars.m_subsampling = H2V1; break;
	case JE_H2V2: encoder_pars.m_subsampling = H2V2; break;
    }

    switch( img->format )
    {
        case PFMT_GRAYSCALE_8:
    	    num_channels = 1;
    	    pixel_size = 1;
	    break;
	case PFMT_RGBA_8888:
    	    num_channels = 3;
    	    pixel_size = 4;
	    break;
	case PFMT_SUNDOG_COLOR:
    	    num_channels = 3;
    	    pixel_size = COLORLEN;
	    encoder_pars.convert_to_YCC = sundog_color_to_YCC;
            encoder_pars.convert_to_Y = sundog_color_to_Y;
	    break;
	default:
	    rv = -1;
	    goto save_jpeg_end;
	    break;
    }
    
    if( filename && f == 0 ) f = sfs_open( filename, "wb" );
    if( f )
    {
        jpeg_encoder je;
    
        je_init( &je );
        if( !je_set_params( f, img->width, img->height, num_channels, &encoder_pars, &je ) ) 
        {
    	    rv = -2;
	    goto save_jpeg_end;
	}
 
	int width = img->width;
	int height = img->height;
        const uint8_t* buf = (const uint8_t*)img->data;
        for( uint pass_index = 0; pass_index < ( encoder_pars.m_two_pass_flag ? 2 : 1 ); pass_index++ )
        {
            int line_size = width * pixel_size;
            for( int i = 0; i < height; i++ )
            {
                if( !je_process_scanline( buf, &je ) )
                {
            	    rv = -3;
            	    goto save_jpeg_end;
            	}
                buf += line_size;
            }
            if( !je_process_scanline( NULL, &je ) ) 
            {
        	rv = -4;
        	goto save_jpeg_end;
    	    }
        }
 
        je_deinit( &je );
    }
 
save_jpeg_end:

    if( filename && f ) sfs_close( f );
    if( rv )
    {
	slog( "JPEG saving error: %d\n", rv );
    }

    return rv;    
}

#endif //NOJPEG

//
// PNG
//

#ifndef NOPNG

#include "png.h"

static void sfs_png_read( png_structp png_ptr, png_bytep data, png_size_t length )
{
    sfs_file f = *(sfs_file*)png_get_io_ptr( png_ptr );
    sfs_read( data, 1, length, f );
}

static void sfs_png_write( png_structp png_ptr, png_bytep data, png_size_t length )
{
    sfs_file f = *(sfs_file*)png_get_io_ptr( png_ptr );
    sfs_write( data, 1, length, f );
}

static void sfs_png_flush( png_structp png_ptr )
{
    sfs_file f = *(sfs_file*)png_get_io_ptr( png_ptr );
    sfs_flush( f );
}

int sfs_load_png( const char* filename, sfs_file f, simage_desc* img )
{
    int rv = -1;
    
    if( filename && f == 0 )
	f = sfs_open( filename, "rb" );
    if( f == 0 ) return -1;
    
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep* row_pointers = NULL;
    int width = 0;
    int height = 0;
    void* palette = NULL;
	
    while( 1 )
    {
	png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
	if( !png_ptr ) break;
        info_ptr = png_create_info_struct( png_ptr );
        if( !info_ptr ) break;
        if( setjmp( png_jmpbuf( png_ptr ) ) ) break;
        
        png_set_read_fn( png_ptr, &f, sfs_png_read );
        png_read_info( png_ptr, info_ptr );

        img->width = width = png_get_image_width( png_ptr, info_ptr );
        img->height = height = png_get_image_height( png_ptr, info_ptr );
        int color_type = png_get_color_type( png_ptr, info_ptr );
        int bits_per_channel = png_get_bit_depth( png_ptr, info_ptr );
        int channels = png_get_channels( png_ptr, info_ptr );
	
	if( bits_per_channel != 8 )
	{
	    slog( "PNG load: unsupported number of bits per channel %d\n", bits_per_channel );
	    break;
	}
	
	int number_of_passes = png_set_interlace_handling( png_ptr );
        png_read_update_info( png_ptr, info_ptr );
	
	if( setjmp( png_jmpbuf( png_ptr ) ) ) break;
        row_pointers = (png_bytep*)smem_new( sizeof( png_bytep ) * height );
        for( int y = 0; y < height; y++ )
            row_pointers[ y ] = (png_byte*)smem_new( png_get_rowbytes( png_ptr, info_ptr ) );
        png_read_image( png_ptr, row_pointers );

	int dest_bpp = g_simage_pixel_format_size[ img->format ];
	int dest_format = img->format;
	
	if( color_type == PNG_COLOR_TYPE_PALETTE )
	{
	    png_colorp pp = NULL;
	    int items = 0;
	    if( png_get_PLTE( png_ptr, info_ptr, &pp, &items ) )
	    {
		palette = smem_new( items * dest_bpp );
		switch( dest_format )
		{
		    case PFMT_GRAYSCALE_8:
			for( int i = 0; i < items; i++ )
			{
			    int r = pp[ i ].red;
			    int g = pp[ i ].green;
			    int b = pp[ i ].blue;
			    ((uint8_t*)palette)[ i ] = ( r + g + b ) / 3;
			}
			break;
		    case PFMT_RGBA_8888:
			for( int i = 0; i < items; i++ )
			{
			    int r = pp[ i ].red;
			    int g = pp[ i ].green;
			    int b = pp[ i ].blue;
			    ((uint8_t*)palette)[ i * 4 + 0 ] = r;
			    ((uint8_t*)palette)[ i * 4 + 1 ] = g;
			    ((uint8_t*)palette)[ i * 4 + 2 ] = b;
			    ((uint8_t*)palette)[ i * 4 + 3 ] = 255;
			}
			break;
		    case PFMT_SUNDOG_COLOR:
			for( int i = 0; i < items; i++ )
			{
			    int r = pp[ i ].red;
			    int g = pp[ i ].green;
			    int b = pp[ i ].blue;
			    ((COLORPTR)palette)[ i ] = get_color( r, g, b );
			}
			break;
		}
	    }
	}

	const int dest_bpl = width * dest_bpp;
        uint8_t* dest = (uint8_t*)smem_new( dest_bpl * height );
	if( !dest ) break;
	
	if( bits_per_channel == 8 )
	{
	    for( int y = 0; y < height; y++ )
	    {
		uint8_t* src = row_pointers[ y ];
		uint8_t* dest_line = dest + y * dest_bpl;
		switch( channels )
		{
		    case 1:
			if( palette )
			{
			    switch( dest_format )
			    {
				case PFMT_GRAYSCALE_8:
				    for( int x = 0; x < width; x++ )
				    {
					int i = *src; src++;
					*dest_line = ((uint8_t*)palette)[ i ]; dest_line++;
				    }
				    break;
				case PFMT_RGBA_8888:
				    for( int x = 0; x < width; x++ )
				    {
					int i = *src; src++;
					*dest_line = ((uint8_t*)palette)[ i ]; dest_line++;
					*dest_line = ((uint8_t*)palette)[ i ]; dest_line++;
					*dest_line = ((uint8_t*)palette)[ i ]; dest_line++;
					*dest_line = ((uint8_t*)palette)[ i ]; dest_line++;
				    }
				    break;
				case PFMT_SUNDOG_COLOR:
				    for( int x = 0; x < width; x++ )
				    {
					int i = *src; src++;
					*((COLORPTR)dest_line) = ((COLORPTR)palette)[ i ]; dest_line += COLORLEN;
				    }
				    break;
			    }
			}
			else
			{
			    switch( dest_format )
			    {
				case PFMT_GRAYSCALE_8:
				    for( int x = 0; x < width; x++ )
				    {
					int v = *src; src++;
					*dest_line = v; dest_line++;
				    }
				    break;
				case PFMT_RGBA_8888:
				    for( int x = 0; x < width; x++ )
				    {
					int v = *src; src++;
					*dest_line = v; dest_line++;
					*dest_line = v; dest_line++;
					*dest_line = v; dest_line++;
					*dest_line = 255; dest_line++;
				    }
				    break;
				case PFMT_SUNDOG_COLOR:
				    for( int x = 0; x < width; x++ )
				    {
					int v = *src; src++;
					*((COLORPTR)dest_line) = get_color( v, v, v ); dest_line += COLORLEN;
				    }
				    break;
			    }
			}
			break;
		    case 3:
			switch( dest_format )
			{
			    case PFMT_GRAYSCALE_8:
				for( int x = 0; x < width; x++ )
				{
				    int r = *src; src++;
				    int g = *src; src++;
				    int b = *src; src++;
				    *dest_line = ( r + g + b ) / 3; dest_line++;
				}
				break;
			    case PFMT_RGBA_8888:
				for( int x = 0; x < width; x++ )
				{
				    int r = *src; src++;
				    int g = *src; src++;
				    int b = *src; src++;
				    *dest_line = r; dest_line++;
				    *dest_line = g; dest_line++;
				    *dest_line = b; dest_line++;
				    *dest_line = 255; dest_line++;
				}
				break;
			    case PFMT_SUNDOG_COLOR:
			        for( int x = 0; x < width; x++ )
				{
				    int r = *src; src++;
				    int g = *src; src++;
				    int b = *src; src++;
				    *((COLORPTR)dest_line) = get_color( r, g, b ); dest_line += COLORLEN;
				}
				break;
			}
			break;
		    case 4:
			switch( dest_format )
			{
			    case PFMT_GRAYSCALE_8:
				for( int x = 0; x < width; x++ )
				{
				    int r = *src; src++;
				    int g = *src; src++;
				    int b = *src; src++;
				    /*int a = *src;*/ src++;
				    *dest_line = ( r + g + b ) / 3; dest_line++;
				}
				break;
			    case PFMT_RGBA_8888:
				for( int x = 0; x < width; x++ )
				{
				    int r = *src; src++;
				    int g = *src; src++;
				    int b = *src; src++;
				    int a = *src; src++;
				    *dest_line = r; dest_line++;
				    *dest_line = g; dest_line++;
				    *dest_line = b; dest_line++;
				    *dest_line = a; dest_line++;
				}
				break;
			    case PFMT_SUNDOG_COLOR:
			        for( int x = 0; x < width; x++ )
				{
				    int r = *src; src++;
				    int g = *src; src++;
				    int b = *src; src++;
				    /*int a = *src;*/ src++;
				    *((COLORPTR)dest_line) = get_color( r, g, b ); dest_line += COLORLEN;
				}
				break;
			}
			break;
		}
	    }
	}
	
	img->data = dest;
	rv = 0;
	break;
    }

    png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
    if( row_pointers )
    {
        for( int y = 0; y < height; y++ )
            smem_free( row_pointers[ y ] );
        smem_free( row_pointers );
    }
    
    smem_free( palette );

    if( f && filename )
    {
        sfs_close( f );
    }

    return rv;
}

#endif //NOPNG

#endif //NOIMAGEFORMATS
