/*
This file is part of the SunVox library.
Copyright (C) 2007 - 2022 Alexander Zolotov <nightradio@gmail.com>
WarmPlace.ru

MINIFIED VERSION
(not yet intended to be modified)

License: (MIT)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.
*/

  
#include "psynth.h"

#define MODULE_DATA psynth_loop_data
#define MODULE_HANDLER psynth_loop
#define MODULE_INPUTS 2
#define MODULE_OUTPUTS 2
 struct MODULE_DATA {  PS_CTYPE ctl_volume;
 PS_CTYPE ctl_delay; PS_CTYPE ctl_stereo; PS_CTYPE ctl_repeats; PS_CTYPE ctl_mode;  int rep_counter; bool anticlick; 
 PS_STYPE2 anticlick_from[ MODULE_OUTPUTS ];  int buf_size;
 PS_STYPE* buf[ MODULE_OUTPUTS ]; int buf_ptr; bool buf_clean; bool empty; int tick_size; uint8_t ticks_per_line; };
 PS_RETTYPE MODULE_HANDLER( 
 PSYNTH_MODULE_HANDLER_PARAMETERS ) {
 psynth_module* mod; MODULE_DATA* data; if( mod_num >= 0 ) { mod = &pnet->mods[ mod_num ];
 data = (MODULE_DATA*)mod->data_ptr; } PS_RETTYPE retval = 0;

 switch( event->command ) { case PS_CMD_GET_DATA_SIZE: retval = sizeof( MODULE_DATA );
 break;  case PS_CMD_GET_NAME: retval = (PS_RETTYPE)"Loop";
 break;
 case PS_CMD_GET_INFO: { const char* lang = slocale_get_lang(); while( 1 ) { if( smem_strstr( lang, "ru_" ) ) { retval = (PS_RETTYPE)"Эффект многократного повторения звука.\nДля сброса нужно либо поменять контроллер \"Повторы\", либо послать модулю какую-нибудь ноту."; break;
 } retval = (PS_RETTYPE)"Tape loop - repeats a fragment of the incoming sound a specified number of times.\nTo reset the loop: either change the \"Repeats\" ctl, or send some note to this module."; break; } } break; 
 case PS_CMD_GET_COLOR:
 retval = (PS_RETTYPE)"#FF7FC5";
 break;  case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break; case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break; 
 case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT | PSYNTH_FLAG_GET_SPEED_CHANGES; break; case PS_CMD_GET_FLAGS2: retval = PSYNTH_FLAG2_NOTE_RECEIVER; break;  case PS_CMD_INIT: psynth_resize_ctls_storage( mod_num, 5, pnet ); psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 256, 256, 0, &data->ctl_volume, -1, 0, pnet ); psynth_register_ctl( mod_num, ps_get_string( STR_PS_DELAY ), ps_get_string( STR_PS_LINE128 ), 0, 256, 256, 0, &data->ctl_delay, -1, 0, pnet ); psynth_register_ctl( mod_num, ps_get_string( STR_PS_CHANNELS ), ps_get_string( STR_PS_MONO_STEREO ), 0, 1, 1, 1, &data->ctl_stereo, -1, 0, pnet ); psynth_register_ctl( mod_num, ps_get_string( STR_PS_REPEATS ), "", 0, 64, 0, 1, &data->ctl_repeats, -1, 0, pnet ); psynth_register_ctl( mod_num, ps_get_string( STR_PS_MODE ), ps_get_string( STR_PS_LOOP_MODES ), 0, 1, 0, 1, &data->ctl_mode, -1, 1, pnet ); data->buf_size = pnet->sampling_freq / 3; for( int i = 0; i < MODULE_OUTPUTS; i++ ) { data->buf[ i ] = (PS_STYPE*)smem_new( data->buf_size * sizeof( PS_STYPE ) ); smem_zero( data->buf[ i ] );
 }
 data->buf_ptr = 0;
 data->buf_clean = true; data->rep_counter = 0; data->anticlick = 0;
 for( int i = 0; i < MODULE_OUTPUTS; i++ ) data->anticlick_from[ i ] = 0; data->empty = 1; retval = 1;
 break;  case PS_CMD_CLEAN: if( data->buf_clean == false ) { for( int i = 0; i < MODULE_OUTPUTS; i++ ) { smem_zero( data->buf[ i ] ); } data->buf_clean = true; } data->buf_ptr = 0;
 data->rep_counter = 0; data->anticlick = 0; for( int i = 0; i < MODULE_OUTPUTS; i++ ) data->anticlick_from[ i ] = 0; data->empty = 1; retval = 1;
 break; 
 case PS_CMD_RENDER_REPLACE: {
 PS_STYPE** inputs = mod->channels_in; PS_STYPE** outputs = mod->channels_out;
 int offset = mod->offset; int frames = mod->frames;  if( data->ctl_stereo ) { if( psynth_get_number_of_outputs( mod ) != MODULE_OUTPUTS ) { psynth_set_number_of_outputs( MODULE_OUTPUTS, mod_num, pnet );
 psynth_set_number_of_inputs( MODULE_INPUTS, mod_num, pnet ); } } else { if( psynth_get_number_of_outputs( mod ) != 1 ) { psynth_set_number_of_outputs( 1, mod_num, pnet ); psynth_set_number_of_inputs( 1, mod_num, pnet ); } }  int outputs_num = psynth_get_number_of_outputs( mod );  for( int ch = 0; ch < outputs_num; ch++ )
 {
 if( mod->in_empty[ ch ] < offset + frames ) { data->empty = 0;
 break; }
 } if( data->empty ) break;  data->buf_clean = false;  int line_size = ( data->tick_size * data->ticks_per_line ) / 256; int buf_size = ( data->ctl_delay * line_size ) / 128; if( buf_size > data->buf_size ) buf_size = data->buf_size; if( buf_size == 0 ) buf_size = 1;  if( data->buf_ptr >= buf_size )  { data->buf_ptr = data->buf_ptr % buf_size; } 
 int rep_counter = 0; bool anticlick = 0;
 int buf_ptr = 0;
 PS_STYPE2 volume = PS_NORM_STYPE( data->ctl_volume, 256 );  int anticlick_len = pnet->sampling_freq / 1000; if( anticlick_len > buf_size / 2 ) anticlick_len = buf_size / 2; 
 for( int ch = 0; ch < outputs_num; ch++ ) { PS_STYPE* in = inputs[ ch ] + offset; PS_STYPE* out = outputs[ ch ] + offset; PS_STYPE* cbuf = data->buf[ ch ];
 buf_ptr = data->buf_ptr; rep_counter = data->rep_counter;
 anticlick = data->anticlick;  int i = 0; while( i < frames )
 { int size2 = buf_size - buf_ptr; if( size2 > frames - i ) size2 = frames - i; int i_end = i + size2;
 if( rep_counter == 0 ) {  if( anticlick ) {
 while( i < i_end ) {
 if( buf_ptr < anticlick_len ) { int c = ( buf_ptr << 8 ) / anticlick_len; out[ i ] = ( in[ i ] * c ) / 256 + ( data->anticlick_from[ ch ] * (256-c) ) / 256; } else 
 { out[ i ] = in[ i ]; } cbuf[ buf_ptr ] = in[ i ]; buf_ptr++;
 i++; } } else  { while( i < i_end ) { out[ i ] = in[ i ]; cbuf[ buf_ptr ] = in[ i ]; buf_ptr++; i++; } } } else { 
 switch( data->ctl_mode ) { case 0: 
 {
 PS_STYPE2 interp_from = cbuf[ buf_size - 1 ]; while( i < i_end ) { if( buf_ptr < anticlick_len ) { int c = ( buf_ptr << 8 ) / anticlick_len; PS_STYPE2 v = ( (PS_STYPE2)cbuf[ buf_ptr ] * c ) / 256 + ( interp_from * (256-c) ) / 256; out[ i ] = v; } else 
 { out[ i ] = cbuf[ buf_ptr ]; } buf_ptr++; i++; } } break; case 1:
 
 if( rep_counter & 1 ) {  while( i < i_end )
 { out[ i ] = cbuf[ buf_size - 1 - buf_ptr ];
 buf_ptr++; i++; } } else
 {
  while( i < i_end ) { out[ i ] = cbuf[ buf_ptr ]; buf_ptr++;
 i++; } } break; }
 } if( buf_ptr >= buf_size )  { rep_counter++;
 anticlick = 0; if( rep_counter > data->ctl_repeats ) { if( rep_counter > 1 ) { if( data->ctl_mode == 1 && ( ( rep_counter & 1 ) == 0 ) ) data->anticlick_from[ ch ] = cbuf[ 0 ];
 else data->anticlick_from[ ch ] = cbuf[ buf_size - 1 ]; anticlick = 1; }
 rep_counter = 0;
 } buf_ptr = 0; } } if( data->ctl_volume != 256 ) { for( i = 0; i < frames; i++ ) { PS_STYPE2 v = out[ i ]; v = PS_NORM_STYPE_MUL( v, volume, 256 ); out[ i ] = v; } }
 } data->buf_ptr = buf_ptr;
 data->rep_counter = rep_counter; data->anticlick = anticlick;

 retval = 1; if( data->ctl_volume == 0 ) retval = 2; } break;  case PS_CMD_SET_LOCAL_CONTROLLER: case PS_CMD_SET_GLOBAL_CONTROLLER: if( event->controller.ctl_num != 3 ) break;
  case PS_CMD_NOTE_ON:  data->rep_counter = 0; data->buf_ptr = 0; break; 
 case PS_CMD_SPEED_CHANGED:
 data->tick_size = event->speed.tick_size; data->ticks_per_line = event->speed.ticks_per_line; retval = 1; break; 
 case PS_CMD_CLOSE:
 for( int i = 0; i < MODULE_OUTPUTS; i++ ) { smem_free( data->buf[ i ] ); } retval = 1; break;
 default: break; }  return retval;
} 
