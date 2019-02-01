/*
 * XPath.h
 *
 * Using the pugixml parser library to implement a XPath query with boolean, single and/or multiple results.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef QUERY_XPATH_H_
#define QUERY_XPATH_H_

#include "../Helper/Strings.h"

#include "../Parsing/XML.h"

#include <pugixml.hpp>

#include <exception>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace crawlservpp::Query {
	class XPath {
	public:
		XPath();
		virtual ~XPath();

		bool compile(const std::string& xpath, bool textOnly);
		bool getBool(const crawlservpp::Parsing::XML& doc, bool& resultTo) const;
		bool getFirst(const crawlservpp::Parsing::XML& doc, std::string& resultTo) const;
		bool getAll(const crawlservpp::Parsing::XML& doc, std::vector<std::string>& resultTo) const;

		std::string getErrorMessage() const;

	private:
		// walker class for text-only conversion
		class TextOnlyWalker : public pugi::xml_tree_walker {
		public:
			virtual bool for_each(pugi::xml_node& node);
			std::string getResult() const;

		protected:
			std::string result;
		};

		std::unique_ptr<pugi::xpath_query> query;
		bool isTextOnly;

		static std::string nodeToString(const pugi::xpath_node& node, bool textOnly);

		mutable std::string errorMessage;
	};
}

#endif /* QUERY_XPATH_H_ */
