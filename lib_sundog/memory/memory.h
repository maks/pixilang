#pragma once

#if HEAPSIZE <= 16 && !defined(SMEM_USE_NAMES)
    #define SMEM_FAST_MODE
#endif
#if defined(SMEM_FAST_MODE) && defined(SMEM_USE_NAMES)
    #error SMEM_FAST_MODE cant be used with SMEM_USE_NAMES
#endif

#define SMEM_MAX_NAME_SIZE 24
struct smem_block
{
    size_t size;
#ifdef SMEM_USE_NAMES
    char name[ SMEM_MAX_NAME_SIZE ];
#endif
#ifndef SMEM_FAST_MODE
    smem_block* next;
    smem_block* prev;
#endif
};

extern size_t g_smem_error;

//
// Base functions
// (only for blocks allocated by smem_new())
//

int smem_global_init( void );
int smem_global_deinit( void );
size_t smem_get_usage( void );
void smem_print_usage( void );
void* smem_new2( size_t size, const char* name ); //Each memory block has its own name
#define smem_new( size ) smem_new2( size, __FUNCTION__ )
#define smem_new_struct( STRUCT ) (STRUCT*)smem_new( sizeof(STRUCT) )
void smem_free( void* ptr );
void* smem_get_stdc_ptr( void* ptr, size_t* data_offset );
void smem_zero( void* ptr );
inline void* smem_znew( size_t size ) { void* p = smem_new( size ); smem_zero( p ); return p; }
void* smem_resize( void* ptr, size_t size );
void* smem_resize2( void* ptr, size_t size ); //With zero padding
void* smem_copy_d( void* dest, void* src, size_t dest_offset, size_t size ); //Dynamic version with smem_copy() (with autoresize and zero padding). Use it with the SunDog (smem_new) memory blocks only!
void* smem_clone( void* ptr );
int smem_objlist_add( void*** objlist, const void* obj, bool obj_is_cstring, uint n );
int smem_intlist_add( int** intlist, size_t* len, int v, uint n, int step );
inline size_t smem_get_size( void* ptr )
{
    if( ptr == NULL ) return 0;
    smem_block* m = (smem_block*)( (int8_t*)ptr - sizeof( smem_block ) );
    return m->size;
}
inline char* smem_get_name( void* ptr )
{
#ifdef SMEM_USE_NAMES
    if( ptr == NULL ) return NULL;
    smem_block* m = (smem_block*)( (int8_t*)ptr - sizeof( smem_block ) );
    return m->name;
#else
    return NULL;
#endif
}

class smem_wrapper //without zero initialization
{
    void* operator new( size_t size ) { void* p = smem_new( size ); return p; }
    void operator delete( void* p ) { smem_free( p ); }
};

//
// Additional functions
// (for any pointers)
//

inline void smem_clear( void* ptr, size_t size )
{
    if( ptr == NULL ) return;
    memset( ptr, 0, size );
}
#define smem_clear_struct( s ) smem_clear( &( s ), sizeof( s ) )
inline void smem_copy( void* dest, const void* src, size_t size ) //Overlapping allowed
{
    if( dest == NULL || src == NULL ) return;
    memmove( dest, src, size ); //Overlapping allowed
}
inline int smem_cmp( const void* p1, const void* p2, size_t size )
{
    if( p1 == NULL || p2 == NULL ) return 0;
    return memcmp( p1, p2, size );
}

//
// C string manipulation
//

int smem_strcat( char* dest, size_t dest_size, const char* src );
char* smem_strcat_d( char* dest, const char* src ); //Dynamic version with smem_resize(). Use it with the SunDog (smem_new) memory blocks only!
#define smem_strcat_resize( dest, src ) dest = smem_strcat_d( dest, src )
inline int smem_strcmp( const char* s1, const char* s2 )
{
    if( s1 == NULL || s2 == NULL ) return 1;
    return strcmp( s1, s2 );
}
inline const char* smem_strstr( const char* s1, const char* s2 )
{
    if( s1 == NULL || s2 == NULL ) return NULL;
    return strstr( s1, s2 );
}
inline char* smem_strstr( char* s1, const char* s2 )
{
    if( s1 == NULL || s2 == NULL ) return NULL;
    return strstr( s1, s2 );
}
inline const char* smem_strchr( const char* str, char character )
{
    if( str == NULL ) return NULL;
    return strchr( str, character );
}
inline char* smem_strchr( char* str, char character )
{
    if( str == NULL ) return NULL;
    return strchr( str, character );
}
size_t smem_strlen( const char* );
size_t smem_strlen_utf16( const uint16_t* s );
size_t smem_strlen_utf32( const uint32_t* s );
char* smem_strdup( const char* s1 );
size_t smem_replace_str( char* dest, size_t dest_size, const char* src, const char* from, const char* to );
inline void smem_replace_char( char* str, char from, char to )
{
    while( *str != 0 ) { if( *str == from ) *str = to; str++; }
}
//Split string by delimiter and put the substring num to dest buffer; retval = pointer to the next substring:
const char* smem_split_str( char* dest, size_t dest_size, const char* src, char delim, uint num );
/* Example:
    char substr[ 32 ];
    const char* next = src;
    while( 1 )
    {
        substr[ 0 ] = 0;
        next = smem_split_str( substr, sizeof( substr ), next, ',', 0 );
        if( substr[ 0 ] != 0 ) { ... handle substr ... }
        if( !next ) break;
    }
*/
