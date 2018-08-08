#include <sys/time.h>
#include <cstdlib> // EXIT_SUCCESS

#include <iostream>
using namespace std;

#include "time_unit.h"

void print_ts(const struct timespec& ts)
{
	cout << "tv_sec: " << ts.tv_sec << endl;
	cout << "tv_nsec: " << ts.tv_nsec << endl;
}

int main(int, char* [])
{
	auto ts = time_unit::read_ntptime();

	cout << "ntp: " << endl;
	print_ts(ts);

	time_unit ntp_time(false);
	ntp_time.set_timespec(ts);

	struct timeval now_tv;
	gettimeofday(&now_tv, NULL);
	time_unit now(false);
	now.set_timeval(now_tv);
	cout << "NOW: " << endl;
	print_ts(now._timespec);

	cout << "now: " << now << endl;
	cout << "ntp: " << ntp_time << endl;
	time_unit diff(false);
	if (now > ntp_time) {
		cout << "difference: (setting time EARLIER by the following)" << endl;
		diff = now - ntp_time;
	} else {
		cout << "difference: (setting time LATER by the following)" << endl;
		diff = ntp_time - now;
	}
	//cout << diff.get_seconds() << endl;
	cout << "diff: " << endl;
	print_ts(diff._timespec);

	auto tv = ntp_time.get_timeval();
	int rtn = settimeofday(&tv, NULL);
	cout << "rtn: " << rtn << endl;

	return EXIT_SUCCESS;
}
