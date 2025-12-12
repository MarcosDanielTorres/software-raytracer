#include <windows.h>
#include "win32_main.h"


static b32 g_running = 1;

typedef struct OS_Handle OS_Handle;
struct OS_Handle
{
    u64 handle;
};

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


int main ()
{
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
    OS_Handle os_handle = {(u64)&window};
    OS_W32_Window* window_from_handle = (OS_W32_Window*)os_handle.handle;


    ///// fuck off

    i32 numcolors = GetDeviceCaps(hdc, NUMCOLORS);
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
    
    wchar_t engine_absolute_path[MAX_PATH];
    u32 len = GetModuleFileNameW(0, engine_absolute_path, MAX_PATH);

    HMODULE dll_handle = LoadLibraryExW(L"E:\\software-raytracer\\build\\game.dll", 0, 0);
    Game_Update_And_Render* app_update_and_render = (Game_Update_And_Render*) GetProcAddress(dll_handle, "game_update_and_render");

    while(g_running)
    {
        win32_process_pending_msgs();
        //game_update_and_render(buffer);
        app_update_and_render(buffer);
        StretchDIBits(hdc, 0, 0, 800, 600, 0, 0, buffer->width, buffer->height, (void*) buffer->data, &buffer->info, DIB_RGB_COLORS, SRCCOPY);
    }
}