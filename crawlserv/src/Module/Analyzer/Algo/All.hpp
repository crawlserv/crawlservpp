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

#include "../../../Main/Database.hpp"
#include "../../../Struct/ThreadOptions.hpp"

#include <memory>

namespace crawlservpp::Module::Analyzer::Algo {
	// for convenience
	typedef std::unique_ptr<Module::Analyzer::Thread> AlgoThreadPtr;
	typedef Struct::ThreadOptions ThreadOptions;

	AlgoThreadPtr initAlgo(
			bool recreate,
			unsigned long id,
			Main::Database& database,
			const std::string& status,
			bool paused,
			const ThreadOptions& options,
			unsigned long last
	);

} /* crawlservpp::Module::Analyzer */

// macro for algorithm thread creation
#define REGISTER_ALGORITHM(ID, CLASS) \
	if(id == (ID)) { \
		if(recreate) \
			return std::make_unique<CLASS>(database, id, status, paused, options, last); \
		else \
			return std::make_unique<CLASS>(database, options); \
	}

#endif /* MODULE_ANALYZER_ALGO_ALL_HPP_ */
