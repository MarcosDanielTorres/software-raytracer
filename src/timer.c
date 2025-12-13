LONGLONG timer_frequency = {0};

static LONGLONG timer_get_os_freq() {
    LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	return frequency.QuadPart;
}

static void timer_init()
{
   timer_frequency = timer_get_os_freq();
}

static LONGLONG timer_get_os_time() {
	LARGE_INTEGER result;
	QueryPerformanceCounter(&result);
	return result.QuadPart;
}

static DWORD64 timer_cpu_cycles() {
	return __rdtsc();
}

static double timer_os_time_to_sec(LONGLONG time) {
	double result = ((double)time / (double)timer_frequency);
	return result;
}

static double timer_os_time_to_ms(LONGLONG time) {
	double result = ((double)time * 1000.0f / (double)timer_frequency);
	return result;
}
