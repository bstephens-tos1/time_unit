#include <unistd.h>
#include <csignal>
#include <cstdio>

#include <limits>

#include "time_period.h"
#include "time_unit.h"

/**
 * DESCRIPTION:
 * If using cycles rather than timespec backing storage of time, then the
 * processor speed must be calculated fairly accurately. One way to do this is by
 * using ntp and counting the number of cycles over a reasonably long length of
 * time.
 */

// http://dbp-consulting.com/tutorials/SuppressingGCCWarnings.html
#if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 402
#define GCC_DIAG_STR(s) #s
#define GCC_DIAG_JOINSTR(x,y) GCC_DIAG_STR(x ## y)
# define GCC_DIAG_DO_PRAGMA(x) _Pragma (#x)
# define GCC_DIAG_PRAGMA(x) GCC_DIAG_DO_PRAGMA(GCC diagnostic x)
# if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406
#  define GCC_DIAG_OFF(x) GCC_DIAG_PRAGMA(push) \
    GCC_DIAG_PRAGMA(ignored GCC_DIAG_JOINSTR(-W,x))
    #  define GCC_DIAG_ON(x) GCC_DIAG_PRAGMA(pop)
    # else
    #  define GCC_DIAG_OFF(x) GCC_DIAG_PRAGMA(ignored GCC_DIAG_JOINSTR(-W,x))
    #  define GCC_DIAG_ON(x)  GCC_DIAG_PRAGMA(warning GCC_DIAG_JOINSTR(-W,x))
    # endif
    #else
    # define GCC_DIAG_OFF(x)
    # define GCC_DIAG_ON(x)
#endif

#ifdef __cplusplus
	#define ISVARSIGNED(V) (std::numeric_limits<decltype(V)>::is_signed)
#else
	#define ISVARSIGNED(V) ({ typeof(V) _V = -1; _V < 0 ? 1 : 0; })
#endif
#define OUTPUT_MSG_NUM(msg, var) do { \
	printf(msg); \
	printf(" "); \
	int S = ISVARSIGNED(var); \
	if (S) printf("%lld", (long long int)var); \
	else printf("%llu", (unsigned long long int)var); \
	printf ("\n"); \
} while(0)

/*
	_Pragma("GCC diagnostic push"); \
	_Pragma("GCC diagnostic ignored \"-Wtype-limits\""); \
	OUTPUT_MSG_NUM(#var, var); \
	_Pragma("GCC diagnostic pop"); \
*/

#define OUTPUT_NUM(var) do { \
	GCC_DIAG_OFF(type-limits) \
	OUTPUT_MSG_NUM(#var, var); \
	GCC_DIAG_ON(type-limits) \
} while(0)

bool done = false;

void SIG_handler(int)
{
	done = true;
}

int main()
{
	struct sigaction sa;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	// Register the handler
	sa.sa_handler = SIG_handler;
	sigaction(SIGTERM, &sa, 0);
	sigaction(SIGINT, &sa, 0);  // ctrl-c

	printf("press ctrl-c to stop calibration\n");

	time_period time(true);

	// holds timespecs from ntp server
	time_unit tu_start(false);
	time_unit tu_stop(false);

	uint64_t cpu_hz;
	struct timespec ts_start;

	time.start();
	ts_start = time_unit::read_ntptime();
	tu_start.set_timespec(ts_start);

	while(!done) {

		sleep(1);

		time.stop();
		struct timespec ts_stop = time_unit::read_ntptime();
		tu_stop.set_timespec(ts_stop);

		// double - precision = 53 bits (10 bits exponent, 1 bit sign)
		// 2^53/10^9/60/60/24
		// 104.24999137431703703703 days representable by double,
		// assuming nanoseconds, before losing precision
		GCC_DIAG_OFF(conversion);
		cpu_hz = (double)time.get_diff_cycles() / (double)((tu_stop - tu_start).get_nanosecs()) * 1E9;
		GCC_DIAG_ON(conversion);

		GCC_DIAG_OFF(type-limits);
		OUTPUT_NUM(cpu_hz);
		GCC_DIAG_ON(type-limits);
	}

	time.stop();
	tu_stop.set_timespec(time_unit::read_ntptime());

	GCC_DIAG_OFF(conversion);
	cpu_hz = (double)time.get_diff_cycles() / (double)(tu_stop.get_nanosecs() - tu_start.get_nanosecs()) * 1E9;
	GCC_DIAG_ON(conversion);

	GCC_DIAG_OFF(type-limits);
	OUTPUT_NUM(cpu_hz);
	GCC_DIAG_ON(type-limits);

	return 0;
}
