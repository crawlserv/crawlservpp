/*
 * CorpusProperties.h
 *
 * Basic corpus properties (source type, table and field).
 *
 *  Created on: Feb 2, 2019
 *      Author: ans
 */

#ifndef STRUCT_CORPUSPROPERTIES_H_
#define STRUCT_CORPUSPROPERTIES_H_

#include <string>

namespace crawlservpp::Struct {
	struct CorpusProperties {
		unsigned short sourceType;
		std::string sourceTable;
		std::string sourceField;

		// constructors
		CorpusProperties() { this->sourceType = 0; }
		CorpusProperties(unsigned short setSourceType, const std::string& setSourceTable, const std::string& setSourceField) {
			this->sourceType = setSourceType;
			this->sourceTable = setSourceTable;
		}
	};
}

#endif /* STRUCT_CORPUSPROPERTIES_H_ */
