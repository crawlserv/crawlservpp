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
 * QueryProperties.hpp
 *
 * Query properties (name, text, type and result type(s)).
 *
 *  Created on: Feb 2, 2019
 *      Author: ans
 */

#ifndef STRUCT_QUERYPROPERTIES_HPP_
#define STRUCT_QUERYPROPERTIES_HPP_

#include <string>	// std::string

namespace crawlservpp::Struct {

	//! %Query properties containing its name, text, type, and result type(s).
	struct QueryProperties {
		///@name Properties
		///@{

		//! The name of the query.
		std::string name;

		//! The query.
		std::string text;

		//! The type of the query.
		std::string type;

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

		//! Indicates whether the query should be considered text-only.
		bool textOnly{false};

		///@}
		///@name Construction
		///@{

		//! Default constructor.
		QueryProperties() = default;

		//! Constructor setting properties, including the name of the query.
		/*!
		 * \param setName Constant reference
		 *   to a string containing the name
		 *   of the query.
		 * \param setText Constant reference
		 *   to a string containing the text
		 *   of the query.
		 * \param setType Constant reference
		 *   to a string containing the type
		 *   of the query.
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
		 * \param setTextOnly Set whether
		 *   the query should be considered
		 *   text-only.
		 */
		QueryProperties(
				const std::string& setName,
				const std::string& setText,
				const std::string& setType,
				bool setResultBool,
				bool setResultSingle,
				bool setResultMulti,
				bool setResultSubSets,
				bool setTextOnly
		) : name(setName),
			text(setText),
			type(setType),
			resultBool(setResultBool),
			resultSingle(setResultSingle),
			resultMulti(setResultMulti),
			resultSubSets(setResultSubSets),
			textOnly(setTextOnly) {}

		//! Constructor setting properties, but not the name of the query.
		/*!
		 * \note The name of the query will be
		 *   set to an empty string.
		 *
		 * \param setText Constant reference
		 *   to a string containing the text
		 *   of the query.
		 * \param setType Constant reference
		 *   to a string containing the type
		 *   of the query.
		 * \param setResultBool Set whether
		 *   the query generates a boolean
		 *   result.
		 * \param setResultSingle Set whether
		 *   the query generates a single
		 *   result.
		 * \param setResultMulti Set whether
		 *   the query generates multiple
		 *   results.
		 * \param setResultSubSets Set whether
		 *   the query generates subsets as
		 *   results.
		 * \param setTextOnly Set whether the
		 *   query should be considered text-
		 *   only.
		 */
		QueryProperties(
				const std::string& setText,
				const std::string& setType,
				bool setResultBool,
				bool setResultSingle,
				bool setResultMulti,
				bool setResultSubSets,
				bool setTextOnly
		) : QueryProperties(
				"",
				setText,
				setType,
				setResultBool,
				setResultSingle,
				setResultMulti,
				setResultSubSets,
				setTextOnly
			) {}

		///@}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_QUERYPROPERTIES_HPP_ */
