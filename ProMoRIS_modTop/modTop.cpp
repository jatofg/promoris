/*
 * This file is part of ProMoRIS
 *   - A Process Monitoring and Resource Information System.
 * Copyright (C) 2017 J. Flaig
 * 
 * ProMoRIS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * ProMoRIS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with ProMoRIS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "modTop.h"
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <fstream>

// definitions
param_t modTop::param;
bool modTop::stopmon;
std::thread modTop::t1;
std::vector<std::vector<std::string>> modTop::values;
std::mutex modTop::values_m;

const std::vector<std::string> modTop::explodeTop(const std::string& line) {
	std::string b = "";
	std::vector<std::string> v;

	for(auto currentChar : line) {
		if(currentChar != ' ' && currentChar != '\n') b += currentChar;
		else if(currentChar == ' ' && b != "") {
			v.push_back(b);
			b = "";
		}
	}
	if (b != "") v.push_back(b);

	return v;
}

int modTop::runMon() {

	// file handle to read command with popen
	FILE *fp;
	// one line of the top output
	char line[1000];
	// string array of the relevant values from the current line
	std::string current_values[3];
	// shell command to execute
	std::string cmd;
	// shell command as C char* array
	const char *cmdc;
	// popen/pclose status
	int status;
	// space to save the values from the current line
	std::vector<std::string> topvalues;

	// start top and open pipe
	cmd = "COLUMNS=998 LINES=50 LC_NUMERIC=en_US top -b -d " + 
			std::to_string(param.interval) + " -p " + param.pid;
	cmdc = cmd.c_str();
	fp = popen(cmdc, "r");

	if (fp == NULL) {
		std::cerr << "modTop: Opening command failed" << std::endl;
		return 1;
	}
	
	// open tracefile if option is set
	std::ofstream tracefile;
	bool trace = false;
	if (param.options.size() > 0) {
		trace = true;
		tracefile.open(param.options[0]);
		if (!tracefile.is_open()) {
			std::cerr << "modTop: trace file could not be opened" << std::endl;
			return 1;
		}
	}

	int lineCount = 0;

	// read and analyze the relevant lines of the top output
	while (fgets(line, 1000, fp)) {
		
		// write to tracefile
		if (trace) tracefile << line;
		
		if (lineCount == 7) {

			// split the line into single values
			topvalues = explodeTop(line);
			
			lineCount = -1;
			
			std::stringstream timeGen;
			timeGen << time(nullptr);

			// write current timestamp and relevant values to values array
			std::lock_guard<std::mutex> values_guard(values_m);
			values.push_back(std::vector<std::string> {timeGen.str(),
					topvalues[4], topvalues[5], topvalues[6], 
					topvalues[8], topvalues[9]});
			
		}
		else ++lineCount;
		
		if (stopmon == true) break;
	}
	
	stopmon = true;
	
	if (trace) tracefile.close();

	status = pclose(fp);
	
	if (status == -1) {
		std::cerr << "modTop: Error closing pipe" << std::endl;
		return 1;
	}

	return 0;

}

void modTop::setParameters(param_t sparam) {
	param = sparam;

}

int modTop::startMon() {
	stopmon = false;
	// start monitoring thread (runMon)

	t1 = std::thread(runMon);

	return 0;
}

int modTop::stopMon() {
	stopmon = true;
	t1.join();
	return 0;

}

bool modTop::running() {
	return !stopmon;
}

std::vector<std::vector<std::string>> * modTop::getValuesPtr() {
	return &values;
}

std::mutex * modTop::getValuesMPtr() {
	return &values_m;
}

std::vector<std::string> modTop::getLabels() {
	return {"time", "virt", "res", "shr", "pcpu", "pmem"};
}

std::vector<int> modTop::getColMaxSize() {
	return {10, 15, 15, 15, 5, 5};
}

extern "C" module* createMod() {
	return new modTop;
}

extern "C" void destroyMod(module* p) {
	delete p;
}

extern "C" const char* getHelp() {
	return "Module usage:\n"
			"-m MODTOP [-p PREFIX] [-c time] [-c virt] [-c res] [-c shr] "
			"[-c pcpu] [-c pmem] [-o TRACEFILE]\n\n"
			"Columns:\n"
			"time: UNIX timestamp when snapshot was taken\n"
			"virt: Virtual memory usage (KiB)\n"
			"res: Resident memory usage (KiB)\n"
			"shr: Shared memory usage (KiB)\n"
			"pcpu: CPU usage (%)\n"
			"pmem: Resident memory usage (%)\n\n"
			"Options and defaults:\n"
			"TRACEFILE -- if set, writes the raw batch output of top to TRACEFILE";
}
