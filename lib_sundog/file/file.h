#pragma once

/*
Naming Conventions
Filesystem root:
    /FILE		- FILE at the root of the file system (all OS except Windows);
    X:/FILE		- FILE on disk X (Windows only); examples: A:/file; C:/file;
File system packed in a file:
    vfsX:/FILE		- FILE on a packed file system; where the X - filesystem descriptor (supported formats: TAR);
Virtual disks (quick access to standard app directories):
    1:/FILE		- FILE in the current working directory (app documents, backups, last sessions, etc.):
				Linux and Windows: current working directory of the calling process;
				iOS: local storage for app docs;
				macOS: path to the same directory where the application bundle is located;
				Android: local storage for app docs - primary shared storage (internal memory);
				WinCE: filesystem root (/);
    2:/FILE		- FILE in the app/user directory for configs, templates and some hidden data:
				Linux: /home/username/.config/appname;
				Windows and WinCE: directory for application-specific data (/Documents and Settings/username/Application Data);
		    		iOS: application support files;
		    		macOS: application support files (/Users/username/Library/Application Support/appname);
				Android: internal files directory;
    3:/FILE		- FILE in the temporary directory;
    4,5,6,7...          - additional virtual disks (system dependent) from ADD_VIRT_DISK to 9;
Reserved characters (don't use it in the file name):
    <>:"/\|?*
*/

#if defined(OS_WIN) || defined(OS_WINCE)
    #include <shlobj.h>
#endif

#ifdef OS_UNIX
    #include <dirent.h> //for file find
#endif

//
// Local system disks and working directories
//

#ifdef OS_WIN
    #define MAX_DISKS	128
#else
    #define MAX_DISKS	16
#endif
#define DISKNAME_SIZE	4
#define MAX_DIR_LEN	2048 //Don't use it! Will be removed soon...
#define ADD_VIRT_DISK   4

void sfs_refresh_disks( void ); //get info about local disks
const char* sfs_get_disk_name( uint n ); //get disk name (for example: "C:/", "H:/", "/")
int sfs_get_disk_num( const char* path );
uint sfs_get_current_disk( void ); //get number of the current disk
uint sfs_get_disk_count( void );
const char* sfs_get_work_path( void ); //get current working directory (example: "C:/mydir/"); virtual disk 1:/
const char* sfs_get_conf_path( void ); //get config directory; virtual disk 2:/
const char* sfs_get_temp_path( void ); //virtual disk 3:/

//
// Main functions
//

#define SFS_MAX_DESCRIPTORS	256
#define SFS_FOPEN_MAX		( SFS_MAX_DESCRIPTORS - 3 )

//Std files:
#define SFS_STDIN		( SFS_MAX_DESCRIPTORS - 0 )
#define SFS_STDOUT		( SFS_MAX_DESCRIPTORS - 1 )
#define SFS_STDERR		( SFS_MAX_DESCRIPTORS - 2 )

//Seek access:
#define SFS_SEEK_SET            0
#define SFS_SEEK_CUR            1
#define SFS_SEEK_END            2

enum sfs_fd_type
{
    SFS_FILE_NORMAL,
    SFS_FILE_IN_MEMORY,
};

//File format ID:
enum sfs_file_fmt
{
    //Audio:
    SFS_FILE_FMT_UNKNOWN = 0,
    SFS_FILE_FMT_WAVE,
    SFS_FILE_FMT_AIFF,
    SFS_FILE_FMT_OGG,
    SFS_FILE_FMT_MP3,
    SFS_FILE_FMT_FLAC,
    SFS_FILE_FMT_MIDI,
    SFS_FILE_FMT_SUNVOX,
    SFS_FILE_FMT_SUNVOXMODULE,
    SFS_FILE_FMT_XM,
    SFS_FILE_FMT_MOD,
    //Image:
    SFS_FILE_FMT_JPEG,
    SFS_FILE_FMT_PNG,
    SFS_FILE_FMT_GIF,
    //Video:
    SFS_FILE_FMT_AVI,
    SFS_FILE_FMT_MP4,
    //Other:
    SFS_FILE_FMT_ZIP,
    SFS_FILE_FMT_PIXICONTAINER,
    SFS_FILE_FMTS,
};

struct sfs_fd_struct
{
    char*	    	filename;
    void*	    	f;
    sfs_fd_type    	type;
    int8_t*	    	virt_file_data;
    bool		virt_file_data_autofree;
    size_t	    	virt_file_ptr;
    size_t	    	virt_file_size;
    size_t		user_data; //Some user-defined parameter
};

typedef uint sfs_file;

int sfs_global_init( void );
int sfs_global_deinit( void );
//expand/shring virtual disk name (e.g. expand: 1:/file -> /home/user/file; shrink: /home/user/file -> 1:/file):
char* sfs_make_filename( const char* filename, bool expand ); //smem_free() required!
char* sfs_get_filename_path( const char* filename ); //smem_free() required! retval example: 1:/dir1/dir2/
const char* sfs_get_filename_without_dir( const char* filename );
const char* sfs_get_filename_extension( const char* filename );
sfs_fd_type sfs_get_type( sfs_file f );
void* sfs_get_data( sfs_file f );
size_t sfs_get_data_size( sfs_file f );
void sfs_set_user_data( sfs_file f, size_t user_data );
size_t sfs_get_user_data( sfs_file f );
sfs_file sfs_open_in_memory( void* data, size_t size );
sfs_file sfs_open( const char* filename, const char* filemode );
int sfs_close( sfs_file f );
void sfs_rewind( sfs_file f );
int sfs_getc( sfs_file f );
size_t sfs_tell( sfs_file f );
int sfs_seek( sfs_file f, long int offset, int access );
int sfs_eof( sfs_file f );
int sfs_flush( sfs_file f );
size_t sfs_read( void* ptr, size_t el_size, size_t elements, sfs_file f ); //Return value: total number of elements (NOT bytes!) successfully read
size_t sfs_write( const void* ptr, size_t el_size, size_t elements, sfs_file f ); //Return value: total number of elements (NOT bytes!) successfully written
int sfs_putc( int val, sfs_file f );
int sfs_remove( const char* filename ); //remove a file or directory
int sfs_remove_file( const char* filename );
int sfs_rename( const char* old_name, const char* new_name );
int sfs_mkdir( const char* pathname, uint mode );
size_t sfs_get_file_size( const char* filename );
int sfs_copy_file( const char* dest, const char* src );
int sfs_copy_files( const char* dest, const char* src, const char* mask, const char* with_str_in_filename, bool move ); //Return value: number of files copied/moved; dest/src: "somedir/"; mask: "xm/mod/it" or NULL
void sfs_remove_support_files( const char* prefix ); //Remove the app support files from the 2:/ ; prefix example: ".sunvox_"

//
// Searching files
//

//type in sfs_find_struct:
enum sfs_find_item_type
{
    SFS_FILE = 0,
    SFS_DIR
};

#define SFS_FIND_OPT_FILESIZE	( 1 << 0 )

struct sfs_find_struct
{
    uint32_t opt; //options
    const char* start_dir; //Example: "c:/mydir/" "d:/"
    const char* mask; //Example: "xm/mod/it" (or NULL for all files)

    char name[ MAX_DIR_LEN ]; //Found file name
    char temp_name[ MAX_DIR_LEN ];
    sfs_find_item_type type; //Found file type
    size_t size;

#if defined(OS_WIN)
    WIN32_FIND_DATAW find_data;
#endif
#if defined(OS_WINCE)
    WIN32_FIND_DATA find_data;
#endif
#if defined(OS_WIN) | defined(OS_WINCE)
    HANDLE find_handle;
    char win_mask[ MAX_DIR_LEN ]; //Example: "*.xm *.mod *.it"
    char* win_start_dir; //Example: "mydir\*.xm"
#endif
#ifdef OS_UNIX
    DIR* dir;
    struct dirent* current_file;
    char new_start_dir[ MAX_DIR_LEN ];
#endif
};

int sfs_find_first( sfs_find_struct* ); //Return values: 0 - no files
int sfs_find_next( sfs_find_struct* ); //Return values: 0 - no files
void sfs_find_close( sfs_find_struct* );

//
// File format
//

sfs_file_fmt sfs_get_file_format( const char* filename, sfs_file f ); //get file format ID
const char* sfs_get_mime_type( sfs_file_fmt fmt );
const char* sfs_get_extension( sfs_file_fmt fmt );
int sfs_get_clipboard_type( sfs_file_fmt fmt );

//
// Helper functions for reading and writing various file formats
//

struct simage_desc; //misc.h

enum sfs_jpeg_enc_subsampling
{
    JE_Y_ONLY, //Y (grayscale) only
    JE_H1V1, //YCbCr, no subsampling (H1V1, YCbCr 1x1x1, 3 blocks per MCU)
    JE_H2V1, //YCbCr, H2V1 subsampling (YCbCr 2x1x1, 4 blocks per MCU)
    JE_H2V2, //YCbCr, H2V2 subsampling (YCbCr 4x1x1, 6 blocks per MCU, very common)
};

class sfs_jpeg_enc_params : public smem_wrapper
{
public:
    int				quality; //1-100, higher is better
    sfs_jpeg_enc_subsampling	subsampling;
    bool			two_pass_flag;
    sfs_jpeg_enc_params()
    {
	quality = 85;
	subsampling = JE_H2V2;
	two_pass_flag = 0;
    };
};

int sfs_load_jpeg( const char* filename, sfs_file f, simage_desc* img );
int sfs_save_jpeg( const char* filename, sfs_file f, simage_desc* img, sfs_jpeg_enc_params* pars );

int sfs_load_png( const char* filename, sfs_file f, simage_desc* img );

//
// Other
//

#if defined(OS_APPLE)
int apple_sfs_global_init( void );
int apple_sfs_global_deinit( void );
#endif
