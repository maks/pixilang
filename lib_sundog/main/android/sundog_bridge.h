#pragma once

#include <jni.h>
#include <android/log.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "native-activity", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "native-activity", __VA_ARGS__))

extern char* g_android_cache_int_path;
extern char* g_android_cache_ext_path;
extern char* g_android_files_int_path;
extern char* g_android_files_ext_path;
extern char* g_android_version;
extern char g_android_version_correct[ 16 ];
extern int g_android_version_nums[ 8 ];
extern char* g_android_lang;
extern int g_android_audio_buf_size; //number of audio frames that the HAL (Hardware Abstraction Layer) buffer can hold
extern int g_android_audio_sample_rate; //optimal sample rate

extern int g_android_sundog_screen_ppi;
extern int g_android_sundog_screen_orientation; //ACONFIGURATION_ORIENTATION_xxxx
extern bool g_android_sundog_gl_buffer_preserved;
extern EGLDisplay g_android_sundog_display;
extern EGLSurface g_android_sundog_surface;

#ifndef NOMAIN
int android_sundog_option_exists( const char* name );
JNIEnv* android_sundog_get_jni( void );
void android_sundog_release_jni( void );
void android_sundog_screen_redraw( void );
void android_sundog_event_handler( window_manager* wm );
int android_sundog_check_for_permissions( sundog_engine* s, int p ); //p: 1 - write ext.storage; 2 - record_audio; 4 - camera;
char* android_sundog_get_external_files_dir( int n ); //n: 0 - primary; 1 - secondary; retval: string allocated with malloc()
int android_sundog_copy_resources( void );
char* android_sundog_get_host_ips( sundog_engine* s, int mode );
void android_sundog_open_url( sundog_engine* s, const char* url_text );
void android_sundog_send_file_to_gallery( sundog_engine* s, const char* path );
void android_sundog_clipboard_copy( const char* txt );
char* android_sundog_clipboard_paste( void );
int* android_sundog_get_exclusive_cores( sundog_engine* s, size_t* len );
int android_sundog_set_sustained_performance_mode( sundog_engine* s, int enable );
void android_sundog_set_safe_area( sundog_engine* s );
int android_sundog_set_system_ui_visibility( sundog_engine* s, int v );
int android_sundog_open_camera( sundog_engine* s, int cam_id, void* user_data );
int android_sundog_close_camera( sundog_engine* s, int con_index );
int android_sundog_get_camera_width( sundog_engine* s, int con_index );
int android_sundog_get_camera_height( sundog_engine* s, int con_index );
int android_sundog_get_camera_format( sundog_engine* s, int con_index );
int android_sundog_get_camera_focus_mode( sundog_engine* s, int con_index );
int android_sundog_set_camera_focus_mode( sundog_engine* s, int con_index, int mode );
struct android_app* android_sundog_get_app_struct( window_manager* wm );
int android_sundog_midi_init( sundog_engine* s );
char* android_sundog_get_midi_devports( sundog_engine* s, int flags ); //get list of strings: device name + port name \n ...
int android_sundog_midi_open_devport( sundog_engine* s, const char* name, int port_id );
int android_sundog_midi_close_devport( sundog_engine* s, int con_index );
int android_sundog_midi_send( sundog_engine* s, int con_index, int msg, int msg_len, int t );
#endif
