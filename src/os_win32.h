#pragma once
#include <windows.h>

struct Software_Render_Buffer
{
    i32 width;
    i32 height;
    BITMAPINFO info;
    u32 *data;
};

typedef struct OS_W32_Window OS_W32_Window;
struct OS_W32_Window
{
    OS_W32_Window *next;
    OS_W32_Window *prev;
    HWND hwnd;
    HDC hdc;
};

// --- win32 ---
internal WIN32_FIND_DATAA win32_get_file_attributes(u8* filename);
internal u64 win32_filetime_to_u64(FILETIME time);
