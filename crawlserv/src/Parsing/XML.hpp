/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
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
 * XML.hpp
 *
 * Encapsulation of the HTML Tidy API implemented by tidy library and the pugixml parser library to parse a HTML document into clean XML.
 *
 *  Created on: Oct 18, 2018
 *      Author: ans
 */

#ifndef PARSING_XML_HPP_
#define PARSING_XML_HPP_

#include "HTML.hpp"

#include "../Main/Exception.hpp"

#include <pugixml.hpp>

#include <memory>	// std::make_unique, std::unique_ptr
#include <queue>	// std::queue
#include <sstream>	// std::istringstream, std::ostringstream
#include <string>	// std::string, std::to_string

namespace crawlservpp::Query {

	class XPath;

} /* crawlservpp::Query */

namespace crawlservpp::Parsing {

	class XML {
		friend class Query::XPath;

	public:
		// constructors and destructor
		XML();
		XML(const pugi::xml_node& node);
		virtual ~XML();

		// getter
		void getContent(std::string& resultTo) const;

		// setter
		void setOptions(bool showWarnings, unsigned int numOfErrors);

		// parsing function
		void parse(const std::string& content, std::queue<std::string>& warningsTo, bool repairCData);

		// clearing function
		void clear();

		// class for XML exceptions
		MAIN_EXCEPTION_CLASS();

		// operators
		explicit operator bool() const noexcept;
		bool operator!() const noexcept;

		// only moveable (using default), not copyable
		XML(const XML&) = delete;
		XML(XML&&) noexcept = default;
		XML& operator=(const XML&) = delete;
		XML& operator=(XML&&) noexcept = default;

	protected:
		// unique pointer to (pugi)XML document
		std::unique_ptr<pugi::xml_document> doc;

		// internal static helper function
		static void cDataRepair(std::string& content);

	private:
		// options
		bool warnings;
		unsigned int errors;
	};

} /* crawlservpp::Parsing */

#endif /* PARSING_XML_HPP_ */
