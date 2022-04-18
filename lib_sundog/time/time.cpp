/*
    time.cpp - time management (thread-safe)
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2022 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

#ifdef OS_UNIX
    #include <time.h>
#endif
#if defined(OS_APPLE)
    #include <mach/mach.h>	    //mach_absolute_time() ...
    #include <mach/mach_time.h>	    //mach_absolute_time() ...
    #include <unistd.h>
#endif
#ifdef OS_FREEBSD
    #include <sys/time.h>
#endif
#ifdef OS_LINUX
    #include <sys/select.h>
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    #include <time.h>
#endif

#if defined(OS_APPLE)
#ifndef OS_IOS
    #define TIMEHIGHPREC
#endif
static mach_timebase_info_data_t g_timebase_info;
static uint64_t g_time_bias = 0;
double g_time_scale1 = 0; //lowres
double g_time_scale2 = 0; //highres
static uint64_t muldiv128( uint64_t i, uint32_t numer, uint32_t denom )
{
    uint64_t high = ( i >> 32 ) * numer;
    uint64_t low = ( i & 0xffffffffull ) * numer / denom;
    uint64_t high_rem = ( ( high % denom ) << 32 ) / denom;
    high /= denom;
    return ( high << 32 ) + high_rem + low;
}
ticks_hr_t convert_mach_absolute_time_to_sundog_ticks( uint64_t mt, int tick_type ) //0 - lowres (1000 TPS); 1 - highres;
{
#ifdef TIMEHIGHPREC
    uint64_t t = muldiv128( mt - g_time_bias, g_timebase_info.numer, g_timebase_info.denom );
    if( tick_type == 0 )
    	t /= (1000000000/stime_ticks_per_second());
    else
	t /= (1000000000/stime_ticks_per_second_hires());
    return (ticks_hr_t)t;
#else
    double t = (double)( mt - g_time_bias );
    if( tick_type == 0 )
	t *= g_time_scale1;
    else
	t *= g_time_scale2;
    return (ticks_hr_t)t;
#endif
}
uint64_t convert_sundog_ticks_to_mach_absolute_time( ticks_hr_t t, int tick_type ) //0 - lowres (1000 TPS); 1 - highres;
{
    double mt = t;
    if( tick_type == 0 )
	mt /= g_time_scale1;
    else
	mt /= g_time_scale2;
    //t overflow (which will give incorrect result of this function) protection:
    /*if( t > 0xFFFFFFFF - stime_ticks_per_second_hires() * 60 * 15 ) 
    {
	//FIRST TEST IT!!
	stime_ticks_reset(); 
	mt = 0;
    }*/
    return (uint64_t)mt + g_time_bias;
}
#endif

#if defined(OS_WIN) || defined(OS_WINCE)
static uint64_t g_ticks_per_second = 0;
static ticks_hr_t g_ticks_per_second_norm = 0;
static uint g_ticks_div = 1;
static uint g_ticks_mul = 0;
#endif

int stime_global_init( void )
{
    stime_ticks_reset();
    return 0;
}

int stime_global_deinit( void )
{
    return 0;
}

stime_t stime_time( void )
{
#if defined(OS_UNIX) || defined(OS_WIN)
    time_t t;
    time( &t );
    return (stime_t)t;
#endif
#ifdef OS_WINCE
    FILETIME ft;
    SYSTEMTIME st;
    GetSystemTime( &st );
    if( SystemTimeToFileTime( &st, &ft ) )
	return (stime_t)ft.dwLowDateTime | ( (stime_t)ft.dwHighDateTime << 32 ); //number of 100-nanosecond intervals since January 1, 1601 (UTC).
    return 0;
#endif
}

int64_t stime_time_diff_sec( stime_t t1, stime_t t2 ) //t1-t2
{
#if defined(OS_UNIX) || defined(OS_WIN)
    return difftime( (time_t)t1, (time_t)t2 );
#endif
#ifdef OS_WINCE
    return ( t1 - t2 ) / 10000000;
#endif
}

uint stime_year( void )
{
#if defined(OS_UNIX) || defined(OS_WIN)
    time_t t;
    time( &t );
    return localtime( &t )->tm_year + 1900;
#endif
#ifdef OS_WINCE
    SYSTEMTIME st;
    GetLocalTime( &st );
    return st.wYear;
#endif
}

uint stime_month( void )
{
#if defined(OS_UNIX) || defined(OS_WIN)
    time_t t;
    time( &t );
    return localtime( &t )->tm_mon + 1;
#endif
#ifdef OS_WINCE
    SYSTEMTIME st;
    GetLocalTime( &st );
    return st.wMonth;
#endif
}

const char* stime_month_string( void )
{
    switch( stime_month() )
    {
	case 1: return "jan"; break;
	case 2: return "feb"; break;
	case 3: return "mar"; break;
	case 4: return "apr"; break;
	case 5: return "may"; break;
	case 6: return "jun"; break;
	case 7: return "jul"; break;
	case 8: return "aug"; break;
	case 9: return "sep"; break;
	case 10: return "oct"; break;
	case 11: return "nov"; break;
	case 12: return "dec"; break;
	default: return ""; break;
    }
}

uint stime_day( void )
{
#if defined(OS_UNIX) || defined(OS_WIN)
    time_t t;
    time( &t );
    return localtime( &t )->tm_mday;
#endif
#ifdef OS_WINCE
    SYSTEMTIME st;
    GetLocalTime( &st );
    return st.wDay;
#endif
}

uint stime_hours( void )
{
#if defined(OS_UNIX) || defined(OS_WIN)
    time_t t;
    time( &t );
    return localtime( &t )->tm_hour;
#endif
#ifdef OS_WINCE
    SYSTEMTIME st;
    GetLocalTime( &st );
    return st.wHour;
#endif
}

uint stime_minutes( void )
{
#if defined(OS_UNIX) || defined(OS_WIN)
    time_t t;
    time( &t );
    return localtime( &t )->tm_min;
#endif
#ifdef OS_WINCE
    SYSTEMTIME st;
    GetLocalTime( &st );
    return st.wMinute;
#endif
}

uint stime_seconds( void )
{
#if defined(OS_UNIX) || defined(OS_WIN)
    time_t t = 0;
    time( &t );
    return localtime( &t )->tm_sec;
#endif
#ifdef OS_WINCE
    SYSTEMTIME st;
    GetLocalTime( &st );
    return st.wSecond;
#endif
}

void stime_ticks_reset( void )
{
#if defined(OS_APPLE)
    mach_timebase_info( &g_timebase_info );
    g_time_bias = mach_absolute_time();
    g_time_scale1 = (double)g_timebase_info.numer / (double)g_timebase_info.denom / (double)(1000000000/stime_ticks_per_second()); //1000 TPS
    g_time_scale2 = (double)g_timebase_info.numer / (double)g_timebase_info.denom / (double)(1000000000/stime_ticks_per_second_hires()); //highres
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    QueryPerformanceFrequency( (LARGE_INTEGER*)&g_ticks_per_second );
    if( g_ticks_per_second > 50000 )
    {
	g_ticks_div = g_ticks_per_second / 50000;
	g_ticks_per_second_norm = (ticks_hr_t)( g_ticks_per_second / g_ticks_div );
    }
    else
    {
	g_ticks_mul = 50000 / g_ticks_per_second;
	if( g_ticks_mul * g_ticks_per_second < 50000 ) g_ticks_mul++;
	g_ticks_per_second_norm = (ticks_hr_t)( g_ticks_per_second * g_ticks_mul );
    }
    /*
    printf( "TPS: %d\n", (int)g_ticks_per_second );
    printf( "TPSN: %d\n", (int)g_ticks_per_second_norm );
    printf( "DIV: %d\n", (int)g_ticks_div );
    printf( "MUL: %d\n", (int)g_ticks_mul );
    */
#endif
}

ticks_t stime_ticks( void )
{
#ifdef OS_UNIX
    #if defined(OS_APPLE)
	return (ticks_t)convert_mach_absolute_time_to_sundog_ticks( mach_absolute_time(), 0 );
    #else
	timespec t;
	clock_gettime( CLOCK_REALTIME, &t );
	return (ticks_t)( t.tv_nsec / 1000000 ) + t.tv_sec * 1000;
    #endif
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    return (uint64_t)stime_ticks_hires() * 1000 / stime_ticks_per_second_hires();
    //return (ticks_t)GetTickCount();
#endif
}

#if defined(OS_WIN) || defined(OS_WINCE)
ticks_hr_t stime_ticks_per_second_hires( void )
{
    return g_ticks_per_second_norm;
}
#endif
#ifdef OS_WIN
ticks_hr_t __attribute__ ((force_align_arg_pointer)) stime_ticks_hires( void )
#else
ticks_hr_t stime_ticks_hires( void )
#endif
{
#ifdef OS_UNIX
    #if defined(OS_APPLE)
	return (ticks_hr_t)convert_mach_absolute_time_to_sundog_ticks( mach_absolute_time(), 1 );
    #else
	//Other UNIX systems:
        timespec t;
	clock_gettime( CLOCK_REALTIME, &t );
        return (ticks_hr_t)( t.tv_nsec / ( 1000000000 / stime_ticks_per_second_hires() ) ) + t.tv_sec * stime_ticks_per_second_hires();
    #endif
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    uint64_t tick;
    QueryPerformanceCounter( (LARGE_INTEGER*)&tick );
    if( g_ticks_mul )
	return (ticks_hr_t)( tick * g_ticks_mul );
    else
	return (ticks_hr_t)( tick / g_ticks_div );
#endif
    return 0;
}

void stime_sleep( int milliseconds )
{
#ifdef OS_UNIX
    #if defined(OS_APPLE)
	while( 1 )
	{
	    int t = milliseconds;
	    if( t > 1000 ) t = 1000;
	    usleep( t * 1000 );
	    milliseconds -= t;
	    if( milliseconds <= 0 ) break;
	}
    #else
	#ifdef OS_EMSCRIPTEN
	    emscripten_sleep( milliseconds );
	#else
	    timeval t;
	    t.tv_sec = milliseconds / 1000;
	    t.tv_usec = ( milliseconds % 1000 ) * 1000;
	    select( 0 + 1, 0, 0, 0, &t );
	#endif
    #endif
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    Sleep( milliseconds );
#endif
}
