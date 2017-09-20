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

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <stdio.h>
#include "main.h"
#include "module.h"
#include <unistd.h>
#include <iomanip>
#include <csignal>
#include <stdlib.h>
#include <array>
#include <sstream>
#include <dlfcn.h>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <sys/wait.h>
#include <signal.h>
#include <mutex>
#include <tuple>

std::vector<module*> modules_p;
std::vector<destroyMod_t*> modules_destructors_p;
bool output_file;
std::ofstream outfile;
bool exec_mode;

// save the indices of the columns
std::vector<std::vector<int>> modules_columns_indices;
// vector of pointers to the value vectors of the modules
std::vector<std::vector<std::vector < std::string>>*> modules_values_p;
// vector of pointers to the mutexes of the value vectors
std::vector<std::mutex*> modules_values_mp;
// column width
std::vector<int> columns_width;

param_t modules_param;
std::vector<std::string> modules_load_list;
std::vector<std::vector < std::string>> modules_columns_list;
std::vector<std::vector < std::string>> modules_options_list;
std::vector<std::string> modules_prefix_list;

// for alternative alignment: last time stamp
long alignment_timestamp = -1;
// for alternative alignment: last skip vector
std::vector<bool> alignment_skip;

void shutDown(int retval) {

	// close output file
	if (output_file) {
		outfile.close();
	}

	exit(retval);

}

void exitHandler(int signum) {

	// stop every monitoring module
	for (auto mod : modules_p) {
		mod->stopMon();
	}

	if (signum == -1) shutDown(1);
	shutDown(0);

}

// create a new row by collecting the outputs (timing / alignment)

std::vector<std::string> createRow() {

	int mod_i;
	std::mutex* mt_values_mp;
	std::vector<std::vector < std::string>>*mt_values_p;

	std::vector<std::string> current_row;
	mod_i = -1;
	for (auto &mod : modules_p) {
		++mod_i;
		mt_values_mp = modules_values_mp[mod_i];
		mt_values_p = modules_values_p[mod_i];
		// check if module is still running, if not, terminate program
		if (!mod->running()) {
			std::cerr << "Module " << modules_load_list[mod_i] << " stopped running." << std::endl;
			exitHandler(-1);
		}
		
		std::unique_lock<std::mutex> values_lock(*mt_values_mp, std::defer_lock);
		values_lock.lock();
		if (mt_values_p->empty()) {
			// push empty values if no row of values available
			for (int i = 0; i < modules_columns_indices[mod_i].size(); ++i)
				current_row.push_back("");
		} else {
			// only push the values in the columns the user wants to have
			std::vector<std::string> mt_values_back = mt_values_p->back();
			for (int i = 0; i < modules_columns_indices[mod_i].size(); ++i)
				current_row.push_back(mt_values_back[modules_columns_indices[mod_i][i]]);
		}
		values_lock.unlock();
	}

	return current_row;
}


// alternative alignment
// requires that the time stamp is saved in col 0 for every module
// to work with microseconds, microsecond time stamp must be in col 0
std::vector<std::string> createRowAlt() {

	int mod_i;
	std::mutex* mt_values_mp;
	std::vector<std::vector < std::string>>*mt_values_p;

	// alternative alignment: find last timestamp with data set by every module
	// ignore modules with empty vector, do not fetch a time stamp twice
	mod_i = -1;
	// earliest timestamp which is greater than alignment_timestamp (last one printed)
	long timestamp_earliest = -1;

	// values to be skipped because no unused value is available in the vector
	std::vector<bool> skip;
	bool skipped_one = false;

	for (auto &mod : modules_p) {
		++mod_i;
		mt_values_mp = modules_values_mp[mod_i];
		mt_values_p = modules_values_p[mod_i];

		// check if module is still running, if not, terminate program
		if (!mod->running()) {
			std::cerr << "Module " << modules_load_list[mod_i] << " stopped running." << std::endl;
			exitHandler(-1);
		}

		std::unique_lock<std::mutex> values_lock(*mt_values_mp, std::defer_lock);
		values_lock.lock();

		if (mt_values_p->empty()) {
			values_lock.unlock();
			skip.push_back(true);
			skipped_one = true;
		} else {
			long timestamp_current = std::stol(mt_values_p->back()[0]);
			values_lock.unlock();

			// no earliest time stamp set yet
			if (timestamp_earliest < 0) {
				// data set with later or same time stamp already printed
				if (alignment_timestamp >= timestamp_current) {
					skip.push_back(true);
					skipped_one = true;
				} else {
					timestamp_earliest = timestamp_current;
					skip.push_back(false);
				}
			}				// earliest time stamp existing
			else {
				if (alignment_timestamp >= timestamp_current) {
					skip.push_back(true);
					skipped_one = true;
				} else if (timestamp_earliest > timestamp_current) {
					timestamp_earliest = timestamp_current;
					skip.push_back(false);
				} else {
					skip.push_back(false);
				}
			}

		}

	}

	// alignment timestamp only increased if it has a data set for all modules
	// OR: if the last skip time stamp was identical to avoid jumps back in time and allow moving forward
	if(!skipped_one || skip == alignment_skip) alignment_timestamp = timestamp_earliest;
	// update alignment_skip to current skip
	alignment_skip = skip;

	// choose the latest time stamp coming closest to timestamp_earliest
	std::vector<std::string> current_row;
	mod_i = -1;
	for (auto &mod : modules_p) {
		++mod_i;
		
		if (skip[mod_i] == true) {
			// push empty values if data set should be skipped
			for (int i = 0; i < modules_columns_indices[mod_i].size(); ++i)
				current_row.push_back("");
			continue;
		}
		
		mt_values_mp = modules_values_mp[mod_i];
		mt_values_p = modules_values_p[mod_i];

		std::unique_lock<std::mutex> values_lock(*mt_values_mp, std::defer_lock);
		values_lock.lock();

		// iterate through vector from back
		std::vector<std::string> best;
		long distance = 0;
		for (int i = mt_values_p->size() - 1; i >= 0; --i) {
			long ts_curr = std::stol(mt_values_p->at(i)[0]);
			if (ts_curr > timestamp_earliest) {
				best = mt_values_p->at(i);
				distance = ts_curr - timestamp_earliest;
			} else if (ts_curr == timestamp_earliest) {
				best = mt_values_p->at(i);
				break;
			} else {
				if (timestamp_earliest - ts_curr < distance) {
					best = mt_values_p->at(i);
				}
				break;
			}
		}

		// only push the values in the columns the user wants to have
		for (int i = 0; i < modules_columns_indices[mod_i].size(); ++i)
			current_row.push_back(best[modules_columns_indices[mod_i][i]]);

		values_lock.unlock();
	}

	return current_row;
}

/*
 * 
 */
int main(int argc, char** argv) {

	// help mode
	if (argc < 2 || strcmp(argv[1], "-h") == 0) {

		// module specific help
		if (argc > 2) {
			// load module
			void* mlc_load = dlopen(argv[2], RTLD_LAZY);
			if (!mlc_load) {
				std::cerr << "Cannot load module: " << dlerror() << std::endl;
				return 1;
			}
			// reset dl errors
			dlerror();
			// load dlsym and check for errors
			getHelp_t* mlc_help = (getHelp_t*) dlsym(mlc_load, "getHelp");
			const char* dlsym_err = dlerror();
			if (dlsym_err) {
				std::cerr << "Cannot load symbol getHelp: " << dlsym_err << std::endl;
				return 1;
			}
			// print help
			std::cout << mlc_help() << std::endl;
		}// generic help
		else {

			std::cout << "Usage as a regular expression:\n"
					"promoris (-m MODULE (-p PREFIX)? (-c COLUMN)* (-o OPTION)*)+ "
					"(-i INTERVAL | -I INTERVAL_MICRO)? (-f LOGFILE)? (-a)? "
					"(-P PID | PROGRAM (PARAM)*)\n\n"
					"Explanation:\n"
					"-m MODULE\n"
					"	Use module MODULE. MODULE must be a shared library (.so) "
					"extending the abstract class module from the ProMoRIS module.h header.\n"
					"	Path must be relative starting with ./ or absolute.\n"
					"	At least one module must be loaded to run ProMoRIS.\n"
					"-p PREFIX\n"
					"	Display the columns provided by MODULE as PREFIX:COLUMN.\n"
					"	If no -p option is provided, a number will be used as PREFIX.\n"
					"-c COLUMN\n"
					"	All columns passed after -c options will be displayed.\n"
					"	Columns should be passed without prefix.\n"
					"	To see all columns provided by a module, use the module specific help (see below).\n"
					"	Invalid column names are ignored.\n"
					"	If no -c options are passed for a module, all columns will be displayed.\n"
					"-o OPTION\n"
					"	OPTION will be passed on to MODULE.\n"
					"	To see all possible options for a module, use the module specific help (see below).\n"
					"-i INTERVAL\n"
					"	One data set is printed every INTERVAL seconds. Default is 10.\n"
					"	The interval is also passed to all modules, which can use them for optimization.\n"
					"-I INTERVAL_MICRO\n"
					"	One dataset is printed every INTERVAL_MICRO microseconds.\n"
					"	INTERVAL is set to 1 when INTERVAL_MICRO is provided for "
					"modules which do not support microseconds.\n"
					"	When both -i and -I are given, the last one will win.\n"
					"-f LOGFILE\n"
					"	Write the data sets to LOGFILE instead of stdout.\n"
					"-a\n"
					"	Use alternative alignment algorithm. Requires compatible modules (time stamp in column 0).\n"
					"-P PID\n"
					"	Monitor the existing process PID. Unreliable, please do not use.\n"
					"	All arguments passed after PID will be truncated.\n"
					"PROGRAM (PARAM)*\n"
					"	Start process PROGRAM for monitoring.\n"
					"	PROGRAM should be a relative or absolute path.\n"
					"	All arguments after PROGRAM are passed to PROGRAM.\n\n"
					"Get module specific help:\n"
					"	promoris -h MODULE"
					<< std::endl;

		}

		return 0;
	}

	signal(SIGTERM, exitHandler);

	// analyze arguments
	/*
	 * arg_mode: current argument should be
	 * 0: "-m" or "-i" or "-I" or "-f" or "-a" or PID
	 * 1: module (last was "-m")
	 * 2: "-m" or "-c" or "-p" or "-o" or "-i" or "-I" or "-f" or "-a" or PID
	 * 3: column (last was "-c") (module specific)
	 * 4: prefix (last was "-p") (module specific)
	 * 5: options (last was "-o") (module specific)
	 * 6: interval (last was "-i")
	 * 7: output file (last was "-f")
	 * 8: PID (last was "-P")
	 * 9: microseconds interval (last was "-I")
	 * 
	 */
	int arg_mode = 0;
	// sleep time / interval
	modules_param.interval = 10;
	// microseconds interval
	modules_param.interval_micro = 0;
	// for redirecting output to file
	std::ostream* out = &std::cout;
	output_file = false;
	// exec mode for directly starting process
	exec_mode = true;
	bool program_passed = false;
	char* exec_program;
	int exec_param_c;
	char** exec_param_v;
	bool alternative_alignment = false;
	for (int i = 1, module_i = -1; i < argc; ++i) {
		if (arg_mode == 1) {
			++module_i;
			modules_load_list.push_back(argv[i]);
			modules_columns_list.push_back(std::vector<std::string>());
			modules_options_list.push_back(std::vector<std::string>());
			modules_prefix_list.push_back(std::to_string(module_i) + ":");
			arg_mode = 2;
		} else if (arg_mode == 3) {
			// if it is not really a column, it will just be ignored
			modules_columns_list[module_i].push_back(argv[i]);
			arg_mode = 2;
		} else if (arg_mode == 4) {
			modules_prefix_list[module_i] = std::string(argv[i]) + ":";
			arg_mode = 2;
		} else if (arg_mode == 5) {
			modules_options_list[module_i].push_back(argv[i]);
			arg_mode = 2;
		} else if (arg_mode == 6) {
			modules_param.interval = atoi(argv[i]);
			modules_param.interval_micro = 0;
			if (modules_param.interval < 1) {
				std::cerr << "Monitoring interval must be 1 or greater" << std::endl;
				return 1;
			}
			arg_mode = 0;
		} else if (arg_mode == 7) {
			outfile.open(argv[i]);
			if (!outfile.is_open()) {
				std::cerr << "Output file could not be opened" << std::endl;
				return 1;
			}
			output_file = true;
			out = &outfile;
			arg_mode = 0;
		} else if (arg_mode == 8) {
			modules_param.pid = argv[i];
			exec_mode = false;
			program_passed = true;
			break;
		} else if (arg_mode == 9) {
			modules_param.interval_micro = atoi(argv[i]);
			modules_param.interval = 1;
			if (modules_param.interval_micro < 1) {
				std::cerr << "Microseconds monitoring interval must be 1 or greater" << std::endl;
				return 1;
			}
			arg_mode = 0;
		} else if (strcmp(argv[i], "-m") == 0) {
			arg_mode = 1;
		} else if (strcmp(argv[i], "-c") == 0) {
			if (arg_mode != 2) {
				std::cerr << "Columns may only be passed after modules" << std::endl;
				return 1;
			}
			arg_mode = 3;
		} else if (strcmp(argv[i], "-p") == 0) {
			if (arg_mode != 2) {
				std::cerr << "Prefixes may only be passed after modules" << std::endl;
				return 1;
			}
			arg_mode = 4;
		} else if (strcmp(argv[i], "-o") == 0) {
			if (arg_mode != 2) {
				std::cerr << "Options may only be passed after modules" << std::endl;
				return 1;
			}
			arg_mode = 5;
		} else if (strcmp(argv[i], "-i") == 0) {
			arg_mode = 6;
		} else if (strcmp(argv[i], "-f") == 0) {
			arg_mode = 7;
		} else if (strcmp(argv[i], "-P") == 0) {
			arg_mode = 8;
		} else if (strcmp(argv[i], "-I") == 0) {
			arg_mode = 9;
		} else if (strcmp(argv[i], "-a") == 0) {
			alternative_alignment = true;
			arg_mode = 0;
		} else {
			exec_program = argv[i];
			// save params to pass to program
			exec_param_c = argc - i;
			exec_param_v = new char*[argc - i + 1];
			for (int j = 0; j < exec_param_c; ++j) {
				exec_param_v[j] = argv[i + j];
			}
			exec_param_v[exec_param_c] = NULL;
			program_passed = true;
			break;
		}
	}

	// error handling
	if (!program_passed) {
		std::cerr << "Please pass a program to be started or a PID" << std::endl;
		shutDown(1);
	}
	if (modules_load_list.size() == 0) {
		std::cerr << "Please pass at least one module as an argument" << std::endl;
		shutDown(1);
	}

	// start process via fork and execve
	if (exec_mode) {
		pid_t exec_pid;
		exec_pid = fork();
		if (exec_pid == 0) {
			execv(exec_param_v[0], exec_param_v);
			std::cerr << "Could not start program" << std::endl;
			exit(0);
		} else {
			modules_param.pid_struct = exec_pid;
			modules_param.pid = std::to_string(exec_pid);
			usleep(10000);
			int exec_status;
			pid_t tpid = waitpid(modules_param.pid_struct, &exec_status, WNOHANG);
			if (tpid == modules_param.pid_struct) {
				shutDown(1);
			}
		}
	}

	// load modules, creators and destructors
	for (auto &modules_load_current : modules_load_list) {
		void* mlc_load = dlopen(modules_load_current.c_str(), RTLD_LAZY);
		if (!mlc_load) {
			std::cerr << "Cannot load module: " << dlerror() << std::endl;
			shutDown(1);
		}
		// reset dl errors
		dlerror();

		// load dlsym and check for errors
		createMod_t * mlc_create = (createMod_t*) dlsym(mlc_load, "createMod");
		const char* dlsym_err = dlerror();
		if (dlsym_err) {
			std::cerr << "Cannot load symbol createMod: " << dlsym_err << std::endl;
			shutDown(1);
		}
		destroyMod_t * mlc_destroy = (destroyMod_t*) dlsym(mlc_load, "destroyMod");
		dlsym_err = dlerror();
		if (dlsym_err) {
			std::cerr << "Cannot load symbol destroyMod: " << dlsym_err << std::endl;
			shutDown(1);
		}
		modules_destructors_p.push_back(mlc_destroy);

		// create an object pointer
		module * mlc_objp = mlc_create();
		modules_p.push_back(mlc_objp);

	}

	if (modules_p.size() < 1) {
		std::cerr << "No modules loaded" << std::endl;
		shutDown(1);
	}


	// START ALL MODULES

	int mod_i = -1;
	for (auto &mod : modules_p) {
		++mod_i;
		param_t par = modules_param;
		par.options = modules_options_list[mod_i];
		mod->setParameters(par);
		mod->startMon();
	}

	// PRINT ALL ROW NAMES and get pointer to values and values_m
	mod_i = -1;
	for (auto &mod : modules_p) {
		++mod_i;
		// get pointers
		modules_values_mp.push_back(mod->getValuesMPtr());
		modules_values_p.push_back(mod->getValuesPtr());
		// push an entry for the module to modules_columns_indices
		modules_columns_indices.push_back(std::vector<int>());
		// row names
		std::vector<int> colmaxsize = mod->getColMaxSize();
		int col_i = -1;
		for (auto &col_label : mod->getLabels()) {
			++col_i;

			// if there is a list of columns for this module, only show those in the list
			if (!modules_columns_list[mod_i].empty() &&
					std::find(modules_columns_list[mod_i].begin(),
					modules_columns_list[mod_i].end(), col_label) == modules_columns_list[mod_i].end()) {
				continue;
			}

			// save index of this column to modules_columns_indices
			modules_columns_indices[mod_i].push_back(col_i);

			// prefix the col_label
			std::string col_label_prefixed = modules_prefix_list[mod_i] + col_label;

			// col width should be the greater one of colmaxsize+1 and label size +1
			int col_width_temp;
			if (col_label_prefixed.size() >= colmaxsize[col_i])
				col_width_temp = col_label_prefixed.size() + 1;
			else col_width_temp = colmaxsize[col_i] + 1;
			*out << std::setw(col_width_temp) << std::left << col_label_prefixed;

			// save col width to columns_width
			columns_width.push_back(col_width_temp);

		}
	}
	*out << std::endl;

	// COLLECT ALL OUTPUTS

	// iterate through module outputs and print data
	std::mutex* mt_values_mp;
	std::vector<std::vector < std::string>>*mt_values_p;
	while (true) {

		// sleep for microseconds, if -I option is provided
		if (modules_param.interval_micro > 0) {
			usleep(modules_param.interval_micro);
		} else {
			sleep(modules_param.interval);
		}

		// check if process is still running, if not, terminate program
		if (exec_mode) {
			int exec_status;
			pid_t tpid = waitpid(modules_param.pid_struct, &exec_status, WNOHANG);
			if (tpid == modules_param.pid_struct) {
				exitHandler(0);
			}
		}

		// create the row
		// alternative alignment when -a option present
		std::vector<std::string> current_row;
		if(alternative_alignment) current_row = createRowAlt();
		else current_row = createRow();

		// print the row
		for (int i = 0; i < current_row.size(); ++i) {
			*out << std::setw(columns_width[i]) << std::left << current_row[i];
		}
		*out << std::endl;

	}

}
