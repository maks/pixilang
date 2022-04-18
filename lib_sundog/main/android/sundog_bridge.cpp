/*
    sundog_bridge.cpp - SunDog<->System bridge
    This file is part of the SunDog engine.
    Copyright (C) 2012 - 2022 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"
#include "sundog_bridge.h"

#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <android/sensor.h>
#include <android/log.h>
#include <android/window.h>
#include <android_native_app_glue.h>

struct android_sundog_engine
{
    volatile bool initialized;
    pthread_t pth;
    volatile bool pth_finished;
    volatile bool exit_request;
    volatile bool eventloop_stop_request;
    volatile bool eventloop_stop_answer;
    volatile bool eventloop_gfx_stop_request;
    sem_t eventloop_sem; //instead of stime_sleep()
    sundog_engine s;

    bool sustained_performance_mode;
    int sys_ui_visible;
    bool sys_ui_vchanged;
    volatile uint32_t camera_connections = 0; // 1 << con_index

    sundog_state* in_state; //Input state (document)
};

struct sundog_bridge_struct
{
    struct android_app* app;

    char package_name[ 256 ];

    //Global references:
    jclass gref_na_class; //android/app/NativeActivity
    jclass gref_main_class; //PackageName/MyNativeActivity
    jclass alib_class; //nightradio/androidlib/AndroidLib
    jobject alib_obj;

    ASensorManager* sensorManager;
    const ASensor* accelerometerSensor;
    ASensorEventQueue* sensorEventQueue;

    EGLDisplay display;
    volatile EGLSurface surface;
    EGLContext context; //state, textures and other GL objects
    EGLConfig cur_config;

    android_sundog_engine sd;
};

char* g_android_cache_int_path = NULL;
char* g_android_cache_ext_path = NULL;
char* g_android_files_int_path = NULL;
char* g_android_files_ext_path = NULL;
char* g_android_version = NULL;
char g_android_version_correct[ 16 ] = { 0 };
int g_android_version_nums[ 8 ] = { 0 };
char* g_android_lang = NULL;
char* g_intent_file_name = NULL;
int g_android_audio_buf_size = 0;
int g_android_audio_sample_rate = 0;

#ifndef NOMAIN

#define DISP_SURFACE_ONLY		( 1 << 0 )
int engine_init_display( uint32_t flags );
void engine_term_display( uint32_t flags );

int g_android_sundog_screen_ppi = 0;
int g_android_sundog_screen_orientation = ACONFIGURATION_NAVIGATION_ANY;
bool g_android_sundog_gl_buffer_preserved = 0;
EGLDisplay g_android_sundog_display;
EGLSurface g_android_sundog_surface;

sundog_bridge_struct g_sundog_bridge;


static pthread_key_t g_key_jni;
static pthread_once_t g_key_once = PTHREAD_ONCE_INIT;
static void make_global_pthread_keys( void )
{
    pthread_key_create( &g_key_jni, NULL );
}

int android_sundog_option_exists( const char* name )
{
    int rv = 0;
    char* ts = (char*)malloc( strlen( g_android_files_ext_path ) + strlen( name ) + 64 );
    if( ts )
    {
	ts[ 0 ] = 0;
	strcat( ts, g_android_files_ext_path );
	strcat( ts, name );
	FILE* f = fopen( ts, "rb" );
	if( f == 0 )
	{
	    strcat( ts, ".txt" );
	    f = fopen( ts, "rb" );
	}
	if( f )
	{
	    rv = fgetc( f );
	    if( rv < 0 ) rv = 1;
	    fclose( f );
	}
	free( ts );
    }
    return rv;
}

JNIEnv* android_sundog_get_jni( void )
{
    JNIEnv* jni = (JNIEnv*)pthread_getspecific( g_key_jni );
    if( !jni )
    {
	g_sundog_bridge.app->activity->vm->AttachCurrentThread( &jni, NULL );
        pthread_setspecific( g_key_jni, jni );
    }
    return jni;
}

void android_sundog_release_jni( void )
{
    JNIEnv* jni = (JNIEnv*)pthread_getspecific( g_key_jni );
    if( jni )
    {
	g_sundog_bridge.app->activity->vm->DetachCurrentThread();
	pthread_setspecific( g_key_jni, NULL );
    }
}

//ticks_hr_t t1 = stime_ticks_hires();
//ticks_hr_t t2 = stime_ticks_hires();
//LOGI( "XXXX: %f", (float)( t2 - t1 ) / stime_ticks_per_second_hires() * 1000.0F );

jclass java_find_class( JNIEnv* jni, const char* class_name )
{
    jclass class1 = g_sundog_bridge.gref_na_class;
    if( class1 == NULL )
    {
        LOGW( "NativeActivity class not found!" );
    }
    else
    {
        jmethodID getClassLoader = jni->GetMethodID( class1, "getClassLoader", "()Ljava/lang/ClassLoader;" );
        jobject class_loader_obj = jni->CallObjectMethod( g_sundog_bridge.app->activity->clazz, getClassLoader );
        jclass class_loader = jni->FindClass( "java/lang/ClassLoader" );
        jmethodID loadClass = jni->GetMethodID( class_loader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;" );
        jstring class_name_str = jni->NewStringUTF( class_name );
        jclass class2 = (jclass)jni->CallObjectMethod( class_loader_obj, loadClass, class_name_str );
        if( class2 == NULL )
        {
    	    LOGW( "%s class not found!", class_name );
        }
        return class2;
    }
    return 0;
}

int java_call_i_s( const char* method_name, const char* str_par )
{
    int rv = 0;

    JNIEnv* jni = android_sundog_get_jni();
    jclass c = g_sundog_bridge.gref_main_class;
    if( c )
    {
        jmethodID m = jni->GetMethodID( c, method_name, "(Ljava/lang/String;)I" );
	if( m )
    	{
	    rv = jni->CallIntMethod( g_sundog_bridge.app->activity->clazz, m, jni->NewStringUTF( str_par ) );
    	}
    }

    return rv;
}

int java_call_i_v( const char* method_name )
{
    int rv = 0;

    JNIEnv* jni = android_sundog_get_jni();
    jclass c = g_sundog_bridge.gref_main_class;
    if( c )
    {
        jmethodID m = jni->GetMethodID( c, method_name, "()I" );
	if( m )
    	{
	    rv = jni->CallIntMethod( g_sundog_bridge.app->activity->clazz, m );
    	}
    }

    return rv;
}

int java_call_i_i( const char* method_name, int int_par )
{
    int rv = 0;

    JNIEnv* jni = android_sundog_get_jni();
    jclass c = g_sundog_bridge.gref_main_class;
    if( c )
    {
        jmethodID m = jni->GetMethodID( c, method_name, "(I)I" );
	if( m )
    	{
	    rv = jni->CallIntMethod( g_sundog_bridge.app->activity->clazz, m, int_par );
    	}
    }

    return rv;
}

int java_call2_i_si( const char* method_name, const char* str_par, int int_par )
{
    int rv = -1;

    JNIEnv* jni = android_sundog_get_jni();
    jmethodID m = jni->GetMethodID( g_sundog_bridge.alib_class, method_name, "(Ljava/lang/String;I)I" );
    if( m )
    {
	rv = jni->CallIntMethod( g_sundog_bridge.alib_obj, m, jni->NewStringUTF( str_par ), int_par );
    }

    return rv;
}

int java_call2_i_i( const char* method_name, int pars_count, int int_par1, int int_par2, int int_par3, int int_par4 )
{
    int rv = 0;

    JNIEnv* jni = android_sundog_get_jni();
    jmethodID m;
    switch( pars_count )
    {
	case 0: m = jni->GetMethodID( g_sundog_bridge.alib_class, method_name, "()I" ); if( m )	rv = jni->CallIntMethod( g_sundog_bridge.alib_obj, m ); break;
	case 1: m = jni->GetMethodID( g_sundog_bridge.alib_class, method_name, "(I)I" ); if( m ) rv = jni->CallIntMethod( g_sundog_bridge.alib_obj, m, int_par1 ); break;
	case 2: m = jni->GetMethodID( g_sundog_bridge.alib_class, method_name, "(II)I" ); if( m ) rv = jni->CallIntMethod( g_sundog_bridge.alib_obj, m, int_par1, int_par2 ); break;
	case 3: m = jni->GetMethodID( g_sundog_bridge.alib_class, method_name, "(III)I" ); if( m ) rv = jni->CallIntMethod( g_sundog_bridge.alib_obj, m, int_par1, int_par2, int_par3 ); break;
	case 4: m = jni->GetMethodID( g_sundog_bridge.alib_class, method_name, "(IIII)I" ); if( m ) rv = jni->CallIntMethod( g_sundog_bridge.alib_obj, m, int_par1, int_par2, int_par3, int_par4 ); break;
	default: break;
    }

    return rv;
}

int java_call2_i_ii64( const char* method_name, int int_par1, int64_t int_par2 )
{
    int rv = 0;

    JNIEnv* jni = android_sundog_get_jni();
    jmethodID m = jni->GetMethodID( g_sundog_bridge.alib_class, method_name, "(IJ)I" );
    if( m )
    {
	rv = jni->CallIntMethod( g_sundog_bridge.alib_obj, m, int_par1, int_par2 );
    }

    return rv;
}

int64_t java_call2_i64_i( const char* method_name, int pars_count, int int_par1 )
{
    int64_t rv = 0;

    JNIEnv* jni = android_sundog_get_jni();
    jmethodID m;
    switch( pars_count )
    {
	case 0: m = jni->GetMethodID( g_sundog_bridge.alib_class, method_name, "()J" ); if( m )	rv = jni->CallLongMethod( g_sundog_bridge.alib_obj, m ); break;
	case 1: m = jni->GetMethodID( g_sundog_bridge.alib_class, method_name, "(I)J" ); if( m ) rv = jni->CallLongMethod( g_sundog_bridge.alib_obj, m, int_par1 ); break;
	default: break;
    }

    return rv;
}

int* java_call2_iarr_i( const char* method_name, size_t* len, int par )
{
    int* rv = NULL;

    JNIEnv* jni = android_sundog_get_jni();
    jmethodID m = jni->GetMethodID( g_sundog_bridge.alib_class, method_name, "(I)[I" );
    if( m )
    {
	jintArray arr = (jintArray)jni->CallObjectMethod( g_sundog_bridge.alib_obj, m, par );
	if( arr )
	{
	    const jsize length = jni->GetArrayLength( arr );
	    *len = length;
	    int* ptr = jni->GetIntArrayElements( arr, NULL );
    	    rv = (int*)malloc( length * sizeof( int ) );
    	    smem_copy( rv, ptr, length * sizeof( int ) );
    	    jni->ReleaseIntArrayElements( arr, ptr, 0 );
    	}
    }

    return rv;
}

char* java_call2_s_i( const char* method_name, int par )
{
    char* rv = NULL;

    JNIEnv* jni = android_sundog_get_jni();
    jmethodID m = jni->GetMethodID( g_sundog_bridge.alib_class, method_name, "(I)Ljava/lang/String;" );
    if( m )
    {
	jstring s = (jstring)jni->CallObjectMethod( g_sundog_bridge.alib_obj, m, par );
	if( s )
	{
    	    const char* str = jni->GetStringUTFChars( s, 0 );
    	    rv = strdup( str );
    	    jni->ReleaseStringUTFChars( s, str );
    	}
    }

    return rv;
}

char* java_call2_s_s( const char* method_name, const char* str_par )
{
    char* rv = NULL;

    JNIEnv* jni = android_sundog_get_jni();
    jmethodID m = jni->GetMethodID( g_sundog_bridge.alib_class, method_name, "(Ljava/lang/String;)Ljava/lang/String;" );
    if( m )
    {
	jstring s = (jstring)jni->CallObjectMethod( g_sundog_bridge.alib_obj, m, jni->NewStringUTF( str_par ) );
	if( s )
	{
    	    const char* str = jni->GetStringUTFChars( s, 0 );
    	    rv = strdup( str );
    	    jni->ReleaseStringUTFChars( s, str );
    	}
    }

    return rv;
}

void sundog_state_set( sundog_engine* s, int io, sundog_state* state ) //io: 0 - app input; 1 - app output;
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    sundog_state* prev_state = NULL;
    if( io == 0 )
    {
        prev_state = sd->in_state;
        sd->in_state = state;
    }
    else
    {
    }
    sundog_state_remove( prev_state );
}

sundog_state* sundog_state_get( sundog_engine* s, int io ) //io: 0 - app input; 1 - app output;
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    sundog_state* state = NULL;
    if( io == 0 )
    {
        state = sd->in_state;
        sd->in_state = NULL;
    }
    else
    {
    }
    return state;
}

void* sundog_thread( void* arg )
{
    android_sundog_engine* sd = (android_sundog_engine*)arg;

    LOGI( "sundog_thread() ..." );

    if( engine_init_display( 0 ) )
    {
	ANativeActivity_finish( g_sundog_bridge.app->activity );
	goto sundog_thread_end;
    }
    ANativeActivity_setWindowFlags( g_sundog_bridge.app->activity, AWINDOW_FLAG_KEEP_SCREEN_ON, 0 );

    LOGI( "sundog_main() ..." );

    LOGI( "pause 100ms ..." );
    stime_sleep( 100 ); //Small pause due to strange NativeActivity behaviour (trying to close window just after the first start):
                        //in this case the sundog_main() will not be started, and we just close the thread and open it again;
    if( sd->exit_request == 0 )
    {
	sundog_main( &sd->s, true );
    }

    LOGI( "sundog_main() finished" );

    engine_term_display( 0 );

    if( sd->camera_connections )
    {
	for( int i = 0; i < 32; i++ )
	{
	    if( sd->camera_connections & ( 1 << i ) )
		android_sundog_close_camera( &sd->s, i );
	}
	sd->camera_connections = 0;
    }

    if( sd->exit_request == 0 )
    {
	//No exit requests from the OS. SunDog just closed.
	//So tell the OS to close this app:
	LOGI( "force close the activity..." );
	ANativeActivity_finish( g_sundog_bridge.app->activity );
    }

sundog_thread_end:

    LOGI( "sundog_thread() finished" );

    sd->pth_finished = 1;
    android_sundog_release_jni();
    pthread_exit( NULL );
}

static void android_sundog_wait_for_suspend( android_sundog_engine* sd, bool gfx )
{
    if( !sd->initialized ) return;
    if( sd->pth_finished ) return;

    int i = 0;
    int timeout = 1000;
    int step = 1;
    for( i = 0; i < timeout; i += step )
    {
        if( sd->pth_finished ) break;
        if( gfx )
        {
    	    if( sd->eventloop_stop_answer && g_sundog_bridge.surface == EGL_NO_SURFACE ) break;
        }
        else
        {
    	    if( sd->eventloop_stop_answer ) break;
    	}
        stime_sleep( step );
    }
    if( i >= timeout )
    {
        LOGI( "SunDog SUSPEND TIMEOUT" );
    }
}

static void android_sundog_wait_for_resume( android_sundog_engine* sd )
{
    if( !sd->initialized ) return;
    if( sd->pth_finished ) return;

    int i = 0;
    int timeout = 1000;
    int step = 1;
    for( i = 0; i < timeout; i += step )
    {
        if( sd->pth_finished ) break;
        if( sd->eventloop_stop_answer == false ) break;
        if( sd->eventloop_stop_request ) break; //suspended by some other request; don't wait here
        if( sd->eventloop_gfx_stop_request ) break; //gfx is also suspended, so we don't need to wait for resuming here
        stime_sleep( step );
    }
    if( i >= timeout )
    {
        LOGI( "SunDog RESUME TIMEOUT" );
    }
}

static void android_sundog_pause( android_sundog_engine* sd, bool pause )
{
    if( !sd->initialized ) return;
    if( sd->pth_finished ) return;
    if( pause )
    {
	//Suspend SunDog eventloop:
	sd->eventloop_stop_request = true;
	android_sundog_wait_for_suspend( sd, false );
    }
    else
    {
	//Resume SunDog eventloop:
	sd->eventloop_stop_request = false;
	android_sundog_wait_for_resume( sd );
    }
}

static void android_sundog_init( android_sundog_engine* sd )
{
    int err;

    LOGI( "android_sundog_init() ..." );

    while( 1 )
    {
	if( !sd->initialized ) break;
	if( sd->pth_finished ) break;
	//Resume:
	sd->eventloop_gfx_stop_request = false;
	android_sundog_wait_for_resume( sd );
	return;
	break;
    }

    size_t zsize = (size_t)( (char*)&sd->in_state - (char*)sd );
    memset( sd, 0, zsize );
    sd->s.device_specific = sd;
    sd->sys_ui_visible = 1;
    sem_init( &sd->eventloop_sem, 0, 0 );

    err = pthread_create( &sd->pth, 0, &sundog_thread, sd );
    if( err == 0 )
    {
	//The pthread_detach() function marks the thread identified by thread as
	//detached. When a detached thread terminates, its resources are 
	//automatically released back to the system.
	err = pthread_detach( sd->pth );
	if( err != 0 )
	{
	    LOGW( "android_sundog_init(): pthread_detach error %d", err );
	    return;
	}
    }
    else
    {
	LOGW( "android_sundog_init(): pthread_create error %d", err );
	return;
    }

    LOGI( "android_sundog_init(): done" );

    sd->initialized = 1;
}

static void android_sundog_deinit( android_sundog_engine* sd, bool try_to_stay_in_background )
{
    if( sd->initialized == 0 ) return;
    if( sd->pth_finished ) return;

    LOGI( "android_sundog_deinit() ..." );

    while( try_to_stay_in_background )
    {
	if( !sd->s.wm.initialized ) break;
	if( sd->s.wm.exit_request ) break;
	if( g_sundog_bridge.display == EGL_NO_DISPLAY ) break;
        if( g_sundog_bridge.context == EGL_NO_CONTEXT ) break;
	if( g_sundog_bridge.surface == EGL_NO_SURFACE ) break;
	//Suspend:
	LOGI( "android_sundog_deinit(): stay in background..." );
	sd->eventloop_gfx_stop_request = true;
	android_sundog_wait_for_suspend( sd, true );
	return;
	break;
    }

    //APP_CMD_DESTROY -> Stop the thread:
    //Here the system can completely kill the app in a short time
    //(~10ms on Samsung with Android 11 ?)
    //So don't save data here!
    sd->exit_request = 1;
    win_destroy_request( &sd->s.wm );
    sd->eventloop_stop_request = false; //Resume SunDog eventloop if it was in the Pause state
    sd->eventloop_gfx_stop_request = false;
    sem_post( &sd->eventloop_sem );
    int timeout_counter = 1000 * 7; //ms
    int step = 1; //ms
    while( timeout_counter > 0 )
    {
	win_destroy_request( &sd->s.wm );
	struct timespec delay;
	delay.tv_sec = 0;
	delay.tv_nsec = 1000000 * step;
	if( sd->pth_finished ) break;
	nanosleep( &delay, NULL ); //Sleep for delay time
	timeout_counter -= step;
    }
    if( timeout_counter <= 0 )
    {
	LOGW( "android_sundog_deinit(): thread timeout" );
    }
    else
    {
	sem_destroy( &sd->eventloop_sem );
    }

    LOGI( "android_sundog_deinit(): done" );

    sd->initialized = 0;
}

void android_sundog_screen_redraw( void )
{
    if( eglSwapBuffers( g_sundog_bridge.display, g_sundog_bridge.surface ) == EGL_FALSE )
    {
	LOGI( "eglSwapBuffers error %x", eglGetError() );
	//EGL_BAD_SURFACE!
    }
}

void android_sundog_event_handler( window_manager* wm )
{
    HANDLE_THREAD_EVENTS;
    android_sundog_engine* sd = (android_sundog_engine*)wm->sd->device_specific;

    if( sd->eventloop_stop_request || sd->eventloop_gfx_stop_request )
    {
#ifdef ALLOW_WORK_IN_BG
#ifndef SUNDOG_MODULE
        uint32_t ss_idle_start = wm->sd->ss_idle_frame_counter;
        uint32_t ss_sample_rate = wm->sd->ss_sample_rate;
#endif
#endif
	bool term_disp = false;
	ticks_t term_disp_t = 0;
	win_suspend( true, wm );
	while( sd->eventloop_stop_request || sd->eventloop_gfx_stop_request )
	{
    	    if( sd->eventloop_gfx_stop_request )
    	    {
    		if( !term_disp )
    		{
	    	    engine_term_display( DISP_SURFACE_ONLY );
		    term_disp = true;
    		    term_disp_t = stime_ticks();
		}
		else
		{
		    bool exit_app = false;
#ifdef ALLOW_WORK_IN_BG
#ifndef SUNDOG_MODULE
		    if( ss_sample_rate && wm->sd->ss_idle_frame_counter - ss_idle_start > ss_sample_rate * 60 * 4 )
        	    {
                	LOGI( "Sound idle timeout" );
                	exit_app = true;
        	    }
#endif
#else
		    ticks_t tt = stime_ticks() - term_disp_t; //amount of time spent in a suspended state
		    if( tt > stime_ticks_per_second() * 4 ) exit_app = true; //work in the background is not allowed for this app; so just close it...
#endif
		    if( exit_app )
		    {
			win_exit_request( wm );
			sd->eventloop_stop_request = false;
			sd->eventloop_gfx_stop_request = false;
			break;
		    }
		}
    	    }
	    sd->eventloop_stop_answer = true;
	    if( 0 )
	    {
    		stime_sleep( 100 );
    	    }
    	    else
    	    {
    		int timeout = 100; //ms
    		struct timespec t;
                clock_gettime( CLOCK_REALTIME, &t );
	        t.tv_sec += timeout / 1000;
    	        t.tv_nsec += ( timeout % 1000 ) * 1000000;
	        if( t.tv_nsec >= 1000000000 )
    	        {
        	    t.tv_sec++;
            	    t.tv_nsec -= 1000000000;
        	}
        	sem_timedwait( &sd->eventloop_sem, &t );
    	    }
	}
	if( term_disp )
	{
    	    engine_init_display( DISP_SURFACE_ONLY );
	}
	sd->eventloop_stop_answer = false;
	win_suspend( false, wm );
    }

    if( g_intent_file_name )
    {
	sundog_state* state = sundog_state_new( g_intent_file_name, SUNDOG_STATE_TEMP );
	free( g_intent_file_name ); g_intent_file_name = NULL;
	if( state )
        {
	    sundog_state_set( wm->sd, 0, state );
    	    send_event( wm->root_win, EVT_LOADSTATE, wm );
        }
    }
}

int android_sundog_check_for_permissions( sundog_engine* s, int p )
{
    int rv = java_call2_i_i( "CheckForPermissions", 1, p, 0, 0, 0 );
    if( ( rv & p ) == p )
	return rv;
    rv = 0;
    int t = 0;
    while( 1 )
    {
	int rv2 = java_call2_i_i( "CheckForPermissions", 1, -1, 0, 0, 0 );
	if( rv2 != -1 )
	{
	    rv = rv2;
	    break;
	}
	stime_sleep( 100 );
        t += 100;
        if( t > 60 * 1000 )
        {
    	    slog( "check_for_permissions() timeout\n" );
            break;
        }
    };
    return rv;
}

char* android_sundog_get_external_files_dir( int n ) //n: 0 - primary; 1 - secondary; retval: string allocated with malloc()
{
    char* rv = NULL;
    char ts[ 32 ];
    const char* nstr = "external_files";
    if( n >= 1 )
    {
	sprintf( ts, "external_files%d", n + 1 );
	nstr = (const char*)ts;
    }
    char* str = java_call2_s_s( "GetDir", nstr );
    if( str )
    {
	mkdir( str, 0770 );
	rv = (char*)malloc( strlen( str ) + 8 ); rv[ 0 ] = 0;
	strcat( rv, str );
	strcat( rv, "/" );
	free( str );
    }
    return rv;
}

int android_sundog_copy_resources( void )
{
    return java_call_i_v( "CopyResources" );
}

void android_sundog_open_url( sundog_engine* s, const char* url_text )
{
    java_call2_i_si( "OpenURL", url_text, 0 );
}

void android_sundog_send_file_to_gallery( sundog_engine* s, const char* path )
{
    if( g_android_version_nums[ 0 ] < 10 ) android_sundog_check_for_permissions( s, 1 );
    java_call2_i_si( "SendFileToGallery", path, 0 );
}

void android_sundog_clipboard_copy( const char* txt )
{
    java_call2_i_si( "ClipboardCopy", txt, 0 );
}

char* android_sundog_clipboard_paste( void )
{
    return java_call2_s_i( "ClipboardPaste", 0 );
}

//Get the native or optimal output buffer size - 
//number of audio frames that the HAL (Hardware Abstraction Layer) buffer can hold.
//You should construct your audio buffers so that they contain an exact multiple of this number.
//If you use the correct number of audio frames, your callbacks occur at regular intervals, which reduces jitter.
int android_sundog_get_audio_buffer_size( void )
{
    return java_call2_i_i( "GetAudioOutputBufferSize", 0, 0, 0, 0, 0 );
}

//Get the native or optimal output sample rate
int android_sundog_get_audio_sample_rate( void )
{
    return java_call2_i_i( "GetAudioOutputSampleRate", 0, 0, 0, 0, 0 );
}

int* android_sundog_get_exclusive_cores( sundog_engine* s, size_t* len )
{
    if( g_android_version_nums[ 0 ] < 7 )
    {
	//Not supported:
	return NULL;
    }
    return java_call2_iarr_i( "GetExclusiveCores", len, 0 );
}

int android_sundog_set_sustained_performance_mode( sundog_engine* s, int enable )
{
    int rv = -1;
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    if( g_android_version_nums[ 0 ] < 7 )
    {
	//Not supported:
	return -1;
    }
    if( (bool)enable != sd->sustained_performance_mode )
    {
	sd->sustained_performance_mode = enable;
	rv = java_call2_i_i( "SetSustainedPerformanceMode", 1, enable, 0, 0, 0 );
    }
    return rv;
}

static uint64_t android_sundog_get_window_insets( android_sundog_engine* sd )
{
    if( g_android_version_nums[ 0 ] < 10 )
    {
	//Not supported:
	return 0;
    }
    if( sd->sys_ui_visible == 0 ) return 0;
    return java_call2_i64_i( "GetWindowInsets", 0, 0 );
}

void android_sundog_set_safe_area( sundog_engine* s )
{
    if( g_android_version_nums[ 0 ] < 10 )
    {
	//Not supported:
	return;
    }
    if( !s ) return;
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    if( !sd ) return;
    uint64_t i = android_sundog_get_window_insets( sd );
    int x = ( i & 0xFFFF );
    int y = ( ( i >> 16 ) & 0xFFFF );
    int x2 = ( ( i >> 32 ) & 0xFFFF );
    int y2 = ( ( i >> 48 ) & 0xFFFF );
    int w = 0;
    int h = 0;
    if( i )
    {
	w = s->screen_xsize - x - x2; if( w < 0 ) w = 0;
	h = s->screen_ysize - y - y2; if( h < 0 ) h = 0;
    }
    s->screen_safe_area[ 0 ] = x;
    s->screen_safe_area[ 1 ] = y;
    s->screen_safe_area[ 2 ] = w;
    s->screen_safe_area[ 3 ] = h;
    //LOGI( "SAFE AREA: %d %d %d %d\n", x, y, x2, y2 );
}

static int android_sundog_set_system_ui_visibility_( android_sundog_engine* sd, int v )
{
    if( sd->sys_ui_vchanged == false && v == 1 )
    {
	//Already visible (by default)
	return -1;
    }
    if( g_android_version_nums[ 0 ] < 4 )
    {
	//Not supported:
	return -1;
    }
    sd->sys_ui_vchanged = true;
    sd->sys_ui_visible = v;
    int rv = java_call2_i_i( "SetSystemUIVisibility", 1, v, 0, 0, 0 );
    if( rv == 0 ) sd->s.screen_changed_w++;
    return rv;
}

int android_sundog_set_system_ui_visibility( sundog_engine* s, int v )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    if( !sd ) return -1;
    return android_sundog_set_system_ui_visibility_( sd, v );
}

int android_sundog_open_camera( sundog_engine* s, int cam_id, void* user_data )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    android_sundog_check_for_permissions( s, 4 );
    int rv = java_call2_i_ii64( "OpenCamera", cam_id, (int64_t)user_data );
    if( rv >= 0 )
    {
	sd->camera_connections |= ( 1 << rv );
    }
    return rv;
}

int android_sundog_close_camera( sundog_engine* s, int con_index )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    int rv = java_call2_i_i( "CloseCamera", 1, con_index, 0, 0, 0 );
    if( rv == 0 )
    {
	sd->camera_connections &= ~( 1 << con_index );
    }
    return rv;
}

int android_sundog_get_camera_width( sundog_engine* s, int con_index )
{
    return java_call2_i_i( "GetCameraWidth", 1, con_index, 0, 0, 0 );
}

int android_sundog_get_camera_height( sundog_engine* s, int con_index )
{
    return java_call2_i_i( "GetCameraHeight", 1, con_index, 0, 0, 0 );
}

int android_sundog_get_camera_format( sundog_engine* s, int con_index )
{
    return java_call2_i_i( "GetCameraFormat", 1, con_index, 0, 0, 0 );
}

int android_sundog_get_camera_focus_mode( sundog_engine* s, int con_index )
{
    return java_call2_i_i( "GetCameraFocusMode", 1, con_index, 0, 0, 0 );
}

int android_sundog_set_camera_focus_mode( sundog_engine* s, int con_index, int mode )
{
    return java_call2_i_i( "SetCameraFocusMode", 2, con_index, mode, 0, 0 );
}

struct android_app* android_sundog_get_app_struct( window_manager* wm )
{
    return g_sundog_bridge.app;
}

int android_sundog_midi_init( sundog_engine* s )
{
    return java_call2_i_i( "MIDIInit", 0, 0, 0, 0, 0 );
}

//flags: 1 - for reading; 2 - for writing;
char* android_sundog_get_midi_devports( sundog_engine* s, int flags )
{
    return java_call2_s_i( "GetMIDIDevports", flags );
}

//rw: 0 - for reading; 1 - for writing;
int android_sundog_midi_open_devport( sundog_engine* s, const char* name, int port_id )
{
    return java_call2_i_si( "OpenMIDIDevport", name, port_id );
}

int android_sundog_midi_close_devport( sundog_engine* s, int con_index )
{
    return java_call2_i_i( "CloseMIDIDevport", 1, con_index, 0, 0, 0 );
}

int android_sundog_midi_send( sundog_engine* s, int con_index, int msg, int msg_len, int t )
{
    return java_call2_i_i( "MIDISend", 4, con_index, msg, msg_len, t );
}

//
// Main
//

static const char* get_egl_error_str( EGLint err )
{
    const char* rv = "?";
    switch( err )
    {
	case EGL_SUCCESS: rv = "EGL_SUCCESS"; break;
	case EGL_NOT_INITIALIZED: rv = "EGL_NOT_INITIALIZED"; break;
	case EGL_BAD_ACCESS: rv = "EGL_BAD_ACCESS"; break;
	case EGL_BAD_ALLOC: rv = "EGL_BAD_ALLOC"; break;
	case EGL_BAD_ATTRIBUTE: rv = "EGL_BAD_ATTRIBUTE"; break;
	case EGL_BAD_CONTEXT: rv = "EGL_BAD_CONTEXT"; break;
	case EGL_BAD_CONFIG: rv = "EGL_BAD_CONFIG"; break;
	case EGL_BAD_CURRENT_SURFACE: rv = "EGL_BAD_CURRENT_SURFACE"; break;
	case EGL_BAD_DISPLAY: rv = "EGL_BAD_DISPLAY"; break;
	case EGL_BAD_SURFACE: rv = "EGL_BAD_SURFACE"; break;
	case EGL_BAD_MATCH: rv = "EGL_BAD_MATCH"; break;
	case EGL_BAD_PARAMETER: rv = "EGL_BAD_PARAMETER"; break;
	case EGL_BAD_NATIVE_PIXMAP: rv = "EGL_BAD_NATIVE_PIXMAP"; break;
	case EGL_BAD_NATIVE_WINDOW: rv = "EGL_BAD_NATIVE_WINDOW"; break;
	case EGL_CONTEXT_LOST: rv = "EGL_CONTEXT_LOST"; break;
    }
    return rv;
}

// Initialize an EGL context for the current display.
int engine_init_display( uint32_t flags )
{
    android_sundog_engine* sd = &g_sundog_bridge.sd;

    if( g_sundog_bridge.app->window == NULL )
    {
	LOGW( "NULL window" );
	return -1;
    }

    if( flags & DISP_SURFACE_ONLY )
    {
	while( 1 )
	{
	    LOGI( "Creating a new OpenGL ES surface..." );
	    EGLSurface s = eglCreateWindowSurface( g_sundog_bridge.display, g_sundog_bridge.cur_config, g_sundog_bridge.app->window, NULL );
	    if( s == EGL_NO_SURFACE )
	    {
    		LOGW( "eglCreateWindowSurface() error %d", eglGetError() );
		break;
	    }
	    if( eglMakeCurrent( g_sundog_bridge.display, s, s, g_sundog_bridge.context ) != EGL_TRUE ) 
	    {
    		LOGW( "eglMakeCurrent error %d", eglGetError() );
    		break;
	    }
	    if( g_android_sundog_gl_buffer_preserved )
	    {
    		eglSurfaceAttrib( g_sundog_bridge.display, s, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED );
		EGLint sb = EGL_BUFFER_DESTROYED;
    		eglQuerySurface( g_sundog_bridge.display, s, EGL_SWAP_BEHAVIOR, &sb );
    		if( sb != EGL_BUFFER_PRESERVED )
    		{
    		    //Something went wrong and we can no longer set EGL_SWAP_BEHAVIOR to EGL_BUFFER_PRESERVED :(
		    sd->s.wm.screen_buffer_preserved = false;
    	    	    g_android_sundog_gl_buffer_preserved = false;
    		    LOGI( "EGL_BUFFER_DESTROYED" );
		}
	    }
	    g_sundog_bridge.surface = s;
	    g_android_sundog_surface = s;
	    sd->s.screen_changed_w++;

	    return 0;
	    break;
	}
	engine_term_display( 0 );
	window_manager* wm = &sd->s.wm;
	if( wm->initialized ) win_restart_request( wm );
    }

    //Main OpenGL ES init:
    g_sundog_bridge.display = eglGetDisplay( EGL_DEFAULT_DISPLAY );
    if( g_sundog_bridge.display == EGL_NO_DISPLAY )
    {
	LOGW( "eglGetDisplay() error %d", eglGetError() );
	return -1;
    }
    if( eglInitialize( g_sundog_bridge.display, NULL, NULL ) != EGL_TRUE )
    {
	LOGW( "eglInitialize() error %d", eglGetError() );
    	return -1;
    }

    //Get a list of EGL frame buffer configurations that match specified attributes:
    const EGLint attribs1[] =
    { //desired config 1:
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_SWAP_BEHAVIOR_PRESERVED_BIT,
	EGL_DEPTH_SIZE, 16, //minimum value
        EGL_NONE
    };
    const EGLint attribs2[] = 
    { //desired config 2:
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_DEPTH_SIZE, 16,
        EGL_NONE
    };
    EGLint numConfigs = 0;
    EGLint num = 0;
    EGLConfig config[ 2 ];
    bool buf_preserved = true;
#ifdef GLNORETAIN
    buf_preserved = false;
#endif
    if( android_sundog_option_exists( "option_glnoretain" ) )
    {
        LOGI( "option_glnoretain found. OpenGL Buffer Preserved = NO" );
        buf_preserved = false;
    }
    if( buf_preserved )
    {
	if( eglChooseConfig( g_sundog_bridge.display, attribs1, &config[ numConfigs ], 1, &num ) != EGL_TRUE )
	{
    	    LOGW( "eglChooseConfig() error %d", eglGetError() );
	}
	LOGI( "EGL Configs with EGL_SWAP_BEHAVIOR_PRESERVED_BIT: %d", num );
	numConfigs += num;
    }
    num = 0;
    if( eglChooseConfig( g_sundog_bridge.display, attribs2, &config[ numConfigs ], 1, &num ) != EGL_TRUE )
    {
	LOGW( "eglChooseConfig() error %d", eglGetError() );
    }
    LOGI( "EGL Configs without EGL_SWAP_BEHAVIOR_PRESERVED_BIT: %d", num );
    numConfigs += num;
    if( numConfigs == 0 )
    {
	LOGW( "ERROR: No matching configs (numConfigs == 0)" );
	return -1;
    }

    //Print the configs:
    /*for( EGLint cnum = 0; cnum < numConfigs; cnum++ )
    {
	EGLint cv, r, g, b, d;
	eglGetConfigAttrib( g_sundog_bridge.display, config[ cnum ], EGL_RED_SIZE, &r );
	eglGetConfigAttrib( g_sundog_bridge.display, config[ cnum ], EGL_GREEN_SIZE, &g );
	eglGetConfigAttrib( g_sundog_bridge.display, config[ cnum ], EGL_BLUE_SIZE, &b );
	eglGetConfigAttrib( g_sundog_bridge.display, config[ cnum ], EGL_DEPTH_SIZE, &d );
	eglGetConfigAttrib( g_sundog_bridge.display, config[ cnum ], EGL_SURFACE_TYPE, &cv );
	char stype[ 256 ];
	stype[ 0 ] = 0;
	if( cv & EGL_PBUFFER_BIT ) strcat( stype, "PBUF " );
	if( cv & EGL_PIXMAP_BIT ) strcat( stype, "PXM " );
	if( cv & EGL_WINDOW_BIT ) strcat( stype, "WIN " );
	if( cv & EGL_VG_COLORSPACE_LINEAR_BIT ) strcat( stype, "CL " );
	if( cv & EGL_VG_ALPHA_FORMAT_PRE_BIT ) strcat( stype, "AFP " );
	if( cv & EGL_MULTISAMPLE_RESOLVE_BOX_BIT ) strcat( stype, "MRB " );
	if( cv & EGL_SWAP_BEHAVIOR_PRESERVED_BIT ) strcat( stype, "PRESERVED " );
	LOGI( "EGL Config %d. RGBT: %d %d %d %d %x %s", cnum, r, g, b, d, cv, stype );
    }*/

    //Choose the config:
    EGLint w = 0, h = 0, format;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    bool configErr = 1;
    for( EGLint cnum = 0; cnum < numConfigs; cnum++ )
    {
	LOGI( "EGL Config %d", cnum );

	if( surface != EGL_NO_SURFACE ) eglDestroySurface( g_sundog_bridge.display, surface );
	if( context != EGL_NO_CONTEXT ) eglDestroyContext( g_sundog_bridge.display, context );
	surface = EGL_NO_SURFACE;
	context = EGL_NO_CONTEXT;

	/* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
         * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
         * As soon as we picked a EGLConfig, we can safely reconfigure the
         * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
        if( eglGetConfigAttrib( g_sundog_bridge.display, config[ cnum ], EGL_NATIVE_VISUAL_ID, &format ) != EGL_TRUE )
        {
    	    LOGW( "eglGetConfigAttrib() error %d", eglGetError() );
	    continue;
	}

        ANativeWindow_setBuffersGeometry( g_sundog_bridge.app->window, 0, 0, format );

        surface = eglCreateWindowSurface( g_sundog_bridge.display, config[ cnum ], g_sundog_bridge.app->window, NULL );
	if( surface == EGL_NO_SURFACE )
        {
    	    LOGW( "eglCreateWindowSurface() error %d", eglGetError() );
	    continue;
	}

	EGLint gles2_attrib[] = 
	{
	    EGL_CONTEXT_CLIENT_VERSION, 2,
	    EGL_NONE
	};
	context = eglCreateContext( g_sundog_bridge.display, config[ cnum ], NULL, gles2_attrib );
	if( surface == EGL_NO_CONTEXT )
        {
	    LOGW( "eglCreateContext() error %d", eglGetError() );
	    continue;
	}

	if( eglMakeCurrent( g_sundog_bridge.display, surface, surface, context ) != EGL_TRUE ) 
	{
    	    LOGW( "eglMakeCurrent error %d", eglGetError() );
    	    continue;
	}

	if( buf_preserved )
	{
    	    if( eglSurfaceAttrib( g_sundog_bridge.display, surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED ) != EGL_TRUE )
    		LOGW( "eglSurfaceAttrib error %d", eglGetError() );
	    EGLint sb = EGL_BUFFER_DESTROYED;
    	    if( eglQuerySurface( g_sundog_bridge.display, surface, EGL_SWAP_BEHAVIOR, &sb ) != EGL_TRUE )
    		LOGW( "eglQuerySurface error %d", eglGetError() );
    	    if( sb == EGL_BUFFER_PRESERVED )
	    {
		g_android_sundog_gl_buffer_preserved = 1;
		LOGI( "EGL_BUFFER_PRESERVED" );
    	    }
	    else
    	    {
    	        g_android_sundog_gl_buffer_preserved = 0;
    		LOGI( "EGL_BUFFER_DESTROYED" );
	    }
	}
	else
	{
    	    g_android_sundog_gl_buffer_preserved = 0;
    	}

    	g_sundog_bridge.cur_config = config[ cnum ];
	configErr = 0;
	break;
    }
    if( configErr )
    {
	LOGW( "ERROR: No matching configs." );
	return -1;
    }

    eglQuerySurface( g_sundog_bridge.display, surface, EGL_WIDTH, &w );
    if( eglQuerySurface( g_sundog_bridge.display, surface, EGL_HEIGHT, &h ) != EGL_TRUE )
	LOGW( "eglQuerySurface error %s", get_egl_error_str( eglGetError() ) );
    if( w <= 1 ) LOGW( "BAD EGL_WIDTH %d", w );
    if( h <= 1 ) LOGW( "BAD EGL_HEIGHT %d", h );

    g_sundog_bridge.context = context;
    g_sundog_bridge.surface = surface;
    g_android_sundog_surface = surface;
    g_android_sundog_display = g_sundog_bridge.display;

    sd->s.screen_xsize = w;
    sd->s.screen_ysize = h;
    android_sundog_set_safe_area( &sd->s );

    glDisable( GL_DEPTH_TEST );
#ifndef GLNORETAIN
    if( g_android_sundog_gl_buffer_preserved == 0 )
    {
	glDisable( GL_DITHER );
    }
#endif

    return 0;
}

// Tear down the EGL context currently associated with the display.
void engine_term_display( uint32_t flags )
{
    if( flags & DISP_SURFACE_ONLY )
    {
	if( g_sundog_bridge.display != EGL_NO_DISPLAY )
	{
    	    eglMakeCurrent( g_sundog_bridge.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT ); //release the current context
    	    if( g_sundog_bridge.surface != EGL_NO_SURFACE ) eglDestroySurface( g_sundog_bridge.display, g_sundog_bridge.surface );
	    g_sundog_bridge.surface = EGL_NO_SURFACE;
	    g_android_sundog_surface = EGL_NO_SURFACE;
    	}
    	return;
    }
    if( g_sundog_bridge.display != EGL_NO_DISPLAY )
    {
        eglMakeCurrent( g_sundog_bridge.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT ); //release the current context
        if( g_sundog_bridge.context != EGL_NO_CONTEXT ) eglDestroyContext( g_sundog_bridge.display, g_sundog_bridge.context );
        if( g_sundog_bridge.surface != EGL_NO_SURFACE ) eglDestroySurface( g_sundog_bridge.display, g_sundog_bridge.surface );
        eglTerminate( g_sundog_bridge.display );
    }
    g_sundog_bridge.context = EGL_NO_CONTEXT;
    g_sundog_bridge.display = EGL_NO_DISPLAY;
    g_android_sundog_display = EGL_NO_DISPLAY;
    g_sundog_bridge.surface = EGL_NO_SURFACE;
    g_android_sundog_surface = EGL_NO_SURFACE;
}

static uint16_t convert_key( int32_t vk )
{
    uint16_t rv = 0xFFFF;

    switch( vk )
    {
	case AKEYCODE_BACK: rv = KEY_ESCAPE; break;
	case AKEYCODE_MENU: rv = KEY_MENU; break;

	case AKEYCODE_SOFT_LEFT: rv = KEY_LEFT; break;
	case AKEYCODE_SOFT_RIGHT: rv = KEY_RIGHT; break;
	case AKEYCODE_0: case 0x90: rv = '0'; break;
	case AKEYCODE_1: case 0x91: rv = '1'; break;
	case AKEYCODE_2: case 0x92: rv = '2'; break;
        case AKEYCODE_3: case 0x93: rv = '3'; break;
	case AKEYCODE_4: case 0x94: rv = '4'; break;
	case AKEYCODE_5: case 0x95: rv = '5'; break;
	case AKEYCODE_6: case 0x96: rv = '6'; break;
	case AKEYCODE_7: case 0x97: rv = '7'; break;
        case AKEYCODE_8: case 0x98: rv = '8'; break;
	case AKEYCODE_9: case 0x99: rv = '9'; break;
        case AKEYCODE_STAR: rv = '*'; break;
        case AKEYCODE_POUND: rv = '#'; break;
	case AKEYCODE_DPAD_UP: rv = KEY_UP; break;
	case AKEYCODE_DPAD_DOWN: rv = KEY_DOWN; break;
	case AKEYCODE_DPAD_LEFT: rv = KEY_LEFT; break;
	case AKEYCODE_DPAD_RIGHT: rv = KEY_RIGHT; break;
	case AKEYCODE_DPAD_CENTER: rv = KEY_SPACE; break;
	case AKEYCODE_A: rv = 'a'; break;
	case AKEYCODE_B: rv = 'b'; break;
	case AKEYCODE_C: rv = 'c'; break;
	case AKEYCODE_D: rv = 'd'; break;
        case AKEYCODE_E: rv = 'e'; break;
	case AKEYCODE_F: rv = 'f'; break;
	case AKEYCODE_G: rv = 'g'; break;
        case AKEYCODE_H: rv = 'h'; break;
	case AKEYCODE_I: rv = 'i'; break;
        case AKEYCODE_J: rv = 'j'; break;
	case AKEYCODE_K: rv = 'k'; break;
	case AKEYCODE_L: rv = 'l'; break;
        case AKEYCODE_M: rv = 'm'; break;
	case AKEYCODE_N: rv = 'n'; break;
        case AKEYCODE_O: rv = 'o'; break;
	case AKEYCODE_P: rv = 'p'; break;
	case AKEYCODE_Q: rv = 'q'; break;
        case AKEYCODE_R: rv = 'r'; break;
	case AKEYCODE_S: rv = 's'; break;
	case AKEYCODE_T: rv = 't'; break;
	case AKEYCODE_U: rv = 'u'; break;
	case AKEYCODE_V: rv = 'v'; break;
	case AKEYCODE_W: rv = 'w'; break;
        case AKEYCODE_X: rv = 'x'; break;
        case AKEYCODE_Y: rv = 'y'; break;
        case AKEYCODE_Z: rv = 'z'; break;
        case AKEYCODE_COMMA: rv = ','; break;
        case AKEYCODE_PERIOD: rv = '.'; break;
	case AKEYCODE_ALT_LEFT: rv = KEY_ALT; break;
	case AKEYCODE_ALT_RIGHT: rv = KEY_ALT; break;
	case AKEYCODE_SHIFT_LEFT: rv = KEY_SHIFT; break;
        case AKEYCODE_SHIFT_RIGHT: rv = KEY_SHIFT; break;
        case AKEYCODE_TAB: rv = KEY_TAB; break;
        case AKEYCODE_SPACE: rv = ' '; break;
	case AKEYCODE_ENTER: rv = KEY_ENTER; break;
        case AKEYCODE_DEL: rv = KEY_BACKSPACE; break;
        //case 112: rv = KEY_DELETE; break; //AKEYCODE_FORWARD_DEL
        case AKEYCODE_GRAVE: rv = '`'; break;
	case AKEYCODE_MINUS: rv = '-'; break;
        case AKEYCODE_EQUALS: rv = '='; break;
        case AKEYCODE_LEFT_BRACKET: rv = '['; break;
	case AKEYCODE_RIGHT_BRACKET: rv = ']'; break;
	case AKEYCODE_BACKSLASH: rv = '\\'; break;
	case AKEYCODE_SEMICOLON: rv = ';'; break;
	case AKEYCODE_APOSTROPHE: rv = '\''; break;
	case AKEYCODE_SLASH: rv = '/'; break;
	case AKEYCODE_AT: rv = '@'; break;
	case AKEYCODE_PLUS: rv = '+'; break;
	case AKEYCODE_PAGE_UP: rv = KEY_PAGEUP; break;
	case AKEYCODE_PAGE_DOWN: rv = KEY_PAGEDOWN; break;
	case AKEYCODE_BUTTON_A: rv = 'a'; break;
	case AKEYCODE_BUTTON_B: rv = 'b'; break;
        case AKEYCODE_BUTTON_C: rv = 'c'; break;
        case AKEYCODE_BUTTON_X: rv = 'x'; break;
        case AKEYCODE_BUTTON_Y: rv = 'y'; break;
        case AKEYCODE_BUTTON_Z: rv = 'z'; break;
        case 131: rv = KEY_F1; break;
        case 132: rv = KEY_F2; break;
        case 133: rv = KEY_F3; break;
        case 134: rv = KEY_F4; break;
        case 135: rv = KEY_F5; break;
        case 136: rv = KEY_F6; break;
        case 137: rv = KEY_F7; break;
        case 138: rv = KEY_F8; break;
        case 139: rv = KEY_F9; break;
        case 140: rv = KEY_F10; break;
        case 141: rv = KEY_F11; break;
        case 142: rv = KEY_F12; break;
    }

    if( rv == 0xFFFF ) rv = KEY_UNKNOWN + vk;
    return rv;
}

static uint get_modifiers( int32_t ms )
{
    uint evt_flags = 0;
    if( ms & AMETA_ALT_ON ) evt_flags |= EVT_FLAG_ALT;
    if( ms & AMETA_SHIFT_ON ) evt_flags |= EVT_FLAG_SHIFT;
    if( ms & AMETA_CTRL_ON ) evt_flags |= EVT_FLAG_CTRL;
    return evt_flags;
}

static void send_key_event( bool down, uint16_t key, uint16_t scancode, uint16_t flags, window_manager* wm )
{
    if( !wm ) return;
    if( !wm->initialized ) return;
    int type;
    if( down )
        type = EVT_BUTTONDOWN;
    else
        type = EVT_BUTTONUP;
    send_event( 0, type, flags, 0, 0, key, scancode, 1024, wm );
}

// Process the next input event.
static int32_t engine_handle_input( struct android_app* app, AInputEvent* event ) 
{
    int32_t rv = 0;
    android_sundog_engine* sd = &g_sundog_bridge.sd;
    if( sd->initialized == 0 ) return 0;
    window_manager* wm = &sd->s.wm;
    int x, y;
    int pressure;
    int32_t id;
    int32_t evt_type = AInputEvent_getType( event );
    if( evt_type == AINPUT_EVENT_TYPE_KEY )
    {
	int32_t sc = AKeyEvent_getScanCode( event );
	int32_t vk = AKeyEvent_getKeyCode( event );
	uint16_t sundog_key = convert_key( vk );
	uint evt_flags = get_modifiers( AKeyEvent_getMetaState( event ) );
	int32_t evt = AKeyEvent_getAction( event );
	switch( evt )
	{
	    case AKEY_EVENT_ACTION_DOWN:
		//slog( "KEY DOWN sc:%d vk:%d ms:%x\n", sc, vk, ms );
		send_key_event( 1, sundog_key, sc, evt_flags, wm );
		break;
	    case AKEY_EVENT_ACTION_UP:
		//slog( "KEY UP sc:%d vk:%d ms:%x\n", sc, vk, ms );
		send_key_event( 0, sundog_key, sc, evt_flags, wm );
		break;
	}
	switch( vk )
	{
	    case AKEYCODE_BACK:
	    case AKEYCODE_MENU:
		rv = 1; //prevent default handler
		break;
	}
    }
    if( evt_type == AINPUT_EVENT_TYPE_MOTION )
    {
	size_t pcount = AMotionEvent_getPointerCount( event );
	int32_t evt = AMotionEvent_getAction( event );
	uint evt_flags = get_modifiers( AMotionEvent_getMetaState( event ) );
	switch( evt & AMOTION_EVENT_ACTION_MASK )
	{
	    case AMOTION_EVENT_ACTION_DOWN:
		//First touch (primary pointer):
		x = AMotionEvent_getX( event, 0 );
		y = AMotionEvent_getY( event, 0 );
		pressure = 1024; //AMotionEvent_getPressure( event, 0 ) * 1024;
		id = AMotionEvent_getPointerId( event, 0 );
		collect_touch_events( 0, TOUCH_FLAG_REALWINDOW_XY | TOUCH_FLAG_LIMIT, evt_flags, x, y, pressure, id, wm );
		rv = 1;
		break;
	    case AMOTION_EVENT_ACTION_MOVE:
		{
		    for( size_t i = 0; i < pcount; i++ )
    		    {
			id = AMotionEvent_getPointerId( event, i );
			x = AMotionEvent_getX( event, i );
			y = AMotionEvent_getY( event, i );
			pressure = 1024; //AMotionEvent_getPressure( event, i ) * 1024;
			uint evt_flags2 = 0;
			if( i < pcount - 1 ) evt_flags2 |= EVT_FLAG_DONTDRAW;
			collect_touch_events( 1, TOUCH_FLAG_REALWINDOW_XY | TOUCH_FLAG_LIMIT, evt_flags | evt_flags2, x, y, pressure, id, wm );
    		    }
		}
		rv = 1;
		break;
	    case AMOTION_EVENT_ACTION_UP:
	    case AMOTION_EVENT_ACTION_CANCEL:
		{
		    for( size_t i = 0; i < pcount; i++ )
    		    {
			id = AMotionEvent_getPointerId( event, i );
			x = AMotionEvent_getX( event, i );
			y = AMotionEvent_getY( event, i );
			pressure = 1024; //AMotionEvent_getPressure( event, i ) * 1024;
			collect_touch_events( 2, TOUCH_FLAG_REALWINDOW_XY | TOUCH_FLAG_LIMIT, evt_flags, x, y, pressure, id, wm );
    		    }
		}
		rv = 1;
		break;
	    case AMOTION_EVENT_ACTION_POINTER_DOWN:
		{
		    int i = ( evt & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK ) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
		    id = AMotionEvent_getPointerId( event, i );
		    x = AMotionEvent_getX( event, i );
		    y = AMotionEvent_getY( event, i );
		    pressure = 1024; //AMotionEvent_getPressure( event, i ) * 1024;
		    collect_touch_events( 0, TOUCH_FLAG_REALWINDOW_XY | TOUCH_FLAG_LIMIT, evt_flags, x, y, pressure, id, wm );
		}
		rv = 1;
		break;
	    case AMOTION_EVENT_ACTION_POINTER_UP:
		{
		    int i = ( evt & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK ) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
		    id = AMotionEvent_getPointerId( event, i );
		    x = AMotionEvent_getX( event, i );
		    y = AMotionEvent_getY( event, i );
		    pressure = 1024; //AMotionEvent_getPressure( event, i ) * 1024;
		    collect_touch_events( 2, TOUCH_FLAG_REALWINDOW_XY | TOUCH_FLAG_LIMIT, evt_flags, x, y, pressure, id, wm );
		}
		rv = 1;
		break;
	}
	send_touch_events( wm );
    }
    return rv;
}

// Process the next main command.
static void engine_handle_cmd( struct android_app* app, int32_t cmd ) 
{
    android_sundog_engine* sd = &g_sundog_bridge.sd;
    switch( cmd )
    {
        case APP_CMD_DESTROY:
    	    LOGI( "APP_CMD_DESTROY" );
    	    break;

        case APP_CMD_START:
    	    LOGI( "APP_CMD_START" );
    	    break;
        case APP_CMD_STOP: //can come when you press Home or Power (lock)
    	    LOGI( "APP_CMD_STOP" );
    	    break;

	case APP_CMD_PAUSE:
    	    LOGI( "APP_CMD_PAUSE" );
    	    android_sundog_pause( sd, true );
	    break;
	case APP_CMD_RESUME:
    	    LOGI( "APP_CMD_RESUME" );
    	    {
    		char* new_intent = java_call2_s_i( "GetIntentFile", 1 );
    		if( new_intent ) g_intent_file_name = new_intent;
	    }
    	    android_sundog_pause( sd, false );
	    break;

        case APP_CMD_SAVE_STATE:
    	    LOGI( "APP_CMD_SAVE_STATE" );
            break;

        case APP_CMD_INIT_WINDOW:
            // The window is being shown, get it ready.
    	    LOGI( "APP_CMD_INIT_WINDOW" );
            if( g_sundog_bridge.app->window ) android_sundog_init( sd );
            break;
        case APP_CMD_TERM_WINDOW:
            // The window is being hidden or closed, clean it up.
    	    LOGI( "APP_CMD_TERM_WINDOW" );
            if( g_sundog_bridge.app->window ) android_sundog_deinit( sd, true );
            break;
        case APP_CMD_GAINED_FOCUS:
            // When our app gains focus, we start monitoring the accelerometer.
    	    LOGI( "APP_CMD_GAINED_FOCUS" );
            /*if( g_sundog_bridge.accelerometerSensor != NULL) 
	    {
                // We'd like to get 60 events per second (in us).
                //ASensorEventQueue_enableSensor( engine->sensorEventQueue, engine->accelerometerSensor );
                //ASensorEventQueue_setEventRate( engine->sensorEventQueue, engine->accelerometerSensor, (1000L/60)*1000 );
            }*/
            {
        	int v = -1;
        	if( sd->sys_ui_visible != 1 )
        	{
        	    v = android_sundog_set_system_ui_visibility_( sd, sd->sys_ui_visible );
    		}
    		else
    		{
    		    //System bars are already visible (by default or after the previous set_system_ui_visibility() call)
    		}
    		if( v != 0 )
    		{
    		    //Screen size may change without APP_CMD_CONFIG_CHANGED ? (2021 Android 11)
    		    //So let's check it one more time just in case:
		    sd->s.screen_changed_w++;
		}
	    }
            break;
        case APP_CMD_LOST_FOCUS:
            // When our app loses focus, we stop monitoring the accelerometer.
            // This is to avoid consuming battery while not being used.
    	    LOGI( "APP_CMD_LOST_FOCUS" );
            /*if( g_sundog_bridge.accelerometerSensor != NULL ) 
	    {
                //ASensorEventQueue_disableSensor( engine->sensorEventQueue, engine->accelerometerSensor );
            }*/
            break;
	case APP_CMD_CONFIG_CHANGED:
    	    LOGI( "APP_CMD_CONFIG_CHANGED" );
	    sd->s.screen_changed_w++;
	    break;
    }
}

volatile int g_activity_state = 0;
void android_main( struct android_app* state )
{
    char* str = NULL;
    JNIEnv* jni = NULL;
    LOGI( "ANDROID MAIN " ARCH_NAME " " __DATE__ " " __TIME__ );

    if( g_activity_state )
    {
	//Only one app instance is allowed in this version of SunDog engine
	//For future updates:
	// 1) add support of multiple instances (without globals, like g_sundog_bridge etc.);
	// 2) manifest: remove android:launchMode="singleTask" or replace it by "singleTop"; then test "open in..." from the other apps;
	//              OS 4.4.2: "singleTop" does not work as expected?...
	// 3) manifest: add android:resizeableActivity ? (level 24, OS 7.0);
	LOGE( "Only one app instance is allowed. Closing current activity..." );
	ANativeActivity_finish( state->activity );
	while( 1 )
        {
	    int ident;
            int events;
	    struct android_poll_source* source;
    	    while( ( ident = ALooper_pollAll( -1, NULL, &events, (void**)&source ) ) >= 0 )
	    {
        	if( source ) source->process( state, source );
    		if( state->destroyRequested != 0 ) return;
    	    }
	}
    	return;
    }
    g_activity_state = 1;

    pthread_once( &g_key_once, make_global_pthread_keys );

    memset( &g_sundog_bridge, 0, sizeof( g_sundog_bridge ) );
    //For future updates: don't forget to clear in_state at the first initialization, like in iOS bridge!
    state->userData = &g_sundog_bridge;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;
    g_sundog_bridge.app = state;

    jni = android_sundog_get_jni();

    {
	jclass c = NULL;

	c = jni->FindClass( "android/app/NativeActivity" );
	if( !c ) { LOGE( "NativeActivity class not found" ); goto break_all; }
        g_sundog_bridge.gref_na_class = (jclass)jni->NewGlobalRef( c );
	jni->DeleteLocalRef( c ); c = NULL;

        jmethodID getApplicationContext = jni->GetMethodID( g_sundog_bridge.gref_na_class, "getApplicationContext", "()Landroid/content/Context;" );
	jobject context_obj = jni->CallObjectMethod( state->activity->clazz, getApplicationContext );
	if( !context_obj ) { LOGE( "Context error" ); goto break_all; }
	jclass context_class = jni->GetObjectClass( context_obj );
	jmethodID getPackageName = jni->GetMethodID( context_class, "getPackageName", "()Ljava/lang/String;" );
	jstring packName = (jstring)jni->CallObjectMethod( context_obj, getPackageName );
	if( !packName ) { LOGE( "Can't get package name" ); goto break_all; }

	char ts[ 256 ];
	const char* pkg_name_cstr = jni->GetStringUTFChars( packName, NULL );
	g_sundog_bridge.package_name[ 0 ] = 0;
	sprintf( g_sundog_bridge.package_name, "%s", pkg_name_cstr );
	sprintf( ts, "%s/MyNativeActivity", pkg_name_cstr );
	jni->ReleaseStringUTFChars( packName, pkg_name_cstr );

	c = java_find_class( jni, ts ); //Main app class "PackageName/MyNativeActivity"
	if( !c ) { LOGE( "MyNativeActivity class not found" ); goto break_all; }
    	g_sundog_bridge.gref_main_class = (jclass)jni->NewGlobalRef( c );
	jni->DeleteLocalRef( c ); c = NULL;

	jfieldID fid = jni->GetFieldID( g_sundog_bridge.gref_main_class, "lib", "Lnightradio/androidlib/AndroidLib;" );
	if( fid )
	{
	    jobject lib_obj = jni->GetObjectField( state->activity->clazz, fid );
	    if( lib_obj )
	    {
	        c = java_find_class( jni, "nightradio/androidlib/AndroidLib" );
	        if( c )
	        {
    	    	    g_sundog_bridge.alib_class = (jclass)jni->NewGlobalRef( c );
    		    g_sundog_bridge.alib_obj = jni->NewGlobalRef( lib_obj );
		    jni->DeleteLocalRef( c ); c = NULL;
		    jni->DeleteLocalRef( lib_obj ); lib_obj = NULL;
		}
	    }
	    else
	    {
	        LOGE( "AndroidLib object not found" );
	        goto break_all;
	    }
	}
	else
	{
	    LOGE( "AndroidLib field not found" );
	    goto break_all;
	}
    }

    // Get system info:

    g_android_sundog_screen_ppi = AConfiguration_getDensity( state->config );
    g_android_sundog_screen_orientation = AConfiguration_getOrientation( state->config );

    str = java_call2_s_s( "GetDir", "internal_cache" ); //Files in this folder may be deleted by system
    if( str )
    {
	mkdir( str, 0770 );
	char* str2 = (char*)malloc( strlen( str ) + 8 ); str2[ 0 ] = 0;
	strcat( str2, str );
	strcat( str2, "/" );
	g_android_cache_int_path = str2;
	free( str );
    }
    else g_android_cache_int_path = strdup( "" );

    str = java_call2_s_s( "GetDir", "internal_files" ); //Hidden for the user
    if( str )
    {
	mkdir( str, 0770 );
	char* str2 = (char*)malloc( strlen( str ) + 8 ); str2[ 0 ] = 0;
	strcat( str2, str );
	strcat( str2, "/" );
	g_android_files_int_path = str2;
	free( str );
    }
    else g_android_files_int_path = strdup( "" );

    str = java_call2_s_s( "GetDir", "external_cache" ); //Primary storage device: cache files that may be deleted by system
    if( str )
    {
	mkdir( str, 0770 );
	char* str2 = (char*)malloc( strlen( str ) + 8 ); str2[ 0 ] = 0;
	strcat( str2, str );
	strcat( str2, "/" );
	g_android_cache_ext_path = str2;
	free( str );
    }
    else g_android_cache_ext_path = strdup( "" );

    g_android_files_ext_path = android_sundog_get_external_files_dir( 0 ); //Primary storage device
    if( !g_android_files_ext_path )
	g_android_files_ext_path = strdup( "" );

    str = java_call2_s_i( "GetOSVersion", 0 );
    if( str )
    {
	g_android_version = str;
    }
    else g_android_version = strdup( "2.3" );

    str = java_call2_s_i( "GetLanguage", 0 );
    if( str )
    {
	g_android_lang = str;
    }
    else g_android_lang = strdup( "en_US" );

    g_intent_file_name = java_call2_s_i( "GetIntentFile", 0 );

    LOGI( "Internal Cache Dir: %s", g_android_cache_int_path );
    LOGI( "Internal Files Dir: %s", g_android_files_int_path );
    LOGI( "External Cache Dir: %s", g_android_cache_ext_path );
    LOGI( "External Files Dir: %s", g_android_files_ext_path );
    LOGI( "OS Version: %s", g_android_version );
    LOGI( "Language: %s", g_android_lang );
    if( g_intent_file_name ) LOGI( "Intent File Name: %s\n", g_intent_file_name );

    memset( g_android_version_nums, 0, sizeof( g_android_version_nums ) );
    for( int i = 0, android_version_ptr = 0; ; i++ )
    {
        char c = g_android_version[ i ];
        if( c == 0 ) break;
        if( c == '.' ) android_version_ptr++;
        if( c >= '0' && c <= '9' )
        {
            g_android_version_nums[ android_version_ptr ] *= 10;
            g_android_version_nums[ android_version_ptr ] += c - '0';
        }
    }
    sprintf( g_android_version_correct, "%d.%d.%d", g_android_version_nums[ 0 ], g_android_version_nums[ 1 ], g_android_version_nums[ 2 ] );
    LOGI( "Android version (correct): %s\n", g_android_version_correct );

    g_android_audio_buf_size = android_sundog_get_audio_buffer_size();
    g_android_audio_sample_rate = android_sundog_get_audio_sample_rate();

    // Prepare to monitor accelerometer:
    /*g_sundog_bridge.sensorManager = ASensorManager_getInstance();
    g_sundog_bridge.accelerometerSensor = ASensorManager_getDefaultSensor( g_sundog_bridge.sensorManager, ASENSOR_TYPE_ACCELEROMETER );
    g_sundog_bridge.sensorEventQueue = ASensorManager_createEventQueue( g_sundog_bridge.sensorManager, state->looper, LOOPER_ID_USER, NULL, NULL );*/

    // Loop waiting for stuff:

    while( 1 ) 
    {
        // Read all pending events.
        int ident;
        int events;
        struct android_poll_source* source;

        // If not animating, we will block forever waiting for events.
        // If animating, we loop until all events are read, then continue
        // to draw the next frame of animation.
        while( ( ident = ALooper_pollAll( -1, NULL, &events, (void**)&source ) ) >= 0 ) 
        {

            // Process this event.
            if( source != NULL ) 
            {
                source->process( state, source );
            }

            // If a sensor has data, process it now.
            /*if( ident == LOOPER_ID_USER ) 
            {
                if( g_sundog_bridge.accelerometerSensor != NULL ) 
                {
                    ASensorEvent event;
                    while( ASensorEventQueue_getEvents( g_sundog_bridge.sensorEventQueue, &event, 1 ) > 0 ) 
                    {
                        //LOGI( "accelerometer: x=%f y=%f z=%f", event.acceleration.x, event.acceleration.y, event.acceleration.z );
                    }
                }
            }*/

            // Check if we are exiting.
            if( state->destroyRequested != 0 ) 
            {
	        /*
	        (after APP_CMD_DESTROY)
	        From the Android docs:
	        do not count on this method being called as a place for saving data!
	        For example, if an activity is editing data in a content provider, those edits should be committed in either onPause() or onSaveInstanceState(Bundle), not here.
	        */
                android_sundog_deinit( &g_sundog_bridge.sd, false );
                goto break_all;
            }
        }
    }

break_all:

    free( g_android_cache_int_path );
    free( g_android_cache_ext_path );
    free( g_android_files_int_path );
    free( g_android_files_ext_path );
    free( g_android_version );
    free( g_android_lang );

    if( jni )
    {
	jni->DeleteGlobalRef( g_sundog_bridge.gref_na_class );
	jni->DeleteGlobalRef( g_sundog_bridge.gref_main_class );
	jni->DeleteGlobalRef( g_sundog_bridge.alib_class );
	jni->DeleteGlobalRef( g_sundog_bridge.alib_obj );
    }
    android_sundog_release_jni();

    g_activity_state = 0;
    LOGI( "ANDROID MAIN FINISHED" );
}

#endif //!NOMAIN
