// TODO stretchdibbits in release - debug takes around 12% of the time when running it close to a minute.
//  Try to use dx11 for presentation to see if the time lowers
#include "base_core.h"
#include "base_arena.h"
#include "base_string.h"
#include "base_os.h"
#include "input.h"
#include "os_win32.h"
#include "timer.h"
#include "base_arena.c"
#include "base_string.c"
#include "os_win32.c"
#include "timer.c"


#include "winnt.h"
#include "main.h"

global Arena *arena;

static b32 g_running = 1;
static b32 g_window_is_active = 0;
static HWND hwnd;
static HDC hdc;

global Software_Render_Buffer *buffer;
global Software_Depth_Buffer *depth_buffer;

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
            if(g_window_is_active)
            {
                win32_clip_cursor_to_client(hwnd);
            }
        } break;
        case WM_SETCURSOR:
        {
            if(LOWORD(lParam) == HTCLIENT)
            {
                if(g_window_is_active)
                {
                    SetCursor(0);
                }
                else
                {
                    SetCursor(LoadCursor(0, IDC_ARROW));
                }
                return 1;
            }
        } break;
        case WM_MOVE:
        {
            if(g_window_is_active)
            {
                win32_clip_cursor_to_client(hwnd);
            }
        } break;
        case WM_ACTIVATE:
        {
            if(LOWORD(wParam) != WA_INACTIVE)
            {
                g_window_is_active = 1;
                win32_clip_cursor_to_client(hwnd);
            }
            else
            {
                g_window_is_active = 0;
                win32_unclip_cursor();
            }
        } break;
        case WM_DESTROY: 
        {
            g_window_is_active = 0;
            win32_unclip_cursor();
            g_running = 0;
        } break;
        default: 
            return DefWindowProc(hwnd, uMsg, wParam, lParam); 
    } 
    return 0; 
}

void win32_process_pending_msgs(Game_Input *input) 
{
    TempArena scratch = scratch_begin();
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
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                WPARAM wparam = Message.wParam;
                LPARAM lparam = Message.lParam;

                u32 key = (u32)wparam;
                u32 alt_key_is_pressed = (lparam & (1 << 29)) != 0;
                u32 was_pressed = (lparam & (1 << 30)) != 0;
                u32 is_pressed = (lparam & (1 << 31)) == 0;
                input->curr_keyboard_state.keys[key] = (u8)(is_pressed);
                input->prev_keyboard_state.keys[key] = (u8)(was_pressed);
                #if 0
                if (GetKeyState(VK_SHIFT) & (1 << 15)) {
                    os_modifiers |= OS_Modifiers_Shift;
                }
                // NOTE reason behin this is that every key will be an event and each event knows what are the modifiers pressed when they key was processed
                // It the key is a modifier as well then the modifiers of this key wont containt itself. Meaning if i press Shift and i consult the modifiers pressed
                // for this key, shift will not be set
                if (GetKeyState(VK_CONTROL) & (1 << 15)) {
                    os_modifiers |= OS_Modifiers_Ctrl;
                }
                if (GetKeyState(VK_MENU) & (1 << 15)) {
                    os_modifiers |= OS_Modifiers_Alt;
                }
                if (key == Keys_Shift && os_modifiers & OS_Modifiers_Shift) {
                    os_modifiers &= ~OS_Modifiers_Shift;
                }
                if (key == Keys_Control && os_modifiers & OS_Modifiers_Ctrl) {
                    os_modifiers &= ~OS_Modifiers_Ctrl;
                }
                if (key == Keys_Alt && os_modifiers & OS_Modifiers_Alt) {
                    os_modifiers &= ~OS_Modifiers_Alt;
                }
                #endif
            } break;

            case WM_MOUSEMOVE:
            {
                i32 xPos = (Message.lParam & 0x0000FFFF); 
                i32 yPos = ((Message.lParam & 0xFFFF0000) >> 16); 

                i32 xxPos = LOWORD(Message.lParam);
                i32 yyPos = HIWORD(Message.lParam);

                input->curr_mouse_state.x = xPos;
                input->curr_mouse_state.y = yPos;
            }
            break;
            case WM_INPUT: {
                UINT size;
                GetRawInputData((HRAWINPUT)Message.lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));

                RAWINPUT *raw = (RAWINPUT*)(u8*)arena_push_size(scratch.arena, u8, size);
                GetRawInputData((HRAWINPUT)Message.lParam, RID_INPUT, raw, &size, sizeof(RAWINPUTHEADER));
                if ((raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0 && raw->header.dwType == RIM_TYPEMOUSE)
                {
                    LONG dx = raw->data.mouse.lLastX;
                    LONG dy = raw->data.mouse.lLastY;
                    input->dx += (f32)(dx);
                    input->dy += (f32)(-dy);
                }

                // GLFW does this, but im not sure if it makes sense for my usecase! Also they dont do it here
                // recenter cursor center re center
                //POINT center = { LONG(SRC_WIDTH)/2, LONG(SRC_HEIGHT)/2 };
                //ClientToScreen(global_w32_window.handle, &center);
                //SetCursorPos(center.x, center.y);

            } break;
            case WM_LBUTTONUP:
            {
                input->curr_mouse_state.button[MouseButtons_LeftClick] = 0;

            } break;
            case WM_MBUTTONUP:
            {
                input->curr_mouse_state.button[MouseButtons_MiddleClick] = 0;
            } break;
            case WM_RBUTTONUP:
            {
                input->curr_mouse_state.button[MouseButtons_RightClick] = 0;
            } break;

            case WM_LBUTTONDOWN:
            {
                input->curr_mouse_state.button[MouseButtons_LeftClick] = 1;

            } break;
            case WM_MBUTTONDOWN:
            {
                input->curr_mouse_state.button[MouseButtons_MiddleClick] = 1;
            } break;
            case WM_RBUTTONDOWN:
            {
                input->curr_mouse_state.button[MouseButtons_RightClick] = 1;
            } break;
            default:
            {
                DispatchMessageA(&Message);
            } break;
        }
    }
    scratch_end(scratch);
}

int main ()
{
    timer_init();
    arena = arena_alloc(mb(2));
    g_transient_arena = arena_alloc(mb(10));
    // 4:3 resolutions
    // 320 x 240
    // 640 x 480    (VGA)
    // 800 x 600    (SVGA)
    // 940 x 540    
    // 1024 x 768   (XGA)
    // 1280 x 960
    // 1400 x 1050
    // 1600 x 1200  (UXGA)
    // 1920 x 1440
    i32 window_width = 800; 
    i32 window_height = 600;
    window_width = 1400; 
    window_height = 1050;
    window_width = 1280; 
    window_height = 960;
    window_width = 1280; 
    window_height = 720;
    window_width = 1920; 
    window_height = 1080;

    i32 buffer_width = 640;
    i32 buffer_height = 480;
    buffer_width = 320;
    buffer_height = 240;
    buffer_width = 960;
    buffer_height = 540;
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
    g_window_is_active = (GetForegroundWindow() == hwnd);
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
	buffer->info.bmiHeader.biBitCount = 32;
	buffer->info.bmiHeader.biCompression = BI_RGB;
	buffer->info.bmiHeader.biSizeImage = 0;
	buffer->info.bmiHeader.biXPelsPerMeter = 0;
	buffer->info.bmiHeader.biYPelsPerMeter = 0;
	buffer->info.bmiHeader.biClrUsed = 0;
	buffer->info.bmiHeader.biClrImportant = 0;

    depth_buffer = VirtualAlloc(0, sizeof(Software_Depth_Buffer), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    depth_buffer->width = buffer_width;
    depth_buffer->height = buffer_height;
    depth_buffer->data = VirtualAlloc(0, sizeof(f32) * buffer->width * buffer->height, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    
    depth_buffer->info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	depth_buffer->info.bmiHeader.biWidth = buffer->width;
	depth_buffer->info.bmiHeader.biHeight = -buffer->height;
	depth_buffer->info.bmiHeader.biPlanes = 1;
	depth_buffer->info.bmiHeader.biBitCount = 32;
	depth_buffer->info.bmiHeader.biCompression = BI_RGB;
	depth_buffer->info.bmiHeader.biSizeImage = 0;
	depth_buffer->info.bmiHeader.biXPelsPerMeter = 0;
	depth_buffer->info.bmiHeader.biYPelsPerMeter = 0;
	depth_buffer->info.bmiHeader.biClrUsed = 0;
	depth_buffer->info.bmiHeader.biClrImportant = 0;

    Game_Memory memory = {0};
    memory.persistent_memory_size = mb(2);
    memory.transient_memory_size = mb(1);

    memory.persistent_memory = VirtualAlloc(0, memory.persistent_memory_size + memory.transient_memory_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    memory.transient_memory = (u8*)memory.persistent_memory + memory.persistent_memory_size;

    Game_Input input = {0};

    SetCursor(0);
    RAWINPUTDEVICE rid = {0};
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02;
    rid.dwFlags = 0;
    rid.hwndTarget = window.hwnd;
    if(!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
    {
        printf("RegisterRawInputDevices failed: %lu\n", GetLastError());
    }
    if(g_window_is_active)
    {
        win32_clip_cursor_to_client(hwnd);
    }

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


    #if defined(AIM_TEST_SUITE)
    u8* dll_game_name = cstring_from_str8(arena, str8_concat(arena, absolute_path, str8_literal("\\test")));
    OS_LoadedDLL loaded_dll = os_create_dll(arena, dll_game_name);
    os_load_dll(&loaded_dll);
    #else
    u8* dll_game_name = cstring_from_str8(arena, str8_concat(arena, absolute_path, str8_literal("\\game")));
    OS_LoadedDLL loaded_dll = os_create_dll(arena, dll_game_name);
    os_load_dll(&loaded_dll);
    #endif

    LONGLONG last_time = timer_get_os_time();
    LONGLONG total_time = 0;
    while(g_running)
    {
        LONGLONG now = timer_get_os_time();
        LONGLONG dt = now - last_time;
        total_time += dt;
        last_time = now;
        //printf("Game loop: %.2fms\n", timer_os_time_to_ms(dt));
        win32_process_pending_msgs(&input);

        os_perform_hot_reload(&loaded_dll);
        loaded_dll.app_update_and_render(buffer, depth_buffer, &memory, &input, timer_os_time_to_ms(total_time), timer_os_time_to_sec(dt));
        //StretchDIBits(hdc, 0, 0, buffer->width, buffer->height, 0, 0, buffer->width, buffer->height, (void*) buffer->data, &buffer->info, DIB_RGB_COLORS, SRCCOPY);

        //LONGLONG stretch_now = timer_get_os_time();
        StretchDIBits(hdc, 0, 0, window_width, window_height, 0, 0, buffer->width, buffer->height, (void*) buffer->data, &buffer->info, DIB_RGB_COLORS, SRCCOPY);
        //LONGLONG stretch_last = timer_get_os_time();
        //printf("StretchDIBits time: %.2fms\n", timer_os_time_to_ms(stretch_last - stretch_now));
        input_update(&input);
    }
}
