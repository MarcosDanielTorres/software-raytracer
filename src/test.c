#include "base_core.h"
#include "base_arena.h"
#include "base_os.h"
#include "base_string.h"
#include "os_win32.h"
#include "timer.h"
#include "base_arena.c"
#include "os_win32.c"
#include "base_math.h"
#include "timer.c"
#include "obj.h"
#include <assert.h>
#include <emmintrin.h>
#include <immintrin.h>
#include <xmmintrin.h>
#include "base_string.c"
#include "main.h"
//#include "font_loader.h"
//#include "font_loader.c"
#undef internal
#include <ft2build.h>
#include FT_FREETYPE_H
#define internal static


UPDATE_AND_RENDER(update_and_render)
{
    u32 red = 0xffff0000;
    clear_screen(buffer, red);
}