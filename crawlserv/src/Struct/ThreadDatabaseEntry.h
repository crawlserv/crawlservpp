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

#include "ThreadOptions.h"

#include "../Module/Analyzer/Algo/Enum.h"

#include <string>

namespace crawlservpp::Struct {
	struct ThreadDatabaseEntry {
		unsigned long id; // thread id
		std::string module; // thread module
		std::string status; // thread status
		bool paused; // is thread paused?
		ThreadOptions options; // options for thread
		unsigned long last; // last id

		// constructors
		ThreadDatabaseEntry() { this->id = 0; this->paused = false; this->last = 0; }
		ThreadDatabaseEntry(unsigned long setId, const std::string& setModule, const std::string& setStatus,
				bool setPaused,	const ThreadOptions& setOptions, unsigned long setLast) {
			this->id = setId;
			this->module = setModule;
			this->status = setStatus;
			this->paused = setPaused;
			this->options = setOptions;
			this->last = setLast;
		}
	};
}

#endif /* STRUCT_THREADDATABASEENTRY_H_ */
