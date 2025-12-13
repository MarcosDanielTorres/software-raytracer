#include <windows.h>
#include "win32_main.h"

#include "base_arena.h"
#include "base_string.h"

#include "base_arena.c"
#include "base_string.c"
#include "timer.c"

global Arena *arena;

typedef struct Win32LoadedDLL Win32LoadedDLL;
struct Win32LoadedDLL
{
    HANDLE handle;
    u8* name;
    u8* name_temp;
    u8* lock_filename;
    Update_And_Render *app_update_and_render;
    FILETIME last_write_time;
    b32 is_valid;
};

static b32 g_running = 1;

typedef struct OS_W32_Window OS_W32_Window;
struct OS_W32_Window
{
    OS_W32_Window *next;
    OS_W32_Window *prev;
    HWND hwnd;
    HDC hdc;
};

global Software_Render_Buffer *buffer;
static HWND hwnd;
static HDC hdc;

LRESULT MainWndProc(
    HWND hwnd,        // handle to window
    UINT uMsg,        // message identifier
    WPARAM wParam,    // first message parameter
    LPARAM lParam)    // second message parameter
{
    switch (uMsg) 
    { 
        case WM_SIZE: 
        {
            RECT rect = { 0 };
            GetClientRect(hwnd, &rect);
            //StretchDIBits(hdc, 0, 0, rect.right - rect.left, rect.top - rect.bottom, 0, 0, horz_res, vert_res, (void*) buffer, &bitmap_info, DIB_RGB_COLORS, SRCCOPY);
        } break;
 
        case WM_DESTROY: 
        {
            g_running = 0;
        } break;
 
        default: 
            return DefWindowProc(hwnd, uMsg, wParam, lParam); 
    } 
    return 0; 
}

void win32_process_pending_msgs() 
{
    MSG Message;
    while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&Message);
        switch(Message.message)
        {
            case WM_QUIT:
            {
                g_running = 0;
            } break;
            case WM_MOUSEMOVE:
            {
                // TODO move to obsidian!
                // alright so the position return by `GetCursorPos` its the pixel position of the mouse relative to the OS. By passing it to `ScreenToClient` i can get it back
                // into the program's window coordinate frame. Instead of using these two functions one after the other I could just call LOWORD and HIWORD on message.lParam to get the same
                // result!
                POINT p = {0};
                GetCursorPos(&p);
                printf("mouse coord: %.2f, %.2f\n", (f32)p.x, (f32)p.y);
                ScreenToClient(hwnd, &p);
                printf("mouse coord in client: %.2f, %.2f\n", (f32)p.x, (f32)p.y);
                i32 xPos = LOWORD(Message.lParam);
                i32 yPos = HIWORD(Message.lParam);
                printf("MOUSE MOVE: x: %d, y: %d\n", xPos, yPos);

            } break;
            default:
            {
                DispatchMessageA(&Message);
            } break;
        }
    }
}

internal WIN32_FIND_DATAA
win32_get_file_attributes(u8* filename)
{
    WIN32_FIND_DATAA result = {0};
    GetFileAttributesEx(filename, GetFileExInfoStandard, &result);
    return result;
}

internal void
win32_load_dll(Win32LoadedDLL *dll)
{
    WIN32_FIND_DATAA ignored;
    {
        WIN32_FIND_DATAA file_data;
        if(GetFileAttributesEx(dll->name, GetFileExInfoStandard, &file_data))
        {
            dll->last_write_time = file_data.ftLastWriteTime;
        }
        for(u32 attempt_index = 0; attempt_index < 10000; attempt_index++)
        {

            if(CopyFile(dll->name, dll->name_temp, FALSE))
            {
                break;
            }
            else
            {
                printf("Failed to copy file, retrying...\n");
            }
        }

        dll->handle = LoadLibraryExA(dll->name_temp, 0, 0);
        if(dll->handle)
        {
            dll->app_update_and_render = (Update_And_Render*) GetProcAddress(dll->handle, "update_and_render");
            dll->is_valid = dll->app_update_and_render != 0;
        }
        if (!dll->handle || !dll->app_update_and_render)
        {
            printf("Defaulting to stub...\n");
            dll->app_update_and_render = update_and_render_stub;
        }
    }
}

internal void
win32_unload_dll(Win32LoadedDLL *dll)
{
    if (dll->handle)
        FreeLibrary(dll->handle);
    dll->handle = 0;
    dll->app_update_and_render = 0;
    dll->is_valid = 0;
}

int main ()
{
    timer_init();
    arena = arena_alloc(mb(2));
    g_transient_arena = arena_alloc(mb(2));
    i32 window_width = 1920;
    i32 window_height = 1080;
    WNDCLASSEXW wnd_class = 
    {
        .cbSize = sizeof(WNDCLASSEXW),
        .style = CS_HREDRAW | CS_VREDRAW,
        .hCursor = LoadCursor(0, IDC_ARROW),
        .lpfnWndProc = MainWndProc,
        .lpszClassName = L"graphical-window"
    };
    RegisterClassExW(&wnd_class);
    RECT window_rect = {0, 0, window_width, window_height};
    u32 screen_width = GetSystemMetrics(SM_CXSCREEN);
    u32 screen_height = GetSystemMetrics(SM_CYSCREEN);
    #if 0
    SetRect(&window_rect,
            (screen_width / 2) - (window_width / 2),
            (screen_height / 2) - (window_height / 2),
            (screen_width / 2) + (window_width / 2),
            (screen_height / 2) + (window_height / 2)); 
    AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0);
    #endif
    if(window_rect.left < 0)
    {
        window_rect.left = CW_USEDEFAULT;
    }

    if(window_rect.top < 0)
    {
        window_rect.top = CW_USEDEFAULT;
    }
    hwnd = CreateWindowExW(WS_EX_APPWINDOW, L"graphical-window", L"My window!", WS_OVERLAPPEDWINDOW, window_rect.left, window_rect.top, window_rect.right - window_rect.left, window_rect.bottom - window_rect.top, 0, 0, 0, 0);
    if(!hwnd)
    {
        printf("error code: %d\n", GetLastError());
        return 0;
    }
    ShowWindow(hwnd, SW_SHOW);
    hdc = GetDC(hwnd);
    OS_W32_Window window = {0};
    window.hwnd = hwnd;


    buffer = VirtualAlloc(0, sizeof(Software_Render_Buffer), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    buffer->width = 800;
    buffer->height = 600;
    buffer->data = VirtualAlloc(0, sizeof(u32) * buffer->width * buffer->height, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    
    buffer->info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	buffer->info.bmiHeader.biWidth = buffer->width;
	buffer->info.bmiHeader.biHeight = -buffer->height;
	buffer->info.bmiHeader.biPlanes = 1;
	buffer->info.bmiHeader.biBitCount = sizeof(u32) * 8;
	buffer->info.bmiHeader.biCompression = BI_RGB;
	buffer->info.bmiHeader.biSizeImage = 0;
	buffer->info.bmiHeader.biXPelsPerMeter = 0;
	buffer->info.bmiHeader.biYPelsPerMeter = 0;
	buffer->info.bmiHeader.biClrUsed = 0;
	buffer->info.bmiHeader.biClrImportant = 0;


    // TODO move to obisidan
    // it seems `GetDIBits` it's not needed at all!
    // Only `StretchDIBits`, `GetDC` (i guess), and `CreateCompatibleBitmap`

    String8 absolute_path;
    {
        u8 buf[MAX_PATH];
        u32 len = GetModuleFileNameA(0, buf, MAX_PATH);
        absolute_path = str8(buf, len);
        absolute_path = str8_strip_last(absolute_path, str8_literal("\\"));
    }


    u8* dll_name = cstring_from_str8(arena, str8_concat(arena, absolute_path, str8_literal("\\game")));

    Win32LoadedDLL game_dll = {0};
    game_dll.name = cstring_concat(arena, dll_name, ".dll");
    game_dll.name_temp = cstring_concat(arena, dll_name, "-temp.dll");
    game_dll.lock_filename = cstring_concat(arena, dll_name, "-lock.temp");

    win32_load_dll(&game_dll);

    while(g_running)
    {
        win32_process_pending_msgs();

        WIN32_FIND_DATAA file_data = win32_get_file_attributes(game_dll.name);
        if(CompareFileTime(&file_data.ftLastWriteTime, &game_dll.last_write_time) != 0)
        {
            WIN32_FIND_DATAA ignored;
            if(!GetFileAttributesEx(game_dll.lock_filename, GetFileExInfoStandard, &ignored))
            {
                LONGLONG start_time = timer_get_os_time();
                win32_unload_dll(&game_dll);
                win32_load_dll(&game_dll);
                LONGLONG end_time = timer_get_os_time();
                printf("[Hot-Reloading] %s: took: %.2f ms\n", game_dll.name, timer_os_time_to_ms(end_time - start_time));
            }
        }
        game_dll.app_update_and_render(buffer);
        StretchDIBits(hdc, 0, 0, 800, 600, 0, 0, buffer->width, buffer->height, (void*) buffer->data, &buffer->info, DIB_RGB_COLORS, SRCCOPY);
    }
}