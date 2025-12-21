#ifdef AIM_PROFILER
void aim_profiler_print() {
	LONGLONG total_time = profiler.os_end - profiler.os_start;
    char buf[100];
    sprintf(buf, "Total execution time: %f ms\n", aim_timer_ticks_to_ms(total_time, profiler.os_freq));
    OutputDebugStringA(buf);
    printf(buf);
	for (int i = 1; i < sizeof(profiled_anchors) / sizeof(profiled_anchors[0]); i++) {
		ProfiledAnchor anchor = profiled_anchors[i];
		if (anchor.elapsed_inclusive) 
        {
            char buf[500];
            u32 written = sprintf(
                buf, "   %s[%d] %.4fms (%.4f%%)",
                anchor.label,
                anchor.hitcount,
                aim_timer_ticks_to_ms(anchor.elapsed_inclusive, profiler.os_freq),
                ((double)anchor.elapsed_inclusive / (double)total_time) * 100.0f
            );

            if(anchor.hitcount != 1)
            {
                written += sprintf(
                    buf + written,
                    " | total average: (%.4fms)",
                    aim_timer_ticks_to_ms(anchor.elapsed_inclusive, profiler.os_freq) / anchor.hitcount
                );
            }
            if (anchor.reset_mark_index)
            {
                written += sprintf(
                    buf + written,
                    " | avg per frame: [%d](%.4fms)",
                    anchor.hitcount_per_frame,
                    aim_timer_ticks_to_ms(anchor.elapsed_inclusive, profiler.os_freq) / anchor.hitcount_per_frame
                );

            }
            OutputDebugStringA(buf);
            printf(buf);
            
			if (anchor.elapsed_exclusive != anchor.elapsed_inclusive) {
                char buf[500];
                sprintf(buf, " | without children: %.4fms (%.4f%%)", aim_timer_ticks_to_ms(anchor.elapsed_exclusive, profiler.os_freq), ((double)(anchor.elapsed_exclusive)) / (double)(total_time) * 100.0f);
                OutputDebugStringA(buf);
                printf(buf);
			}
            
			if (anchor.bytes_count) {
				uint64_t mb =  mb(1);
				uint64_t gb = gb(1);
                char buf[500];
                sprintf(buf, "%.3f mb at %.2f gb/s", (double)anchor.bytes_count / mb, (((double)anchor.bytes_count / gb) / aim_timer_ticks_to_sec(anchor.elapsed_inclusive, profiler.os_freq)));
                OutputDebugStringA(buf);
                printf(buf);
			}
			printf("\n");
		}
	}
}

ProfiledBlock::ProfiledBlock(const char* label, uint32_t index, uint64_t bytes_count, u32 reset_mark_index) {
	this->parent_index = global_parent_index;
	this->label = label;
	this->index = index;
	this->time_start = aim_timer_get_os_time();
    
	ProfiledAnchor* anchor = &profiled_anchors[this->index];
    anchor->reset_mark_index = reset_mark_index;
	OldTSCElapsedAtRoot = anchor->elapsed_inclusive;
	anchor->bytes_count += bytes_count;
	if (OldTSCElapsedAtRoot != 0) {
		//abort();
	}
    
	global_parent_index = index;
}

ProfiledBlock::~ProfiledBlock() {
	LONGLONG elapsed = aim_timer_get_os_time() - this->time_start;
	ProfiledAnchor* anchor = &profiled_anchors[this->index];
	ProfiledAnchor* parent = &profiled_anchors[this->parent_index];
	anchor->label = this->label;
	anchor->hitcount++;
    
	/*
		parent->elapsed	+= elapsed				exclusive
		anchor->TSCElapsedAtRoot = Elapsed;		inclusive
	*/
    
	parent->elapsed_exclusive -= elapsed;
	anchor->elapsed_exclusive += elapsed;
	anchor->elapsed_inclusive = OldTSCElapsedAtRoot + elapsed;
    
	global_parent_index = this->parent_index;


    if (anchor->reset_mark_index)
    {
        // TODO better naming
        // TODO for now is just the index in the profiled_anchors array of the "scope" where this should be reset
        // while(true)
        //   root_idx = profile_zone()
        //   profile_function(..., root_idx)
        //   
        // ...
        // when root_idx died, if profiled_anchors[root_idx][elem_idx]->hitcount_per_frame++ and elapsed_inclusive / hitcount_per_frame
        // when root_idx died, if profiled_anchors[root_idx]->hitcount_per_frame++
        //
        //
    }
}
#else
void aim_profiler_print() {}
#endif

void aim_profiler_begin() {
	profiler.os_freq = aim_timer_get_os_freq();
	profiler.os_start = aim_timer_get_os_time();
	//profiler.cpu_start = aim_timer_cpu_cycles();
}

void aim_profiler_end() {
	profiler.os_end = aim_timer_get_os_time();
	//profiler.cpu_end = aim_timer_cpu_cycles();
}
