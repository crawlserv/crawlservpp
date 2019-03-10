/*
 * ThreadDatabaseEntry.h
 *
 * Thread status as saved in the database (id, module, status message, pause status, options, id of last processed URL).
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef STRUCT_THREADDATABASEENTRY_HPP_
#define STRUCT_THREADDATABASEENTRY_HPP_

#include "../Module/Analyzer/Algo/Enum.hpp"

#include <string>
#include "ThreadOptions.hpp"

namespace crawlservpp::Struct {

	struct ThreadDatabaseEntry {
		unsigned long id; // thread id
		std::string module; // thread module
		std::string status; // thread status
		bool paused; // is thread paused?
		ThreadOptions options; // options for thread
		unsigned long last; // last id

		// constructors
		ThreadDatabaseEntry() : id(0), paused(false), last(0) {}
		ThreadDatabaseEntry(unsigned long setId, const std::string& setModule, const std::string& setStatus,
				bool setPaused,	const ThreadOptions& setOptions, unsigned long setLast)
				: id(setId), module(setModule), status(setStatus), paused(setPaused), options(setOptions), last(setLast) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_THREADDATABASEENTRY_HPP_ */
