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
	AlgoThreadPtr initAlgo(const AlgoThreadProperties& thread) {

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
