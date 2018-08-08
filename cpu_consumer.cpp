#include <csignal>

#include <iostream>
#include <fstream>
#include <string>
using namespace std;

#include "x86_tsc.h"

#include "cpu_consumer.h"
#include "gcc_helpers/debug.h"

volatile bool cpu_consumer::stop_program = false;
bool cpu_consumer::do_record = false;

ssize_t cpu_consumer::preempt_pts_curr_idx = 0;
uint64_t* cpu_consumer::preempt_pts = nullptr;

time_unit cpu_consumer::run_time = time_unit(true);
time_unit cpu_consumer::max_preempt = time_unit(true);
time_unit cpu_consumer::exec_time = time_unit(true);
time_unit cpu_consumer::solo_cycle = time_unit(true);

const string PROGRAM_NAME = "cpu_consumer";

void SIG_handler(int)
{
	cout << "stop caught (" << PROGRAM_NAME << ")" << endl;
	cpu_consumer::stop_program = true;
}

void SIG_start(int)
{
	cout << "Signal start caught (" << PROGRAM_NAME << ")" << endl;
}

/**
 * DESCRIPTION:
 * This is the main execution code that consumes CPU time by spinning.
 *
 * @run_time
 * 		(out) - total amount of wall-clock time elapsed
 * 		(in)  - maximum amount of time to elapse before exiting
 *
 * @exec_time
 * 		(out) - total CPU time this code was able to consume
 *
 * @max_preempt
 * 		(out) - maximum continuous length of time execution was preempted
 *
 * NOTE:
 * This function may be interrupted by a signal and may not run
 * all of run_time specified.
 *
 * is_init - init version of function used to initializes solo_cycle
 *
 * !is_init - version used to measure cpu time (only slightly different than
 * the initialization function/loop).
 *
 * exec - run_time is time to consume before exiting
 * !exec - run_time is wall-clock time to pass before exiting
 *
 */
template <bool is_init, bool exec>
void cpu_consumer::trial_loop(time_unit& run_time, time_unit& exec_time, time_unit& max_preempt)
{
	// TODO: verify that solo_cycle is not be greater than max_no_preempt
	// TODO: max_no_preempt is just an estimate, set automatically; maybe
	// proportional to solo_cycle? Maybe obtain it from initialization
	// function?
	// Maybe compare with kernel's recorded number of context switches, but may
	// detect other preemptions not counted as context switches by kernel.
	const static time_unit max_no_preempt = time_unit::NANOSECS(200, true);
	static time_unit total(true); total._cycles = 0;
	int nr_preempts = 0;

	// min is used to assign value to solo_cycle.
	// Start with min being one sec, but assume it be much less than 1 sec.
	const static time_unit one_sec = time_unit::SECS(1, true);
	static time_unit min(true); min = one_sec;

	static time_unit before(true);
	static time_unit diff(true);

	max_preempt._cycles = 0;

	static time_unit begin(true); begin._cycles = read_tsc();
	static time_unit stop(true);
	if (!exec) {
		// stop is not used if stop condition is a cpu time amount consumed
		// could cause problems if run_time is set to the max (i.e., overflow)
		stop = begin + run_time;
	}

	static time_unit curr(true); curr._cycles = read_tsc();

	// ensure diff = curr - before is large enough so that the first itertation
	// is sure to overwrite min with diff. The issue is that diff may be may be
	// very small on the first iteration (which would translate into a
	// artificially samll solo_cycle value.
	// NOTE: min is not used measuring loop, only used to initialize solo_cycle
	if (is_init)
		curr._cycles -= one_sec._cycles;

	// For achieving maximum accuracy this loop should be as short as possible.
	// Each iteration (regardless of branches) should idealy take exactly the
	// same amount of time.
	while (stop_program == false) {
		before._cycles = curr._cycles;
		curr._cycles = read_tsc();

		diff._cycles = curr._cycles - before._cycles;

		if (diff._cycles > max_no_preempt._cycles) {
			nr_preempts++;
			// We were preempted, only count one solo_cycle worth
			// of execution.  We have no way to know exactly how
			// much time we actually consumed so just use the
			// minimum amount we could have possibly consumed
			total._cycles += solo_cycle._cycles;

			// checking if !first_iteration is not be necessary
			// max_preempt is not used in initialization code
			// if the thread is preempted, on the first iteration and it takes
			// a long time, this is still a valid max_preempt

			// track the largest length of time this thread is preempted
			if (diff._cycles > max_preempt._cycles)
				max_preempt._cycles = diff._cycles;

			if (do_record) {
				// store preemption time interval
				if (preempt_pts_curr_idx <= preempt_pts_last_usable_idx) {
					preempt_pts[preempt_pts_curr_idx++] = before._cycles;
					preempt_pts[preempt_pts_curr_idx++] = curr._cycles;
				} else {
					cout << "out of preemption point storage, exiting." << endl;
					break;
				}
			}
		} else {
			// NOT preempted. Therefore know EXACTLY how much time was consumed
			// in this iteration (i.e., diff cycles).
			total._cycles = total._cycles + diff._cycles;

			// update min, which is used to obtain solo_cycle value
			// TODO: maybe use a dummy variable operation as an else statement to
			// make the loop iteration closer to being equal for all code paths

			// NOTE: min is only updated if not preempted (certainly if
			// preempted, the value cannot be the min time to execute loop)

			// In the measurement (vs initialization) version of the loop, min is not used.
			// min is only used to set solo_cycle in the initialization loop
			if (diff._cycles < min._cycles)
				min._cycles = diff._cycles;
		}

		if (exec) {
			if (total._cycles >= run_time._cycles)
				break;
		} else {
			if (curr._cycles > stop._cycles)
				break;
		}
	}
	static time_unit trial_run_time(true);
	trial_run_time._cycles = read_tsc() - begin._cycles;

	// NOTE: don't reset solo_cycle in measurement loop so as to allow it to be
	// reused for another trial if needed
	if (is_init) {
		if (min == one_sec) {
			cout << "min not updated, unable to initialize solo_cycle, exiting." << endl;
			exit(EXIT_FAILURE);
		}
		solo_cycle = min;
	}

	exec_time._cycles = total._cycles;
	if (!exec) {
		if (exec_time._cycles > run_time._cycles) {
			cout << "Warning: exec_time > run_interval, setting exec_time = run_interval." << endl;
			cout << "Maybe solo_cycles was not calibrated correctly?" << endl;
			OUTPUT_NUM(exec_time._cycles);
			OUTPUT_NUM(run_time._cycles);
			cout << "nr_preempts: " << nr_preempts << endl;
			exec_time = run_time;
		}
	}
	run_time = trial_run_time;

	//cout << "nr_preempts: " << nr_preempts << endl;
}

void
cpu_consumer::init_all()
{
	// use cycles for all time measurments
	time_unit::default_use_cycles = true;

	// storage for preemption time instants
	// TODO: note that this memory is never freed
	// TODO: do_record could be set to true after init_all() and cause a crash
	// possible fix would be to create an object? (would also allow cleanup of memory)
	if (do_record) {
		preempt_pts = new u64[preempt_pts_size];
		preempt_pts_curr_idx = 0;
	}

	init_signals();

	init_solo_cycle();
}

void
cpu_consumer::init_solo_cycle()
{
	// length of time to execute the measurement loop for initialization (e.g., solo_cycle)
	time_unit init_run_time = time_unit::SECS(2, true);

	preempt_pts_curr_idx = 0;
	// used to initialize solo_cycle
	trial_loop<true>(init_run_time, exec_time, max_preempt);
}

/**
 * Consume as much cpu time as possible in the given @run_window_length.
 */
void
cpu_consumer::consume_time(time_unit run_window_length)
{
	preempt_pts_curr_idx = 0;
	run_time = run_window_length;
	trial_loop<false>(run_time, exec_time, max_preempt);
}

/**
 * Once the amount of cpu time (@amt) has been consumed, exit.
 */
void
cpu_consumer::consume_exec_time(time_unit amt)
{
	preempt_pts_curr_idx = 0;
	run_time = amt;
	trial_loop<false, true>(run_time, exec_time, max_preempt);
}

void
cpu_consumer::max_nonpreempt()
{
	time_unit long_time(true);
	long_time.set_max();

	consume_exec_time(long_time);
}

void
cpu_consumer::init_signals()
{
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	// register handler used to START measurement
	sa.sa_handler = SIG_handler;
	sigaction(SIGTERM, &sa, 0);
	sigaction(SIGINT, &sa, 0); // CTRL-C

	// register handler used to STOP measurement
	sa.sa_handler = SIG_start;
	sigaction(SIGUSR1, &sa, 0);
}

void
cpu_consumer::preempt_pts_to_file()
{
	if (!do_record) {
		cout << "WARNING(" << __func__ << "): do_record is set to false, but trying to write pts!" << endl;
		return;
	}

	ofstream ostm_output;
	string filename = "preempt_pts.dat";

	ostm_output.open(filename.c_str(), ofstream::out);

	ostm_output << "# " << (uint64_t)time_unit::_cpu_hz << endl;
	for (int i=0; i<preempt_pts_curr_idx; ++i)
		ostm_output << preempt_pts[i] << endl;

	ostm_output.close();
}
