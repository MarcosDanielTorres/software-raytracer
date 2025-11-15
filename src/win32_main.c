#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <windows.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef uint32_t b32;

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

static i32 horz_res = 800;
static i32 vert_res = 600;
static HWND hwnd;
static HDC hdc;
static u32 *buffer;
static BITMAPINFO bitmap_info;

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

static void draw_rectangle(u32 *buffer, f32 x, f32 y, f32 width, f32 height, u32 color)
{
	u32 x_min = (u32)roundf(x);
	u32 x_max = x_min + (u32)roundf(width);
	u32 y_min = (u32)roundf(y);
	u32 y_max = y_min + (u32)roundf(height);
	u32 *ptr = buffer + y_min * horz_res + x_min;
	for(u32 y = y_min; y < y_max; y++)
	{
		u32* row = ptr;
		for(u32 x = x_min; x < x_max; x++)
		{
			*row++ = color;
		}
		ptr += horz_res;
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
    HBITMAP bitmap_handle = CreateCompatibleBitmap(hdc, horz_res, vert_res);
    buffer = VirtualAlloc(0, sizeof(u32) * horz_res * vert_res, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    
    bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmap_info.bmiHeader.biWidth = horz_res;
	bitmap_info.bmiHeader.biHeight = -vert_res;
	bitmap_info.bmiHeader.biPlanes = 1;
	bitmap_info.bmiHeader.biBitCount = sizeof(u32) * 8;
	bitmap_info.bmiHeader.biCompression = BI_RGB;
	bitmap_info.bmiHeader.biSizeImage = 0;
	bitmap_info.bmiHeader.biXPelsPerMeter = 0;
	bitmap_info.bmiHeader.biYPelsPerMeter = 0;
	bitmap_info.bmiHeader.biClrUsed = 0;
	bitmap_info.bmiHeader.biClrImportant = 0;

    // TODO move to obisidan
    // it seems `GetDIBits` it's not needed at all!
    // Only `StretchDIBits`, `GetDC` (i guess), and `CreateCompatibleBitmap`


    //if(GetDIBits(hdc, bitmap_handle, 0, 0, (void*)buffer, &bitmap_info, DIB_RGB_COLORS) == ERROR_INVALID_PARAMETER)
    {
        int x = 1231;
    }

    ////////
    while(g_running)
    {
        draw_rectangle(buffer, 0, 0, horz_res, vert_res, 0xff007f00);
        draw_rectangle(buffer, 100, 100, 50, 50, 0xffffff00);
        win32_process_pending_msgs();
        StretchDIBits(hdc, 0, 0, 800, 600, 0, 0, horz_res, vert_res, (void*) buffer, &bitmap_info, DIB_RGB_COLORS, SRCCOPY);
        int x = 123;
    }
}