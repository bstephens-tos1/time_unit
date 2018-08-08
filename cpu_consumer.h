#pragma once

#include <sys/types.h>

#include "time_unit.h"

class cpu_consumer
{
public:
	template <bool is_init, bool exec=false>
	static void trial_loop(time_unit& run_time, time_unit& exec_time, time_unit& max_preempt);

	static void consume_time(time_unit run_window_length);
	static void consume_exec_time(time_unit amt);
	static void max_nonpreempt(void);

	static void init_all();
	static void init_signals();
	static void init_solo_cycle();

	static void preempt_pts_to_file();

	static volatile bool stop_program;
	static bool do_record;

	static constexpr size_t preempt_pts_size = (size_t)1E8;
	static constexpr ssize_t preempt_pts_last_usable_idx = preempt_pts_size - 2;
	static_assert(preempt_pts_last_usable_idx >= 0, "preempt_pts_size is too small");
	static ssize_t preempt_pts_curr_idx;
	static uint64_t* preempt_pts;

	static time_unit solo_cycle;
	static time_unit run_time;
	static time_unit exec_time;
	static time_unit max_preempt;

private:
	cpu_consumer();
};

void SIG_handler(int);
void SIG_start(int);
