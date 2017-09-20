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

#include "modIperf3.h"
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <regex>
#include <cstring>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"
#include <fstream>
#include <fcntl.h>

// definitions
param_t modIperf3::param;
bool modIperf3::stopmon;
std::thread modIperf3::t1;
std::vector<std::vector<std::string>> modIperf3::values;
std::mutex modIperf3::values_m;

int modIperf3::runMon() {

	// server could be passed as first -o parameter
	char* i_server;
	if (param.options.size() >= 1) i_server = const_cast<char*> (param.options[0].c_str());
	else i_server = "localhost";

	// port could be passed as second -o parameter
	char* i_port;
	if (param.options.size() > 1) i_port = const_cast<char*> (param.options[1].c_str());
	else i_port = "5201";

	// time could be passed as third
	char* i_time;
	if (param.options.size() > 2) i_time = const_cast<char*> (param.options[2].c_str());
	else i_time = "5";

	// sleep could be passed as fourth
	int i_sleep;
	if (param.options.size() > 3) i_sleep = std::stoi(param.options[3]);
	else i_sleep = 10;

	// omit could be passed as fifth
	char* i_omit;
	if (param.options.size() > 4) i_omit = const_cast<char*> (param.options[4].c_str());
	else i_omit = "0";

	// arguments
	char* ps_args[] = {"iperf3", "-c", i_server, "-p", i_port, "-J",
		"-t", i_time, "-O", i_omit, "-i", "0", nullptr};
	
	// open JSON file if option is set
	std::ofstream tracefile;
	bool trace = false;
	if (param.options.size() > 5) {
		trace = true;
		tracefile.open(param.options[5]);
		if (!tracefile.is_open()) {
			std::cerr << "modIperf3: trace file could not be opened" << std::endl;
			return 1;
		}
	}

	while (!stopmon) {

		// to spawn iperf3 process
		int ps_exit;
		int ps_cout[2];
		posix_spawn_file_actions_t ps_action;

		if (pipe(ps_cout)) {
			std::cerr << "modIperf3: could not create pipe" << std::endl;
			stopmon = true;
			return 1;
		}

		// redirect stdout
		posix_spawn_file_actions_init(&ps_action);
		posix_spawn_file_actions_addclose(&ps_action, ps_cout[0]);
		posix_spawn_file_actions_adddup2(&ps_action, ps_cout[1], 1);
		posix_spawn_file_actions_addclose(&ps_action, ps_cout[1]);

		pid_t ps_pid;
		if (posix_spawnp(&ps_pid, "/usr/bin/iperf3", &ps_action, nullptr, ps_args, nullptr) != 0) {
			std::cerr << "modIperf3: posix_spawnp failed with error: " << strerror(errno) << std::endl;
			stopmon = true;
			return 1;
		}
		close(ps_cout[1]);
		waitpid(ps_pid, &ps_exit, 0);

		const size_t ps_buffer_size = 1024;
		std::string ps_buffer;
		ps_buffer.resize(ps_buffer_size);
		ssize_t ps_read;
		std::string ps_out;
		int readflags = fcntl(ps_cout[0], F_GETFL, 0);
		fcntl(ps_cout[0], F_SETFL, readflags | O_NONBLOCK);
		while ((ps_read = read(ps_cout[0], &ps_buffer[0], ps_buffer_size)) > 0) {
			ps_out.append(ps_buffer.substr(0, ps_read + 1));
		}

		posix_spawn_file_actions_destroy(&ps_action);

		// write JSON file if option is set
		if (trace) tracefile << ps_out;

		// process JSON
		rapidjson::Document json_d;
		json_d.Parse<rapidjson::kParseStopWhenDoneFlag>(ps_out.c_str());

		if (json_d.HasParseError()) {
			fprintf(stderr, "modIperf3: JSON parsing error (offset %u): %s\n",
					(unsigned) json_d.GetErrorOffset(),
					rapidjson::GetParseError_En(json_d.GetParseError()));
			stopmon = true;
			return 1;
		}

		// pass output
		std::stringstream timeGen;
		timeGen << time(nullptr);

		assert(json_d.IsObject());

		if (json_d.HasMember("error") && json_d["error"].IsString()) {
			std::cerr << "modIperf3: iperf3 error: " << json_d["error"].GetString() << std::endl;
		} else {

			assert(json_d.HasMember("end"));
			assert(json_d["end"].HasMember("sum_sent"));
			assert(json_d["end"]["sum_sent"].HasMember("bits_per_second"));
			assert(json_d["end"].HasMember("sum_received"));
			assert(json_d["end"]["sum_received"].HasMember("bits_per_second"));
			assert(json_d["end"].HasMember("cpu_utilization_percent"));
			assert(json_d["end"]["cpu_utilization_percent"].HasMember("host_total"));
			assert(json_d["end"]["cpu_utilization_percent"].HasMember("host_user"));
			assert(json_d["end"]["cpu_utilization_percent"].HasMember("host_system"));

			// write current timestamp and relevant values to values array
			{
				std::lock_guard<std::mutex> values_guard(values_m);
				values.push_back(std::vector<std::string>{timeGen.str(),
					std::to_string(static_cast<long> (json_d["end"]["sum_sent"]["bits_per_second"].GetDouble())),
					std::to_string(static_cast<long> (json_d["end"]["sum_received"]["bits_per_second"].GetDouble())),
					std::to_string(json_d["end"]["cpu_utilization_percent"]["host_total"].GetDouble()),
					std::to_string(json_d["end"]["cpu_utilization_percent"]["host_user"].GetDouble()),
					std::to_string(json_d["end"]["cpu_utilization_percent"]["host_system"].GetDouble())});
			}

		}

		if (!stopmon) sleep(i_sleep);

	}
	
	if (trace) tracefile.close();

	stopmon = true;

	return 0;

}

void modIperf3::setParameters(param_t sparam) {
	
	param = sparam;
}

int modIperf3::startMon() {
	stopmon = false;
	// start monitoring thread (runMon)

	t1 = std::thread(runMon);

	return 0;
}

int modIperf3::stopMon() {
	stopmon = true;
	t1.join();
	return 0;
}

bool modIperf3::running() {
	return !stopmon;
}

std::vector<std::vector<std::string>> *modIperf3::getValuesPtr() {
	return &values;
}

std::mutex * modIperf3::getValuesMPtr() {
	return &values_m;
}

std::vector<std::string> modIperf3::getLabels() {
	return {"time", "bps_sent", "bps_received", "cpu_total", "cpu_user", "cpu_system"};
}

std::vector<int> modIperf3::getColMaxSize() {
	return {10, 12, 12, 10, 10, 10};
}

extern "C" module * createMod() {
	return new modIperf3;
}

extern "C" void destroyMod(module* p) {
	delete p;
}

extern "C" const char* getHelp() {
	return "Module usage:\n"
	"-m MODIPERF3 [-p PREFIX] [-c time] [-c bps_sent] [-c bps_received] "
	"[-c cpu_total] [-c cpu_user] [-c cpu_system] "
	"[-o SERVER [-o PORT [-o TIME [-o SLEEP [-o OMIT [-o JSONFILE]]]]]]\n\n"
	"Columns:\n"
	"time: UNIX timestamp when snapshot was taken\n"
	"bps_sent: Sending bit rate (bit/s)\n"
	"bps_received: Receiving bit rate (bit/s)\n"
	"cpu_total: Total CPU usage (%)\n"
	"cpu_user: User CPU usage (%)\n"
	"cpu_system: System CPU usage (%)\n\n"
	"Options and defaults:\n"
	"SERVER = localhost -- the iperf3 server to connect to\n"
	"PORT = 5201 -- the port on which the server at SERVER is running\n"
	"TIME = 5 -- the time the iperf3 test should be running for (in seconds)\n"
	"SLEEP = 10 -- the sleep time between to iperf3 tests (in seconds)\n"
	"OMIT = 0 -- omit the first OMIT seconds of the tests for statistics\n"
	"JSONFILE -- if set, writes the raw iperf3 JSON output to JSONFILE";
}
