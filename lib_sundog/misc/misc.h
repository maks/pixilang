#pragma once

struct ssymtab;

int smisc_global_init( void );
int smisc_global_deinit( void );

//
// List of strings
//

struct slist_data
{
    char* heap; //Items data
    uint heap_usage; //Number of used bytes in heap
    uint* items; //Item pointers
    uint items_num; //Number of items
    //Public fields (can be changed directly):
    //...actually, the best place for these fields is the slist_handler (WM)...
    int selected_item;
    int start_item;
};

void slist_init( slist_data* data );
void slist_deinit( slist_data* data );
void slist_clear( slist_data* data );
void slist_reset_items( slist_data* data );
void slist_reset_selection( slist_data* data );
uint slist_get_items_num( slist_data* data );
int slist_add_item( const char* item, uint8_t attr, slist_data* data );
void slist_delete_item( uint item_num, slist_data* data );
void slist_move_item_up( uint item_num, slist_data* data );
void slist_move_item_down( uint item_num, slist_data* data );
void slist_move_item( uint from, uint to, slist_data* data );
char* slist_get_item( uint item_num, slist_data* data );
uint8_t slist_get_attr( uint item_num, slist_data* data );
void slist_sort( slist_data* data );
void slist_exchange( uint item1, uint item2, slist_data* data );
int slist_save( const char* fname, slist_data* data );
int slist_load( const char* fname, slist_data* data );

//
// Ring buffer
//

struct sring_buf
{
    smutex m;
    uint32_t flags;
    uint8_t* buf;
    size_t buf_size;
    std::atomic_size_t wp;
    std::atomic_size_t rp;
};

#define SRING_BUF_FLAG_SINGLE_RTHREAD	( 1 << 0 )
#define SRING_BUF_FLAG_SINGLE_WTHREAD	( 1 << 1 )
#define SRING_BUF_FLAG_ATOMIC_SPINLOCK	( 1 << 2 )

sring_buf* sring_buf_new( size_t size, uint32_t flags );
void sring_buf_remove( sring_buf* b );
void sring_buf_read_lock( sring_buf* b );
void sring_buf_read_unlock( sring_buf* b );
void sring_buf_write_lock( sring_buf* b );
void sring_buf_write_unlock( sring_buf* b );
size_t sring_buf_write( sring_buf* b, void* data, size_t size ); //Use with Lock+Unlock
size_t sring_buf_read( sring_buf* b, void* data, size_t size ); //Use with Lock+Unlock
void sring_buf_next( sring_buf* b, size_t size ); //Use with Lock+Unlock
size_t sring_buf_avail( sring_buf* b );

//
// Exchange box for large messages; thread-safe
//

struct smbox_msg
{
    const void* id;
    void* data; //ONLY allocated by smem_new(); will be freed by smbox_remove_msg();
    ticks_t created_t;
    int lifetime_s; //0 - no limit
};

struct smbox
{
    smutex mutex;
    smbox_msg** msg;
    int capacity; //max number of messages
    int active; //number of active messages
};

smbox* smbox_new();
void smbox_remove( smbox* mb );
smbox_msg* smbox_new_msg( void );
void smbox_remove_msg( smbox_msg* msg );
int smbox_add( smbox* mb, smbox_msg* msg );
smbox_msg* smbox_get( smbox* mb, const void* id, int timeout_ms );

//
// Profile
//

#define KEY_DENORM_NUMBERS	"denorm"
#define KEY_TIMER_RESOLUTION	"timerres"
#define KEY_LANG		"locale_lang"
#define KEY_SCREENX		"width"
#define KEY_SCREENY		"height"
#define KEY_DONT_SAVE_WINSIZE	"nosws"
#define KEY_ROTATE		"rotate"
#define KEY_FULLSCREEN		"fullscreen"
#define KEY_MAXIMIZED		"maximized"
#define KEY_ZOOM		"zoom"
#define KEY_PPI			"ppi"
#define KEY_SCALE		"scale"
#define KEY_FONT_SCALE		"fscale"
#define KEY_FONT_MEDIUM_MONO	"font_mm"
#define KEY_FONT_BIG		"font_b"
#define KEY_FONT_SMALL		"font_s"
#define KEY_NO_FONT_UPSCALE	"no_font_upscale"
#define KEY_NOCURSOR		"nocursor"
#define KEY_NOAUTOLOCK		"noautolock"
#define KEY_NOBORDER		"noborder"
#define KEY_NOSYSBARS		"nosysbars" //No system bars
#define KEY_WINDOWNAME		"windowname"
#define KEY_VIDEODRIVER		"videodriver"
#define KEY_TOUCHCONTROL	"touchcontrol"
#define KEY_PENCONTROL		"pencontrol"
#define KEY_SHOW_ZOOM_BUTTONS	"zoombtns"
#define KEY_DOUBLECLICK		"doubleclick"
#define KEY_KBD_AUTOREPEAT_DELAY	"ar_d1"
#define KEY_KBD_AUTOREPEAT_FREQ		"ar_f1"
#define KEY_MOUSE_AUTOREPEAT_DELAY	"ar_d2"
#define KEY_MOUSE_AUTOREPEAT_FREQ	"ar_f2"
#define KEY_MAXFPS              "maxfps"
#define KEY_VSYNC		"vsync"
#define KEY_FRAMEBUFFER		"framebuffer"
#define KEY_NOWINDOW		"nowin"
#define KEY_SCREENBUF_SWAP_BEHAVIOR	"swap_behavior" //1 - screen buffer will be preserved; 0 - screen buffer will be destroyed;
#define KEY_SHOW_VIRT_KBD	"show_virt_kbd"
#define KEY_HIDE_VIRT_KBD	"hide_virt_kbd"
#define KEY_FDIALOG_PREVIEW	"fpreview"
#define KEY_FDIALOG_PREVIEW_YSIZE	"fpreview_ys"
#define KEY_FDIALOG_NORECENT	"nofrecent"
#define KEY_FDIALOG_RECENT_Y	"frecent_y"
#define KEY_FDIALOG_RECENT_XSIZE	"frecent_xs"
#define KEY_FDIALOG_RECENT_MAXFILES	"frecent_fmax"
#define KEY_FDIALOG_RECENT_MAXDIRS	"frecent_dmax"
#define KEY_FDIALOG_SHOWHIDDEN	"showhf"
#define KEY_COLOR_THEME		"builtin_theme"
#define KEY_SOUNDBUFFER		"buffer"
#define KEY_AUDIODEVICE		"audiodevice"
#define KEY_AUDIODEVICE_IN	"audiodevice_in"
#define KEY_AUDIODRIVER		"audiodriver"
#define KEY_MIDIDRIVER		"mididriver"
#define KEY_FREQ		"frequency"
#define KEY_CAMERA		"camera"
#define KEY_CAM_ROTATE		"cam_rotate"
#define KEY_SUNVOX_EDIT_MODE_P	"edit_mode_p" //pattern
#define KEY_SUNVOX_EDIT_MODE_M	"edit_mode_m" //modules
#define KEY_SUNVOX_EDIT_MODE_T	"edit_mode_t" //timeline

struct sprofile_key
{
    char* key;
    char* value;
    int line_num;
    bool deleted;
};

struct sprofile_data
{
    int file_num;
    char* file_name;
    char* source;
    sprofile_key* keys;
    ssymtab* st;
    int num;
    bool changed;
    srwlock lock;
};

void sprofile_new( sprofile_data* p ); //non thread safe
void sprofile_remove_key( const char* key, sprofile_data* p );
void sprofile_set_str_value( const char* key, const char* value, sprofile_data* p );
void sprofile_set_int_value( const char* key, int value, sprofile_data* p );
void sprofile_set_int_value2( const char* key, int value, int default_value, sprofile_data* p ); //with auto remove
int sprofile_get_int_value( const char* key, int default_value, sprofile_data* p );
char* sprofile_get_str_value( const char* key, const char* default_value, sprofile_data* p );
void sprofile_close( sprofile_data* p ); //non thread safe
void sprofile_load( const char* filename, sprofile_data* p ); //non thread safe
void sprofile_load_from_string( const char* config, char delim, sprofile_data* p ); //example: config="buffer=1024|audiodriver=alsa|audiodevice=hw:0,0|nowin"; delim='|';
int sprofile_save( sprofile_data* p );
void sprofile_remove_all_files( void );

//
// App state
//

#define SUNDOG_STATE_TEMP ( 1 << 0 ) //temporary file, the app is responsible for removing it
#define SUNDOG_STATE_ORIGINAL ( 1 << 1 ) //original file, the app can modify it directly
struct sundog_state
{
    uint32_t flags;
    char* fname;
    void* data; //stdc pointer (malloc/free) only!
    size_t data_offset;
    size_t data_size;
    //full data size = data_size + data_offset
};
#if defined(SUNDOG_MODULE) || defined(OS_ANDROID) || defined(OS_IOS)
    #define SUNDOG_STATE
#endif
sundog_state* sundog_state_new( const char* fname, uint32_t flags );
sundog_state* sundog_state_new( void* data, size_t data_offset, size_t data_size, uint32_t flags );
void sundog_state_remove( sundog_state* s );
void sundog_state_set( sundog_engine* sd, int io, sundog_state* state ); //io: 0 - app input; 1 - app output;
sundog_state* sundog_state_get( sundog_engine* sd, int io ); //io: 0 - app input; 1 - app output;

//
// String manipulation
//

void int_to_string( int value, char* str );
void hex_int_to_string( uint32_t value, char* str );
int get_int_string_len( int value );
int hex_get_int_string_len( uint32_t value );
int string_to_int( const char* str );
int hex_string_to_int( const char* str );
char int_to_hchar( int value );
//void truncate_float_str( char* str );
void float_to_string( float value, char* str, int precision ); //fast and rough f2s conversion;
//examples:
//  precision 1: 1.0="1"; 1.1235="1.1"
//  precision 2: 1.0="1"; 1.1235="1.12"; 1.1005="1.1"

uint16_t* utf8_to_utf16( uint16_t* dest, int dest_size, const char* s ); //dest_size = sizeof( destination buffer ) / sizeof( int16 )
uint32_t* utf8_to_utf32( uint32_t* dest, int dest_size, const char* s ); //dest_size = sizeof( destination buffer ) / sizeof( int32 )
char* utf16_to_utf8( char* dst, int dest_size, const uint16_t* src );
char* utf32_to_utf8( char* dst, int dest_size, const uint32_t* src );

int utf8_to_utf32_char( const char* str, uint32_t* res ); //retval: number of bytes read (utf8 char length)
int utf8_to_utf32_char_safe( char* str, size_t str_size, uint32_t* res );

void utf8_unix_slash_to_windows( char* str );
void utf16_unix_slash_to_windows( uint16_t* str );
void utf32_unix_slash_to_windows( uint32_t* str );

int make_string_lowercase( char* dest, size_t dest_size, char* src );
int make_string_uppercase( char* dest, size_t dest_size, char* src );

void get_color_from_string( char* str, uint8_t* r, uint8_t* g, uint8_t* b );
void get_string_from_color( char* dest, uint dest_size, int r, int g, int b );

#ifdef OS_WIN
    #ifdef _WIN64
	#define PRINTF_SIZET "%I64u"
	#define PRINTF_SIZET_CONV (size_t)
    #else
	#define PRINTF_SIZET "%u"
	#define PRINTF_SIZET_CONV (unsigned int)
    #endif
#else
    #define PRINTF_SIZET "%zu"
    #define PRINTF_SIZET_CONV (size_t)
#endif

//
// Locale
//

int slocale_init( void );
void slocale_deinit( void );
const char* slocale_get_lang( void ); //Return language in POSIX format

//
// UNDO manager (action stack)
//

#define UNDO_HANDLER_PARS bool redo, undo_action* action, undo_data* u
#define UNDO_ACTION_FATAL_ERROR		( 1 << 24 )

//action_handler() retval:
// 0 - ok;
// not 0 - empty action or some non-fatal error:
//   undo_add_action(): action will be ignored;
//   execute_undo/redo(): action stack will be cleared;
// UNDO_ACTION_FATAL_ERROR - fatal error bit; action stack will be cleared;

enum 
{
    UNDO_STATUS_NONE = 0,
    UNDO_STATUS_ADD_ACTION,
    UNDO_STATUS_UNDO,
    UNDO_STATUS_REDO
};

struct undo_action
{
    uint32_t level;
    uint32_t type; //Action type
    uint32_t par[ 5 ]; //User defined parameters...
    void* data;
};

struct undo_data
{
    int status; //check it inside the action_handler()

    size_t data_size;
    size_t data_size_limit;

    size_t actions_num_limit;

    uint32_t level;
    size_t first_action;
    size_t cur_action; //Relative to first_action
    size_t actions_num;
    undo_action* actions;
    int (*action_handler)( UNDO_HANDLER_PARS );

    void* user_data;
};

void undo_init( size_t size_limit, int (*action_handler)( UNDO_HANDLER_PARS ), void* user_data, undo_data* u );
void undo_deinit( undo_data* u );
void undo_reset( undo_data* u );
int undo_add_action( undo_action* action, undo_data* u );
void undo_next_level( undo_data* u );
void execute_undo( undo_data* u );
void execute_redo( undo_data* u );

//
// Symbol table (hash table)
//

union SSYMTAB_VAL
{
    int i;
    float f;
    void* p;
};

struct ssymtab_item //Symbol
{
    char*		name;
    int        		type;
    SSYMTAB_VAL		val;
    ssymtab_item*	next;
};

struct ssymtab //Symbol table
{
    int			size;
    ssymtab_item**	symtab;
};

#define SSYMTAB_TABSIZE_NUM 16
extern int g_ssymtab_tabsize[]; //53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317, 196613, 393241, 786433, 1572869

ssymtab* ssymtab_new( int size_level ); //size_level: 0...15 - level; 15... - real symtab size (prime number)
int ssymtab_remove( ssymtab* st );
int ssymtab_hash( const char* name, int size );
//auto hash calculation: -1
ssymtab_item* ssymtab_lookup( const char* name, int hash, bool create, int initial_type, SSYMTAB_VAL initial_val, bool* created, ssymtab* st );
ssymtab_item* ssymtab_get_list( ssymtab* st );
int ssymtab_iset( const char* sym_name, int val, ssymtab* st ); //set int value of the symbol
int ssymtab_iset( uint32_t sym_name, int val, ssymtab* st );
int ssymtab_iget( const char* sym_name, int notfound_val, ssymtab* st ); //get int value of the symbol
int ssymtab_iget( uint32_t sym_name, int notfound_val, ssymtab* st );

//
// System-wide copy/paste buffer
//

#ifndef NOFILEUTILS
    #define CAN_COPYPASTE
    #if defined(OS_IOS)
	#define CAN_COPYPASTE_AV
        //macOS: additional tests required (can't paste WAV from SunDog in other apps)
        //       need to use NSPasteboardTypeSound and NSSound?
    #endif
#endif
enum
{
    sclipboard_type_utf8_text = 0,
    sclipboard_type_image,
    sclipboard_type_audio,
    sclipboard_type_video, //only video
    sclipboard_type_movie, //audio+video
    sclipboard_type_av //audio and/or video
};
int sclipboard_copy( sundog_engine* s, const char* filename, uint32_t flags );
char* sclipboard_paste( sundog_engine* s, int type, uint32_t flags ); //retval: filename

//
// URL
//

void open_url( sundog_engine* s, const char* url );

//
// Export / Import functions provided by the system
//

#ifndef NOFILEUTILS
    #if defined(OS_IOS) || defined(OS_ANDROID)
	#define CAN_SEND_TO_GALLERY
    #endif
    #if defined(OS_IOS)
	#define CAN_SEND_TO_EMAIL
	#define CAN_EXPORT
	#define CAN_EXPORT2
	#define CAN_IMPORT
    #endif
#endif
int send_file_to_gallery( sundog_engine* s, const char* filename );
void send_text_to_email( sundog_engine* s, const char* email, const char* subj, const char* body );
#define EIFILE_MODE_IMPORT		0 /* import file */
#define EIFILE_MODE_EXPORT		1 /* export file */
#define EIFILE_MODE_EXPORT2		2 /* open file in another app */
#define EIFILE_MODE			15
#define EIFILE_FLAG_DELFILE		( 1 << 4 ) /* if possible, delete the file after the operation is completed */
int export_import_file( sundog_engine* s, const char* filename, uint32_t flags ); //non blocking! import through the EVT_LOADSTATE

//
// Random generator
//

void set_pseudo_random_seed( uint32_t seed );
uint32_t pseudo_random( void ); //OUT: 0...32767
uint32_t pseudo_random( uint32_t* seed ); //OUT: 0...32767

//
// 3D transformation matrix operations
//

//Matrix structure:
// | 0  4  8  12 |
// | 1  5  9  13 |
// | 2  6  10 14 |
// | 3  7  11 15 |

void matrix_4x4_reset( float* m );
void matrix_4x4_mul( float* res, float* m1, float* m2 );
void matrix_4x4_rotate( float angle, float x, float y, float z, float* m );
void matrix_4x4_translate( float x, float y, float z, float* m );
void matrix_4x4_scale( float x, float y, float z, float* m );
void matrix_4x4_ortho( float left, float right, float bottom, float top, float z_near, float z_far, float* m ); //Multiply by an orthographic matrix

//
// Image
//

enum simage_pixel_format
{
    PFMT_GRAYSCALE_8,
    PFMT_RGBA_8888,
    PFMT_SUNDOG_COLOR,
    PFMT_CNT
};

extern uint8_t g_simage_pixel_format_size[ PFMT_CNT ];

struct simage_desc
{
    void*               data;
    simage_pixel_format	format;
    int                 width;
    int                 height;
};

//
// Misc
//

size_t round_to_power_of_two( size_t v );
uint sqrt_newton( uint l );
int scale_check( int v, int max, int new_max ); //scale and check the inverse transformation (result * max / new_max = v)
int div_round( int v1, int v2 ); //divide v1/v2 and round to the nearest

#define INT32_SWAP( n ) \
    ( ( (((uint32_t)n) << 24 ) & 0xFF000000 ) | \
      ( (((uint32_t)n) << 8 ) & 0x00FF0000 ) | \
      ( (((uint32_t)n) >> 8 ) & 0x0000FF00 ) | \
      ( (((uint32_t)n) >> 24 ) & 0x000000FF ) )

#define INT16_SWAP( n ) \
    ( ( (((uint16_t)n) << 8 ) & 0xFF00 ) | \
      ( (((uint16_t)n) >> 8 ) & 0x00FF ) )

#define LIMIT_NUM( val, bottom, top ) { if( val < bottom ) val = bottom; if( val > top ) val = top; }

#define MAX_NUM( x, y ) ( ( (x) > (y) ) ? (x) : (y) )
#define MIN_NUM( x, y ) ( ( (x) < (y) ) ? (x) : (y) )

//REG_X and REG_LEN will be changed;
//REG_LEN <= 0 if the region is not visible;
#define CROP_REGION( REG_X, REG_LEN, CROP_X, CROP_LEN ) \
{ \
    if( (REG_X) < (CROP_X) ) { REG_LEN -= (CROP_X) - (REG_X); REG_X = CROP_X; } \
    if( (REG_X) + (REG_LEN) > (CROP_X) + (CROP_LEN) ) REG_LEN = (CROP_X) + (CROP_LEN) - (REG_X); \
}
