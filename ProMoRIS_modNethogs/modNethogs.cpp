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

#include "modNethogs.h"
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <regex>
#include <cstring>
#include <fstream>

// definitions
param_t modNethogs::param;
bool modNethogs::stopmon;
std::thread modNethogs::t1;
std::vector<std::vector<std::string>> modNethogs::values;
std::mutex modNethogs::values_m;

int modNethogs::runMon() {

	// file handle to read command with popen
	FILE *fp;
	// one line of the top output
	char line[1000];
	// string array of the relevant values from the current line
	std::string current_values[3];
	// popen/pclose status
	int status;
	// space to save the values from the current line
	std::vector<std::string> nhvalues;

	// start nethogs and open pipe
	std::string n_iface;
	if (param.options.size() > 0) n_iface = param.options[0] + " ";
	else n_iface = "";
	std::string cmd = "COLUMNS=998 LINES=50 LC_NUMERIC=en_US nethogs " + n_iface + "-d " + std::to_string(param.interval) + " -t 2>/dev/null";
	const char * cmdc = cmd.c_str();
	fp = popen(cmdc, "r");

	if (fp == NULL) {
		std::cerr << "modNethogs: Opening command failed" << std::endl;
		return 1;
	}

	int lineCount = 0;

	// open tracefile if option is set
	std::ofstream tracefile;
	bool trace = false;
	if (param.options.size() > 1) {
		trace = true;
		tracefile.open(param.options[1]);
		if (!tracefile.is_open()) {
			std::cerr << "modNethogs: trace file could not be opened" << std::endl;
			return 1;
		}
	}

	while (fgets(line, 1000, fp) != NULL) {

		// write to tracefile
		if (trace) tracefile << line;

		// extract the values we need via regex
		std::regex explode("/" + (param.pid) + "/[0-9]+\\t+([0-9]+\\.?[0-9]*)\\t+([0-9]+\\.?[0-9]*)");
		std::smatch nethogs_values;
		std::string line_s = line;
		// if the line contains information about the process being monitored
		if (std::regex_search(line_s, nethogs_values, explode)) {

			std::stringstream timeGen;
			timeGen << time(nullptr);

			// write current timestamp and relevant values to values array
			std::lock_guard<std::mutex> values_guard(values_m);
			values.push_back(std::vector<std::string>{timeGen.str(),
				nethogs_values[1], nethogs_values[2]});
		}

		if (stopmon == true) break;
	}

	stopmon = true;

	if (trace) tracefile.close();

	status = pclose(fp);

	if (status == -1) {
		std::cerr << "modNethogs: Error closing pipe" << std::endl;
		return 1;
	}

	return 0;

}

void modNethogs::setParameters(param_t sparam) {
	param = sparam;

}

int modNethogs::startMon() {
	stopmon = false;
	
	// start monitoring thread (runMon)
	t1 = std::thread(runMon);

	return 0;
}

int modNethogs::stopMon() {
	stopmon = true;
	t1.join();
	return 0;
}

bool modNethogs::running() {
	return !stopmon;
}

std::vector<std::vector<std::string>> *modNethogs::getValuesPtr() {
	return &values;
}

std::mutex * modNethogs::getValuesMPtr() {
	return &values_m;
}

std::vector<std::string> modNethogs::getLabels() {
	return {"time", "sent", "received"};
}

std::vector<int> modNethogs::getColMaxSize() {
	return {10, 10, 10};
}

extern "C" module* createMod() {
	return new modNethogs;
}

extern "C" void destroyMod(module* p) {
	delete p;
}

extern "C" const char* getHelp() {
	return "Module usage:\n"
	"-m MODNETHOGS [-p PREFIX] [-c time] [-c sent] [-c received] "
	"[-o IFACE [-o TRACEFILE]]\n\n"
	"Columns:\n"
	"time: UNIX timestamp when snapshot was taken\n"
	"sent: Sending bit rate (KiB/s)\n"
	"received: Receiving bit rate (KiB/s)\n\n"
	"Please note that nethogs must run as root.\n"
	"The monitoring system with MODNETHOGS therefore also needs to run as root.\n\n"
	"Options and defaults:\n"
	"IFACE -- the network interface nethogs should listen to (default eth0 or equivalent)\n"
	"TRACEFILE -- if set, writes the raw nethogs trace to TRACEFILE";
}
