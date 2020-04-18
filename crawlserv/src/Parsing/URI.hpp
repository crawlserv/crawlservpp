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
 * URI.hpp
 *
 * Encapsulation of the RFC 3986 URI parsing library to parse URLs, checking whether their domain is identical with the current domain
 *  and getting the sub-URL for the current domain.
 *
 *  Created on: Oct 18, 2018
 *      Author: ans
 */

#ifndef PARSING_URI_HPP_
#define PARSING_URI_HPP_

#include "../Main/Exception.hpp"
#include "../Helper/Strings.hpp"
#include "../Wrapper/URI.hpp"
#include "../Wrapper/URIQueryList.hpp"

#include <uriparser/Uri.h>

#include <algorithm>	// std::find
#include <memory>		// std::make_unique, std::unique_ptr
#include <string>		// std::string, std::to_string
#include <vector>		// std::vector

namespace crawlservpp::Parsing {

	class URI final {
	public:
		// constructor and destructor
		URI();
		virtual ~URI();

		// setters
		void setCurrentDomain(const std::string& currentDomain);
		void setCurrentUrl(const std::string& currentUrl);

		// getters
		bool isSameDomain() const;
		std::string getSubUrl();
		std::string getSubUrl(const std::vector<std::string>& args, bool whiteList) const;

		// parser function
		bool parseLink(const std::string& linkToParse);

		// static helper functions
		static std::string escape(const std::string& string, bool plusSpace);
		static std::string unescape(const std::string& string, bool plusSpace);
		static std::string escapeUrl(const std::string& urlToEscape);
		static void makeAbsolute(const std::string& base, std::vector<std::string>& urls);

		// class for URI exceptions
		MAIN_EXCEPTION_CLASS();

		// only moveable (using default), not copyable
		URI(URI&) = delete;
		URI(URI&&) = default;
		URI& operator=(URI) = delete;
		URI& operator=(URI&&) = default;

	private:
		std::string domain;
		std::string subUrl;
		std::string current;
		std::string link;

		bool crossDomain;

		UriParserStateA state;

		Wrapper::URI base, uri;

		static std::string textRangeToString(const UriTextRangeA& range);
		static std::string toString(const Wrapper::URI& uri);
	};

} /* crawlservpp::Parsing */

#endif /* PARSING_URI_HPP_ */
