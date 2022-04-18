#pragma once

void vplayer_set_filename( int mod_num, char* filename, psynth_net* pnet );
uint64_t vplayer_get_pcm_time( int mod_num, psynth_net* pnet );
uint64_t vplayer_get_total_pcm_time( int mod_num, psynth_net* pnet );
void vplayer_set_pcm_time( int mod_num, uint64_t t, psynth_net* pnet );
void vplayer_load_file( int mod_num, char* filename, psynth_net* pnet );