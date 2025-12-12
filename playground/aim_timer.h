#pragma once
#include <windows.h>

static LONGLONG aim_timer_get_os_freq();
static LONGLONG aim_timer_get_os_time();
static DWORD64 aim_timer_cpu_cycles();
static double aim_timer_ticks_to_sec(LONGLONG time, LONGLONG frequency);
static double aim_timer_ticks_to_ms(LONGLONG time, LONGLONG frequency);