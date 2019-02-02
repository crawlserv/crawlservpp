/*
 * QueryProperties.h
 *
 * Basic query properties (name, text, type and result type)
 *
 *  Created on: Feb 2, 2019
 *      Author: ans
 */

#ifndef STRUCT_QUERYPROPERTIES_H_
#define STRUCT_QUERYPROPERTIES_H_

#include <string>

namespace crawlservpp::Struct {
	struct QueryProperties {
		std::string name;
		std::string text;
		std::string type;
		bool resultBool;
		bool resultSingle;
		bool resultMulti;
		bool textOnly;

		// constructors
		QueryProperties() {
			this->resultBool = false;
			this->resultSingle = false;
			this->resultMulti = false;
			this->textOnly = false;
		}
		QueryProperties(const std::string& setName, const std::string& setText, const std::string& setType,
				bool setResultBool, bool setResultSingle, bool setResultMulti, bool setTextOnly) {
			this->name = setName;
			this->text = setText;
			this->type = setType;
			this->resultBool = setResultBool;
			this->resultSingle = setResultSingle;
			this->resultMulti = setResultMulti;
			this->textOnly = setTextOnly;
		}
		QueryProperties(const std::string& setText, const std::string& setType,
				bool setResultBool, bool setResultSingle, bool setResultMulti, bool setTextOnly) :
					QueryProperties("", setText, setType, setResultBool, setResultSingle, setResultMulti, setTextOnly) {}
	};
}

#endif /* STRUCT_QUERYPROPERTIES_H_ */
