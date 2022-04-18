//
// Windows: sound
//

#ifndef OS_WINCE
    #define DSOUND
#endif
#if !defined(OS_WINCE) && !defined(NOASIO)
    #define ASIO
#endif

enum
{
#ifdef DSOUND
    SDRIVER_DSOUND,
#endif
#ifdef ASIO
    SDRIVER_ASIO,
#endif
    SDRIVER_MMSOUND,
    NUMBER_OF_SDRIVERS
};
#define DEFAULT_SDRIVER 0

const char* g_sdriver_infos[] =
{
#ifdef DSOUND
    "DirectSound",
#endif
#ifdef ASIO
    "ASIO",
#endif
    "Waveform Audio"
};

const char* g_sdriver_names[] =
{
#ifdef DSOUND
    "dsound",
#endif
#ifdef ASIO
    "asio",
#endif
    "mmsound"
};

const int g_default_buffer_size = 4096;

//DirectSound:
#ifdef DSOUND
#include <dsound.h>
#define NUMEVENTS 2
struct device_sound_dsound
{
    LPDIRECTSOUND           	lpds;
    LPDIRECTSOUNDCAPTURE	lpdsCapture;
    LPDIRECTSOUNDNOTIFY     	lpdsNotify;
    WAVEFORMATEX		wfx;
    DSBPOSITIONNOTIFY       	rgdsbpn[ NUMEVENTS ];
    HANDLE                  	rghEvent[ NUMEVENTS ];
    DSBUFFERDESC            	buf_desc;
    LPDIRECTSOUNDBUFFER     	buf;
    LPDIRECTSOUNDBUFFER     	buf_primary;
    DSCBUFFERDESC            	input_buf_desc;
    LPDIRECTSOUNDCAPTUREBUFFER	input_buf;
    uint			input_rp;
    HANDLE 			input_thread;
    volatile int 		input_exit_request;
    volatile int 		output_exit_request;
    HANDLE 			output_thread;
};
struct device_sound_dsound_enum
{
    LPGUID			guids[ 128 ];
    int 			guids_num;
    char*** 		infos;
    bool			log;
    bool			input;
};
#endif

//Waveform Audio (MMSYSTEM)
#define MM_USE_THREAD
#define MM_MAX_BUFFERS	2
struct device_sound_mmsound
{
    HWAVEOUT			waveOutStream;
    HANDLE			waveOutThread;
    DWORD			waveOutThreadID;
    volatile bool		waveOutThreadClosed;
    WAVEHDR			outBuffersHdr[ MM_MAX_BUFFERS ];
    HWAVEIN			waveInStream;
    HANDLE			waveInThread;
    DWORD			waveInThreadID;
    volatile bool		waveInThreadClosed;
    WAVEHDR			inBuffersHdr[ MM_MAX_BUFFERS ];
};

//ASIO:
//(modularity: 0%)
#ifdef ASIO
#include <iasiodrv.h>
#define ASIO_DEVS 32
struct asio_device
{
    CLSID clsid;
};
asio_device g_asio_devs[ ASIO_DEVS ];
ASIOChannelInfo g_asio_channels[ 32 ];
ASIOBufferInfo g_asio_bufs[ 32 ];
ASIOCallbacks g_asio_callbacks;
int g_asio_sample_size = 0;
int g_asio_input_sample_size = 0;
sundog_sound* g_asio_ss = 0;
#endif

struct device_sound //device-specific data of the sundog_sound object
{
    int 			buffer_size;
    int 			frame_size;
    void* 			output_buffer;
    bool			first_frame_rendered;
#ifdef DSOUND
    device_sound_dsound*	ds; //DirectSound
#endif
    device_sound_mmsound*	mm; //Waveform Audio (MMSYSTEM)
#ifdef COMMON_SOUNDINPUT_VARIABLES
    COMMON_SOUNDINPUT_VARIABLES;
#endif
};

//
// DirectSound
//

#ifdef DSOUND

//DirectSound output thread:

void StreamToBuffer( sundog_sound* ss, uint evtNum )
{
    device_sound* d = (device_sound*)ss->device_specific;
    device_sound_dsound* ds = d->ds;

    if( !ds->buf ) return;

    uint offset = ds->rgdsbpn[ ( evtNum + NUMEVENTS / 2 ) % NUMEVENTS ].dwOffset;
    uint size = ds->buf_desc.dwBufferBytes / NUMEVENTS;

    void* lockPtr1 = 0;
    void* lockPtr2 = 0;
    DWORD lockBytes1 = 0;
    DWORD lockBytes2 = 0;

    for( int a = 0; a < 2; a++ )
    {
	HRESULT res = ds->buf->Lock( 
    	    offset,           // Offset of lock start
    	    size,             // Number of bytes to lock
    	    &lockPtr1,        // Address of lock start
    	    &lockBytes1,      // Count of bytes locked
    	    &lockPtr2,        // Address of wrap around
    	    &lockBytes2,      // Count of wrap around bytes
    	    0 );              // Flags
	if( a == 0 && res == DSERR_BUFFERLOST )
	{
	    ds->buf->Restore();
	    continue;
	}
	if( res != DS_OK ) return;
	break;
    }

    //Write data to the locked buffer:

    ss->out_time = stime_ticks_hires() + ( ( (uint64_t)d->buffer_size * (uint64_t)stime_ticks_per_second_hires() ) / (uint64_t)ss->freq );
    ss->out_frames = lockBytes1 / ( 2 * ss->out_channels );
    get_input_data( ss, ss->out_frames );
    if( ss->out_type == sound_buffer_int16 )
    {
        ss->out_buffer = lockPtr1;
        sundog_sound_callback( ss, 0 );
    }
    if( ss->out_type == sound_buffer_float32 )
    {
        ss->out_buffer = d->output_buffer;
        sundog_sound_callback( ss, 0 );
        float* fb = (float*)d->output_buffer;
        int16_t* sb = (int16_t*)lockPtr1;
        for( uint i = 0; i < (unsigned)ss->out_frames * ss->out_channels; i++ )
        {
	    SMP_FLOAT32_TO_INT16( sb[ i ], fb[ i ] );
        }
    }

    ds->buf->Unlock( lockPtr1, lockBytes1, lockPtr2, lockBytes2 );

    return;
}

DWORD __stdcall ds_output_thread_body( void* par )
{
    sundog_denormal_numbers_check();
    sundog_sound* ss = (sundog_sound*)par;
    device_sound* d = (device_sound*)ss->device_specific;
    device_sound_dsound* ds = d->ds;
    uint evt = -1;
    while( ds->output_exit_request == 0 )
    {
	DWORD dwEvt = MsgWaitForMultipleObjects(
	    NUMEVENTS, // How many possible events
	    ds->rghEvent, // Location of handles
	    FALSE, // Wait for all?
	    200, // How long to wait
	    QS_ALLINPUT ); // Any message is an event

	dwEvt -= WAIT_OBJECT_0;

	// If the event was set by the buffer, there's input
	// to process. 

	if( dwEvt < NUMEVENTS && dwEvt != evt ) 
	{
	    evt = dwEvt;
	    StreamToBuffer( ss, evt ); // copy data to output stream
	}
    }
    ds->output_exit_request = 0;
    return 0;
}

DWORD __stdcall ds_input_thread_body( void* par )
{
    sundog_sound* ss = (sundog_sound*)par;
    device_sound* d = (device_sound*)ss->device_specific;
    device_sound_dsound* ds = d->ds;
    while( ds->input_exit_request == 0 )
    {
	uint c, r;
        ds->input_buf->GetCurrentPosition( (DWORD*)&c, (DWORD*)&r );
        if( r != ds->input_rp )
        {
    	    int size = r - ds->input_rp;
    	    if( size < 0 ) size += ds->input_buf_desc.dwBufferBytes;

    	    void* lockInPtr1 = 0;
    	    void* lockInPtr2 = 0;
    	    DWORD lockInBytes1 = 0;
    	    DWORD lockInBytes2 = 0;
    	    HRESULT res = ds->input_buf->Lock(
        	ds->input_rp,      // Offset of lock start
        	size,             // Number of bytes to lock
        	&lockInPtr1,      // Address of lock start
        	&lockInBytes1,    // Count of bytes locked
        	&lockInPtr2,      // Address of wrap around
        	&lockInBytes2,    // Count of wrap around bytes
        	0 );              // Flags
    	    if( res == DS_OK )
    	    {
    		for( int lockNum = 0; lockNum < 2; lockNum++ )
    		{
    		    int size;
    		    int8_t* ptr;
    		    if( lockNum == 0 )
    		    {
    			size = lockInBytes1;
    			ptr = (int8_t*)lockInPtr1;
    		    }
    		    else
    		    {
    			size = lockInBytes2;
    			ptr = (int8_t*)lockInPtr2;
    		    }
    		    if( size == 0 ) continue;
    		    int ds_frame_size = 2 * ss->in_channels;
    		    int frame_size = g_sample_size[ ss->in_type ] * ss->in_channels;
    		    size /= ds_frame_size;
    		    while( size > 0 )
    		    {
    			int size2 = size;
        		if( d->input_buffer_wp + size2 > d->input_buffer_size )
            		    size2 = d->input_buffer_size - d->input_buffer_wp;
        		int8_t* buf_ptr = (int8_t*)d->input_buffer + d->input_buffer_wp * frame_size;
        		smem_copy( buf_ptr, ptr, size2 * ds_frame_size );
        		if( ss->in_type == sound_buffer_float32 )
        		{
            		    //Convert from INT16 to FLOAT32
            		    float* fb = (float*)buf_ptr;
            		    int16_t* sb = (int16_t*)buf_ptr;
            		    for( int i = size2 * ss->in_channels - 1; i >= 0; i-- )
            		    SMP_INT16_TO_FLOAT32( fb[ i ], sb[ i ] );
        		}
        		volatile uint new_wp = ( d->input_buffer_wp + size2 ) & ( d->input_buffer_size - 1 );
        		d->input_buffer_wp = new_wp;
        		size -= size2;
        		ptr += size2 * ds_frame_size;
    		    }
    		}

    		ds->input_buf->Unlock( lockInPtr1, lockInBytes1, lockInPtr2, lockInBytes2 );
    	    }

    	    ds->input_rp = r;
        }
        else Sleep( 1 );
    }
    ds->input_exit_request = 0;
    return 0;
}

BOOL CALLBACK DSEnumCallback (
    LPGUID GUID,
    LPCSTR Description,
    LPCSTR Module,
    VOID* Context
)
{
    device_sound_dsound_enum* c = (device_sound_dsound_enum*)Context;
    if( c->infos )
    {
	char*** infos = (char***)c->infos;
	if( *infos == 0 )
	{
    	    *infos = (char**)smem_new( 512 * sizeof( void* ) );
	}
	char* ts = (char*)smem_new( 2048 );
	sprintf( ts, "%s (%d)", Description, c->guids_num );
	(*infos)[ c->guids_num ] = (char*)smem_new( smem_strlen( ts ) + 1 ); (*infos)[ c->guids_num ][ 0 ] = 0; smem_strcat_resize( (*infos)[ c->guids_num ], ts );
	smem_free( ts );
    }
    if( c->log )
    {
	if( c->input )
    	    slog( "Found input device %d: %s\n", c->guids_num, Description );
    	else
    	    slog( "Found output device %d: %s\n", c->guids_num, Description );
    }
    c->guids[ c->guids_num ] = GUID;
    c->guids_num++;
    return 1;
}

int device_sound_init_dsound( sundog_sound* ss, bool input )
{
    device_sound* d = (device_sound*)ss->device_specific;
    int rv = 0;
    int sound_dev;
    HRESULT res;
    
    device_sound_dsound* ds = d->ds;
    if( ds == 0 )
    {
	slog( "DSOUND init...\n" );
	ds = (device_sound_dsound*)smem_new( sizeof( device_sound_dsound ) );
	smem_zero( ds );
	d->ds = ds;
    }

    if( input )
    {
	if( ds->lpdsCapture ) return 0; //Already open
	sound_dev = sprofile_get_int_value( KEY_AUDIODEVICE_IN, -1, 0 );
	ds->input_exit_request = 0;
	d->input_buffer_wp = 0;
        d->input_buffer_rp = 0;
    }
    else
    {
	if( ds->lpds ) return 0; //Already open
	sound_dev = sprofile_get_int_value( KEY_AUDIODEVICE, -1, 0 );
	ds->output_exit_request = 0;
    }
    
    HWND hWnd = GetForegroundWindow();
    if( hWnd == NULL )
    {
        hWnd = GetDesktopWindow();
    }
    device_sound_dsound_enum EnumContext;
    smem_clear_struct( EnumContext );
    EnumContext.log = true;
    LPCGUID guid = 0;
    if( input )
    {
	EnumContext.input = true;
	DirectSoundCaptureEnumerate( DSEnumCallback, &EnumContext );
	if( sound_dev >= 0 && sound_dev < EnumContext.guids_num )
	    guid = EnumContext.guids[ sound_dev ];
	res = DirectSoundCaptureCreate( guid, &ds->lpdsCapture, NULL );
	if( res != DS_OK )
	{
	    slog( "DSOUND INPUT: DirectSoundCreate error %d\n", res );
    	    MessageBox( hWnd, "DSOUND INPUT: DirectSoundCreate error", "Error", MB_OK );
	    ds->lpdsCapture = 0;
	    rv = 1;
	    goto init_dsound_end;
	}

	// Create capture buffer:

	memset( &ds->input_buf_desc, 0, sizeof( DSCBUFFERDESC ) ); 
	ds->input_buf_desc.dwSize = sizeof( DSCBUFFERDESC );
	ds->input_buf_desc.dwBufferBytes = ds->buf_desc.dwBufferBytes;
	ds->input_buf_desc.lpwfxFormat = &ds->wfx;
	res = ds->lpdsCapture->CreateCaptureBuffer( &ds->input_buf_desc, &ds->input_buf, NULL );
	if( res != DS_OK )
	{
    	    slog( "DSOUND INPUT: Create capture buffer error %d\n", res );
	    MessageBox( hWnd, "DSOUND INPUT: Create capture buffer error", "Error", MB_OK );
	    rv = 2;
	    goto init_dsound_end;
	}
	ds->input_buf->GetCurrentPosition( 0, (DWORD*)&ds->input_rp );

	// Create input thread:
    
	ds->input_thread = CreateThread( 0, 1024 * 16, &ds_input_thread_body, ss, 0, 0 );
	SetThreadPriority( ds->input_thread, THREAD_PRIORITY_TIME_CRITICAL );

	// Start:
	
	ds->input_buf->Start( DSCBSTART_LOOPING );
    }
    else
    {
	DirectSoundEnumerate( DSEnumCallback, &EnumContext );    
	if( sound_dev >= 0 && sound_dev < EnumContext.guids_num )
	    guid = EnumContext.guids[ sound_dev ];
	slog( "DSOUND: selected device: %d\n", sound_dev );
	res = DirectSoundCreate( guid, &ds->lpds, NULL );
	if( res != DS_OK )
	{
	    slog( "DSOUND: DirectSoundCreate error %d\n", res );
    	    MessageBox( hWnd, "DSOUND: DirectSoundCreate error", "Error", MB_OK );
	    ds->lpds = 0;
    	    rv = 3;
    	    goto init_dsound_end;
	}
	res = ds->lpds->SetCooperativeLevel( hWnd, DSSCL_PRIORITY );
	if( res != DS_OK )
	{
	    slog( "DSOUND: SetCooperativeLevel error %d\n", res );
	    MessageBox( hWnd, "DSOUND: SetCooperativeLevel error", "Error", MB_OK );
	    rv = 4;
	    goto init_dsound_end;
	}

	memset( &ds->wfx, 0, sizeof( WAVEFORMATEX ) ); 
	ds->wfx.wFormatTag = WAVE_FORMAT_PCM; 
        ds->wfx.nChannels = ss->out_channels; 
	ds->wfx.nSamplesPerSec = ss->freq; 
        ds->wfx.wBitsPerSample = 16; 
        ds->wfx.nBlockAlign = ds->wfx.wBitsPerSample / 8 * ds->wfx.nChannels;
        ds->wfx.nAvgBytesPerSec = ds->wfx.nSamplesPerSec * ds->wfx.nBlockAlign;

	// Primary buffer:
    
	memset( &ds->buf_desc, 0, sizeof( DSBUFFERDESC ) ); 
	ds->buf_desc.dwSize = sizeof( DSBUFFERDESC ); 
	ds->buf_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
	res = ds->lpds->CreateSoundBuffer( &ds->buf_desc, &ds->buf_primary, NULL );
	if( res != DS_OK )
	{
	    slog( "DSOUND: Create primary buffer error %d\n", res );
	    MessageBox( hWnd, "DSOUND: Create primary buffer error", "Error", MB_OK );
	    rv = 5;
	    goto init_dsound_end;
	}    
	ds->buf_primary->SetFormat( &ds->wfx );

	// Secondary buffer:
    
	memset( &ds->buf_desc, 0, sizeof( DSBUFFERDESC ) ); 
	ds->buf_desc.dwSize = sizeof( DSBUFFERDESC ); 
	ds->buf_desc.dwFlags = 
	    ( 0
	    | DSBCAPS_GETCURRENTPOSITION2 // Always a good idea
	    | DSBCAPS_STICKYFOCUS
	    | DSBCAPS_GLOBALFOCUS // Allows background playing
	    | DSBCAPS_CTRLPOSITIONNOTIFY
	    );
	ds->buf_desc.dwBufferBytes = d->buffer_size * 2 * ss->out_channels * 2;
	ds->buf_desc.lpwfxFormat = &ds->wfx;
	res = ds->lpds->CreateSoundBuffer( &ds->buf_desc, &ds->buf, NULL );
	if( res != DS_OK )
	{
    	    slog( "DSOUND: Create secondary buffer error %d\n", res );
	    MessageBox( hWnd, "DSOUND: Create secondary buffer error", "Error", MB_OK );
	    rv = 6;
	    goto init_dsound_end;
	}
    
        // Create buffer events:

        if( ds->rghEvent[ 0 ] == 0 )
	{    
	    for( int i = 0; i < NUMEVENTS; i++ )
	    {
		ds->rghEvent[ i ] = CreateEvent( NULL, FALSE, FALSE, NULL );
    		if( ds->rghEvent[ i ] == NULL ) 
		{
		    slog( "DSOUND: Create event error\n" );
		    MessageBox( hWnd, "DSOUND: Create event error", "Error", MB_OK );
		    for( int ii = 0; ii < i; ii++ ) { CloseHandle( ds->rghEvent[ ii ] ); ds->rghEvent[ ii ] = 0; }
		    rv = 7;
		    goto init_dsound_end;
		}
	    }
	}
	for( int i = 0; i < NUMEVENTS; i++ )
	{
	    ds->rgdsbpn[ i ].dwOffset = ( ds->buf_desc.dwBufferBytes / NUMEVENTS ) * i;
	    ds->rgdsbpn[ i ].hEventNotify = ds->rghEvent[ i ];
	}
	res = ds->buf->QueryInterface( IID_IDirectSoundNotify, (VOID **)&ds->lpdsNotify );
	if( res != DS_OK )
	{
    	    slog( "DSOUND: QueryInterface error %d\n", res );
    	    MessageBox( hWnd, "DSOUND: QueryInterface error", "Error", MB_OK );
    	    rv = 8;
    	    goto init_dsound_end;
	}    
	res = ds->lpdsNotify->SetNotificationPositions( NUMEVENTS, ds->rgdsbpn );
	if( res != DS_OK )
	{
	    slog( "DSOUND: SetNotificationPositions error %d\n", res );
	    MessageBox( hWnd, "DSOUND: SetNotificationPositions error", "Error", MB_OK );
	    rv = 9;
	    goto init_dsound_end;
	}

	// Create output thread:

	slog( "DSOUND thread starting...\n" );    
	ds->output_thread = CreateThread( 0, 1024 * 64, &ds_output_thread_body, ss, 0, 0 );
	SetThreadPriority( ds->output_thread, THREAD_PRIORITY_TIME_CRITICAL );
    	
	// Play:

	ds->buf->Play( 0, 0, DSBPLAY_LOOPING );
	slog( "DSOUND play\n" );
    }

init_dsound_end:
    if( rv )
    {
	if( input )
	{
	    if( ds->lpdsCapture ) ds->lpdsCapture->Release(); ds->lpdsCapture = NULL;
	}
	else
	{
	    if( ds->lpdsNotify ) ds->lpdsNotify->Release(); ds->lpdsNotify = NULL;
	    if( ds->buf ) ds->buf->Release(); ds->buf = NULL;
	    if( ds->buf_primary ) ds->buf_primary->Release(); ds->buf_primary = NULL;
	    if( ds->lpds ) ds->lpds->Release(); ds->lpds = NULL;
	}
    }
    
    return rv;
}

#endif

//
// Waveform Audio (MMSYSTEM)
//

void WaveOutSendBuffer( sundog_sound* ss, WAVEHDR* waveHdr )
{
    device_sound* d = (device_sound*)ss->device_specific;
    device_sound_mmsound* mm = d->mm;
    ss->out_time = stime_ticks_hires() + ( ( (uint64_t)d->buffer_size * (uint64_t)stime_ticks_per_second_hires() ) / (uint64_t)ss->freq );
    ss->out_frames = waveHdr->dwBufferLength / ( 2 * ss->out_channels );
    get_input_data( ss, ss->out_frames );
    if( ss->out_type == sound_buffer_int16 )
    {
	ss->out_buffer = waveHdr->lpData;
	sundog_sound_callback( ss, 0 );
    }
    if( ss->out_type == sound_buffer_float32 )
    {
        ss->out_buffer = d->output_buffer;
        sundog_sound_callback( ss, 0 );
        float* fb = (float*)d->output_buffer;
        int16_t* sb = (int16_t*)waveHdr->lpData;
        for( uint i = 0; i < (uint)ss->out_frames * ss->out_channels; i++ )
        {
	    SMP_FLOAT32_TO_INT16( sb[ i ], fb[ i ] );
        }
    }
    MMRESULT mres = waveOutWrite( mm->waveOutStream, waveHdr, sizeof( WAVEHDR ) );
    if( mres != MMSYSERR_NOERROR )
    {
        slog( "ERROR in waveOutWrite: %d\n", mres );
    }
}

void WaveInReceiveBuffer( sundog_sound* ss, WAVEHDR* waveHdr )
{
    device_sound* d = (device_sound*)ss->device_specific;
    device_sound_mmsound* mm = d->mm;
    if( waveHdr->dwBytesRecorded )
    {
	int mm_frame_size = 2 * ss->in_channels;
	int frame_size = g_sample_size[ ss->in_type ] * ss->in_channels;
        int size = waveHdr->dwBytesRecorded / mm_frame_size;
        int ptr = 0;
        while( size > 0 )
        {
    	    int size2 = size;
            if( d->input_buffer_wp + size2 > d->input_buffer_size )
                size2 = d->input_buffer_size - d->input_buffer_wp;
            int8_t* buf_ptr = (int8_t*)d->input_buffer + d->input_buffer_wp * frame_size;
            smem_copy( buf_ptr, ((int8_t*)waveHdr->lpData) + ptr, size2 * mm_frame_size );
            if( ss->in_type == sound_buffer_float32 )
            {
                //Convert from INT16 to FLOAT32
                float* fb = (float*)buf_ptr;
                int16_t* sb = (int16_t*)buf_ptr;
                for( int i = size2 * ss->in_channels - 1; i >= 0; i-- )
                SMP_INT16_TO_FLOAT32( fb[ i ], sb[ i ] );
            }
            volatile uint new_wp = ( d->input_buffer_wp + size2 ) & ( d->input_buffer_size - 1 );
            d->input_buffer_wp = new_wp;
            size -= size2;
            ptr += size2 * mm_frame_size;
        }
    }
    MMRESULT mres = waveInAddBuffer( mm->waveInStream, waveHdr, sizeof( WAVEHDR ) );
    if( mres != MMSYSERR_NOERROR )
    {
        slog( "MMSOUND INPUT ERROR: Can't add buffer (%d)\n", mres );
    }
}

DWORD WINAPI WaveOutThreadProc( LPVOID lpParameter )
{
    sundog_denormal_numbers_check();
    sundog_sound* ss = (sundog_sound*)lpParameter;
    device_sound* d = (device_sound*)ss->device_specific;
    device_sound_mmsound* mm = d->mm;
    bool running = true;
    while( running )
    {
	MSG msg;
	WAVEHDR* waveHdr = NULL;
	int m = GetMessage( &msg, 0, 0, 0 );
        if( m == 0 ) break;
        if( m != -1 )
        {
	    switch( msg.message )
    	    {
    	        case MM_WOM_DONE:
    		    waveHdr = (WAVEHDR*)msg.lParam;
		    WaveOutSendBuffer( ss, waveHdr );
		    break;
		case MM_WOM_CLOSE:
		    running = false;
		    break;
	    }
	}
    }
    mm->waveOutThreadClosed = true;
    return 0;
}

DWORD WINAPI WaveInThreadProc( LPVOID lpParameter )
{
    sundog_sound* ss = (sundog_sound*)lpParameter;
    device_sound* d = (device_sound*)ss->device_specific;
    device_sound_mmsound* mm = d->mm;
    bool running = true;
    while( running )
    {
	MSG msg;
	WAVEHDR* waveHdr = NULL;
	int m = GetMessage( &msg, 0, 0, 0 );
	if( m == 0 ) break;
	if( m != -1 )
	{
    	    switch( msg.message )
    	    {
    	        case MM_WIM_DATA:
    		    waveHdr = (WAVEHDR*)msg.lParam;
    		    WaveInReceiveBuffer( ss, waveHdr );
		    break;
		case MM_WOM_CLOSE:
		    running = false;
		    break;
	    }
	}
    }
    mm->waveInThreadClosed = true;
    return 0;
}

int device_sound_init_mmsound( sundog_sound* ss, bool input )
{
    device_sound* d = (device_sound*)ss->device_specific;
    int rv = 0;
    int sound_dev;
    int dev = -1;
    const char* input_label = "";

    device_sound_mmsound* mm = d->mm;
    if( mm == 0 )
    {
        mm = (device_sound_mmsound*)smem_new( sizeof( device_sound_mmsound ) );
        smem_zero( mm );
        d->mm = mm;
    }
    
    if( input )
    {
	if( mm->waveInStream ) return 0; //Already open
	sound_dev = sprofile_get_int_value( KEY_AUDIODEVICE_IN, -1, 0 );
	input_label = " INPUT";
        mm->waveInThread = 0;
	mm->waveInThreadID = 0;
	mm->waveInThreadClosed = false;
	d->input_buffer_wp = 0;
        d->input_buffer_rp = 0;
    }
    else
    {
	if( mm->waveOutStream ) return 0; //Already open
	sound_dev = sprofile_get_int_value( KEY_AUDIODEVICE, -1, 0 );
        mm->waveOutThread = 0;
	mm->waveOutThreadID = 0;
	mm->waveOutThreadClosed = false;
    }
    
    int soundDevices;
    if( input )
	soundDevices = waveInGetNumDevs();
    else
	soundDevices = waveOutGetNumDevs();
    if( soundDevices == 0 ) 
    { 
	slog( "MMSOUND%s ERROR: No sound devices :(\n", input_label ); 
	rv = 1;
	goto init_mmsound_end;
    } //No sound devices
    slog( "MMSOUND%s: Number of sound devices: %d\n", input_label, soundDevices );
    
    for( int ourDevice = 0; ourDevice < soundDevices; ourDevice++ )
    {
    	if( input )
    	{
    	    WAVEINCAPS deviceCaps;
	    waveInGetDevCaps( ourDevice, &deviceCaps, sizeof( deviceCaps ) );
	    if( deviceCaps.dwFormats & WAVE_FORMAT_4S16 ) 
	    {
		dev = ourDevice;
		break;
	    }
	}
	else
	{
    	    WAVEOUTCAPS deviceCaps;
	    waveOutGetDevCaps( ourDevice, &deviceCaps, sizeof( deviceCaps ) );
	    if( deviceCaps.dwFormats & WAVE_FORMAT_4S16 ) 
	    {
		dev = ourDevice;
		break;
	    }
	}
    }
    if( sound_dev >= 0 && sound_dev < soundDevices )
    {
	dev = sound_dev;
    }
    else
    {
	if( dev == -1 )
	{
	    slog( "MMSOUND%s ERROR: Can't find compatible sound device\n", input_label );
	    rv = 2;
	    goto init_mmsound_end;
	}
    }
    slog( "MMSOUND%s: Dev: %d. Sound freq: %d\n", input_label, dev, ss->freq );
    
    WAVEFORMATEX waveFormat;
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = ss->out_channels;
    waveFormat.nSamplesPerSec = ss->freq;
    waveFormat.nAvgBytesPerSec = ss->freq * 4;
    waveFormat.nBlockAlign = 4;
    waveFormat.wBitsPerSample = 16;
    waveFormat.cbSize = 0;
    MMRESULT mres;
    if( input )
    {
	mm->waveInThread = (HANDLE)CreateThread( NULL, 1024 * 16, WaveInThreadProc, ss, 0, &mm->waveInThreadID );
	SetThreadPriority( mm->waveInThread, THREAD_PRIORITY_TIME_CRITICAL );
	mres = waveInOpen( &mm->waveInStream, dev, &waveFormat, (uint)mm->waveInThreadID, 0, CALLBACK_THREAD );
    }
    else
    {
	mm->waveOutThread = (HANDLE)CreateThread( NULL, 1024 * 64, WaveOutThreadProc, ss, 0, &mm->waveOutThreadID );
	SetThreadPriority( mm->waveOutThread, THREAD_PRIORITY_TIME_CRITICAL );
	mres = waveOutOpen( &mm->waveOutStream, dev, &waveFormat, (uint)mm->waveOutThreadID, 0, CALLBACK_THREAD );
    }
    if( mres != MMSYSERR_NOERROR )
    {
	slog( "MMSOUND%s ERROR: Can't open sound device (%d)\n", input_label, mres );
	switch( mres )
	{
	    case MMSYSERR_INVALHANDLE: slog( "MMSOUND%s ERROR: MMSYSERR_INVALHANDLE\n", input_label ); break;
	    case MMSYSERR_BADDEVICEID: slog( "MMSOUND%s ERROR: MMSYSERR_BADDEVICEID\n", input_label ); break;
	    case MMSYSERR_NODRIVER: slog( "MMSOUND%s ERROR: MMSYSERR_NODRIVER\n", input_label ); break;
	    case MMSYSERR_NOMEM: slog( "MMSOUND%s ERROR: MMSYSERR_NOMEM\n", input_label ); break;
	    case WAVERR_BADFORMAT: slog( "MMSOUND%s ERROR: WAVERR_BADFORMAT\n", input_label ); break;
	    case WAVERR_SYNC: slog( "MMSOUND%s ERROR: WAVERR_SYNC\n", input_label ); break;
	}
	rv = 3;
	goto init_mmsound_end;
    }
    slog( "MMSOUND%s: waveout device opened\n", input_label );
    
    if( input )
    {
	for( int b = 0; b < MM_MAX_BUFFERS; b++ )
	{
    	    ZeroMemory( &mm->inBuffersHdr[ b ], sizeof( WAVEHDR ) );
    	    mm->inBuffersHdr[ b ].lpData = (char *)malloc( d->buffer_size * 2 * ss->out_channels );
    	    mm->inBuffersHdr[ b ].dwBufferLength = d->buffer_size * 2 * ss->out_channels;
	    mres = waveInPrepareHeader( mm->waveInStream, &mm->inBuffersHdr[ b ], sizeof( WAVEHDR ) );
	    if( mres != MMSYSERR_NOERROR )
	    {
		slog( "MMSOUND%s ERROR: Can't prepare %d header (%d)\n", input_label, b, mres );
		rv = 4;
		break;
	    }
	    mres = waveInAddBuffer( mm->waveInStream, &mm->inBuffersHdr[ b ], sizeof( WAVEHDR ) );
	    if( mres != MMSYSERR_NOERROR )
	    {
		slog( "MMSOUND%s ERROR: Can't add %d buffer (%d)\n", input_label, b, mres );
		rv = 5;
		break;
	    }
	}
	if( rv == 0 ) waveInStart( mm->waveInStream );
    }
    else
    {
	for( int b = 0; b < MM_MAX_BUFFERS; b++ )
	{
    	    ZeroMemory( &mm->outBuffersHdr[ b ], sizeof( WAVEHDR ) );
    	    mm->outBuffersHdr[ b ].lpData = (char *)malloc( d->buffer_size * 2 * ss->out_channels );
    	    mm->outBuffersHdr[ b ].dwBufferLength = d->buffer_size * 2 * ss->out_channels;
	    mres = waveOutPrepareHeader( mm->waveOutStream, &mm->outBuffersHdr[ b ], sizeof( WAVEHDR ) );
	    if( mres != MMSYSERR_NOERROR )
	    {
		slog( "MMSOUND ERROR: Can't prepare %d waveout header (%d)\n", b, mres );
		rv = 6;
		break;
	    }
	    WaveOutSendBuffer( ss, &mm->outBuffersHdr[ b ] );
	}
    }

init_mmsound_end:
    if( rv )
    {
	if( input )
	{
	    if( mm->waveInThread )
	    {
		PostThreadMessage( mm->waveInThreadID, WOM_CLOSE, 0, 0 );
                STIME_WAIT_FOR( mm->waveInThreadClosed, SUNDOG_SOUND_DEFAULT_TIMEOUT_MS, 20, "MMSOUND INPUT: Timeout (init failed)" );
                CloseHandle( mm->waveInThread ); mm->waveInThread = 0;
            }
	    if( mm->waveInStream )
	    {
		waveInReset( mm->waveInStream );
		waveInClose( mm->waveInStream );
		mm->waveInStream = 0;
	    }
	}
	else 
	{
	    if( mm->waveOutThread )
	    {
		PostThreadMessage( mm->waveOutThreadID, WOM_CLOSE, 0, 0 );
                STIME_WAIT_FOR( mm->waveOutThreadClosed, SUNDOG_SOUND_DEFAULT_TIMEOUT_MS, 20, "MMSOUND: Timeout (init failed)" );
            	CloseHandle( mm->waveOutThread ); mm->waveOutThread = 0;
	    }
	    if( mm->waveOutStream )
	    {
		waveOutReset( mm->waveOutStream );
	        waveOutClose( mm->waveOutStream );
		mm->waveOutStream = 0;
	    }
	}
    }

    return rv;
}

//
// ASIO
//

#ifdef ASIO

#if !defined(_MSC_VER)

const char* asio_error_str( ASIOError err )
{
    const char* rv = "Unknown";
    switch( err )
    {
	case ASE_NotPresent: rv = "ASE_NotPresent"; break;
	case ASE_HWMalfunction: rv = "ASE_HWMalfunction"; break;
	case ASE_InvalidParameter: rv = "ASE_InvalidParameter"; break;
	case ASE_InvalidMode: rv = "ASE_InvalidMode"; break;
	case ASE_SPNotAdvancing: rv = "ASE_SPNotAdvancing"; break;
	case ASE_NoClock: rv = "ASE_NoClock"; break;
	case ASE_NoMemory: rv = "ASE_NoMemory"; break;
    }
    return rv;
}

const char* asio_sampletype_str( ASIOSampleType type )
{
    const char* rv = "Unknown";
    switch( type )
    {
	case ASIOSTInt16MSB: rv = "ASIOSTInt16MSB"; break;
	case ASIOSTInt24MSB: rv = "ASIOSTInt24MSB"; break;
	case ASIOSTInt32MSB: rv = "ASIOSTInt32MSB"; break;
	case ASIOSTFloat32MSB: rv = "ASIOSTFloat32MSB"; break;
	case ASIOSTFloat64MSB: rv = "ASIOSTFloat64MSB"; break;
	case ASIOSTInt32MSB16: rv = "ASIOSTInt32MSB16"; break;
	case ASIOSTInt32MSB18: rv = "ASIOSTInt32MSB18"; break;
	case ASIOSTInt32MSB20: rv = "ASIOSTInt32MSB20"; break;
	case ASIOSTInt32MSB24: rv = "ASIOSTInt32MSB24"; break;
	case ASIOSTInt16LSB: rv = "ASIOSTInt16LSB"; break;
	case ASIOSTInt24LSB: rv = "ASIOSTInt24LSB"; break;
	case ASIOSTInt32LSB: rv = "ASIOSTInt32LSB"; break;
	case ASIOSTFloat32LSB: rv = "ASIOSTFloat32LSB"; break;
	case ASIOSTFloat64LSB: rv = "ASIOSTFloat64LSB"; break;
	case ASIOSTInt32LSB16: rv = "ASIOSTInt32LSB16"; break;
	case ASIOSTInt32LSB18: rv = "ASIOSTInt32LSB18"; break;
	case ASIOSTInt32LSB20: rv = "ASIOSTInt32LSB20"; break;
	case ASIOSTInt32LSB24: rv = "ASIOSTInt32LSB24"; break;
	case ASIOSTDSDInt8LSB1: rv = "ASIOSTDSDInt8LSB1"; break;
	case ASIOSTDSDInt8MSB1: rv = "ASIOSTDSDInt8MSB1"; break;
	case ASIOSTDSDInt8NER8: rv = "ASIOSTDSDInt8NER8"; break;
    }
    return rv;
}

IASIO* g_iasio = NULL; // Points to the real IASIO

#ifdef ARCH_X86
    //Non-Microsoft compilers don't implement the thiscall calling convention used by IASIO,
    //so we will use the fastcall instead of unimplemented thiscall:
    #define FN_ATTR_STDCALL __attribute__((stdcall))
    #define FN_ATTR_THISCALL __attribute__((fastcall))
    #define FN_THISCALL_P IASIO*,int
    #define FN_THISCALL_PARS g_iasio,0
#else
    #define FN_ATTR_STDCALL
    #define FN_ATTR_THISCALL
    #define FN_THISCALL_P IASIO*
    #define FN_THISCALL_PARS g_iasio
#endif
typedef void (*iasio_release_t) ( IASIO* ) FN_ATTR_STDCALL;
typedef ASIOBool (*iasio_init_t) ( FN_THISCALL_P, void* sysHandle ) FN_ATTR_THISCALL;
typedef void (*iasio_getDriverName_t) ( FN_THISCALL_P, char* name ) FN_ATTR_THISCALL;
typedef long (*iasio_getDriverVersion_t) ( FN_THISCALL_P ) FN_ATTR_THISCALL;
typedef void (*iasio_getErrorMessage_t) ( FN_THISCALL_P, char* string ) FN_ATTR_THISCALL;
typedef ASIOError (*iasio_start_t) ( FN_THISCALL_P ) FN_ATTR_THISCALL;
typedef ASIOError (*iasio_stop_t) ( FN_THISCALL_P ) FN_ATTR_THISCALL;
typedef ASIOError (*iasio_getChannels_t) ( FN_THISCALL_P, long* numInputChannels, long* numOutputChannels ) FN_ATTR_THISCALL;
typedef ASIOError (*iasio_getLatencies_t) ( FN_THISCALL_P, long* inputLatency, long* outputLatency ) FN_ATTR_THISCALL;
typedef ASIOError (*iasio_getBufferSize_t) ( FN_THISCALL_P, long* minSize, long* maxSize, long* preferredSize, long* granularity ) FN_ATTR_THISCALL;
typedef ASIOError (*iasio_canSampleRate_t) ( FN_THISCALL_P, ASIOSampleRate sampleRate ) FN_ATTR_THISCALL;
typedef ASIOError (*iasio_getSampleRate_t) ( FN_THISCALL_P, ASIOSampleRate* sampleRate ) FN_ATTR_THISCALL;
typedef ASIOError (*iasio_setSampleRate_t) ( FN_THISCALL_P, ASIOSampleRate sampleRate ) FN_ATTR_THISCALL;
typedef ASIOError (*iasio_getClockSources_t) ( FN_THISCALL_P, ASIOClockSource* clocks, long* numSources ) FN_ATTR_THISCALL;
typedef ASIOError (*iasio_setClockSource_t) ( FN_THISCALL_P, long reference ) FN_ATTR_THISCALL;
typedef ASIOError (*iasio_getSamplePosition_t) ( FN_THISCALL_P, ASIOSamples* sPos, ASIOTimeStamp* tStamp ) FN_ATTR_THISCALL;
typedef ASIOError (*iasio_getChannelInfo_t) ( FN_THISCALL_P, ASIOChannelInfo* info ) FN_ATTR_THISCALL;
typedef ASIOError (*iasio_createBuffers_t) ( FN_THISCALL_P, ASIOBufferInfo* bufferInfos, long numChannels, long bufferSize, ASIOCallbacks* callbacks ) FN_ATTR_THISCALL;
typedef ASIOError (*iasio_disposeBuffers_t) ( FN_THISCALL_P ) FN_ATTR_THISCALL;
typedef ASIOError (*iasio_controlPanel_t) ( FN_THISCALL_P ) FN_ATTR_THISCALL;
typedef ASIOError (*iasio_future_t)( FN_THISCALL_P, long selector, void* opt ) FN_ATTR_THISCALL;
typedef ASIOError (*iasio_outputReady_t) ( FN_THISCALL_P ) FN_ATTR_THISCALL;

iasio_release_t g_iasio_release = NULL;
iasio_init_t g_iasio_init = NULL;
iasio_getDriverName_t g_iasio_getDriverName = NULL;
iasio_getDriverVersion_t g_iasio_getDriverVersion = NULL;
iasio_getErrorMessage_t g_iasio_getErrorMessage = NULL;
iasio_start_t g_iasio_start = NULL;
iasio_stop_t g_iasio_stop = NULL;
iasio_getChannels_t g_iasio_getChannels = NULL;
iasio_getLatencies_t g_iasio_getLatencies = NULL;
iasio_getBufferSize_t g_iasio_getBufferSize = NULL;
iasio_canSampleRate_t g_iasio_canSampleRate = NULL;
iasio_getSampleRate_t g_iasio_getSampleRate = NULL;
iasio_setSampleRate_t g_iasio_setSampleRate = NULL;
iasio_getClockSources_t g_iasio_getClockSources = NULL;
iasio_setClockSource_t g_iasio_setClockSource = NULL;
iasio_getSamplePosition_t g_iasio_getSamplePosition = NULL;
iasio_getChannelInfo_t g_iasio_getChannelInfo = NULL;
iasio_createBuffers_t g_iasio_createBuffers = NULL;
iasio_disposeBuffers_t g_iasio_disposeBuffers = NULL;
iasio_controlPanel_t g_iasio_controlPanel = NULL;
iasio_future_t g_iasio_future = NULL;
iasio_outputReady_t g_iasio_outputReady = NULL;

#define ASIOFN( ptrnum ) ( ((void**)( p + ( sizeof(void*) * ptrnum ) ))[ 0 ] )

void iasio_load( void )
{
    int8_t* p = ((int8_t**)g_iasio)[ 0 ];
    g_iasio_release = (iasio_release_t)ASIOFN( 2 );
    g_iasio_init = (iasio_init_t)ASIOFN( 3 );
    g_iasio_getDriverName = (iasio_getDriverName_t)ASIOFN( 4 );
    g_iasio_getDriverVersion = (iasio_getDriverVersion_t)ASIOFN( 5 );
    g_iasio_getErrorMessage = (iasio_getErrorMessage_t)ASIOFN( 6 );
    g_iasio_start = (iasio_start_t)ASIOFN( 7 );
    g_iasio_stop = (iasio_stop_t)ASIOFN( 8 );
    g_iasio_getChannels = (iasio_getChannels_t)ASIOFN( 9 );
    g_iasio_getLatencies = (iasio_getLatencies_t)ASIOFN( 10 );
    g_iasio_getBufferSize = (iasio_getBufferSize_t)ASIOFN( 11 );
    g_iasio_canSampleRate = (iasio_canSampleRate_t)ASIOFN( 12 );
    g_iasio_getSampleRate = (iasio_getSampleRate_t)ASIOFN( 13 );
    g_iasio_setSampleRate = (iasio_setSampleRate_t)ASIOFN( 14 );
    g_iasio_getClockSources = (iasio_getClockSources_t)ASIOFN( 15 );
    g_iasio_setClockSource = (iasio_setClockSource_t)ASIOFN( 16 );
    g_iasio_getSamplePosition = (iasio_getSamplePosition_t)ASIOFN( 17 );
    g_iasio_getChannelInfo = (iasio_getChannelInfo_t)ASIOFN( 18 );
    g_iasio_createBuffers = (iasio_createBuffers_t)ASIOFN( 19 );
    g_iasio_disposeBuffers = (iasio_disposeBuffers_t)ASIOFN( 20 );
    g_iasio_controlPanel = (iasio_controlPanel_t)ASIOFN( 21 );
    g_iasio_future = (iasio_future_t)ASIOFN( 22 );
    g_iasio_outputReady = (iasio_outputReady_t)ASIOFN( 23 );
}

void iasio_release( void )
{
    g_iasio_release( g_iasio );
}

ASIOBool iasio_init( void* sysHandle )
{
    return g_iasio_init( FN_THISCALL_PARS, sysHandle );
}

void iasio_getDriverName( char* name )
{
    g_iasio_getDriverName( FN_THISCALL_PARS, name );
}

long iasio_getDriverVersion()
{
    return g_iasio_getDriverVersion( FN_THISCALL_PARS );
}

void iasio_getErrorMessage( char* string )
{
    g_iasio_getErrorMessage( FN_THISCALL_PARS, string );
}

ASIOError iasio_start()
{
    return g_iasio_start( FN_THISCALL_PARS );
}

ASIOError iasio_stop()
{
    return g_iasio_stop( FN_THISCALL_PARS );
}

ASIOError iasio_getChannels( long* numInputChannels, long* numOutputChannels )
{
    return g_iasio_getChannels( FN_THISCALL_PARS, numInputChannels, numOutputChannels );
}

ASIOError iasio_getLatencies( long* inputLatency, long* outputLatency )
{
    return g_iasio_getLatencies( FN_THISCALL_PARS, inputLatency, outputLatency );
}

ASIOError iasio_getBufferSize( long* minSize, long* maxSize, long* preferredSize, long* granularity )
{
    return g_iasio_getBufferSize( FN_THISCALL_PARS, minSize, maxSize, preferredSize, granularity );
}

ASIOError iasio_canSampleRate( ASIOSampleRate sampleRate )
{
    return g_iasio_canSampleRate( FN_THISCALL_PARS, sampleRate );
}

ASIOError iasio_getSampleRate( ASIOSampleRate* sampleRate )
{
    return g_iasio_getSampleRate( FN_THISCALL_PARS, sampleRate );
}

ASIOError iasio_setSampleRate( ASIOSampleRate sampleRate )
{
    return g_iasio_setSampleRate( FN_THISCALL_PARS, sampleRate );
}

ASIOError iasio_getClockSources( ASIOClockSource* clocks, long* numSources )
{
    return g_iasio_getClockSources( FN_THISCALL_PARS, clocks, numSources );
}

ASIOError iasio_setClockSource( long reference )
{
    return g_iasio_setClockSource( FN_THISCALL_PARS, reference );
}

ASIOError iasio_getSamplePosition( ASIOSamples* sPos, ASIOTimeStamp* tStamp )
{
    return g_iasio_getSamplePosition( FN_THISCALL_PARS, sPos, tStamp );
}

ASIOError iasio_getChannelInfo( ASIOChannelInfo* info )
{
    return g_iasio_getChannelInfo( FN_THISCALL_PARS, info );
}

ASIOError iasio_createBuffers( ASIOBufferInfo* bufferInfos, long numChannels, long bufferSize, ASIOCallbacks* callbacks )
{
    return g_iasio_createBuffers( FN_THISCALL_PARS, bufferInfos, numChannels, bufferSize, callbacks );
}

ASIOError iasio_disposeBuffers()
{
    return g_iasio_disposeBuffers( FN_THISCALL_PARS );
}

ASIOError iasio_controlPanel()
{
    return g_iasio_controlPanel( FN_THISCALL_PARS );
}

ASIOError iasio_future( long selector, void* opt )
{
    return g_iasio_future( FN_THISCALL_PARS, selector, opt );
}

ASIOError iasio_outputReady()
{
    return g_iasio_outputReady( FN_THISCALL_PARS );
}

#endif

ASIOTime* asio_buffer_switch_stime_info( ASIOTime* timeInfo, long index, ASIOBool processNow )
{
    //Indicates that both input and output are to be processed. 
    //It also passes extended time information (time code for synchronization purposes) to the host application and back to the driver.

    sundog_sound* ss = g_asio_ss;
    if( !ss ) return NULL;
    device_sound* d = (device_sound*)ss->device_specific;

    if( !d->first_frame_rendered )
    {
	d->first_frame_rendered = true;
	sundog_denormal_numbers_check();
    }

    //Input:
    if( d->input_enabled )
    {
	d->input_buffer2_is_empty = false;
        ss->in_buffer = d->input_buffer2;
	int16_t* dest16 = (int16_t*)ss->in_buffer;
	float* dest32 = (float*)ss->in_buffer;
	for( int c = ss->out_channels; c < ss->out_channels + ss->in_channels; c++ )
	{
	    if( g_asio_bufs[ c ].isInput == 0 ) continue;
	    int cnum = c - ss->out_channels;
	    uint8_t* buf = (uint8_t*)g_asio_bufs[ c ].buffers[ index ];
	    ASIOSampleType sample_type = g_asio_channels[ c ].type;
	    switch( sample_type )
	    {
		case ASIOSTInt16MSB:
		case ASIOSTInt16LSB:
		    {
			int16_t* src16 = (int16_t*)buf;
			if( ss->in_type == sound_buffer_int16 )
			{
			    if( sample_type == ASIOSTInt16MSB )
			    {
				for( int i = 0, i2 = 0; i < d->buffer_size; i++, i2 += ss->out_channels )
				{
				    uint16_t v = (uint16_t)src16[ i ];
				    uint16_t temp = ( v >> 8 ) & 255;
				    v = ( ( v << 8 ) & 0xFF00 ) | temp;
				    dest16[ i2 + cnum ] = (int16_t)v;
				}
			    }
			    else
			    {
				for( int i = 0, i2 = 0; i < d->buffer_size; i++, i2 += ss->out_channels )
				    dest16[ i2 + cnum ] = src16[ i ];
			    }
			}
			if( ss->in_type == sound_buffer_float32 )
			{
			    if( sample_type == ASIOSTInt16MSB )
			    {
				for( int i = 0, i2 = 0; i < d->buffer_size; i++, i2 += ss->out_channels )
				{
				    uint16_t v = (uint16_t)src16[ i ];
				    uint16_t temp = ( v >> 8 ) & 255;
				    v = ( ( v << 8 ) & 0xFF00 ) | temp;
				    SMP_INT16_TO_FLOAT32( dest32[ i2 + cnum ], (int16_t)v );
				}
			    }
			    else
			    {
				for( int i = 0, i2 = 0; i < d->buffer_size; i++, i2 += ss->out_channels )
				    SMP_INT16_TO_FLOAT32( dest32[ i2 + cnum ], src16[ i ] );
			    }
			}
		    }
		    break;
		case ASIOSTInt24LSB:
		case ASIOSTInt24MSB:
		    {
			for( int i = 0, i2 = 0, i3 = 0; i < d->buffer_size; i++, i2 += ss->out_channels, i3 += 3 )
			{
			    int v;
			    if( sample_type == ASIOSTInt24LSB )
				v = buf[ i3 + 0 ] | ( buf[ i3 + 1 ] << 8 ) | ( buf[ i3 + 2 ] << 16 );
			    else
				v = buf[ i3 + 2 ] | ( buf[ i3 + 1 ] << 8 ) | ( buf[ i3 + 0 ] << 16 );
			    if( v & 0x800000 ) v |= 0xFF000000; //sign
			    if( ss->in_type == sound_buffer_int16 )
				dest16[ i2 + cnum ] = v >> 8;
			    if( ss->in_type == sound_buffer_float32 )
				SMP_INT16_TO_FLOAT32( dest32[ i2 + cnum ], v >> 8 );
			}
		    }
		    break;
		case ASIOSTInt32MSB:
        	case ASIOSTInt32LSB:
		    {
			for( int i = 0, i2 = 0; i < d->buffer_size; i++, i2 += ss->out_channels )
			{
			    int v;
			    if( sample_type == ASIOSTInt32LSB )
				v = buf[ i * 4 + 0 ] | ( buf[ i * 4 + 1 ] << 8 ) | ( buf[ i * 4 + 2 ] << 16 ) | ( buf[ i * 4 + 3 ] << 24 );
			    else
				v = buf[ i * 4 + 3 ] | ( buf[ i * 4 + 2 ] << 8 ) | ( buf[ i * 4 + 1 ] << 16 ) | ( buf[ i * 4 + 0 ] << 24 );
			    if( ss->in_type == sound_buffer_int16 )
				dest16[ i2 + cnum ] = v >> 16;
			    if( ss->in_type == sound_buffer_float32 )
				SMP_INT16_TO_FLOAT32( dest32[ i2 + cnum ], v >> 16 );
			}
		    }
    		    break;
    		case ASIOSTFloat32MSB:
        	case ASIOSTFloat32LSB:
        	    {
			for( int i = 0, i2 = 0; i < d->buffer_size; i++, i2 += ss->out_channels )
			{
        		    float f;
			    uint* v = (uint*)&f;
			    if( sample_type == ASIOSTFloat32LSB )
				f = ((float*)buf)[ i ];
			    else
				*v = buf[ i * 4 + 3 ] | ( buf[ i * 4 + 2 ] << 8 ) | ( buf[ i * 4 + 1 ] << 16 ) | ( buf[ i * 4 + 0 ] << 24 );
			    if( ss->in_type == sound_buffer_int16 )
				SMP_FLOAT32_TO_INT16( dest16[ i2 + cnum ], f );
			    if( ss->in_type == sound_buffer_float32 )
				dest32[ i2 + cnum ] = f;
			}
        	    }
    		    break;
	    }
	}
    }
    else
    {
        if( d->input_buffer2_is_empty == false )
        {
            smem_zero( d->input_buffer2 );
            d->input_buffer2_is_empty = true;
        }
        ss->in_buffer = d->input_buffer2;
    }

    ss->out_time = stime_ticks_hires() + ( ( (uint64_t)d->buffer_size * (uint64_t)stime_ticks_per_second_hires() ) / (uint64_t)ss->freq );
    ss->out_buffer = d->output_buffer;
    ss->out_frames = d->buffer_size;
    sundog_sound_callback( ss, 0 );

    //Output:
    int16_t* src16 = (int16_t*)ss->out_buffer;
    float* src32 = (float*)ss->out_buffer;
    for( int ch = 0; ch < ss->out_channels; ch++ )
    {
	if( g_asio_bufs[ ch ].isInput ) continue;
	uint8_t* buf = (uint8_t*)g_asio_bufs[ ch ].buffers[ index ];
	switch( g_asio_channels[ ch ].type )
	{
	    case ASIOSTInt16MSB:
	    case ASIOSTInt16LSB:
		{
		    int16_t* dest16 = (int16_t*)buf;
		    if( ss->out_type == sound_buffer_int16 )
		    {
			for( int i = 0, i2 = 0; i < d->buffer_size; i++, i2 += ss->out_channels )
			{
			    dest16[ i ] = src16[ i2 + ch ];
			}
		    }
		    if( ss->out_type == sound_buffer_float32 )
		    {
			for( int i = 0, i2 = 0; i < d->buffer_size; i++, i2 += ss->out_channels )
			{
			    SMP_FLOAT32_TO_INT16( dest16[ i ], src32[ i2 + ch ] );
			}
		    }
		    if( g_asio_channels[ ch ].type == ASIOSTInt16MSB )
		    {
			for( int i = 0; i < d->buffer_size * 2; i += 2 )
			{
			    uint8_t temp = buf[ i + 1 ];
			    buf[ i ] = buf[ i + 1 ];
			    buf[ i + 1 ] = temp;
			}
		    }
		}
		break;
	    case ASIOSTInt24LSB:
		{
		    int16_t* dest16 = (int16_t*)buf;
		    if( ss->out_type == sound_buffer_int16 )
		    {
			for( int i = 0, i2 = 0; i < d->buffer_size; i++, i2 += ss->out_channels )
			{
			    int v = src16[ i2 + ch ] << 8;
			    buf[ i * 3 + 0 ] = v & 0xFF;
			    buf[ i * 3 + 1 ] = ( v >> 8 ) & 0xFF;
			    buf[ i * 3 + 2 ] = ( v >> 16 ) & 0xFF;
			}
		    }
		    if( ss->out_type == sound_buffer_float32 )
		    {
			for( int i = 0, i2 = 0; i < d->buffer_size; i++, i2 += ss->out_channels )
			{
			    int v = (int)( src32[ i2 + ch ] * (float)( 1 << 23 ) );
			    buf[ i * 3 + 0 ] = v & 0xFF;
			    buf[ i * 3 + 1 ] = ( v >> 8 ) & 0xFF;
			    buf[ i * 3 + 2 ] = ( v >> 16 ) & 0xFF;
			}
		    }
		}
		break;
	    case ASIOSTInt24MSB:
		{
		    if( ss->out_type == sound_buffer_int16 )
		    {
			for( int i = 0, i2 = 0; i < d->buffer_size; i++, i2 += ss->out_channels )
			{
			    int v = src16[ i2 + ch ] << 8;
			    buf[ i * 3 + 2 ] = v & 0xFF;
			    buf[ i * 3 + 1 ] = ( v >> 8 ) & 0xFF;
			    buf[ i * 3 + 0 ] = ( v >> 16 ) & 0xFF;
			}
		    }
		    if( ss->out_type == sound_buffer_float32 )
		    {
			for( int i = 0, i2 = 0; i < d->buffer_size; i++, i2 += ss->out_channels )
			{
			    int v = (int)( src32[ i2 + ch ] * (float)( 1 << 23 ) );
			    buf[ i * 3 + 2 ] = v & 0xFF;
			    buf[ i * 3 + 1 ] = ( v >> 8 ) & 0xFF;
			    buf[ i * 3 + 0 ] = ( v >> 16 ) & 0xFF;
			}
		    }
		}
		break;
	    case ASIOSTInt32MSB:
	    case ASIOSTInt32LSB:
		{
		    int32_t* dest32 = (int32_t*)buf;
		    if( ss->out_type == sound_buffer_int16 )
		    {
			for( int i = 0, i2 = 0; i < d->buffer_size; i++, i2 += ss->out_channels )
			{
			    dest32[ i ] = (int32_t)src16[ i2 + ch ] << 16;
			}
		    }
		    if( ss->out_type == sound_buffer_float32 )
		    {
			for( int i = 0, i2 = 0; i < d->buffer_size; i++, i2 += ss->out_channels )
			{
			    SMP_FLOAT32_TO_INT32( dest32[ i ], src32[ i2 + ch ] );
			}
		    }
		    if( g_asio_channels[ ch ].type == ASIOSTInt32MSB )
		    {
			for( int i = 0; i < d->buffer_size * 4; i += 4 )
			{
			    uint8_t temp1 = buf[ i + 1 ];
			    uint8_t temp2 = buf[ i + 0 ];
			    buf[ i + 0 ] = buf[ i + 3 ];
			    buf[ i + 1 ] = buf[ i + 2 ];
			    buf[ i + 2 ] = temp1;
			    buf[ i + 3 ] = temp2;
			}
		    }
		}
		break;
	    case ASIOSTFloat32MSB:
	    case ASIOSTFloat32LSB:
		{
		    float* dest32 = (float*)buf;
		    if( ss->out_type == sound_buffer_int16 )
		    {
			for( int i = 0, i2 = 0; i < d->buffer_size; i++, i2 += ss->out_channels )
			{
			    SMP_INT16_TO_FLOAT32( dest32[ i ], src16[ i2 + ch ] );
			}
		    }
		    if( ss->out_type == sound_buffer_float32 )
		    {
			for( int i = 0, i2 = 0; i < d->buffer_size; i++, i2 += ss->out_channels )
			{
			    dest32[ i ] = src32[ i2 + ch ];
			}
		    }
		    if( g_asio_channels[ ch ].type == ASIOSTFloat32MSB )
		    {
			for( int i = 0; i < d->buffer_size * 4; i += 4 )
			{
			    uint8_t temp1 = buf[ i + 1 ];
			    uint8_t temp2 = buf[ i + 0 ];
			    buf[ i + 0 ] = buf[ i + 3 ];
			    buf[ i + 1 ] = buf[ i + 2 ];
			    buf[ i + 2 ] = temp1;
			    buf[ i + 3 ] = temp2;
			}
		    }
		}
		break;
	}
    }

    iasio_outputReady();

    return 0;
}

void asio_buffer_switch( long index, ASIOBool processNow )
{
    //Indicates that both input and output are to be processed.

    //As this is a "back door" into the bufferSwitchTimeInfo a timeInfo needs to be created
    //though it will only set the timeInfo.samplePosition and timeInfo.systemTime fields and the according flags
    ASIOTime timeInfo;
    memset( &timeInfo, 0, sizeof( timeInfo ) );

    //get the time stamp of the buffer, not necessary if no
    //synchronization to other media is required
    if( iasio_getSamplePosition( &timeInfo.timeInfo.samplePosition, &timeInfo.timeInfo.systemTime ) == ASE_OK )
	timeInfo.timeInfo.flags = kSystemTimeValid | kSamplePositionValid;

    asio_buffer_switch_stime_info( &timeInfo, index, processNow );
}

void asio_sample_rate_changed( ASIOSampleRate sRate )
{
    slog( "ASIO Sample rate changed: %d\n", (int)sRate );
}

long asio_messages( long selector, long value, void* message, double* opt )
{
    long ret = 0;
    switch( selector )
    {
	case kAsioSelectorSupported:
	    if( value == kAsioEngineVersion || value == kAsioSupportsTimeInfo ) ret = 1;
	    break;
	case kAsioEngineVersion: 
            // return the supported ASIO version of the host application
            // If a host applications does not implement this selector, ASIO 1.0 is assumed
            // by the driver
	    ret = 2;
	    break;
        case kAsioSupportsTimeInfo:
            // informs the driver wether the asioCallbacks.bufferSwitchTimeInfo() callback
            // is supported.
            // For compatibility with ASIO 1.0 drivers the host application should always support
            // the "old" bufferSwitch method, too.
            ret = 1;
            break;
    }
    return ret;
}

int device_sound_init_asio( sundog_sound* ss )
{
    device_sound* d = (device_sound*)ss->device_specific;
    
    if( g_iasio )
    {
	slog( "ASIO ERROR: Only one instance of ASIO client can be opened\n" );
	return 1;
    }

    int rv = 0;    
    ASIOError err;    
    int old_buf_size = d->buffer_size;
    int sound_dev = sprofile_get_int_value( KEY_AUDIODEVICE, -1, 0 );
    
    //Enumerate devices:
    int devs = 0;
    HKEY hk = NULL;
    CHAR keyname[ 128 ];
    rv = RegOpenKey( HKEY_LOCAL_MACHINE, "software\\asio", &hk );
    int dcnt = 0;
    while( ( rv == ERROR_SUCCESS ) && ( devs < ASIO_DEVS ) )
    {
	rv = RegEnumKey( hk, dcnt, (LPTSTR)keyname, 128 );
	dcnt++;
	if( rv == ERROR_SUCCESS )
	{
	    slog( "ASIO Device %d: %s\n", devs, keyname );
	    HKEY hk2;
	    int rv2 = RegOpenKeyEx( hk, (LPCTSTR)keyname, 0, KEY_READ, &hk2 );
	    if( rv2 == ERROR_SUCCESS )
	    {
		CHAR s[ 256 ];
		WCHAR w[ 128 ];
		DWORD datatype = REG_SZ;
		DWORD datasize = 256;
		rv2 = RegQueryValueEx( hk2, "clsid", 0, &datatype, (LPBYTE)s, &datasize );
		if( rv2 == ERROR_SUCCESS )
		{
		    MultiByteToWideChar( CP_ACP, 0, (LPCSTR)s, -1, (LPWSTR)w, 128 );
		    if( CLSIDFromString( (LPOLESTR)w, (LPCLSID)&g_asio_devs[ devs ].clsid ) == S_OK )
		    {
		    }
		}
	    }
	    devs++;
	}
    }

    //Set device number:
    if( devs == 0 )
    {
	slog( "ASIO ERROR: No devices\n" );
	return 1;
    }
    int dev = 0;
    if( sound_dev >= 0 && sound_dev < devs )
    {
	dev = sound_dev;
    }
    slog( "ASIO Selected device: %d\n", dev ); 
    
    //Open device:
    HWND hWnd = GetForegroundWindow();
    if( hWnd == NULL )
    {
        hWnd = GetDesktopWindow();
    }
    CoInitialize( NULL );
    g_iasio = 0;
    rv = CoCreateInstance( g_asio_devs[ dev ].clsid, 0, CLSCTX_INPROC_SERVER, g_asio_devs[ dev ].clsid, (void**)&g_iasio );
    if( rv == S_OK )
    {
	iasio_load();
	slog( "ASIO init\n" );
	if( !iasio_init( (void *)hWnd ) )
	{
	    slog( "ASIO ERROR in init()\n" );
	    g_iasio = 0;
	    return 1;
	}
    }
    else 
    {
	slog( "ASIO ERROR in CoCreateInstance() %x\n", rv );
	g_iasio = 0;
	return 1;
    }
    g_asio_ss = ss;
    
    rv = 0;
    
    //Device init:
    long max_in_channels = 0;
    long max_out_channels = 0;
    long in_channels = 0;
    long out_channels = 0;
    long minSize = 0;
    long maxSize = 0;
    long preferredSize = 0;
    long granularity = 0;
    int in_ch_offset = 0;
    int out_ch_offset = 0;
    ASIOSampleRate asio_srate = 0;
    slog( "ASIO getChannels\n" );
    err = iasio_getChannels( &in_channels, &out_channels );
    if( err != ASE_OK )
    {
	slog( "ASIO ERROR in getChannels() %X %s\n", err, asio_error_str( err ) );
	rv = 1;
	goto init_asio_end;
    }
    max_in_channels = in_channels;
    max_out_channels = out_channels;
    slog( "ASIO CHANNELS: IN:%d OUT:%d\n", (int)in_channels, (int)out_channels );
    if( ss->out_channels > out_channels )
    {
	slog( "ASIO ERROR: Can't set %d channels\n", ss->out_channels );
	rv = 2;
	goto init_asio_end;
    }
    out_channels = ss->out_channels;
    if( ss->in_channels > in_channels )
	ss->in_channels = in_channels;
    in_channels = ss->in_channels;
    iasio_getSampleRate( &asio_srate );
    slog( "ASIO Initial sample rate: %d\n", (int)asio_srate );
    if( asio_srate != ss->freq )
    {
	while( 1 )
	{
	    slog( "ASIO Trying to set %d ...\n", ss->freq );
	    err = iasio_setSampleRate( ss->freq );
	    if( err != ASE_OK )
	    {
		slog( "ASIO ERROR: setSampleRate(%d) failed with code %X %s\n", ss->freq, err, asio_error_str( err ) );
		while( 1 )
		{
		    if( ss->freq == 44100 ) { ss->freq = 48000; break; }
		    if( ss->freq == 48000 ) { ss->freq = 96000; break; }
		    if( ss->freq == 96000 ) { ss->freq = 192000; break; }
		    if( ss->freq == 192000 ) 
		    {
			rv = 3;
			goto init_asio_end;
		    }
		    break;
		}
	    }
	    else 
	    {
		break;
	    }
	}
    }
    iasio_getSampleRate( &asio_srate );
    if( asio_srate != ss->freq )
    {
	slog( "ASIO Real sample rate (!=ss->freq): %d\n", (int)asio_srate );
    }
    in_ch_offset = sprofile_get_int_value( "audio_ch_in", 0, 0 );
    out_ch_offset = sprofile_get_int_value( "audio_ch", 0, 0 );
    g_asio_sample_size = 0;
    g_asio_input_sample_size = 0;
    for( int ch = 0; ch < out_channels + in_channels; ch++ )
    {
	int ch_num = ch;
	bool input = 0;
	if( ch_num >= out_channels ) 
	{
	    //Input:
	    ch_num -= out_channels; //Switch to Input channels
	    ch_num += in_ch_offset;
	    if( ch_num >= max_in_channels )
		ch_num %= max_in_channels;
	    input = 1;
	}
	else
	{
	    //Output:
	    ch_num += out_ch_offset;
	    if( ch_num >= max_out_channels )
		ch_num %= max_out_channels;
	}
	g_asio_channels[ ch ].channel = ch_num;
	g_asio_channels[ ch ].isInput = input;
	iasio_getChannelInfo( &g_asio_channels[ ch ] );
	slog( "ASIO CH %d: ", ch_num );
	if( input ) slog( "input; " ); else slog( "output; " );
	slog( "type=%s (%d);", asio_sampletype_str( g_asio_channels[ ch ].type ), g_asio_channels[ ch ].type );
	slog( "\n" );
	g_asio_bufs[ ch ].isInput = input;
	g_asio_bufs[ ch ].channelNum = ch_num;
	g_asio_bufs[ ch ].buffers[ 0 ] = NULL;
	g_asio_bufs[ ch ].buffers[ 1 ] = NULL;
	int sample_size = 0;
	switch( g_asio_channels[ ch ].type & 0xF )
	{
	    case ASIOSTInt16MSB: 
	    case ASIOSTInt16LSB: 
		sample_size = 2; 
		break;
	    case ASIOSTInt24MSB: 
	    case ASIOSTInt24LSB: 
		sample_size = 3; 
		break;
	    case ASIOSTFloat64MSB: 
	    case ASIOSTFloat64LSB: 
		sample_size = 8; 
		break;
	    default: 
		sample_size = 4;
		break;
	}
	if( input )
	{
	    if( g_asio_input_sample_size == 0 ) g_asio_input_sample_size = sample_size;
	}
	else
	{
	    if( g_asio_sample_size == 0 ) g_asio_sample_size = sample_size;
	}
    }
    printf( "ASIO getBufferSize\n" );
    err = iasio_getBufferSize( &minSize, &maxSize, &preferredSize, &granularity );
    if( err != ASE_OK )
    {
	slog( "ASIO ERROR in getBufferSize() %X %s\n", err, asio_error_str( err ) );
	rv = 4;
	goto init_asio_end;
    }
    slog( "ASIO buffer: minSize=%d maxSize=%d preferredSize=%d granularity=%d\n; SampleSize=%d inputSampleSize=%d\n", (int)minSize, (int)maxSize, (int)preferredSize, (int)granularity, g_asio_sample_size, g_asio_input_sample_size );
    if( granularity == -1 )
    {
	//The buffer size will be a power of 2:
	int size = minSize;
	while( size < d->buffer_size )
	{
	    size *= 2;
	}
	d->buffer_size = size;
    }
    if( granularity > 0 )
    {
	int size = minSize;
	while( size < d->buffer_size )
	{
	    size += granularity;
	}
	d->buffer_size = size;
    }
    if( granularity == 0 )
    {
	d->buffer_size = minSize;
    }
    if( d->buffer_size > maxSize )
	d->buffer_size = maxSize;
    if( d->buffer_size < minSize )
	d->buffer_size = minSize;
    slog( "ASIO buffer size: %d\n", (int)d->buffer_size );
    //Set callbacks:
    g_asio_callbacks.bufferSwitch = asio_buffer_switch;
    g_asio_callbacks.sampleRateDidChange = asio_sample_rate_changed;
    g_asio_callbacks.asioMessage = asio_messages;
    g_asio_callbacks.bufferSwitchTimeInfo = asio_buffer_switch_stime_info;
    //Create buffers:
create_bufs_again:
    slog( "ASIO createBuffers\n" );
    err = iasio_createBuffers( g_asio_bufs, in_channels + out_channels, d->buffer_size, &g_asio_callbacks );
    if( err == ASE_OK )
    {
	for( int i = 0; i < in_channels + out_channels; i++ )
	{
	    int sample_size;
	    if( g_asio_bufs[ i ].isInput )
		sample_size = g_asio_input_sample_size;
	    else
		sample_size = g_asio_sample_size;
	    if( g_asio_bufs[ i ].buffers[ 0 ] )
	    {
		memset( g_asio_bufs[ i ].buffers[ 0 ], 0, d->buffer_size * sample_size );
	    }
	    if( g_asio_bufs[ i ].buffers[ 1 ] )
	    {
		memset( g_asio_bufs[ i ].buffers[ 1 ], 0, d->buffer_size * sample_size );
	    }
	}
	slog( "ASIO outputReady\n" );
	iasio_outputReady();
    }
    else 
    {
	slog( "ASIO ERROR: createBuffers() failed with code %X %s\n", err, asio_error_str( err ) );
	if( err == ASE_InvalidMode )
	{
	    if( d->buffer_size != preferredSize )
	    {
		slog( "ASIO trying to switch to preferred buffer size %d ...\n", preferredSize );
		d->buffer_size = preferredSize;
		goto create_bufs_again;
    	    }
    	}
    	rv = 5;
    	goto init_asio_end;
    }
    
    if( old_buf_size < d->buffer_size )
    {
	smem_free( d->output_buffer );
	d->output_buffer = smem_new( d->buffer_size * d->frame_size );
    }

init_asio_end:
    if( rv )
    {
	slog( "ASIO init error %d\n", rv );
	if( g_iasio )
	{
	    iasio_release();
	    g_iasio = 0;
	    g_asio_ss = 0;
	}
    }
    else
    {
	slog( "ASIO start\n" );
	iasio_start();
    }

    return rv;
}

#endif

//
// Main sound functions
//

int device_sound_init( sundog_sound* ss )
{
    ss->device_specific = smem_new( sizeof( device_sound ) );
    device_sound* d = (device_sound*)ss->device_specific;
    smem_zero( d );

    sundog_sound_set_defaults( ss );

    d->buffer_size = sprofile_get_int_value( KEY_SOUNDBUFFER, g_default_buffer_size, 0 );
    ss->out_latency = d->buffer_size;
    ss->out_latency2 = d->buffer_size;

    d->frame_size = g_sample_size[ ss->out_type ] * ss->out_channels;
    d->output_buffer = smem_new( d->buffer_size * d->frame_size );

    bool sdriver_checked[ NUMBER_OF_SDRIVERS ];
    smem_clear( sdriver_checked, sizeof( sdriver_checked ) );

    while( 1 )
    {
	int prev_buf_size = d->buffer_size;
	int sound_err = 0;
	switch( ss->driver )
	{
	    case SDRIVER_MMSOUND:
		sound_err = device_sound_init_mmsound( ss, false );
		break;
#ifdef DSOUND
	    case SDRIVER_DSOUND:
		sound_err = device_sound_init_dsound( ss, false );
		break;
#endif
#ifdef ASIO
	    case SDRIVER_ASIO:
		sound_err = device_sound_init_asio( ss );
		break;
#endif
	}
	if( sound_err == 0 ) 
	{
	    //Driver found and initialized:
	    break;
	}
	//Driver init error:
	slog( "Can't open sound stream (driver: %s; error: %d)\n", g_sdriver_names[ ss->driver ], sound_err );
	d->buffer_size = prev_buf_size;
	if( ss->driver < NUMBER_OF_SDRIVERS ) sdriver_checked[ ss->driver ] = true;
	ss->driver = 0;
	for( ss->driver = 0; ss->driver < NUMBER_OF_SDRIVERS; ss->driver++ )
	{
	    if( sdriver_checked[ ss->driver ] == 0 ) 
	    {
		slog( "Switching to %s...\n", g_sdriver_names[ ss->driver ] );
		break;
	    }
	}
	if( ss->driver == NUMBER_OF_SDRIVERS )
	{
	    //No sound driver found:
	    REMOVE_DEVICE_SPECIFIC_DATA( ss );
	    return 1;
	}
    }
    
    ss->out_latency = d->buffer_size;
    ss->out_latency2 = d->buffer_size;
    
    return 0;
}

int device_sound_deinit( sundog_sound* ss )
{
    device_sound* d = (device_sound*)ss->device_specific;
    
    device_sound_input( ss, false );
    switch( ss->driver )
    {
	case SDRIVER_MMSOUND:
	    if( d->mm )
	    {
		device_sound_mmsound* mm = d->mm;
		if( mm->waveOutThread )
		{
		    PostThreadMessage( mm->waveOutThreadID, WOM_CLOSE, 0, 0 );
            	    STIME_WAIT_FOR( mm->waveOutThreadClosed, SUNDOG_SOUND_DEFAULT_TIMEOUT_MS, 20, "MMSOUND: Timeout" );
            	    CloseHandle( mm->waveOutThread ); mm->waveOutThread = 0;
            	    slog( "MMSOUND: CloseHandle (soundThread) ok\n" );
		}
		
		MMRESULT mres;
		mres = waveOutReset( mm->waveOutStream );
		if( mres != MMSYSERR_NOERROR )
		{
		    slog( "MMSOUND ERROR in waveOutReset (%d)\n", mres );
		}
		
		for( int b = 0; b < MM_MAX_BUFFERS; b++ )
		{
		    mres = waveOutUnprepareHeader( mm->waveOutStream, &mm->outBuffersHdr[ b ], sizeof( WAVEHDR ) );
		    if( mres != MMSYSERR_NOERROR )
		    {
			slog( "MMSOUND ERROR: Can't unprepare waveout header %d (%d)\n", b, mres );
		    }
		    free( mm->outBuffersHdr[ b ].lpData );
		}
		
		mres = waveOutClose( mm->waveOutStream );
		if( mres != MMSYSERR_NOERROR ) 
		    slog( "MMSOUND ERROR in waveOutClose: %d\n", mres );
		else
		    slog( "MMSOUND: waveOutClose ok\n" );
		
		smem_free( d->mm );
		d->mm = 0;
	    }
	    break;
#ifdef DSOUND
	case SDRIVER_DSOUND:
	    if( d->ds )
	    {
		device_sound_dsound* ds = d->ds;
		if( ds->lpds )
		{
		    ds->output_exit_request = 1;
		    int step = 20; //ms
            	    int timeout = SUNDOG_SOUND_DEFAULT_TIMEOUT_MS;
            	    int t = 0;
            	    while( ds->output_exit_request )
            	    {
                	Sleep( step ); //Waiting for thread stop
                	t += step;
                	if( t > timeout )
                	{
                    	    slog( "DSOUND: Timeout\n" );
                    	    break;
                	}
            	    }
		    CloseHandle( ds->output_thread ); ds->output_thread = 0;
		    ds->output_exit_request = 0;
		
		    if( ds->lpdsNotify ) ds->lpdsNotify->Release(); ds->lpdsNotify = 0;
		    if( ds->buf ) ds->buf->Release(); ds->buf = 0;
		    if( ds->buf_primary ) ds->buf_primary->Release(); ds->buf_primary = 0;
		    if( ds->input_buf ) ds->input_buf->Release(); ds->input_buf = 0;
		    if( ds->lpds ) ds->lpds->Release(); ds->lpds = 0;
		    if( ds->lpdsCapture ) ds->lpdsCapture->Release(); ds->lpdsCapture = 0;
		}
		smem_free( d->ds );
		d->ds = 0;
	    }
	    break;
#endif
#ifdef ASIO
	case SDRIVER_ASIO:
	    if( g_iasio )
	    {
		slog( "ASIO stop...\n" );
		iasio_stop();
		slog( "ASIO disposeBuffers...\n" );
		iasio_disposeBuffers();
		slog( "ASIO release...\n" );
		iasio_release();
		slog( "ASIO closed.\n" );
		g_iasio = 0;
		g_asio_ss = 0;
	    }
	    break;
#endif
    }

    smem_free( d->output_buffer );
    d->output_buffer = 0;
    remove_input_buffers( ss );
    
    REMOVE_DEVICE_SPECIFIC_DATA( ss );
    
    return 0;
}

void device_sound_input( sundog_sound* ss, bool enable )
{
    device_sound* d = (device_sound*)ss->device_specific;
    
    if( enable )
    {
	//Enable:
	set_input_defaults( ss );
        create_input_buffers( ss, d->buffer_size );
        int sound_err = 0;
        switch( ss->driver )
        {
            case SDRIVER_MMSOUND:
        	sound_err = device_sound_init_mmsound( ss, true );
        	break;
#ifdef DSOUND
            case SDRIVER_DSOUND:
        	sound_err = device_sound_init_dsound( ss, true );
        	break;
#endif
#ifdef ASIO
            case SDRIVER_ASIO:
    		Sleep( 1 );
        	break;
#endif
        }
        if( sound_err == 0 )
        {
            d->input_enabled = true;
        }
        else
        {
	    slog( "Can't open sound input (driver: %s; error: %d)\n", g_sdriver_names[ ss->driver ], sound_err );
        }
    }
    else
    {
	//Disable:
	if( d->input_enabled == false ) return;
        switch( ss->driver )
        {
            case SDRIVER_MMSOUND:
        	if( d->mm )
        	{
		    device_sound_mmsound* mm = d->mm;
		    if( mm->waveInThread )
		    {
			PostThreadMessage( mm->waveInThreadID, WOM_CLOSE, 0, 0 );
            		STIME_WAIT_FOR( mm->waveInThreadClosed, SUNDOG_SOUND_DEFAULT_TIMEOUT_MS, 20, "MMSOUND INPUT: Timeout" );
            		CloseHandle( mm->waveInThread ); mm->waveInThread = 0;
            		slog( "MMSOUND INPUT: CloseHandle (soundThread) ok\n" );
            	    }

            	    MMRESULT mres;
            	    mres = waveInReset( mm->waveInStream );
            	    if( mres != MMSYSERR_NOERROR )
            	    {
                	slog( "MMSOUND INPUT ERROR in waveInReset (%d)\n", mres );
            	    }

            	    for( int b = 0; b < MM_MAX_BUFFERS; b++ )
            	    {
                	mres = waveInUnprepareHeader( mm->waveInStream, &mm->inBuffersHdr[ b ], sizeof( WAVEHDR ) );
                	if( mres != MMSYSERR_NOERROR )
                    	    slog( "MMSOUND INPUT ERROR: Can't unprepare header %d (%d)\n", b, mres );
                	free( mm->inBuffersHdr[ b ].lpData );
            	    }

        	    mres = waveInClose( mm->waveInStream );
        	    if( mres != MMSYSERR_NOERROR )
            	        slog( "MMSOUND INPUT ERROR in waveInClose: %d\n", mres );
            	    else
                	slog( "MMSOUND INPUT: waveInClose ok\n" );
            	    
            	    mm->waveInStream = 0;
            	    d->input_enabled = false;
        	}
        	break;
#ifdef DSOUND
            case SDRIVER_DSOUND:
		if( d->ds )
		{
		    device_sound_dsound* ds = d->ds;
		    ds->input_exit_request = 1;
		    int step = 20; //ms
            	    int timeout = SUNDOG_SOUND_DEFAULT_TIMEOUT_MS;
            	    int t = 0;
            	    while( ds->input_exit_request )
            	    {
                	Sleep( step ); //Waiting for thread stop
                	t += step;
                	if( t > timeout )
                	{
                    	    slog( "DSOUND INPUT: Timeout\n" );
                    	    break;
                	}
            	    }
		    CloseHandle( ds->input_thread ); ds->input_thread = 0;
		    ds->input_exit_request = 0;
		    
		    if( ds->input_buf ) ds->input_buf->Release(); ds->input_buf = 0;
		    if( ds->lpdsCapture ) ds->lpdsCapture->Release(); ds->lpdsCapture = 0;
            	    d->input_enabled = false;
		}
        	break;
#endif
#ifdef ASIO
            case SDRIVER_ASIO:
                d->input_enabled = false;
        	break;
#endif
        }
    }
}

int device_sound_get_devices( const char* driver, char*** names, char*** infos, bool input )
{
    int rv = 0;

    if( driver == 0 ) return 0;

    int drv_num = -1;
    for( int drv = 0; drv < NUMBER_OF_SDRIVERS; drv++ )
    {
        if( smem_strcmp( g_sdriver_names[ drv ], driver ) == 0 )
        {
            drv_num = drv;
            break;
        }
        if( smem_strcmp( g_sdriver_infos[ drv ], driver ) == 0 )
        {
            drv_num = drv;
            break;
        }
    }
    if( drv_num == -1 ) return 0;

    *names = 0;
    *infos = 0;
    char* ts = (char*)smem_new( 2048 );
    ts[ 0 ] = 0;
    
    switch( drv_num )
    {
	case SDRIVER_MMSOUND:
	    {
		int devices;
		if( input )
		    devices = waveInGetNumDevs();
		else
		    devices = waveOutGetNumDevs();
	        if( devices == 0 ) break;
#ifdef OS_WINCE
		size_t ts2_size = 256;
		char* ts2 = (char*)smem_new( ts2_size );
		ts2[ 0 ] = 0;
#endif
		for( int d = 0; d < devices; d++ )
		{
                    sprintf( ts, "%d", d );
                    smem_objlist_add( (void***)names, ts, true, rv );
		    if( input )
		    {
    			WAVEINCAPS deviceCaps;
    			smem_clear( &deviceCaps, sizeof( deviceCaps ) );
    			waveInGetDevCaps( d, &deviceCaps, sizeof( deviceCaps ) );
#ifdef OS_WINCE
			utf16_to_utf8( ts2, ts2_size, (const uint16_t*)deviceCaps.szPname );
    			sprintf( ts, "%s (%d)", ts2, d );
#else
    			sprintf( ts, "%s (%d)", deviceCaps.szPname, d );
#endif
		    }
		    else
		    {
    			WAVEOUTCAPS deviceCaps;
    			smem_clear( &deviceCaps, sizeof( deviceCaps ) );
    			waveOutGetDevCaps( d, &deviceCaps, sizeof( deviceCaps ) );
#ifdef OS_WINCE
			utf16_to_utf8( ts2, ts2_size, (const uint16_t*)deviceCaps.szPname );
    			sprintf( ts, "%s (%d)", ts2, d );
#else
    			sprintf( ts, "%s (%d)", deviceCaps.szPname, d );
#endif
    		    }
    		    smem_objlist_add( (void***)infos, ts, true, rv );
    		    rv++;
    		}
#ifdef OS_WINCE
		smem_free( ts2 );
#endif
	    }
	    break;
#ifdef DSOUND
	case SDRIVER_DSOUND:
	    {
	        device_sound_dsound_enum e;
	        smem_clear_struct( e );
		e.infos = infos;
		if( input )
		{
		    DirectSoundCaptureEnumerate( DSEnumCallback, &e );
		    rv = e.guids_num;
		}
		else
		{
		    DirectSoundEnumerate( DSEnumCallback, &e );
		    rv = e.guids_num;
		}
		if( rv == 0 ) break;
		for( int d = 0; d < rv; d++ )
		{
    		    sprintf( ts, "%d", d );
    		    smem_objlist_add( (void***)names, ts, true, d );
		}
	    }
	    break;
#endif
#ifdef ASIO
        case SDRIVER_ASIO:
    	    if( !input )
	    {
		int d = 0;
		HKEY hk = NULL;
		CHAR keyname[ 128 ];
		int err = RegOpenKey( HKEY_LOCAL_MACHINE, "software\\asio", &hk );
		while( ( err == ERROR_SUCCESS ) && ( rv < ASIO_DEVS ) )
		{
    		    err = RegEnumKey( hk, d, (LPTSTR)keyname, 128 );
    		    d++;
    		    if( err == ERROR_SUCCESS )
    		    {
                        sprintf( ts, "%d", rv );
                        smem_objlist_add( (void***)names, ts, true, rv );
                        sprintf( ts, "%s (%d)", keyname, rv );
                        smem_objlist_add( (void***)infos, ts, true, rv );
        		rv++;
    		    }
		}
	    }
    	    break;
#endif
    }
    
    smem_free( ts );
    
    return rv;
}
