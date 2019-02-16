/*
 * ConfigProperties.h
 *
 * Basic corpus properties (source type, table, field and format).
 *
 *  Created on: Feb 2, 2019
 *      Author: ans
 */

#ifndef STRUCT_CORPUSPROPERTIES_H_
#define STRUCT_CORPUSPROPERTIES_H_

#include <string>

namespace crawlservpp::Struct {
	struct ConfigCorpus {
		unsigned short sourceType;
		std::string sourceTable;
		std::string sourceField;
		std::string sourceFormat;

		// constructors
		ConfigCorpus() { this->sourceType = 0; }
		ConfigCorpus(unsigned short setSourceType, const std::string& setSourceTable, const std::string& setSourceField,
				const std::string& setSourceFormat) {
			this->sourceType = setSourceType;
			this->sourceTable = setSourceTable;
			this->sourceFormat = setSourceFormat;
		}
	};
}

#endif /* STRUCT_CORPUSPROPERTIES_H_ */
