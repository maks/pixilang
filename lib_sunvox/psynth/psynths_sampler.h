#pragma once

typedef int32_t SMPPTR; //must be signed; byte number OR frame number; max sample size = 2GB;

int sampler_load( const char* filename, sfs_file f, int mod_num, psynth_net* net, int sample_num, bool load_unsupported_files_as_raw );

