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

#ifndef MODULE_H
#define MODULE_H

#include <array>
#include <string>
#include <vector>
#include <mutex>
#include <unistd.h>

typedef struct param_t {
	std::string pid;
	pid_t pid_struct;
	int interval;
	int interval_micro;
	std::vector<std::string> options;
} param_t;

class module {
public:
	module() {}
	virtual ~module() {}
	virtual void setParameters(param_t sparam) = 0;
	virtual int startMon() = 0;
	virtual int stopMon() = 0;
	virtual bool running() = 0;
	virtual std::vector<std::vector<std::string>> * getValuesPtr() = 0;
	virtual std::mutex * getValuesMPtr() = 0;
	virtual std::vector<std::string> getLabels() = 0;
	virtual std::vector<int> getColMaxSize() = 0;
	
};

typedef module* createMod_t();
typedef void destroyMod_t();
typedef const char* getHelp_t();

#endif /* MODULE_H */
