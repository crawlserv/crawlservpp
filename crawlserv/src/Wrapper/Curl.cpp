/*
 *
 * ---
 *
 *  Copyright (C) 2022 Anselm Schmidt (ans[ät]ohai.su)
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
 * Curl.cpp
 *
 * RAII wrapper for pointer to libcurl instance, also handles global instance if necessary.
 * 	Does NOT have ownership of the pointer.
 * 	The first instance has to be destructed last.
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#include "Curl.hpp"

//! Namespace for RAII wrappers and Wrapper::Database.
namespace crawlservpp::Wrapper {

	// set the libcurl API to not initialized
	std::atomic<unsigned int> Curl::globalInitCounter{};

} /* namespace crawlservpp::Wrapper */
