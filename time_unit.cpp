#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/udp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <limits>
#include <random>
#include <chrono>
using namespace std;

#include <boost/numeric/conversion/cast.hpp>
using boost::numeric_cast;

#include "x86_tsc.h"
#include "gcc_helpers/fs.h"
#include "gcc_helpers/cpp_helpers.h"
#include "gcc_helpers/debug.h"

#include "time_unit.h"

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

//double time_unit::_cpu_hz = 3010643978.40235294117647058823;
double time_unit::_cpu_hz = 0;

// default instantiation of time_unit objects
bool time_unit::default_use_cycles = compile_default_use_cycles;

constexpr auto NSEC_PER_SEC((s64)1E9);

const time_unit time_unit::ONE_MICRO = time_unit((u64)1E3);

time_unit
time_unit::SECS(u64 secs, bool cycles_store)
{
    time_unit rtn(cycles_store);
    rtn.set_seconds(secs);
    return rtn;
}

time_unit
time_unit::MILLISECS(u64 msecs, bool cycles_store)
{
	time_unit rtn(cycles_store);
	rtn.set_millisecs(msecs);
	return rtn;
}

time_unit
time_unit::MICROSECS(u64 usecs, bool cycles_store)
{
	time_unit rtn(cycles_store);
	rtn.set_microsecs(usecs);
	return rtn;
}

time_unit
time_unit::NANOSECS(u64 nsecs, bool cycles_store)
{
	time_unit rtn(cycles_store);
	rtn.set_nanosecs(nsecs);
	return rtn;
}

time_unit
time_unit::NOW(bool cycles_store)
{
	time_unit rtn(cycles_store);
	rtn.set_now();
	return rtn;
}

/**
 * timestamp format is expected to be <sec>.<fraction of second> (e.g., 23.829)
 */
time_unit
time_unit::from_timestamp(const string &timestamp)
{
	double ts_dbl;
	from_string<double>(ts_dbl, timestamp);

	return time_unit::NANOSECS((u64)(ts_dbl * 1E9));
}

/**
 * Get current date/time as a string.
 *
 * default format is YYYY-MM-DD.HH:mm:ss
 */
string
time_unit::now_str(std::string format)
{
    time_t now = time(0);
    struct tm loc_time = *localtime(&now);
    char buf[40];

    strftime(buf, sizeof(buf), format.c_str(), &loc_time);

    return buf;
}

time_unit::time_unit()
	: time_unit(default_use_cycles)
{}

time_unit::time_unit(bool cycles_time_storage)
	: _use_cycles(cycles_time_storage)
{
	// use _cycles -or- _timespec as base timekeeping
	// in general, try to use _timespec due to much larger time interval coverage
	// TODO: _cycles may be only able to represent a much smaller amount of time than _timespec

	// NOTE: if not using cycles and using clock_gettime() then it may be
	// worthwhile to output the clocksource found at:
	// /sys/devices/system/clocksource/clocksource0/current_clocksource
#ifdef ANDROID
	if (_use_cycles) {
		cout << "cannot use cycles on ARM" << endl;
		exit(EXIT_FAILURE);
	}
#endif

	_timespec.tv_sec = 0;
	_timespec.tv_nsec = 0;

	_cycles = 0;

	// _cpu_hz only needed if cycles are used for timekeeping
	if (_use_cycles)
		init_cycles_timekeeping();
}

time_unit::time_unit(u64 nanosecs)
	: time_unit()
{
	this->set_nanosecs(nanosecs);
}

void
time_unit::init_cycles_timekeeping()
{
	// TODO: may be a race condition if multiple threads instatiate objects at the same time.
	// Use double check locking? (easy in C++11 due to memory model)
	// i.e., cpu_hz class that implements lazy initialization
	if (0 == _cpu_hz) {
		if (!init_hz_from_file()) {
			init_hz(4);
		}
	}
}

bool
time_unit::init_hz_from_file()
{
	// TODO: just use home dir for now
	// should allow it to be specified by caller?
	string home_dir = getenv ("HOME");
	string file_name = home_dir + "/.cpu_hz";

	if (!file_exists(file_name)) {
		cout << home_dir << "/.cpu_hz does not exist!" << endl;
		return false;
	}

	string rtn_string;
	str_from_file(file_name, rtn_string);

	u64 my_int;
	if (!from_string<>(my_int, rtn_string)) {
		// TODO: check proper conversion without loss of precision
		_cpu_hz = numeric_cast<decltype(_cpu_hz)>(my_int);
		printf("_cpu_hz (from file): %10.2f\n", _cpu_hz);
		return true;
	} else {
		cout << ".cpu_hz file exists, but unable to parse, exiting.";
		exit(EXIT_FAILURE);
	}

	cout << "Bad reading from " << home_dir << "/.cpu_hz" << endl;;
	return false;
}

// TODO: should be able to call this without object
u64
time_unit::init_hz(int seconds)
{
	u64 cyc_start, cyc_stop;
	struct timespec ts_start, ts_stop;

	printf("initializing _cpu_hz for %d seconds\n", seconds);
	cyc_start = read_tsc();
	ts_start = time_unit::read_ntptime();

	sleep(seconds);

	cyc_stop = read_tsc();
	ts_stop = time_unit::read_ntptime();

	u64 elapsed_cycles = cyc_stop - cyc_start;
	// only really need secs for accuracy
	u64 elapsed_sec = ts_stop.tv_sec - ts_start.tv_sec;

	_cpu_hz = (double)elapsed_cycles/(double)elapsed_sec;

	printf("_cpu_hz: %10.2f\n", _cpu_hz);

	return numeric_cast<u64>(_cpu_hz);
}

/**
 * REF:
 * http://www.abnormal.com/~thogard/ntp/
 * http://souptonuts.sourceforge.net/code/queryTimeServer.c.html
 * http://www.ntp.org/
 * http://www.scss.com.au/family/andrew/gps/ntp/ (GPS ntp server)
 *
 * NOTES:
 * To display time associated with the number of seconds use:
 * date -u -d @<seconds>
 */
struct timespec
time_unit::read_ntptime()
{
	const bool verbose = false;
	struct timespec rtn;
	rtn.tv_sec = 0;
	rtn.tv_nsec = 0;

	// network time server ip address
	// 71.6.202.221
	// 64.73.32.134
	// 3.north-america.pool.ntp.org (50.116.38.157)
	//string ip = "216.184.20.82";
	string ip = "50.116.38.157";

	// NTP uses port 123
	const uint16_t port = 123;
	int sockfd;
	struct sockaddr_in dst;

	const int maxlen = 512;
	// NTP Data Format
	// ---------------------------------------------
	// LI - alarm condition (clock not synchronized)
	// version - 4
	// mode - client
	unsigned char msg[48]={0xE3,0,0,0,0,0,0,0,0};
	u32 buf[maxlen]; // returned buffer (each array element should be 32-bits)

	// setup the destination structure
	memset((char*)&dst, 0, sizeof(dst));

	// convert the text address to binary form
	if(!inet_pton(AF_INET, ip.c_str(), &dst.sin_addr)) {
		fprintf(stderr, "error1\n");
		return rtn;
	}
	// AF_INET - IPv4 Internet protocols
	dst.sin_family = AF_INET;
	// HACK: uint16_t cast should not be needed broken for certain libc versions
	dst.sin_port = (uint16_t)htons(port);

	// setup the endpoint
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		fprintf(stderr, "error3\n");
		return rtn;
	}

	// send the request packet
	auto sent_amt = sendto(sockfd, msg, sizeof(msg), 0, rval_addr(memcpy<struct sockaddr>(dst)), sizeof(dst));
	if (sent_amt != sizeof(msg)) {
		fprintf(stderr, "error4\n");
		return rtn;
	}

	// get the response
	if (verbose) {
		printf("waiting for packet\n");
	}
	ssize_t bytes_recvd = recv(sockfd, buf, sizeof(buf), 0);
	if (-1 == bytes_recvd) {
		fprintf(stderr, "error5\n");
		return rtn;
	}
	// close the connection
	close(sockfd);

	if (verbose)
		cout << "bytes received: " << bytes_recvd << endl;

/*
	for(int i=0; i<12; i++) {
		printf("%d\t%-8u\n",i,ntohl(buf[i]));
	}
*/

	// offset (in seconds) between January 1st, 1900(NTP)
	// and January 1st, 1970(UNIX) (2208988800).
	const auto UNIX_NTP_DIFF(2208988800LU);

	// long in ntohl is 32 bits
	// 10 - transmit timestamp (sec)
	// 11 - transmit timestamp (fractional seconds)
	rtn.tv_sec = ntohl(buf[10]) - UNIX_NTP_DIFF;

	// ntp returns 32-bits indicating the fraction of a second
	// since it's 32-bits, divide the fractional part by 2^32 to
	// get the fraction
	const double POW_2_32 = 4294967296;
	double frac_sec = ntohl(buf[11]);
	//const double nsec_per_fractional = 1E9/POW_2_32;
	// Overflow should not be an issue (~233 psecs per fractional unit)
	rtn.tv_nsec = (decltype(rtn.tv_nsec))(frac_sec / POW_2_32 * 1E9);

	if (verbose) {
		OUTPUT_NUM(rtn.tv_sec);
		OUTPUT_NUM(rtn.tv_nsec);
	}

	return rtn;
}

bool time_unit::using_cycles() const
{
	return _use_cycles;
}

u64
time_unit::get_nanosecs() const
{
	if (_use_cycles) {
		// TODO: would like to make the caller explicitly state that this
		// conversion is desired, since some accuracy is likely to be lost from
		// inaccuracy of cpu_hz and floating point arithmetic (especially for
		// large time values)
		return cycles2nsec(this->_cycles);
	} else {
		return ((u64)_timespec.tv_sec * NSEC_PER_SEC) + (u64)_timespec.tv_nsec;
	}
}

u64
time_unit::get_microsecs() const
{
	return this->get_nanosecs() / (u64)1E3;
}

// TODO: doubles are imprecise
// return u64?
double
time_unit::get_millisecs()
{
	return (double)this->get_nanosecs() / (double)1E6;
}

// TODO: doubles are imprecise
// return u64?
double
time_unit::get_seconds()
{
	return (double)this->get_nanosecs() / (double)1E9;
}

static bool
is_normalized(const struct timespec *ts)
{
	if (ts->tv_nsec >= (s64)1E9) return false;

	if (ts->tv_nsec < 0) return false;

	if (ts->tv_sec < 0) return false;

	return true;
}

/**
 * Put sec and nsec into a timespec.
 *
 * Overwrites existing values in ts.
 */
static void
set_normalized_timespec(struct timespec *ts, s64 sec, s64 nsec)
{
// NOTE: divides work faster for larger numbers, smaller numbers will
// work faster with while loop
# if 1
	if (nsec < 0) {
		// make nsec positive
		s64 borrow_sec = nsec / (s64)-1E9;
		// we are less than 0, so we have to borrow at least 1 sec
		borrow_sec += 1;

		// TODO: check for neg. overflow
		sec -= borrow_sec;
		nsec += (borrow_sec * (s64)1E9);
	}

	// at this point, nsec must be positive, but sec could be negative

	if (sec < 0) {
		// TODO: not sure what to do about negative time?
		// ts->tv_sec is signed, so it should be fine to return a negative seconds value
		// however much of the code relying on time_unit does not expect neg. time
		// maybe set a flag that will remove warning and exit below?
		cerr << "sec < 0, not sure if this is expected?" << endl;
		exit(EXIT_FAILURE);
		sec = 0;
	}

	// ensure nsec input arg is < 1E9 by putting any above 1E9 into secs
	u64 new_sec = nsec / (u64)1E9;
	u64 new_nsec = nsec - (new_sec * NSEC_PER_SEC);

	// TODO: check for overflow
	ts->tv_sec = new_sec + sec;
	ts->tv_nsec = new_nsec;
#else

/**
 * From linux kernel (include/linux/time.h)
 */
	while (nsec >= NSEC_PER_SEC) {
		nsec -= NSEC_PER_SEC;
		++sec;
	}
	while (nsec < 0) {
		nsec += NSEC_PER_SEC;
		--sec;
	}
	// TODO: check for overflow
	ts->tv_sec = (long)sec;
	ts->tv_nsec = (time_t)nsec;
#endif
}

int
time_unit::set_nanosecs(u64 nsecs)
{
	if (_use_cycles) {
		_cycles = nsec2cycles(nsecs);
	} else {
		// TODO: check for overflow of nsecs (u64 -> s64)
		set_normalized_timespec(&_timespec, 0, nsecs);
	}

	return 0;
}

int
time_unit::set_microsecs(u64 usecs)
{
	return set_nanosecs(usecs * (u64)1E3);
}

int
time_unit::set_millisecs(u64 msecs)
{
	return set_nanosecs(msecs*(u64)1E6);
}

int
time_unit::set_seconds(u64 secs)
{
	return set_nanosecs(secs*(u64)1E9);
}

int
time_unit::set_time_unit(const time_unit &tu)
{
	// TODO: check if both this and tu both _use_cycles
	if (_use_cycles) {
		_cycles = tu._cycles;
	} else {
		_timespec = tu._timespec;
	}

	return 0;
}

void
time_unit::set_now()
{
	if (_use_cycles) {
		_cycles = read_tsc();
	} else {
		CHECK(clock_gettime(_clock_id, &_timespec));
	}
}

/**
 * @rhs:
 *     time_unit to subtract
 *
 * @return:
 *     this - rhs
 *
 * DESCRIPTION:
 * Take a time unit and subtract it from this one.
 *
 * NOTE:
 * see set_normalized_timespec() handles for handling of neg. rtn value
 */
time_unit
time_unit::subtract(const time_unit &rhs) const
{
	// this is lhs
	time_unit rtn_val = *this;  // sets configuration of rtn value (e.g., _use_cycles)

	if (_use_cycles) {
		// TODO: ensure both time_units are using _use_cycles

		rtn_val._cycles = this->_cycles - rhs._cycles;

		// check for wrap
		if (rtn_val._cycles > this->_cycles) {
			cout << "negative _cycles will result (incorrect value of _cpu_hz?), exiting." << endl;
			OUTPUT_NUM(this->_cpu_hz);
			OUTPUT_NUM(this->_cycles);
			OUTPUT_NUM(rhs._cycles);
			// maybe just return/set to 0 if close?
			exit(EXIT_FAILURE);
		}
	} else {
		set_normalized_timespec(
				&rtn_val._timespec,
				this->_timespec.tv_sec - rhs._timespec.tv_sec,
				this->_timespec.tv_nsec - rhs._timespec.tv_nsec
		);
	}

	return rtn_val;
}

/**
 * timespec_add_ns - Adds nanoseconds to a timespec
 * @a:      pointer to timespec to be incremented
 * @ns:     unsigned nanoseconds value to be added
 *
 * from Linux kernel include/linux/time.h
 */
static inline void
timespec_add_ns(struct timespec *a, u64 ns)
{
	u64 secs = ns / (u64)1E9;
	u64 nsecs = ns - (secs * (u64)1E9);

	secs += a->tv_sec;
	nsecs += a->tv_nsec;

	set_normalized_timespec(a, secs, nsecs);
}

static inline void
timespec_add_sec(struct timespec *a, time_t sec)
{
	a->tv_sec += sec;
}

/**
 * @return - new time_unit with the rhs added to this
 */
time_unit
time_unit::add(const time_unit &rhs) const
{
	time_unit rtn_val = *this;

	if (_use_cycles) {
		rtn_val._cycles = this->_cycles + rhs._cycles;
		// wrap if sum is less (by unsigned comparison) than either of the operands
		if (rtn_val._cycles < rhs._cycles) {
			cout << "Can't handle _cycles wrap (negative), exiting." << endl;
			exit(EXIT_FAILURE);
		}
	} else {
		timespec_add_ns(&rtn_val._timespec, rhs.get_nanosecs());
	}

	return rtn_val;
}

int
time_unit::set_timespec(const struct timespec &ts)
{
	this->set_nanosecs(ts.tv_nsec);
	// eventually calls normalize_timespec
	this->add_sec(ts.tv_sec);

	return 0;
}

struct timespec
time_unit::get_timespec() const
{
	if (_use_cycles) {
		cerr << "get_timespec() not implemented for _use_cycles" << endl;
		exit(EXIT_FAILURE);
	}

	// verify the timespec is normalized, should be normalized with every set,
	// therefore we can make this method const
	//normalize_timespec(&_timespec); // NOTE: probably not needed
	if (!is_normalized(&_timespec)) {
		cerr << "ts not normalized" << endl;
		exit(EXIT_FAILURE);
	}

	return _timespec;
}

int
time_unit::set_timeval(const struct timeval &tv)
{
	struct timespec ts;

	ts.tv_sec = tv.tv_sec;
	// TODO: overflow possible?  Maybe normalize first
	ts.tv_nsec = tv.tv_usec * 1000;

	this->set_timespec(ts);

	return 0;
}

struct timeval
time_unit::get_timeval()
{
	struct timespec ts;

	u64 nsecs = this->get_nanosecs();
	set_normalized_timespec(&ts, 0, nsecs);

	struct timeval rtn_val;
	rtn_val.tv_sec = ts.tv_sec;
	rtn_val.tv_usec = ts.tv_nsec / 1000;

	return rtn_val;
}

void
time_unit::add_ns(u64 nsecs)
{
	if (_use_cycles) {
		_cycles += nsec2cycles(nsecs);
	} else {
		timespec_add_ns(&_timespec, nsecs);
	}
}

void
time_unit::add_sec(u64 secs)
{
	this->add_ns(secs * (u64)1E9);
}

void
time_unit::sub_sec(u64 secs)
{
	this->set_time_unit(this->subtract(SECS(secs)));
}

bool
time_unit::is_zero_time()
{
	if (_use_cycles) {
		if (_cycles == 0) {
			return true;
		}
	} else {
		if (_timespec.tv_sec == 0 && _timespec.tv_nsec == 0) {
			return true;
		}
	}

	return false;
}

void
time_unit::set_max()
{
	if (_use_cycles) {
		_cycles = numeric_limits<decltype(_cycles)>::max();
	} else {
		_timespec.tv_sec = numeric_limits<decltype(_timespec.tv_sec)>::max();
		_timespec.tv_nsec = 0; // NOTE: just set to zero since the max val won't add much
	}
}

struct timespec
time_unit::nsec2ts(uint64_t nsecs)
{
	time_unit rtn(false);

	rtn.set_nanosecs(nsecs);

	return rtn.get_timespec();
}

void
time_unit::sleep_absolute(bool exit_on_failure) const
{
	// check if *sleep() is even necessary (time may have passed)
	if (time_unit::NOW() >= *this) {
		//cout << "skipping sleep" << endl;
		return;
	}

	this->nanosleep(this->get_timespec(), TIMER_ABSTIME, exit_on_failure);
}

void
time_unit::sleep_relative(bool exit_on_failure) const
{
	this->nanosleep(this->get_timespec(), 0, exit_on_failure);
}

void
time_unit::nanosleep(timespec tspec, int flags, bool exit_on_failure)
{
	long rtn;

	// HACK: may not be a good idea to reuse TIMER_ABSTIME from time.h, for now
	// since TIMER_ABSTIME = 1 it should be ok
	if (flags == TIMER_ABSTIME)
		// last argument can be used to get remaining time
		rtn = ::clock_nanosleep(_clock_id, TIMER_ABSTIME, &tspec, NULL);

		// bypass glibc if needed
		//rtn = syscall(SYS_clock_nanosleep, _clock_id, TIMER_ABSTIME, &tspec, NULL);
	else
		rtn = ::nanosleep(&tspec, NULL);

	if (rtn) {
		cout << "[clock_]nanosleep() failed with value (" << rtn << ")." << endl;
		if (exit_on_failure) {
			cout << "Exiting." << endl;
			exit(EXIT_FAILURE);
		}
	}
}

void
time_unit::nanosleep(u64 nsecs)
{
	// NOTE: easier/better to just use timespec since it is created locally here and OS requires timespec
	time_unit t_sleep(false);

	t_sleep.set_nanosecs(nsecs);

	time_unit::nanosleep(t_sleep.get_timespec());
}

void
time_unit::ssleep(u64 secs)
{
	timespec tspec;

	tspec.tv_sec = secs;
	tspec.tv_nsec = 0;

	::nanosleep(&tspec, NULL);
}

u64
time_unit::random_nr(uint64_t max)
{
	auto distribution = uniform_int_distribution<uint64_t>(0, max);
	default_random_engine generator(chrono::system_clock::now().time_since_epoch().count());

	auto number = distribution(generator);

	return number;
}

void
time_unit::random_sleep(u64 nsecs)
{
	time_unit::nanosleep(random_nr(nsecs));
}

bool operator> (const time_unit &t1, const time_unit &t2)
{
	// TODO: check if t1 and t2 have differing _use_cycles?
	if (t1._use_cycles && t2._use_cycles) {
		return t1._cycles > t2._cycles;
	} else {
		return t1.get_nanosecs() > t2.get_nanosecs();
	}
}

bool operator>=(const time_unit &t1, const time_unit &t2)
{
	// TODO: check if t1 and t2 have differing _use_cycles?
	if (t1._use_cycles && t2._use_cycles) {
		return t1._cycles >= t2._cycles;
	} else {
		return t1.get_nanosecs() >= t2.get_nanosecs();
	}
}

bool operator==(const time_unit &t1, const time_unit &t2)
{
	// TODO: check if t1 and t2 have differing _use_cycles?
	if (t1._use_cycles && t2._use_cycles) {
		return t1._cycles == t2._cycles;
	} else {
		return t1.get_nanosecs() == t2.get_nanosecs();
	}
}

bool operator< (time_unit &t1, time_unit &t2)
{
	// TODO: check if t1 and t2 have differing _use_cycles?
	if (t1._use_cycles && t2._use_cycles) {
		return t1._cycles < t2._cycles;
	} else {
		return t1.get_nanosecs() < t2.get_nanosecs();
	}
}

time_unit operator- (const time_unit &t1, const time_unit &t2)
{
	return t1.subtract(t2);
}

time_unit operator+ (const time_unit &t1, const time_unit &t2)
{
	return t1.add(t2);
}

time_unit operator* (const time_unit &lhs, const u64 multiplier)
{
	time_unit rtn(lhs);
	// TODO: check for overflow?
	rtn.set_nanosecs(rtn.get_nanosecs() * multiplier);

	return rtn;
}

ostream& operator<< (std::ostream &out, const time_unit &t)
{
	// out << "_cycles(" << t._use_cycles << ") ";

	if (t._use_cycles) {
		out << "_cycles(" << t._cycles << ") ";
	}
	out << "get_nanosecs(" << t.get_nanosecs() << ")";

	return out;
}

// Temporarily unused code
/*
int
time_unit::use_cycles(bool choice)
{
	if (choice == _use_cycles) {
		return 0;
	}

	time_unit tmp(choice);
	tmp.set_nanosecs(this->get_nanosecs());
	*this = tmp;

	return 0;
}
*/

/*
static void
normalize_timespec(struct timespec *ts)
{
	set_normalized_timespec(ts, ts->tv_sec, ts->tv_nsec);
}
*/
