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
 * QueryStruct.hpp
 *
 *	Structure to identify a query including its type and its result type(s).
 *
 *  Created on: May 17, 2019
 *      Author: ans
 */

#ifndef STRUCT_QUERYSTRUCT_HPP_
#define STRUCT_QUERYSTRUCT_HPP_

#include <cstddef>	// std::size_t
#include <cstdint>	// std::uint8_t

namespace crawlservpp::Struct {

	struct QueryStruct {
		static constexpr std::uint8_t typeNone = 0;
		static constexpr std::uint8_t typeRegEx = 1;
		static constexpr std::uint8_t typeXPath = 2;
		static constexpr std::uint8_t typeJsonPointer = 3;
		static constexpr std::uint8_t typeJsonPath = 4;
		static constexpr std::uint8_t typeXPathJsonPointer = 5;
		static constexpr std::uint8_t typeXPathJsonPath = 6;

		std::uint8_t type;
		std::size_t index;
		bool resultBool;
		bool resultSingle;
		bool resultMulti;
		bool resultSubSets;

		// constructor A: initialize values
		QueryStruct(
				std::uint8_t type,
				std::size_t index,
				bool resultBool,
				bool resultSingle,
				bool resultMulti,
				bool resultSubSets
		) : 		type(type),
					index(index),
					resultBool(resultBool),
					resultSingle(resultSingle),
					resultMulti(resultMulti),
					resultSubSets(resultSubSets) {}

		// constructor B: set default values via constructor A
		QueryStruct() : QueryStruct(
				QueryStruct::typeNone,
				0,
				false,
				false,
				false,
				false
		) {}

		// boolean operator
		explicit operator bool() const noexcept {
			return this->index != 0;
		}

		// inverse boolean operator
		bool operator!() const noexcept {
			return this->index == 0;
		}
	};

} /* crawlservpp::Query */

#endif /* STRUCT_QUERYSTRUCT_HPP_ */
