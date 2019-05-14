/*
 * CorpusProperties.hpp
 *
 * Basic corpus properties (source type, table and field).
 *
 *  Created on: Feb 2, 2019
 *      Author: ans
 */

#ifndef STRUCT_CORPUSPROPERTIES_HPP_
#define STRUCT_CORPUSPROPERTIES_HPP_

#include <string>	// std::string

namespace crawlservpp::Struct {

	struct CorpusProperties {
		unsigned short sourceType;
		std::string sourceTable;
		std::string sourceField;

		// constructors
		CorpusProperties() : sourceType(0) {}
		CorpusProperties(
				unsigned short setSourceType,
				const std::string& setSourceTable,
				const std::string& setSourceField)
		: sourceType(setSourceType),
		  sourceTable(setSourceTable),
		  sourceField(setSourceField) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_CORPUSPROPERTIES_HPP_ */
