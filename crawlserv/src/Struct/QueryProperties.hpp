/*
 * QueryProperties.hpp
 *
 * Basic query properties (name, text, type and result type).
 *
 *  Created on: Feb 2, 2019
 *      Author: ans
 */

#ifndef STRUCT_QUERYPROPERTIES_HPP_
#define STRUCT_QUERYPROPERTIES_HPP_

#include <string>	// std::string

namespace crawlservpp::Struct {

	struct QueryProperties {
		std::string name;
		std::string text;
		std::string type;
		bool resultBool;
		bool resultSingle;
		bool resultMulti;
		bool resultSubSets;
		bool textOnly;

		// constructors
		QueryProperties() :	resultBool(false),
							resultSingle(false),
							resultMulti(false),
							resultSubSets(false),
							textOnly(false) {}
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
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_QUERYPROPERTIES_HPP_ */
