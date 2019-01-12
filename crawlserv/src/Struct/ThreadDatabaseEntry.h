/*
 * ThreadDatabaseEntry.h
 *
 * Thread status as saved in the database (id, module, status message, pause status, options, id of last processed URL).
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef STRUCT_THREADDATABASEENTRY_H_
#define STRUCT_THREADDATABASEENTRY_H_

#include "../Struct/ThreadOptions.h"

#include <string>

namespace crawlservpp::Struct {
	struct ThreadDatabaseEntry {
		unsigned long id; // thread id
		std::string module; // thread module
		std::string status; // thread status
		bool paused; // is thread paused?
		crawlservpp::Struct::ThreadOptions options; // options for thread
		unsigned long last; // last id
	};
}

#endif /* STRUCT_THREADDATABASEENTRY_H_ */
