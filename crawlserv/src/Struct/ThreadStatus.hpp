/*
 * ThreadStatus.hpp
 *
 * Thread status (thread ID, status message, pause state and progress i.e. last ID)
 *
 *  NOTE:	The thread ID is part of the status, because it will not be assigned
 *  		 until after the thread has been added to the database.
 *
 *  Created on: Apr 30, 2019
 *      Author: ans
 */

#ifndef STRUCT_THREADSTATUS_HPP_
#define STRUCT_THREADSTATUS_HPP_

#include <string>	// std::stirng

namespace crawlservpp::Struct {

	struct ThreadStatus {
		unsigned long id;	// ID
		std::string status;	// status message
		bool paused;		// pause state
		unsigned long last;	// progress i.e. last ID

		// constructors
		ThreadStatus() : id(0), paused(false), last(0) {}
		ThreadStatus(
				unsigned long setId,
				const std::string& setStatus,
				bool setPaused,
				unsigned long setLast
		) : id(setId), status(setStatus), paused(setPaused), last(setLast) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_THREADSTATUS_HPP_ */
