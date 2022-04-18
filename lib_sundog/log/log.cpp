/*
    log.cpp - log management (thread-safe)
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2022 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

#ifdef OS_ANDROID
    #include <android/log.h>
#endif

static volatile bool g_slog_ready = false;
static smutex g_slog_mutex;
static char* g_slog_file = NULL;
static int g_slog_disable_counter = 0;

int slog_global_init( const char* filename )
{
    g_slog_file = NULL;
    g_slog_disable_counter = 0;
    smutex_init( &g_slog_mutex, 0 );
    char* name = sfs_make_filename( filename, true );
    if( name )
    {
        g_slog_file = (char*)strdup( name ); //detach the name from the SunDog memory manager
        smem_free( name );
	sfs_remove_file( g_slog_file );
    }
    g_slog_ready = true;
    return 0;
}

int slog_global_deinit( void )
{
    if( g_slog_ready == false ) return 0;
    smutex_destroy( &g_slog_mutex );
    free( g_slog_file );
    g_slog_file = 0;
    g_slog_ready = false;
    return 0;
}

void slog_disable( void )
{
    if( g_slog_ready == false ) return;
    if( smutex_lock( &g_slog_mutex ) ) return;
    g_slog_disable_counter++;
    smutex_unlock( &g_slog_mutex );
}

void slog_enable( void )
{
    if( g_slog_ready == false ) return;
    if( smutex_lock( &g_slog_mutex ) ) return;
    if( g_slog_disable_counter > 0 )
	g_slog_disable_counter--;
    smutex_unlock( &g_slog_mutex );
}

const char* slog_get_file( void )
{
    return g_slog_file;
}

char* slog_get_latest( size_t size )
{
    const char* log_file = slog_get_file();
    size_t log_size = sfs_get_file_size( log_file );
    if( log_size == 0 ) return NULL;
    if( size > log_size ) size = log_size;
    char* rv = (char*)smem_new( size + 1 );
    if( rv == NULL ) return NULL;
    rv[ 0 ] = 0;
    FILE* f = fopen( log_file, "rb" );
    if( f )
    {
	if( log_size >= size )
	{
	    fseek( f, log_size - size, SEEK_SET );
	    fread( rv, 1, size, f );
	    rv[ size ] = 0;
	}
	else
	{
	    fread( rv, 1, log_size, f );
	    rv[ log_size ] = 0;
	}
	fclose( f );
    }
    return rv;
}

void slog( const char* format, ... )
{
#ifdef NOLOG
    return;
#endif

    while( 1 )
    {
	if( g_slog_disable_counter ) break;

	va_list p;
	va_start( p, format );

#ifdef OS_ANDROID
	__android_log_vprint( ANDROID_LOG_INFO, "native-activity", format, p );
#else
	vprintf( format, p );
#endif

	if( g_slog_file )
	{
	    if( smutex_lock( &g_slog_mutex ) == 0 )
	    {
		FILE* f = fopen( g_slog_file, "ab" );
		if( f )
		{
		    va_start( p, format );
		    vfprintf( f, format, p );
		    fclose( f );
		}
		smutex_unlock( &g_slog_mutex );
	    }
	}

	va_end( p );

	break;
    }
}

void slog_show_error_report( sundog_engine* s )
{
#ifndef NOGUI
#ifdef OS_IOS
    char* log = slog_get_latest( 1024 * 1024 );
    if( log )
    {
	char ts[ 256 ];
	sprintf( ts, "%s ERROR REPORT", g_app_name );
	send_text_to_email( s, "nightradio@gmail.com", ts, log );
	smem_free( log );
    }
#endif
#endif
}
