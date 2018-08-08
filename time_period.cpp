#include <iostream>
using namespace std;

#include "time_period.h"

/*
 * x86 assembly(i.e., rdtsc) will fail on arm compile
 */
#if !defined (__i386__) && !defined(__x86_64__)
//#warning "INFO: Compiling for ANDROID"
#pragma message "INFO: Compiling for ANDROID"
#define ANDROID
#else
//#warning "INFO: Compiling for X86"
//#pragma message "INFO: Compiling for X86"
#endif

time_period::time_period(bool tu_cycles)
	: _start_time(tu_cycles), _stop_time(tu_cycles)
{
}

/*
// Requires conversion between timespec and cycles
int
time_period::use_cycles(bool choice)
{
	_start_time.use_cycles(choice);
	_stop_time.use_cycles(choice);

	return 0;
}
*/

void
time_period::start()
{
	_start_time.set_now();
}

void
time_period::stop()
{
	_stop_time.set_now();
}

u64
time_period::get_diff_nsec()
{
	time_unit tu;

	tu = _stop_time - _start_time;

	return tu.get_nanosecs();
}

double
time_period::get_diff_msec()
{
	time_unit tu;

	tu = _stop_time - _start_time;

	return tu.get_millisecs();
}

u64
time_period::get_diff_usec() const
{
	time_unit tu;

	tu = _stop_time - _start_time;

	return tu.get_microsecs();
}

u64
time_period::get_diff_cycles()
{
	if (!_start_time.using_cycles() || !_stop_time.using_cycles()) {
		cerr << "ERROR: cycles not enabled" << endl;
		exit(EXIT_FAILURE);
	}

	time_unit tu;

	tu = _stop_time - _start_time;

	return tu._cycles;
}

time_unit
time_period::get_diff_tu()
{
	return _stop_time - _stop_time;
}
