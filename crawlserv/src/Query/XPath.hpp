/*
 * XPath.hpp
 *
 * Using the pugixml parser library to implement a XPath query with boolean, single and/or multiple results.
 *  Query is only created when needed.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef QUERY_XPATH_HPP_
#define QUERY_XPATH_HPP_

#include "../Helper/Strings.hpp"
#include "../Main/Exception.hpp"
#include "../Parsing/XML.hpp"

#include <pugixml.hpp>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace crawlservpp::Query {

	class XPath {
	public:
		// constructor
		XPath(const std::string& xpath, bool textOnly);

		// getters
		bool getBool(const Parsing::XML& doc) const;
		void getFirst(const Parsing::XML& doc, std::string& resultTo) const;
		void getAll(const Parsing::XML& doc, std::vector<std::string>& resultTo) const;

		// sub-class for XPath exceptions
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
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

} /* crawlservpp::Query */

#endif /* QUERY_XPATH_HPP_ */
