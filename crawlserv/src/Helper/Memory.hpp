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
 * Memory.hpp
 *
 * Helper functions for memory operations.
 *
 *  Created on: Feb 21, 2021
 *      Author: ans
 */

#ifndef HELPER_MEMORY_HPP_
#define HELPER_MEMORY_HPP_

//! Namespace for global memory helper functions.
namespace crawlservpp::Helper::Memory {

	//! Frees memory by swapping.
	/*!
	 * \param target The object which is to be freed
	 *   by swapping.
	 */
	template<class T> static void free(T& target) {
		T{}.swap(target);
	}

	//! Frees memory early by swapping, if necessary.
	/*!
	 * \param isFree Specifies whether to free memory.
	 * \param target The object which is to be freed
	 *   by swapping.
	 */
	template<class T> static void freeIf(bool isFree, T& target) {
		if(isFree) {
			free(target);
		}
	}

} /* namespace crawlservpp::Helper::Memory */

#endif /* HELPER_MEMORY_HPP_ */
