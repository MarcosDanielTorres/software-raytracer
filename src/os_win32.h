#pragma once
#include <windows.h>

struct Software_Render_Buffer
{
    i32 width;
    i32 height;
    BITMAPINFO info;
    u32 *data;
};

struct Software_Depth_Buffer
{
    i32 width;
    i32 height;
    BITMAPINFO info;
    f32 *data;
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
// static void captureCursor(_GLFWindow* window)
// https://github.com/glfw/glfw/blob/fdd14e65b1c29e4e6df875fb5669ec00d6793531/src/win32_window.c#L246
internal void win32_clip_cursor_to_client(HWND hwnd);
internal void win32_unclip_cursor(void);

/*
glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED) -> _glfwSetCursorModeWin32
updateCursorImage(window) for `GLFW_CURSOR_DISABLED` it calls SetCursor(0) but for some weird case with remote desktop
they create a blank cursor for this `SetCursor(_glfw.win32.blankCursor)`, but for my case SetCursor(0) should suffy:

https://github.com/glfw/glfw/blob/fdd14e65b1c29e4e6df875fb5669ec00d6793531/src/win32_window.c#L1334
```
    const GLFWimage cursorImage = { cursorWidth, cursorHeight, cursorPixels };
    _glfw.win32.blankCursor = createIcon(&cursorImage, 0, 0, FALSE);
```
*/
