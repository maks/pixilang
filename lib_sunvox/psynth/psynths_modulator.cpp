/*
This file is part of the SunVox library.
Copyright (C) 2007 - 2023 Alexander Zolotov <nightradio@gmail.com>
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

   
#include "psynth_net.h"
 
#define MODULE_DATA psynth_modulator_data
#define MODULE_HANDLER psynth_modulator
#define MODULE_INPUTS 2
#define MODULE_OUTPUTS 2
 
#define INTERP_PREC 15
#define INTERP_MASK ( ( 1 << INTERP_PREC ) - 1 )

#define INTERP( val1, val2, p ) ( ( val1 * (( 1 << INTERP_PREC ) - ((p)&INTERP_MASK)) + val2 * ((p)&INTERP_MASK) ) / ( 1 << INTERP_PREC ) )
 struct MODULE_DATA {  PS_CTYPE ctl_volume; PS_CTYPE ctl_type; PS_CTYPE ctl_mono;  int buf_size; int buf_real_size; PS_STYPE* buf[ MODULE_OUTPUTS ]; uint buf_ptr; int empty_frames_counter; };  PS_RETTYPE MODULE_HANDLER( 
 PSYNTH_MODULE_HANDLER_PARAMETERS )
{
 psynth_module* mod;
 MODULE_DATA* data; if( mod_num >= 0 ) { mod = &pnet->mods[ mod_num ]; data = (MODULE_DATA*)mod->data_ptr; } PS_RETTYPE retval = 0;  switch( event->command ) { case PS_CMD_GET_DATA_SIZE: retval = sizeof( MODULE_DATA ); break;  case PS_CMD_GET_NAME: retval = (PS_RETTYPE)"Modulator"; break;  case PS_CMD_GET_INFO: { const char* lang = slocale_get_lang(); while( 1 ) { if( smem_strstr( lang, "ru_" ) ) { retval = (PS_RETTYPE)"Модуль амплитудной или фазовой модуляции.\nПервый подключенный на вход сигнал считается несущим (Carrier). Все последующие подключенные на вход - модулирующие (Modulator), которые влияют на громкость или частоту первого.";
 break; }
 retval = (PS_RETTYPE)"Amplitude or Phase modulator.\nFirst input = Carrier. Other inputs = Modulators."; break; } } break; 
 case PS_CMD_GET_COLOR: retval = (PS_RETTYPE)"#FF7FD1"; break;  case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break; case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;  case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT | PSYNTH_FLAG_DONT_FILL_INPUT; break;  case PS_CMD_INIT: psynth_resize_ctls_storage( mod_num, 3, pnet );
 psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 512, 256, 0, &data->ctl_volume, 256, 0, pnet ); psynth_register_ctl( mod_num, ps_get_string( STR_PS_MODULATION_TYPE ), ps_get_string( STR_PS_MODULATION_TYPES ), 0, 2, 0, 1, &data->ctl_type, -1, 0, pnet ); psynth_register_ctl( mod_num, ps_get_string( STR_PS_CHANNELS ), ps_get_string( STR_PS_STEREO_MONO ), 0, 1, 0, 1, &data->ctl_mono, -1, 0, pnet ); 
 data->buf_size = pnet->sampling_freq / 25; data->buf_real_size = round_to_power_of_two( data->buf_size + 4 ); 
 for( int i = 0; i < MODULE_OUTPUTS; i++ ) {
 data->buf[ i ] = (PS_STYPE*)smem_new( data->buf_real_size * sizeof( PS_STYPE ) ); smem_zero( data->buf[ i ] );
 } data->buf_ptr = 0;  data->empty_frames_counter = data->buf_real_size;  retval = 1; break;
 case PS_CMD_CLEAN: data->empty_frames_counter = data->buf_real_size; retval = 1; break; 
 case PS_CMD_RENDER_REPLACE: { PS_STYPE** inputs = mod->channels_in; PS_STYPE** outputs = mod->channels_out; int offset = mod->offset; int frames = mod->frames;  if( data->ctl_mono ) { if( psynth_get_number_of_outputs( mod ) != 1 ) { psynth_set_number_of_outputs( 1, mod_num, pnet );
 psynth_set_number_of_inputs( 1, mod_num, pnet ); } } else { if( psynth_get_number_of_outputs( mod ) != MODULE_OUTPUTS ) { psynth_set_number_of_outputs( MODULE_OUTPUTS, mod_num, pnet ); psynth_set_number_of_inputs( MODULE_INPUTS, mod_num, pnet );
 } } int outputs_num = psynth_get_number_of_outputs( mod );  bool no_input_signal = true;
 for( int ch = 0; ch < outputs_num; ch++ ) { if( mod->in_empty[ ch ] < offset + frames ) { no_input_signal = false; break; } }
  if( data->ctl_type < 1 ) {  if( data->ctl_volume == 0 ) no_input_signal = 1; if( no_input_signal )  break; } else {  if( no_input_signal )  { if( data->empty_frames_counter >= data->buf_real_size ) { 
 data->buf_ptr += frames; break;
 } data->empty_frames_counter += frames; } else { data->empty_frames_counter = 0; } }
  PS_STYPE* pm_carrier[ MODULE_OUTPUTS ];  for( int i = 0; i < mod->input_links_num; i++ ) { psynth_module* m = psynth_get_module( mod->input_links[ i ], pnet ); if( !m ) continue; int mcc = psynth_get_number_of_outputs( m ); if( mcc == 0 ) continue;
 if( !m->channels_out[ 0 ] ) continue;  if( data->ctl_type < 1 ) {
 
 if( !( m->flags & PSYNTH_FLAG_OUTPUT_IS_EMPTY ) )
 {
 PS_STYPE* in = NULL; PS_STYPE** outputs2 = m->channels_out;
 for( int ch = 0; ch < outputs_num; ch++ ) { if( outputs2[ ch ] && ch < mcc ) in = outputs2[ ch ] + offset; PS_STYPE* out = outputs[ ch ] + offset; if( retval == 0 ) { for( int p = 0; p < frames; p++ ) out[ p ] = in[ p ]; } else { for( int p = 0; p < frames; p++ )
 { PS_STYPE2 v = out[ p ];
 v *= in[ p ]; 
#ifndef PS_STYPE_FLOATINGPOINT
 v /= PS_STYPE_ONE; 
#endif
 out[ p ] = (PS_STYPE)v;
 } } } retval = 1;
 } } else {  PS_STYPE* in = NULL; PS_STYPE** outputs2 = m->channels_out; for( int ch = 0; ch < outputs_num; ch++ ) { if( outputs2[ ch ] && ch < mcc ) in = outputs2[ ch ] + offset; PS_STYPE* out = outputs[ ch ] + offset; if( retval == 0 ) { pm_carrier[ ch ] = in; } else
 {
 if( retval == 1 )
 {
 for( int p = 0; p < frames; p++ ) out[ p ] = in[ p ]; } else { for( int p = 0; p < frames; p++ )
 out[ p ] += in[ p ]; }
 } }
 retval++; } }   if( retval ) { if( data->ctl_type >= 1 )
 {
  if( retval == 1 ) {  for( int ch = 0; ch < outputs_num; ch++ )
 { PS_STYPE* in = pm_carrier[ ch ];
 PS_STYPE* out = outputs[ ch ] + offset; for( int p = 0; p < frames; p++ ) { out[ p ] = in[ p ]; } } } else {  uint buf_ptr = data->buf_ptr; uint buf_mask = data->buf_real_size - 1; for( int ch = 0; ch < outputs_num; ch++ )
 { PS_STYPE* in = pm_carrier[ ch ]; PS_STYPE* out = outputs[ ch ] + offset; PS_STYPE* cbuf = data->buf[ ch ];
 buf_ptr = data->buf_ptr; if( data->ctl_type == 1 )
 {
  for( int p = 0; p < frames; p++, buf_ptr++ ) { cbuf[ buf_ptr & buf_mask ] = in[ p ];  int v; PS_STYPE_TO_INT16( v, out[ p ] );
 v = ( v + 32768 ) * ( data->buf_size - 1 ); int interp = ( v & 0xFFFF ) >> 1; v >>= 16; PS_STYPE2 v1 = cbuf[ ( buf_ptr - v ) & buf_mask ]; PS_STYPE2 v2 = cbuf[ ( buf_ptr - ( v + 1 ) ) & buf_mask ]; PS_STYPE2 v3 = INTERP( v1, v2, interp ); out[ p ] = v3; } } else {  for( int p = 0; p < frames; p++, buf_ptr++ ) { cbuf[ buf_ptr & buf_mask ] = in[ p ]; 
 int v;
 PS_STYPE_TO_INT16( v, out[ p ] );
 if( v < 0 ) v = -v; v = v * ( data->buf_size - 1 ); int interp = v & 32767;
 v >>= 15;
 PS_STYPE2 v1 = cbuf[ ( buf_ptr - v ) & buf_mask ]; PS_STYPE2 v2 = cbuf[ ( buf_ptr - ( v + 1 ) ) & buf_mask ];
 PS_STYPE2 v3 = INTERP( v1, v2, interp ); out[ p ] = v3;
 } } } data->buf_ptr = buf_ptr; } retval = 1; }  if( data->ctl_volume != 256 ) { for( int ch = 0; ch < outputs_num; ch++ ) { PS_STYPE* out = outputs[ ch ] + offset;
#ifdef PS_STYPE_FLOATINGPOINT
 float v = (float)data->ctl_volume / 256.0F; for( int p = 0; p < frames; p++ ) out[ p ] *= v; 
#else
 for( int p = 0; p < frames; p++ ) {
 PS_STYPE2 v = out[ p ]; v *= data->ctl_volume; v /= 256; out[ p ] = (PS_STYPE)v; } 
#endif
 } }
 }
 if( retval ) retval = 1;
 if( data->ctl_volume == 0 ) retval = 2; } break;  case PS_CMD_CLOSE: for( int i = 0; i < MODULE_OUTPUTS; i++ ) { smem_free( data->buf[ i ] ); }
 retval = 1;
 break; 
 default: break; }  return retval; } 
