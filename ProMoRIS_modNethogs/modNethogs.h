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

#ifndef MODNETHOGS_H
#define MODNETHOGS_H

#include <cstdlib>
#include <string>
#include <vector>
#include <tuple>
#include <thread>
#include <mutex>
#include <array>
#include "../ProMoRIS/module.h"

class modNethogs : public module {
public:
	void setParameters(param_t sparam);
	int startMon();
	int stopMon();
	bool running();
	std::vector<std::vector<std::string>> * getValuesPtr();
	std::mutex * getValuesMPtr();
	std::vector<std::string> getLabels();
	std::vector<int> getColMaxSize();
	modNethogs() {}
	virtual ~modNethogs() {}
private:
	static std::vector<std::vector<std::string>> values;
	static std::mutex values_m;
	static param_t param;
	static bool stopmon;
	static std::thread t1;
	static int runMon();
};

#endif /* MODNETHOGS_H */
