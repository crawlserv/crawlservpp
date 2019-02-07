/*
 * XPath.h
 *
 * Using the pugixml parser library to implement a XPath query with boolean, single and/or multiple results.
 *  Query is only crated when needed.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef QUERY_XPATH_H_
#define QUERY_XPATH_H_

#include "../Helper/Strings.h"
#include "../Main/Exception.h"
#include "../Parsing/XML.h"

#include <pugixml.hpp>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace crawlservpp::Query {
	class XPath {
	public:
		// constructors
		XPath();
		XPath(XPath&& other);

		// destructor
		virtual ~XPath();

		// getters
		bool getBool(const crawlservpp::Parsing::XML& doc) const;
		void getFirst(const crawlservpp::Parsing::XML& doc, std::string& resultTo) const;
		void getAll(const crawlservpp::Parsing::XML& doc, std::vector<std::string>& resultTo) const;

		// control function
		void compile(const std::string& xpath, bool textOnly);

		// operator
		XPath& operator=(XPath&& other);

		// not copyable
		XPath(XPath&) = delete;
		XPath& operator=(XPath&) = delete;

		// sub-class for XPath exceptions
		class Exception : public crawlservpp::Main::Exception {
		public:
			Exception(const std::string& description) : crawlservpp::Main::Exception(description) {}
			virtual ~Exception() {}
		};

	private:
		pugi::xpath_query query;
		bool compiled;
		bool isTextOnly;

		// static helper function
		static std::string nodeToString(const pugi::xpath_node& node, bool textOnly);

		// sub-class for text-only conversion walker
		class TextOnlyWalker : public pugi::xml_tree_walker {
		public:
			virtual bool for_each(pugi::xml_node& node);
			std::string getResult() const;

		protected:
			std::string result;
		};
	};
}

#endif /* QUERY_XPATH_H_ */
