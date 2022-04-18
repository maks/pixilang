#pragma once

#include "jpgd.h"

enum jd_pixel_format
{
    JD_GRAYSCALE,
    JD_RGB,
    JD_SUNDOG_COLOR,
};

void* load_jpeg( const char* filename, sfs_file f, int* width, int* height, int* num_components, jd_pixel_format pixel_format );
