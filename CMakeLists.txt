cmake_minimum_required(VERSION 2.6)

# compiler options
	# macros
		add_definitions(-D_FILE_OFFSET_BITS=64)

	# code checks
		add_definitions(-std=c++11)
		add_definitions(-Wall -Wextra)
		add_definitions(-Wconversion)
		add_definitions(-Wshadow)
		#add_definitions(-pedantic)
		#add_definitions(-ansi)
		add_definitions(-Wno-long-long) # don't warn if long long types are used
		add_definitions(-Wunused-macros)
		# check for multiple definitions (multiple definitions in different compilation units)
		add_definitions(-fno-common)
		add_definitions(-Wstrict-aliasing=2)

		# overflow/wrap
			add_definitions(-fstrict-overflow)
			add_definitions(-Wstrict-overflow=5)
			#add_definitions(-ftrapv)

	# optimizations
		#add_definitions(-funroll-loops) # not enabled by -O3
		add_definitions(-O2)
		add_definitions(-fomit-frame-pointer)
		#add_definitions(-g) # debug symbols

# libraries
	add_library(time_period time_period.cpp time_unit.cpp cpu_consumer.cpp)

# executables
	# nanosleep_test
	add_executable(nanosleep_test nanosleep_test.cpp)
		target_link_libraries(nanosleep_test time_period)
		target_link_libraries(nanosleep_test -lrt)

	# cpu_hz
	add_executable(cpu_hz cpu_hz.cpp)
		target_link_libraries(cpu_hz time_period)
		target_link_libraries(cpu_hz -lrt)

	# ntp
	add_executable(ntp ntp.cpp)
		target_link_libraries(ntp time_period)
		target_link_libraries(ntp -lrt)
