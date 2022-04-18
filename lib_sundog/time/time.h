#pragma once

typedef uint32_t ticks_t;
typedef uint32_t ticks_hr_t; //Hires ticks
typedef uint64_t stime_t; //Current calendar time in system dependent units

#if defined(OS_WIN) || defined(OS_WINCE)
    ticks_hr_t stime_ticks_per_second_hires( void );
#else
    #define stime_ticks_per_second_hires() (ticks_hr_t)(50000) //23 hours max
#endif
ticks_hr_t stime_ticks_hires( void );

#if defined(OS_APPLE)
ticks_hr_t convert_mach_absolute_time_to_sundog_ticks( uint64_t mt, int tick_type ); //0 - lowres (1000 TPS); 1 - highres;
uint64_t convert_sundog_ticks_to_mach_absolute_time( ticks_hr_t t, int tick_type ); //0 - lowres (1000 TPS); 1 - highres;
#endif

int stime_global_init( void );
int stime_global_deinit( void );
#define stime_ticks_per_second() (ticks_t)(1000)
void stime_ticks_reset( void );
ticks_t stime_ticks( void );

//The following functions can be used outside the SunDog engine:

stime_t stime_time( void );
int64_t stime_time_diff_sec( stime_t t1, stime_t t2 ); //t1-t2 in seconds
uint stime_year( void );
uint stime_month( void ); //from 1
const char* stime_month_string( void );
uint stime_day( void ); //from 1
uint stime_hours( void ); //0-23
uint stime_minutes( void ); //0-59
uint stime_seconds( void ); //0-59, 0-60 or 0-61 (the extra range is to accommodate for leap seconds in certain systems)
void stime_sleep( int milliseconds );

#define STIME_WAIT_FOR( STOPCOND, TIMEOUT_MS, STEP_MS, TIMEOUT_MSG ) \
    { \
	int t = 0; \
        const char* timeout_msg = TIMEOUT_MSG; \
        while( !(STOPCOND) ) \
        { \
    	    stime_sleep( STEP_MS ); \
    	    t += STEP_MS; \
    	    if( t > TIMEOUT_MS ) \
    	    { \
    		if( timeout_msg ) slog( "%s\n", timeout_msg ); \
    		break; \
    	    } \
    	} \
    }
