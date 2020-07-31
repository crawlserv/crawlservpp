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

	//! Structure to identify a query including its type and result type(s).
	struct QueryStruct {
		/*
		 * CONSTANTS
		 */

		///@name Constants
		///@{

		//! Unspecified query type.
		static constexpr std::uint8_t typeNone{0};

		//! %Query type identifying a RegEx query.
		static constexpr std::uint8_t typeRegEx{1};

		//! %Query type identifying a XPath query.
		static constexpr std::uint8_t typeXPath{2};

		//! %Query type identifying a JSONPointer query.
		static constexpr std::uint8_t typeJsonPointer{3};

		//! %Query type identifying a JSONPath query.
		static constexpr std::uint8_t typeJsonPath{4};

		//! %Query type identifying a combined XPath and JSONPointer query.
		static constexpr std::uint8_t typeXPathJsonPointer{5};

		//! %Query type identifying a combined XPath and JSONPath query.
		static constexpr std::uint8_t typeXPathJsonPath{6};

		///@}
		///@name Properties
		///@{

		//! The type of the query (see above).
		std::uint8_t type{QueryStruct::typeNone};

		//! The index of the query inside its container.
		std::size_t index{0};

		//! Indicates whether the query generates a boolean result.
		bool resultBool{false};

		//! Indicates whether the query generates a single result.
		bool resultSingle{false};

		//! Indicates whether the query generates multiple results.
		bool resultMulti{false};

		//! Indicates whether the query generates subsets as results.
		/*!
		 * Subsets can directly be used to
		 *  run queries on them again.
		 */
		bool resultSubSets{false};

		///@}
		///@name Construction
		///@{

		//! Default constructor.
		QueryStruct() = default;

		//! Constructor explicitly setting the properties.
		/*!
		 * \param setType The type of the
		 *   query (see above).
		 * \param setIndex The index of the
		 *   query.
		 * \param setResultBool Set whether
		 *   the query generates a boolean
		 *   result.
		 * \param setResultSingle Set whether
		 *   the query generates a single
		 *   result.
		 * \param setResultMulti Set whether
		 *   the query generates multiple
		 *   results.
		 * \param setResultSubSets Set
		 *   whether the query generates
		 *   subsets as results.
		 */
		QueryStruct(
				std::uint8_t setType,
				std::size_t setIndex,
				bool setResultBool,
				bool setResultSingle,
				bool setResultMulti,
				bool setResultSubSets
		) : 		type(setType),
					index(setIndex),
					resultBool(setResultBool),
					resultSingle(setResultSingle),
					resultMulti(setResultMulti),
					resultSubSets(setResultSubSets) {}

		///@}
		///@name Getter
		///@{

		//! Gets whether the query is valid, i.e. whether a query ID has been identified.
		/*!
		 * \returns True, if the query is valid
		 *   and a query ID has been identified.
		 *   False otherwise.
		 */
		[[nodiscard]] bool valid() const noexcept {
			return this->index != 0;
		}

		///@}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_QUERYSTRUCT_HPP_ */
