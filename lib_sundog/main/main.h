#pragma once

/*
  SunDog stand-alone app structure:
    sundog_engine* sd = zero-filled structure;
    sundog_main( sd, true );
*/

/*
  SunDog module structure:
    global init (ONCE FOR THE PROCESS; for example, during the library (DLL with the module) loading):
      sundog_global_init();
    global deinit (ONCE FOR THE PROCESS):
      sundog_global_deinit();
    module instance 0 thread:
      sundog_engine* sd = zero-filled structure;
      sundog_main( sd, false );
    module instance 1 thread:
      ...
      ...
*/

extern const char* g_app_name;
extern const char* g_app_name_short;
extern const char* g_app_profile[];
extern const char* g_app_log;
extern const char* g_app_usage;
extern int g_app_window_xsize;
extern int g_app_window_ysize;
extern uint g_app_window_flags; //WIN_INIT_FLAG_xxx

struct app_option
{
    const char* n; //name
    int32_t v; //value
};
extern app_option g_app_options[];

struct app_parameter
{
    uint32_t id;
    const char* id_str; //the parameter's non-localized, permanent name
    const char* name;
    const char* unit; //may be NULL
    float min;
    float max;
    float def; //default value
    uint32_t flags;
};
#define MAX_APP_PARAMETERS 64
#define APP_PAR_FLAG_INDEXED 	( 1 << 0 )
#define APP_PAR_FLAG_EXP2 	( 1 << 1 ) //Display exponential: display_v = 1 - pow( 1 - v, 2 )
#define APP_PAR_FLAG_EXP3 	( 1 << 2 ) //Display exponential: display_v = 1 - pow( 1 - v, 3 )
#define APP_PAR( ID, IDSTR, NAME, UNIT, MIN, MAX, DEF, FLAGS ) \
    pars[ p ].id = ID; \
    pars[ p ].id_str = IDSTR; \
    pars[ p ].name = NAME; \
    pars[ p ].unit = UNIT; \
    pars[ p ].min = MIN; \
    pars[ p ].max = MAX; \
    pars[ p ].def = DEF; \
    pars[ p ].flags = FLAGS; \
    p++;
app_parameter* get_app_parameters( void ); //use system free() to remove the list

#ifdef SDL
    extern volatile bool g_sdl_initialized;
#endif

//These functions will be called by the SunDog Engine:
int app_global_init( void ); //Here you can initialize some global variables that require exclusive access
int app_global_deinit( void );
int app_init( window_manager* wm );
int app_deinit( window_manager* wm );
int app_event_handler( sundog_event* evt, window_manager* wm );

void sundog_denormal_numbers_check( void );

int sundog_global_init( void );
int sundog_global_deinit( void );

//SunDog Engine structure
//Must be cleared and initialized before sundog_main()
struct sundog_engine
{
    //Mandatory objects of the app:
    window_manager		wm; //Window manager
    smbox*			mb; //Exchange box for large messages

    //Links to some important objects of the app/module (may be NULL):
    sundog_sound*		ss; //Primary sound stream; will be initialized in sound.cpp -> sundog_sound_init()
    volatile uint32_t   	ss_idle_frame_counter;
    uint32_t			ss_sample_rate;

    //Command line arguments:
    int                 	argc; //Count (1,2,3...)
    char**         		argv; //Arguments ([0]="program file name";[1]="arg1";[2]="arg2";...)

    uint32_t			id; //Uniqie app/module ID
    int				exit_code;

    void*			device_specific; //Pointer to some outer system-dependent SunDog struct; DONT USE IT INSIDE THE SUNDOG APP!
#ifdef OS_WIN
    HINSTANCE           	hCurrentInst;
    HINSTANCE           	hPreviousInst;
    LPSTR               	lpszCmdLine;
    int                 	nCmdShow;
#endif
#ifdef OS_WINCE
    HINSTANCE           	hCurrentInst;
    HINSTANCE           	hPreviousInst;
    LPWSTR              	lpszCmdLine;
    int                 	nCmdShow;
#endif
#ifdef OS_IOS
    volatile int		screen_xsize;
    volatile int		screen_ysize;
    volatile int		screen_orientation; //counterclockwise
    volatile int		screen_safe_area[ 4 ]; //x y w h
    volatile int		screen_ppi;
    volatile int		screen_changed_w;
    int				screen_changed_r;
    volatile uint32_t		view_framebuffer;
#ifdef AUDIOUNIT_EXTENSION
    volatile int		auext_active; //0 - AU in pause mode, SunDog stream can be modified
    volatile int		auext_active_request; //-1 - request for pause; 1 - request for play
    int				auext_channel_count;
    int				auext_sample_rate;
#endif
#endif
#if defined(OS_APPLE) && !defined(AUDIOUNIT_EXTENSION)
    #define MIDI_NOTIFY_CMDS 16
    volatile struct { uint32_t arg; int8_t cmd; } midi_notify_cmds[ MIDI_NOTIFY_CMDS ]; //for single midi client only
    volatile uint32_t midi_notify_cmds_w;
#endif
#ifdef OS_ANDROID
    volatile int		screen_xsize;
    volatile int		screen_ysize;
    volatile int		screen_safe_area[ 4 ]; //x y w h
    volatile int		screen_changed_w;
    int				screen_changed_r;
#endif
};

int sundog_main( sundog_engine* sd, bool global_init );
