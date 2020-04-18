/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
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

#include <exception>	// std::exception
#include <sstream>		// std::ostringstream
#include <string>		// std::string
#include <vector>		// std::vector

namespace crawlservpp::Query {

	class XPath {
	public:
		// constructor and destructor
		XPath(const std::string& xpath, bool textOnly);
		virtual ~XPath();

		// getters
		bool getBool(const Parsing::XML& doc) const;
		void getFirst(const Parsing::XML& doc, std::string& resultTo) const;
		void getAll(const Parsing::XML& doc, std::vector<std::string>& resultTo) const;
		void getSubSets(const Parsing::XML& doc, std::vector<Parsing::XML>& resultTo) const;

		// class for XPath exceptions
		MAIN_EXCEPTION_CLASS();

		// only moveable (using default), not copyable
		XPath(const XPath&) = delete;
		XPath(XPath&&) = default;
		XPath& operator=(const XPath&) = delete;
		XPath& operator=(XPath&&) = default;

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
