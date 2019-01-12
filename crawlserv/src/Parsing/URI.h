/*
 * URI.h
 *
 * Encapsulation of the RFC 3986 URI parsing library to parse URLs, checking whether their domain is identical with the current domain
 *  and getting the sub-URL for the current domain.
 *
 *  Created on: Oct 18, 2018
 *      Author: ans
 */

#ifndef PARSING_URI_H_
#define PARSING_URI_H_

#include "../Helper/Strings.h"

#include <uriparser/Uri.h>

#include <algorithm>
#include <exception>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace crawlservpp::Parsing {
	class URI {
	public:
		URI();
		virtual ~URI();

		bool setCurrentDomain(const std::string& currentDomain);
		bool setCurrentSubUrl(const std::string& currentSubUrl);
		bool parseLink(const std::string& linkToParse);

		bool isSameDomain() const;
		std::string getSubUrl(const std::vector<std::string>& args, bool whiteList) const;

		std::string getErrorMessage() const;

		static std::string escape(const std::string& string, bool plusSpace);
		static std::string unescape(const std::string& string, bool plusSpace);
		static std::string escapeUrl(const std::string& urlToEscape);

	protected:
		std::string domain;
		std::string subUrl;

	private:
		UriParserStateA state;
		std::string current;
		std::string link;
		UriUriA * base;
		UriUriA * uri;

		mutable std::string errorMessage;

		static std::string uriTextRangeToString(const UriTextRangeA * range);
		static std::string uriToString(const UriUriA * uri);
	};
}

#endif /* PARSING_URI_H_ */
