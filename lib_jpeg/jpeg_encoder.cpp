/*
    jpeg_encoder.cpp
    This file is part of the SunDog engine.
    Copyright (C) 2005 - 2022 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"
#include "jpeg_encoder.h"

const int YR = 19595, YG = 38470, YB = 7471, CB_R = -11059, CB_G = -21709, CB_B = 32768, CR_R = 32768, CR_G = -27439, CR_B = -5329;
static inline uint8_t jpeg_clamp( int i ) { if( (uint)i > 255U ) { if( i < 0 ) i = 0; else if( i > 255 ) i = 255; } return (uint8_t)i; }

void sundog_color_to_YCC( uint8_t* dst, const uint8_t* src, int num_pixels )
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

void sundog_color_to_Y( uint8_t* dst, const uint8_t* src, int num_pixels )
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

void init_je_params( je_params* pars )
{
    pars->quality = 85;
    pars->subsampling = JE_H2V2;
    pars->two_pass_flag = 0;
    pars->pixel_format = JE_SUNDOG_COLOR;
}

int save_jpeg( const char* filename, sfs_file f, int width, int height, const uint8_t* image_data, je_params* pars )
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

    switch( pars->pixel_format )
    {
        case JE_GRAYSCALE:
    	    num_channels = 1;
    	    pixel_size = 1;
	    break;
	case JE_RGB:
    	    num_channels = 1;
    	    pixel_size = 1;
	    break;
	case JE_SUNDOG_COLOR:
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
        if( !je_set_params( f, width, height, num_channels, &encoder_pars, &je ) ) 
        {
    	    rv = -2;
	    goto save_jpeg_end;
	}
 
        for( uint pass_index = 0; pass_index < ( encoder_pars.m_two_pass_flag ? 2 : 1 ); pass_index++ )
        {
            const uint8_t* buf = image_data;
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
