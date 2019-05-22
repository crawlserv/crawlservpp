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
 * QueryStruct.hpp
 *
 *	Structure to identify a query including its type and its result type(s).
 *
 *  Created on: May 17, 2019
 *      Author: ans
 */

#ifndef STRUCT_QUERYSTRUCT_HPP_
#define STRUCT_QUERYSTRUCT_HPP_

namespace crawlservpp::Struct {

	struct QueryStruct {
		static const unsigned char typeNone = 0;
		static const unsigned char typeRegEx = 1;
		static const unsigned char typeXPath = 2;
		static const unsigned char typeJsonPointer = 3;
		static const unsigned char typeJsonPath = 4;
		static const unsigned char typeXPathJsonPointer = 5;
		static const unsigned char typeXPathJsonPath = 6;

		unsigned char type;
		unsigned long index;
		bool resultBool;
		bool resultSingle;
		bool resultMulti;
		bool resultSubSets;

		// constructor: set default values
		QueryStruct() {
			this->type = QueryStruct::typeNone;
			this->index = 0;
			this->resultBool = false;
			this->resultSingle = false;
			this->resultMulti = false;
			this->resultSubSets = false;
		}

		// boolean operators
		explicit operator bool() const noexcept {
			return this->index != 0;
		}

		bool operator!() const noexcept {
			return this->index == 0;
		}
	};

} /* crawlservpp::Query */

#endif /* STRUCT_QUERYSTRUCT_HPP_ */
