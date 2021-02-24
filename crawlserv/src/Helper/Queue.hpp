/*
 *
 * ---
 *
 *  Copyright (C) 2021 Anselm Schmidt (ans[ät]ohai.su)
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
 * Queue.hpp
 *
 * Helper function templates for queues.
 *
 *  Created on: Feb 8, 2021
 *      Author: ans
 */

#ifndef HELPER_QUEUE_HPP_
#define HELPER_QUEUE_HPP_

#include <queue>	// std::queue
#include <stack>	// std::stack

//! Namespace for global queue helper function templates.
namespace crawlservpp::Helper::Queue {

	/*
	 * DECLARATION
	 */

	///@name Queue Reversal
	///@{

	//! Reverses the given queue.
	/*!
	 * Uses a stack to reverse the given queue.
	 *
	 * \param queue Reference to the queue to be reversed.
	 */
	template<typename T> void reverse(std::queue<T>& queue) {
		std::stack<T> stack;

		while(!queue.empty()) {
			stack.push(queue.front());

			queue.pop();
		}

		while(!stack.empty()) {
			queue.push(stack.top());

			stack.pop();
		}
	}

	///@}

	/*
	 * IMPLEMENTATION
	 */

	/* nothing to see here */

} /* namespace crawlservpp::Helper::Queue */

#endif /* HELPER_QUEUE_HPP_ */
