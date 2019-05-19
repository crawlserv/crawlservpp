/*
 * QueryStruct.hpp
 *
 *	Structure to identify a query and its result type(s).
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
		explicit operator bool() {
			return this->index != 0;
		}

		bool operator!() {
			return this->index == 0;
		}
	};

} /* namespace crawlservpp::Query */

#endif /* STRUCT_QUERYSTRUCT_HPP_ */
