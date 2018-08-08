#include <cstdlib> // EXIT_SUCCESS

#include <iostream>
using namespace std;

#include "time_unit.h"
#include "time_period.h"

int main()
{
	time_unit tu;
	time_period tp;

	tu.set_now();

	tu.add_ns(2 * (uint64_t)1E9);
	cout << "going to sleep ..." << endl;
	tp.start();
	tu.sleep_absolute();
	tp.stop();
	cout << "measured sleep time: " << tp.get_diff_nsec() << " (nsecs)" << endl;

	return EXIT_SUCCESS;
}
