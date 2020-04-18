/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
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
 * ThreadDatabaseEntry.hpp
 *
 * Thread status as saved in the database (id, module, status message, pause status, options, id of last processed URL).
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef STRUCT_THREADDATABASEENTRY_HPP_
#define STRUCT_THREADDATABASEENTRY_HPP_

#include "ThreadOptions.hpp"
#include "ThreadStatus.hpp"

namespace crawlservpp::Struct {

	struct ThreadDatabaseEntry {
		ThreadOptions options;	// options for the thread
		ThreadStatus status;	// status of the thread

		// constructors
		ThreadDatabaseEntry() {}
		ThreadDatabaseEntry(
				const ThreadOptions& setOptions,
				const ThreadStatus& setStatus
		) : options(setOptions), status(setStatus) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_THREADDATABASEENTRY_HPP_ */
