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
		XPath();
		virtual ~XPath();

		void compile(const std::string& xpath, bool textOnly);
		bool getBool(const crawlservpp::Parsing::XML& doc) const;
		void getFirst(const crawlservpp::Parsing::XML& doc, std::string& resultTo) const;
		void getAll(const crawlservpp::Parsing::XML& doc, std::vector<std::string>& resultTo) const;

		// sub-class for XPath exceptions
		class Exception : public crawlservpp::Main::Exception {
		public:
			Exception(const std::string& description) : crawlservpp::Main::Exception(description) {}
			virtual ~Exception() {}
		};

		// only moveable, not copyable
		XPath(XPath&) = delete;
		XPath(XPath&& other) {
			this->query = std::move(other.query);
			this->compiled = std::move(other.compiled);
			this->isTextOnly = std::move(other.isTextOnly);
		}
		XPath& operator=(XPath&) = delete;
		XPath& operator=(XPath&& other) {
			if(&other != this) {
				this->query = std::move(other.query);
				this->compiled = std::move(other.compiled);
				this->isTextOnly = std::move(other.isTextOnly);
			}
			return *this;
		}

	private:
		// walker class for text-only conversion
		class TextOnlyWalker : public pugi::xml_tree_walker {
		public:
			virtual bool for_each(pugi::xml_node& node);
			std::string getResult() const;

		protected:
			std::string result;
		};

		pugi::xpath_query query;
		bool compiled;
		bool isTextOnly;

		static std::string nodeToString(const pugi::xpath_node& node, bool textOnly);
	};
}

#endif /* QUERY_XPATH_H_ */
