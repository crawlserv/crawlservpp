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
 * Dictionary.hpp
 *
 * Defines the directory for dictionaries.
 *
 *  Created on: Nov 13, 2020
 *      Author: ans
 */

#ifndef DATA_DICTIONARY_HPP_
#define DATA_DICTIONARY_HPP_

#include <string_view>		// std::string_view_literals

namespace crawlservpp::Data {

	using std::string_view_literals::operator""sv;

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! Directory for dictionaries.
	inline constexpr auto dictDir{"dict"sv};

	///@}

} /* namespace crawlservpp::Data */

#endif /* DATA_DICTIONARY_HPP_ */
