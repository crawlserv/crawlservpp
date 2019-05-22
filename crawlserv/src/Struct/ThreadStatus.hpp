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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
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
