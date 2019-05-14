/*
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
