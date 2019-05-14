/*
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
