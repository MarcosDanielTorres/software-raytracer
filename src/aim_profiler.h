#pragma once
#include <stdio.h>
#include "aim_timer.h"

/*

    I want to store information about the time a function takes per frame.

    I have the total hitcount and could also need how many hitcounts a specific frame had, altough this could be a lot more than
    I should need.

    Because a function can be executed n number of times each frame, if i want to see the average I need to
    out of 60 frames, see how many invocations there was 

    ... this may be too complicated. Lets first start by just getting how many times per "frame" the function has been called!

*/

#ifdef AIM_PROFILER
struct ProfiledAnchor {
	const char* label;
	LONGLONG elapsed_inclusive;
	LONGLONG elapsed_exclusive;
	LONGLONG cpu_start;
	uint64_t bytes_count;
	uint32_t hitcount{ 0 };
    u32 hitcount_per_frame;
    u32 reset_mark_index;
};

static ProfiledAnchor profiled_anchors[100];
static uint32_t global_parent_index;
//static int profiler_address_test = (int)&profiler;

struct ProfiledBlock {
	const char* label;
	uint32_t index;
	uint32_t parent_index;
	LONGLONG time_start;
	LONGLONG OldTSCElapsedAtRoot;

	ProfiledBlock(const char* label, uint32_t index, uint64_t bytes_count, u32 reset_index);
	~ProfiledBlock();
};

#define NameConcat2(A, B) A##B
#define NameConcat(A, B) NameConcat2(A, B)

#define aim_profiler_bandwidth(name, bytes_count, reset_index) ProfiledBlock NameConcat(Block, __LINE__)(name, __COUNTER__ + 1, bytes_count, reset_index);
#define aim_profiler_time_block(name) aim_profiler_bandwidth(name, 0, 0);
#define aim_profiler_time_block_with_reset_at_mark(name, reset_index) aim_profiler_bandwidth(name, 0, reset_index);
#define aim_profiler_time_function_with_reset_at_mark aim_profiler_time_block(__func__, reset_index);
#define aim_profiler_time_function aim_profiler_time_block(__func__);
#else
#define aim_profiler_bandwidth(...)
#define aim_profiler_time_block(...) 
#define aim_profiler_time_function
#endif

struct Profiler {
	LONGLONG os_freq;
	LONGLONG os_start;
	LONGLONG os_end;
};
static Profiler profiler;
void aim_profiler_begin();
void aim_profiler_end();
void aim_profiler_print();
