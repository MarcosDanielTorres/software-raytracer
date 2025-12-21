#include "base_core.h"
#include "base_os.h"
#include "os_win32.h"
#include "timer.h"
#include "os_win32.c"
#include "timer.c"

#include "base_arena.h"
#include "base_string.h"

#include "base_arena.c"
#include "base_string.c"

global Arena *arena;

static b32 g_running = 1;

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
            #if 0
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
            #endif
            default:
            {
                DispatchMessageA(&Message);
            } break;
        }
    }
}

int main ()
{
    timer_init();
    arena = arena_alloc(mb(2));
    g_transient_arena = arena_alloc(mb(2));
    // 4:3 resolutions
    // 640 x 480    (VGA)
    // 800 x 600    (SVGA)
    // 1024 x 768   (XGA)
    // 1280 x 960
    // 1400 x 1050
    // 1600 x 1200  (UXGA)
    // 1920 x 1440
    i32 window_width = 800; 
    i32 window_height = 600;
    window_width = 1280; 
    window_height = 960;
    window_width = 1400; 
    window_height = 1050;
    window_width = 1920; 
    window_height = 1080;

    i32 buffer_width = 640;
    i32 buffer_height = 480;
    //buffer_width = 1920;
    //buffer_height = 1080;
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

    hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"graphical-window",
        L"My window!",
        WS_OVERLAPPEDWINDOW,
        window_rect.left, window_rect.top, window_rect.right - window_rect.left, window_rect.bottom - window_rect.top,
        0, 0, 0, 0
    );

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
    buffer->width = buffer_width;
    buffer->height = buffer_height;
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

    OS_LoadedDLL game_dll = {0};
    game_dll.name = cstring_concat(arena, dll_name, ".dll");
    game_dll.name_temp = cstring_concat(arena, dll_name, "-temp.dll");
    game_dll.lock_filename = cstring_concat(arena, dll_name, "-lock.temp");

    os_load_dll(&game_dll);

    LONGLONG last_time = timer_get_os_time();
    LONGLONG total_time = 0;
    while(g_running)
    {
        LONGLONG now = timer_get_os_time();
        LONGLONG dt = now - last_time;
        total_time += dt;
        last_time = now;
        // ver sie lafrican head tarda 15 en debug tambien en tinyrenderr project
        //printf("%.2fms\n", timer_os_time_to_ms(dt));
        win32_process_pending_msgs();

        os_perform_hot_reload(&game_dll);
        game_dll.app_update_and_render(buffer, timer_os_time_to_ms(total_time), timer_os_time_to_sec(dt));
        //StretchDIBits(hdc, 0, 0, buffer->width, buffer->height, 0, 0, buffer->width, buffer->height, (void*) buffer->data, &buffer->info, DIB_RGB_COLORS, SRCCOPY);
        StretchDIBits(hdc, 0, 0, window_width, window_height, 0, 0, buffer->width, buffer->height, (void*) buffer->data, &buffer->info, DIB_RGB_COLORS, SRCCOPY);
    }
}