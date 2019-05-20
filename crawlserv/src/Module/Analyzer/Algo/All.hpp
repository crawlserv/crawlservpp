/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * All.hpp
 *
 * Registration of implemented algorithms.
 *
 * NOTE: Algorithms need also to be included in 'json/algos.json' in 'crawlserv_frontend' in order to be usable by the frontend.
 *
 *  Created on: Jan 13, 2019
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_ALGO_ALL_HPP_
#define MODULE_ANALYZER_ALGO_ALL_HPP_

#include "../Thread.hpp"

#include "../../../Struct/AlgoThreadProperties.hpp"

#include <memory>	// std::make_unique, std::unique_ptr

namespace crawlservpp::Module::Analyzer::Algo {

	// for convenience
	typedef Struct::AlgoThreadProperties AlgoThreadProperties;
	typedef std::unique_ptr<Module::Analyzer::Thread> AlgoThreadPtr;

	AlgoThreadPtr initAlgo(const AlgoThreadProperties& thread);

} /* crawlservpp::Module::Analyzer */

// macro for algorithm thread creation
#define REGISTER_ALGORITHM(ID, CLASS) \
	if(thread.algoId == (ID)) { \
		if(thread.recreate) \
			return std::make_unique<CLASS>(\
					thread.dbBase, \
					thread.options, \
					thread.status \
			); \
		else \
			return std::make_unique<CLASS>(thread.dbBase, thread.options); \
	}

#endif /* MODULE_ANALYZER_ALGO_ALL_HPP_ */
