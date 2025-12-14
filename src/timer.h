LONGLONG timer_frequency = {0};

internal LONGLONG timer_get_os_freq();
internal void timer_init();
internal LONGLONG timer_get_os_time();
internal DWORD64 timer_cpu_cycles();
internal double timer_os_time_to_sec(LONGLONG time); 
internal double timer_os_time_to_ms(LONGLONG time);
