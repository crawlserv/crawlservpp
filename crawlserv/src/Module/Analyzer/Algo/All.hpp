/*
 *
 * ---
 *
 *  Copyright (C) 2021 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
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
 * NOTE: Algorithms need to be included in 'All.cpp'
 *  and 'json/algos.json' (located in 'crawlserv_frontend')
 *  in order to be usable by the frontend.
 *
 *  Created on: Jan 13, 2019
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_ALGO_ALL_HPP_
#define MODULE_ANALYZER_ALGO_ALL_HPP_

#include "../Thread.hpp"

#include "../../../Struct/AlgoThreadProperties.hpp"

#include <memory>	// std::make_unique, std::unique_ptr

//! Namespace for algorithm classes.
namespace crawlservpp::Module::Analyzer::Algo {

	// for convenience
	using AlgoThreadProperties = Struct::AlgoThreadProperties;

	using AlgoThreadPtr = std::unique_ptr<Module::Analyzer::Thread>;

	///@name Registration
	///@{

	AlgoThreadPtr initAlgo(const AlgoThreadProperties& thread);

	///@}

} /* namespace crawlservpp::Module::Analyzer::Algo */

// macro for algorithm thread creation
//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define REGISTER_ALGORITHM(ID, CLASS) \
	if(thread.algoId == (ID)) { \
		if(thread.status.id > 0) {\
			return std::make_unique<CLASS>(\
					thread.dbBase, \
					thread.options, \
					thread.status \
			); \
		}\
		return std::make_unique<CLASS>(thread.dbBase, thread.options); \
	}

#endif /* MODULE_ANALYZER_ALGO_ALL_HPP_ */
