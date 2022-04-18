#pragma once

int slog_global_init( const char* filename );
int slog_global_deinit( void );
void slog_disable( void );
void slog_enable( void );
const char* slog_get_file( void );
char* slog_get_latest( size_t size );
void slog( const char* format, ... );

void slog_show_error_report( sundog_engine* s );
