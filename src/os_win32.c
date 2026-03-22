#include "base_os.h"
internal OS_FileReadResult os_file_read(Arena *arena, const char *filename)
{
    OS_FileReadResult result = {0};
    FILE *file = fopen(filename, "rb");
    if (!file) 
    {
        printf("Probably not found: %s\n", filename);
        return result;
    }
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    u8 *buf = (u8*)arena_push_size(arena, u8, size + 1);
    fread(buf, 1, size, file);
    
    result.data = buf;
    result.size = size;
    return result;
}

internal WIN32_FIND_DATAA
win32_get_file_attributes(u8* filename)
{
    WIN32_FIND_DATAA result = {0};
    GetFileAttributesEx(filename, GetFileExInfoStandard, &result);
    return result;
}

internal u64
win32_filetime_to_u64(FILETIME time)
{
    return ((u64)time.dwHighDateTime << 32) | time.dwLowDateTime;
}

internal void
win32_clip_cursor_to_client(HWND hwnd)
{
    RECT rect = {0};
    if(!GetClientRect(hwnd, &rect))
    {
        return;
    }

    POINT ul = {rect.left, rect.top};
    POINT lr = {rect.right, rect.bottom};
    ClientToScreen(hwnd, &ul);
    ClientToScreen(hwnd, &lr);

    rect.left   = ul.x;
    rect.top    = ul.y;
    rect.right  = lr.x;
    rect.bottom = lr.y;

    ClipCursor(&rect);
}

internal void
win32_unclip_cursor(void)
{
    ClipCursor(0);
}

internal void os_win32_toggle_fullscreen(HWND handle, WINDOWPLACEMENT *window_placement) 
{
  DWORD window_style = GetWindowLong(handle, GWL_STYLE);
  if (window_style & WS_OVERLAPPEDWINDOW) 
  {
    // if not fullscreen
    MONITORINFO monitor_info = {sizeof(MONITORINFO)};
    if(GetWindowPlacement(handle, window_placement) && 
        GetMonitorInfo(MonitorFromWindow(handle, MONITOR_DEFAULTTOPRIMARY), &monitor_info))
    {
      SetWindowLong(handle, GWL_STYLE, window_style & ~WS_OVERLAPPEDWINDOW);
      SetWindowPos(handle, HWND_TOP,
                   monitor_info.rcMonitor.left,
                   monitor_info.rcMonitor.top,
                   monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
                   monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
                   SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
  }
  else
  {
    SetWindowLong(handle, GWL_STYLE, window_style | WS_OVERLAPPEDWINDOW);
    SetWindowPlacement(handle, window_placement);
    SetWindowPos(handle, 0, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
  }
}

// TODO dont like the name of this because its tighly couple to the implmentation. Is not a generic load of a DLL. Which maybe it shouldnt be
// I think i could have a load dll function but there must be a different load-specifc_case dll to account for other things
// probably its best to remove this function from the os layer!
internal void
os_load_dll(OS_LoadedDLL *dll)
{
    WIN32_FIND_DATAA ignored;
    {
        WIN32_FIND_DATAA file_data;
        if(GetFileAttributesEx(dll->name, GetFileExInfoStandard, &file_data))
        {
            dll->last_write_time = win32_filetime_to_u64(file_data.ftLastWriteTime);
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

        HANDLE handle = LoadLibraryExA(dll->name_temp, 0, 0);
        if(handle)
        {
            dll->app_update_and_render = (Update_And_Render*) GetProcAddress(handle, "update_and_render");
            dll->is_valid = dll->app_update_and_render != 0;
        }
        dll->handle.U64 = (u64)handle;
        if (!handle || !dll->app_update_and_render)
        {
            printf("Defaulting to stub...\n");
            dll->app_update_and_render = update_and_render_stub;
        }
    }
}

internal void
os_unload_dll(OS_LoadedDLL *dll)
{
    HANDLE handle = (HANDLE) dll->handle.U64;
    if (handle)
        FreeLibrary(handle);
    dll->handle.U64 = 0;
    dll->app_update_and_render = 0;
    dll->is_valid = 0;
}

internal OS_LoadedDLL
os_create_dll(Arena *arena, u8* dll_name)
{
    OS_LoadedDLL result_dll = {0};
    result_dll.name = cstring_concat(arena, dll_name, ".dll");
    result_dll.name_temp = cstring_concat(arena, dll_name, "-temp.dll");
    result_dll.lock_filename = cstring_concat(arena, dll_name, "-lock.temp");
    return result_dll;
}

internal void
os_perform_hot_reload(OS_LoadedDLL *dll)
{
    WIN32_FIND_DATAA file_data = win32_get_file_attributes(dll->name);
    u64 new_write_time = win32_filetime_to_u64(file_data.ftLastWriteTime);
    //if(CompareFileTime(&file_data.ftLastWriteTime, &dll->last_write_time) != 0)
    if(new_write_time != dll->last_write_time)
    {
        WIN32_FIND_DATAA ignored;
        if(!GetFileAttributesEx(dll->lock_filename, GetFileExInfoStandard, &ignored))
        {
            LONGLONG start_time = timer_get_os_time();
            os_unload_dll(dll);
            os_load_dll(dll);
            LONGLONG end_time = timer_get_os_time();
            printf("[Hot-Reloading] %s: took: %.2f ms\n", dll->name, timer_os_time_to_ms(end_time - start_time));
        }
    }
}
