/*
 * All.cpp
 *
 * Registration of implemented algorithms. EDIT THIS FILE TO ADD ALGORITHMS.
 *
 * NOTE: Algorithms need also to be included in 'json/algos.json' in 'crawlserv_frontend' in order to be usable by the frontend.
 *
 *  Created on: Mar 12, 2019
 *      Author: ans
 */

#include "All.hpp"

/*
 * ALGORITHM HEADER FILES
 */

// <ADD HEADER OF ALGORITHM HERE>
#include "MarkovText.hpp"
#include "MarkovTweet.hpp"
// </ADD HEADER OF ALGORITHM HERE>

namespace crawlservpp::Module::Analyzer::Algo {

	// register algorithms
	AlgoThreadPtr initAlgo(
			bool recreate,
			unsigned long id,
			Main::Database& database,
			const std::string& status,
			bool paused,
			const ThreadOptions& options,
			unsigned long last
	) {

		/*
		 * ALGORITHM REGISTRATION
		 */

		// <ADD REGISTRATION OF ALGORITHM HERE>
		REGISTER_ALGORITHM(43, MarkovText);
		REGISTER_ALGORITHM(44, MarkovTweet);
		// </ADD REGISTRATION OF ALGORITHM HERE>

		return nullptr;
	}

} /* crawlservpp::Module::Analyzer::Algo */
