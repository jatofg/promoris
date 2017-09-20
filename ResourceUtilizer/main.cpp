/*
 * ResourceUtilizer - A small benchmark tool for testing ProMoRIS.
 * Copyright (C) 2017 J. Flaig
 * 
 * ResourceUtilizer is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * ResourceUtilizer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with ResourceUtilizer.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>

using namespace std;

std::string randomString(size_t length) {
	auto getRandomChar = []() -> char {
		const char characters[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
		const size_t clength = (sizeof (characters) - 1);
		return characters[ rand() % clength ];
	};
	std::string ret(length, 0);
	std::generate_n(ret.begin(), length, getRandomChar);
	return ret;
}

/*
 * 
 */
int main(int argc, char** argv) {

	// print help
	if (argc <= 1 || strcmp(argv[1], "-h") == 0) {
		std::cout << "Usage:\n"
				"resourceutilizer [-i ITERATIONS] [-s SLEEPTIME] [-c MULTIPLIER] "
				"[-m MEMORY] [-v] [-n NAME]\n\n"
				"Options explained:\n"
				"-i ITERATIONS -- run ITERATIONS iterations (instead of unlimited)\n"
				"-s SLEEPTIME -- sleep for SLEEPTIME seconds between 2 iterations (default 0)\n"
				"-c MULTIPLIER -- enable CPU test with multiplier MULTIPLIER\n"
				"-m MEMORY -- enable memory test with MEMORY MB to write to memory\n"
				"-v -- verbose mode (print start and end of iterations, tests and sleep)\n"
				"-n NAME -- name this instance and use it in verbose output (only useful together with -v)"
				<< std::endl;
		return 0;
	}

	int arg_mode = 0;
	long iterations = -1;
	long sleeptime = 0;
	long cpu_mult = 0;
	long mem_mb = 0;
	bool verbose = false;
	const char* iname = "ru";
	for (int i = 1; i < argc; ++i) {
		if (arg_mode == 1) {
			iterations = atoi(argv[i]);
			arg_mode = 0;
		} else if (arg_mode == 2) {
			sleeptime = atoi(argv[i]);
			arg_mode = 0;
		} else if (arg_mode == 3) {
			cpu_mult = atoi(argv[i]);
			arg_mode = 0;
		} else if (arg_mode == 4) {
			mem_mb = atoi(argv[i]);
			arg_mode = 0;
		} else if (arg_mode == 9) {
			iname = argv[i];
			arg_mode = 0;
		} else if (strcmp(argv[i], "-i") == 0) {
			arg_mode = 1;
		} else if (strcmp(argv[i], "-s") == 0) {
			arg_mode = 2;
		} else if (strcmp(argv[i], "-c") == 0) {
			arg_mode = 3;
		} else if (strcmp(argv[i], "-m") == 0) {
			arg_mode = 4;
		} else if (strcmp(argv[i], "-v") == 0) {
			verbose = true;
			arg_mode = 0;
		} else if (strcmp(argv[i], "-n") == 0) {
			arg_mode = 9;
		}
	}

	for (long il = iterations; il != 0; --il) {

		if (verbose) std::cout << iname << ": Starting iteration " << iterations - il << std::endl;

		// cpu
		if (cpu_mult > 0) {
			if (verbose) std::cout << iname << ": Starting CPU test" << std::endl;
			volatile unsigned long long iv;
			for (iv = 0; iv < 1000000000ULL * (unsigned long long) cpu_mult; ++iv);
			if (verbose) std::cout << iname << ": Done with CPU test" << std::endl;
		}

		// cpu + memory
		if (mem_mb > 0) {
			if (verbose) std::cout << iname << ": Starting memory test" << std::endl;
			std::vector<std::string> testmem;
			for (long i = 0; i < mem_mb; ++i) {
				testmem.push_back(randomString(1048576));
			}
			if (verbose) std::cout << iname << ": Done with memory test" << std::endl;
		}

		if (verbose) std::cout << iname << ": Done with iteration " << iterations - il << std::endl;

		// sleep
		if (sleeptime > 0) {
			if (verbose) std::cout << iname << ": Starting to sleep" << std::endl;
			sleep(sleeptime);
			if (verbose) std::cout << iname << ": Waking up from sleep" << std::endl;
		}

	}

	return 0;

}

