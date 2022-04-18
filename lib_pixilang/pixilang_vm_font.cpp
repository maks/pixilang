/*
    pixilang_vm_font.cpp
    This file is part of the Pixilang.
    Copyright (C) 2006 - 2022 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"
#include "pixilang.h"

//#define SHOW_DEBUG_MESSAGES

#ifdef SHOW_DEBUG_MESSAGES
    #define DPRINT( fmt, ARGS... ) slog( fmt, ## ARGS )
#else
    #define DPRINT( fmt, ARGS... ) {}
#endif

pix_vm_font* pix_vm_get_font_for_char( uint32_t c, pix_vm* vm )
{
    int fnum;
    int fonts = sizeof( vm->fonts ) / sizeof( pix_vm_font );
    pix_vm_font* font;
    for( fnum = 0; fnum < fonts; fnum++ )
    {
	font = &vm->fonts[ fnum ];
	if( (unsigned)font->font < (unsigned)vm->c_num )
	{
	    if( c >= font->first && c <= font->last )
	    {
		break;
	    }
	}
    }
    if( fnum == fonts ) return NULL; //Font not found for selected character.
	return font;
}

int pix_vm_set_font( uint32_t first_char, PIX_CID cnum, int xchars, int ychars, pix_vm* vm )
{
    pix_vm_font* font = NULL;
    int fonts = sizeof( vm->fonts ) / sizeof( pix_vm_font );
    for( int fnum = 0; fnum < fonts; fnum++ )
    {
	font = &vm->fonts[ fnum ];
	if( font->first == first_char ) break;
	font = NULL;
    }
    if( font == NULL )
    {
	//No font for selected first_char. Create it:
	for( int fnum = 0; fnum < fonts; fnum++ )
	{
	    font = &vm->fonts[ fnum ];
	    if( font->font == -1 ) 
	    {
		font->xchars = 16;
		font->ychars = 16;
		break;
	    }
	    font = NULL;
	}
    }
    if( font == NULL )
    {
	return 1;
    }
    if( xchars > 0 ) font->xchars = xchars;
    if( ychars > 0 ) font->ychars = ychars;
    font->first = first_char;
    font->last = first_char + font->xchars * font->ychars - 1;
    font->font = cnum;
    return 0;
}
