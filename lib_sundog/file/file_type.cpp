/*
    file_type.cpp
    This file is part of the SunDog engine.
    Copyright (C) 2014 - 2022 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

const uint8_t g_sign_jpeg1[] = { 'I', 'F', '.' };
const uint8_t g_sign_jpeg2[] = { 'i', 'f', '.' };
const uint8_t g_sign_jpeg3[] = { 'I', 'F', 'F', '.' };
const uint8_t g_sign_jpeg4[] = { 0xFF, 0xD8, 0xFF };
const uint8_t g_sign_gif1[] = { 'G', 'I', 'F', '8', '7', 'a' };
const uint8_t g_sign_gif2[] = { 'G', 'I', 'F', '8', '9', 'a' };
const uint8_t g_sign_png[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
const uint8_t g_sign_riff[] = { 'R', 'I', 'F', 'F' };
    const uint8_t g_sign_wave[] = { 'W', 'A', 'V', 'E' };
    const uint8_t g_sign_avi[] = { 'A', 'V', 'I', ' ' };
const uint8_t g_sign_mp41[] = { 0x00, 0x00, 0x00, 0x00, 'f', 't', 'y', 'p' };
    const uint8_t g_sign_mp42[] = { '3', 'g', 'p' }; //3gp
    const uint8_t g_sign_mp43[] = { 'i', 's', 'o', 'm' }; //ISO Base Media file (MPEG-4) v1
    const uint8_t g_sign_mp44[] = { '3', 'g', 'p', '5' }; //3gp5
    const uint8_t g_sign_mp45[] = { 'm', 'p', '4', '2' }; //MPEG-4 video/QuickTime file
    const uint8_t g_sign_mp46[] = { 'M', 'S', 'N', 'V' }; //MSNV
const uint8_t g_sign_aiff[] = { 'F', 'O', 'R', 'M' };
    const uint8_t g_sign_aiff_type0[] = { 'A', 'I', 'F', 'F' };
    const uint8_t g_sign_aiff_type1[] = { 'A', 'I', 'F', 'C' };
const uint8_t g_sign_mp31[] = { 0xFF, 0xFB };
const uint8_t g_sign_mp32[] = { 'I', 'D', '3' };
const uint8_t g_sign_flac[] = { 'f', 'L', 'a', 'C' };
const uint8_t g_sign_ogg[] = { 'O', 'g', 'g', 'S' };
const uint8_t g_sign_midi[] = { 'M', 'T', 'h', 'd' };
const uint8_t g_sign_sunvox[] = { 'S', 'V', 'O', 'X', 0, 0, 0, 0 };
const uint8_t g_sign_sunvox_module[] = { 'S', 'S', 'Y', 'N', 0, 0, 0, 0 };
const uint8_t g_sign_xm[] = { 'E', 'x', 't', 'e', 'n', 'd', 'e', 'd', ' ', 'M', 'o', 'd', 'u', 'l', 'e' };
const uint8_t g_sign_zip1[] = { 'P', 'K', 0x03, 0x04, 'P', 'K', 0x05, 0x06 };
const uint8_t g_sign_zip2[] = { 'P', 'K', 0x07, 0x08 };
const uint8_t g_sign_pixicont[] = { 'p', 'i', 'x', 'i', 'C', 'O', 'N', 'T' };

#define CMP( S, OFF ) ( smem_cmp( &sign[ OFF ], S, sizeof( S ) ) == 0 )

sfs_file_type sfs_get_file_type( const char* filename, sfs_file f )
{
    sfs_file_type type = SFS_FILE_TYPE_UNKNOWN;
    sfs_file_type uncertain_type = SFS_FILE_TYPE_UNKNOWN;

    while( 1 )
    {
	if( filename && f == 0 ) f = sfs_open( filename, "rb" );
	if( f == 0 ) break;

	sfs_rewind( f );
	uint8_t sign[ 32 ];
	uint8_t temp;
	smem_clear( sign, sizeof( sign ) );
	int r = sfs_read( sign, 1, 32, f );
	sfs_rewind( f );
	while( r > 0 )
	{
	    if( sign[ 0 ] < 'P' )
	    {
		if( CMP( g_sign_jpeg1, 0 ) ) { type = SFS_FILE_TYPE_JPEG; break; }
		if( CMP( g_sign_jpeg3, 0 ) ) { type = SFS_FILE_TYPE_JPEG; break; }

		if( CMP( g_sign_gif1, 0 ) ) { type = SFS_FILE_TYPE_GIF; break; }
		if( CMP( g_sign_gif2, 0 ) ) { type = SFS_FILE_TYPE_GIF; break; }

		temp = sign[ 3 ]; sign[ 3 ] = 0;
		if( CMP( g_sign_mp41, 0 ) )
		{
		    if( CMP( g_sign_mp42, 8 ) ) { type = SFS_FILE_TYPE_MP4; break; }
		    if( CMP( g_sign_mp43, 8 ) ) { type = SFS_FILE_TYPE_MP4; break; }
		    if( CMP( g_sign_mp44, 8 ) ) { type = SFS_FILE_TYPE_MP4; break; }
		    if( CMP( g_sign_mp45, 8 ) ) { type = SFS_FILE_TYPE_MP4; break; }
		    if( CMP( g_sign_mp46, 8 ) ) { type = SFS_FILE_TYPE_MP4; break; }
		    uncertain_type = SFS_FILE_TYPE_MP4;
		}
		sign[ 3 ] = temp;

		if( CMP( g_sign_aiff, 0 ) )
		{
		    if( CMP( g_sign_aiff_type0, 8 ) || CMP( g_sign_aiff_type1, 8 ) ) { type = SFS_FILE_TYPE_AIFF; break; }
		}

		if( CMP( g_sign_mp32, 0 ) ) { type = SFS_FILE_TYPE_MP3; break; }

		if( CMP( g_sign_ogg, 0 ) ) { type = SFS_FILE_TYPE_OGG; break; }

		if( CMP( g_sign_midi, 0 ) ) { type = SFS_FILE_TYPE_MIDI; break; }

		if( CMP( g_sign_xm, 0 ) ) { type = SFS_FILE_TYPE_XM; break; }
	    }
	    else
	    {
		if( CMP( g_sign_sunvox, 0 ) ) { type = SFS_FILE_TYPE_SUNVOX; break; }
		if( CMP( g_sign_sunvox_module, 0 ) ) { type = SFS_FILE_TYPE_SUNVOXMODULE; break; }

		if( CMP( g_sign_pixicont, 0 ) ) { type = SFS_FILE_TYPE_PIXICONTAINER; break; }

    		if( CMP( g_sign_jpeg2, 0 ) ) { type = SFS_FILE_TYPE_JPEG; break; }
		if( CMP( g_sign_jpeg4, 0 ) ) { type = SFS_FILE_TYPE_JPEG; break; }

		if( CMP( g_sign_png, 0 ) ) { type = SFS_FILE_TYPE_PNG; break; }

		if( CMP( g_sign_riff, 0 ) )
		{
		    if( CMP( g_sign_wave, 8 ) ) { type = SFS_FILE_TYPE_WAVE; break; }
		    if( CMP( g_sign_avi, 8 ) ) { type = SFS_FILE_TYPE_AVI; break; }
		}

		if( CMP( g_sign_mp31, 0 ) ) { type = SFS_FILE_TYPE_MP3; break; }

		if( CMP( g_sign_flac, 0 ) ) { type = SFS_FILE_TYPE_FLAC; break; }

		if( CMP( g_sign_zip1, 0 ) ) { type = SFS_FILE_TYPE_ZIP; break; }
		if( CMP( g_sign_zip2, 0 ) ) { type = SFS_FILE_TYPE_ZIP; break; }
	    }

	    sfs_seek( f, 1080, 1 );
	    r = sfs_read( sign, 1, 4, f );
	    sfs_rewind( f );
	    if( r == 4 )
	    {
		sign[ 4 ] = 0;
		bool mod = false;
		switch( sign[ 0 ] )
		{
		    case 'M':
			if( CMP( "M.K.", 0 ) || CMP( "M!K!", 0 ) ) mod = true;
			break;
		    case '4':
			if( CMP( "4CHN", 0 ) || CMP( "4FLT", 0 ) ) mod = true;
			break;
		    case '6':
			if( CMP( "6CHN", 0 ) || CMP( "6FLT", 0 ) ) mod = true;
			break;
		    case '8':
			if( CMP( "8CHN", 0 ) || CMP( "8FLT", 0 ) ) mod = true;
			break;
		    case 'F':
			if( CMP( "FLT4", 0 ) || CMP( "FLT6", 0 ) || CMP( "FLT8", 0 ) ) mod = true;
			break;
		    case 'O':
			if( CMP( "OCTA", 0 ) ) mod = true;
			break;
		    case 'C':
			if( CMP( "CD81", 0 ) ) mod = true;
			break;
		}
		if( mod ) { type = SFS_FILE_TYPE_MOD; break; }
	    }

	    break;
	}

	if( filename && f )
	{
	    sfs_close( f );
	}

	break;
    }

    if( type == SFS_FILE_TYPE_UNKNOWN && uncertain_type != SFS_FILE_TYPE_UNKNOWN )
	type = uncertain_type;

    return type;
}

const char* sfs_get_mime_type( sfs_file_type type )
{
    const char* rv = 0;
    switch( type )
    {
	case SFS_FILE_TYPE_WAVE: rv = "audio/vnd.wave"; break;
	case SFS_FILE_TYPE_AIFF: rv = "audio/x-aiff"; break;
	case SFS_FILE_TYPE_OGG: rv = "audio/ogg"; break;
	case SFS_FILE_TYPE_MP3: rv = "audio/mpeg"; break;
	case SFS_FILE_TYPE_FLAC: rv = "audio/ogg"; break;
	case SFS_FILE_TYPE_MIDI: rv = "audio/midi"; break;
	case SFS_FILE_TYPE_SUNVOX: rv = "audio/sunvox"; break;
	case SFS_FILE_TYPE_SUNVOXMODULE: rv = "audio/sunvoxmodule"; break;
	case SFS_FILE_TYPE_XM: rv = "audio/xm"; break;
	case SFS_FILE_TYPE_MOD: rv = "audio/mod"; break;
	case SFS_FILE_TYPE_JPEG: rv = "image/jpeg"; break;
	case SFS_FILE_TYPE_PNG: rv = "image/png"; break;
	case SFS_FILE_TYPE_GIF: rv = "image/gif"; break;
	case SFS_FILE_TYPE_AVI: rv = "video/avi"; break;
	case SFS_FILE_TYPE_MP4: rv = "video/mp4"; break;
	case SFS_FILE_TYPE_ZIP: rv = "application/zip"; break;
	default: rv = "application/octet-stream"; break;
    }
    return rv;
}

const char* sfs_get_extension( sfs_file_type type )
{
    const char* rv = 0;
    switch( type )
    {
	case SFS_FILE_TYPE_WAVE: rv = "wav"; break;
	case SFS_FILE_TYPE_AIFF: rv = "aiff"; break;
	case SFS_FILE_TYPE_OGG: rv = "ogg"; break;
	case SFS_FILE_TYPE_MP3: rv = "mp3"; break;
	case SFS_FILE_TYPE_FLAC: rv = "flac"; break;
	case SFS_FILE_TYPE_MIDI: rv = "mid"; break;
	case SFS_FILE_TYPE_SUNVOX: rv = "sunvox"; break;
	case SFS_FILE_TYPE_SUNVOXMODULE: rv = "sunsynth"; break;
	case SFS_FILE_TYPE_XM: rv = "xm"; break;
	case SFS_FILE_TYPE_MOD: rv = "mod"; break;
	case SFS_FILE_TYPE_JPEG: rv = "jpg"; break;
	case SFS_FILE_TYPE_PNG: rv = "png"; break;
	case SFS_FILE_TYPE_GIF: rv = "gif"; break;
	case SFS_FILE_TYPE_AVI: rv = "avi"; break;
	case SFS_FILE_TYPE_MP4: rv = "mp4"; break;
	case SFS_FILE_TYPE_ZIP: rv = "zip"; break;
	default: break;
    }
    return rv;
}

int sfs_get_clipboard_type( sfs_file_type type )
{
    int rv = -1;
    switch( type )
    {
	case SFS_FILE_TYPE_WAVE:
	case SFS_FILE_TYPE_AIFF:
	case SFS_FILE_TYPE_OGG:
	case SFS_FILE_TYPE_MP3:
	case SFS_FILE_TYPE_FLAC:
	case SFS_FILE_TYPE_MIDI:
	case SFS_FILE_TYPE_SUNVOX:
	case SFS_FILE_TYPE_SUNVOXMODULE:
	case SFS_FILE_TYPE_XM:
	case SFS_FILE_TYPE_MOD:
	    rv = sclipboard_type_audio;
	    break;
	case SFS_FILE_TYPE_JPEG:
	case SFS_FILE_TYPE_PNG:
	case SFS_FILE_TYPE_GIF:
	    rv = sclipboard_type_image;
	    break;
	case SFS_FILE_TYPE_AVI:
	case SFS_FILE_TYPE_MP4:
	    rv = sclipboard_type_av;
	    break;
	default: break;
    }
    return rv;
}
