#ifndef TIME_UNIT_H
#define TIME_UNIT_H

/*
 * DESCRIPTION:
 *
 * Provides mechanisms to retrieve the current time instant.  This can be
 * done with various time sources, including:
 *
 * 	- CPU Time Stamp Counter
 * 	- system calls
 * 	- ntp servers
 *
 * Also, contains some useful functions to deal with various time
 * representations (structures).  These include conversions, adding, etc.
 */

#include <time.h> // CLOCK_REALTIME, etc.

#include <ostream>

#include "data_types.h"

class time_unit {
	public:
		static double _cpu_hz;
		constexpr static bool compile_default_use_cycles = false;
		static bool default_use_cycles;

		// either _cycles or _timespec is used to represent time
		u64 _cycles;
		struct timespec _timespec;

		time_unit(void);
		explicit time_unit(bool cycles_time_storage);

		const static time_unit ONE_MICRO;
		static time_unit SECS(u64 secs, bool cycles_store=compile_default_use_cycles);
		static time_unit MILLISECS(u64 msecs, bool cycles_store=compile_default_use_cycles);
		static time_unit MICROSECS(u64 usecs, bool cycles_store=compile_default_use_cycles);
		static time_unit NANOSECS(u64 nsecs, bool cycles_store=compile_default_use_cycles);
		static time_unit NOW(bool cycles_store=compile_default_use_cycles);
		static time_unit from_timestamp(const std::string &timestamp);

		static std::string now_str(std::string format="%Y-%m-%d.%X");

		u64 init_hz(int seconds);
		bool init_hz_from_file();

		bool using_cycles() const;
		//int use_cycles(bool choice); // requires conversion between cycles and timespec

		u64 get_nanosecs(void) const;
		u64 get_microsecs(void) const;
		double get_millisecs(void);
		double get_seconds(void);
		int set_nanosecs(u64 nsecs);
		int set_microsecs(u64 usecs);
		int set_millisecs(u64 msecs);
		int set_seconds(u64 secs);
		int set_time_unit(const time_unit &tu);
		void set_now(void);  // set the recorded time as the current time
		time_unit subtract(const time_unit &rhs) const;
		time_unit add(const time_unit &rhs) const;
		void add_ns(u64 nsecs);
		void add_sec(u64 secs);
		void sub_sec(u64 secs);

		int set_timespec(const struct timespec &ts);
		struct timespec get_timespec(void) const;

		int set_timeval(const struct timeval &tv);
		struct timeval get_timeval(void);

		bool is_zero_time();

		void set_max();

		static u64 nsec2cycles(u64 nsecs);
		static u64 cycles2nsec(u64 cycles);

		static timespec nsec2ts(uint64_t nsecs);

		static struct timespec read_ntptime(void);

		void sleep_absolute(bool exit_on_failure=true) const;
		void sleep_relative(bool exit_on_failure=true) const;

		static void nanosleep(timespec tspec, int flags = 0, bool exit_on_failure=true);
		static void nanosleep(u64 nsecs);
		static void ssleep(u64 secs);

		static u64 random_nr(uint64_t max);
		static void random_sleep(u64 nsecs);

		friend bool operator>(const time_unit &t1, const time_unit &t2);
		friend bool operator>=(const time_unit &t1, const time_unit &t2);
		friend bool operator<(time_unit &t1, time_unit &t2);
		friend bool operator==(const time_unit &t1, const time_unit &t2);

		friend time_unit operator*(const time_unit &t, const u64 multiplier);
		friend time_unit operator-(const time_unit &t1, const time_unit &t2);
		friend time_unit operator+(const time_unit &t1, const time_unit &t2);

		friend std::ostream& operator<< (std::ostream &out, const time_unit &t);

	private:
		// TODO: maybe make _use_cycles const?
		bool _use_cycles; // use processor cycles to measure/store time
		time_unit(u64);
		void init_cycles_timekeeping(void);

		//CLOCK_MONOTONIC_RAW only supports reading functions, not sleeping functions
		//(see kernel/posix-timers.c)
		//clockid_t time_unit::_clock_id = CLOCK_MONOTONIC_RAW;
		//clockid_t time_unit::_clock_id = CLOCK_REALTIME;
		constexpr static clockid_t _clock_id = CLOCK_MONOTONIC;
};

/**
 * inline functions (must be put in header)
 * https://isocpp.org/wiki/faq/inline-functions
 */

// TODO: the *2cycles should be checked for accuracy and precision of operation
inline
u64 time_unit::nsec2cycles(u64 nsecs)
{
	u64 rtn_val;
#if 0
	// _cycles can potentially be HUGE!, so check for overflow
	if (0 != nsecs) {
		if ( _cpu_hz > ULLONG_MAX / nsecs ) { // a * b would overflow
			printf("Can't handle overflow, exiting");
			exit(1);
		}
	}
#endif
	// TODO: only valid if _cpu_hz is initialized
	// TODO: check for overflow
	// TODO: would a different order of * and / be more precise?
	// TODO: _cpu_hz * 1E9 could be constexpr
	rtn_val = (u64)((double)nsecs / (double)1E9 * _cpu_hz);

	return rtn_val;
}

inline
u64 time_unit::cycles2nsec(u64 cycles)
{
	// TODO: check for overflow
	// TODO: would a different order of * and / be more precise?
	// TODO: _cpu_hz * 1E9 could be constexpr

	// (u64)((double)cycles / _cpu_hz * (double)1E9); // floating arithmetic
	// integer (more accurate for smaller values upto what?) (TODO:really need to check for overflow!)
	//return cycles * (u64)1E9 / (u64)_cpu_hz;
	return (u64)((double)cycles * (1E9 / _cpu_hz));
}

#endif // TIME_UNIT_H
