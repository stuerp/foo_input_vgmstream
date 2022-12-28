
/** $VER: StreamFile.h (2022.12.28) P. Stuer **/

#pragma once

#include <foobar2000/SDK/foobar2000.h>

extern "C"
{
#include <src/vgmstream.h>
}

STREAMFILE * open_foo_streamfile(const char * const filePath, abort_callback * abortHandler, t_filestats * stats);
