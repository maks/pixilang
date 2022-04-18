#pragma once

#if defined(OS_UNIX) && !defined(OS_ANDROID)
    #define SUNDOG_NET
#endif

int snet_global_init( void );
int snet_global_deinit( void );
int snet_get_host_info( char** host_addr, char** addr_list );
