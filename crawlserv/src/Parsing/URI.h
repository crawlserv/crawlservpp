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

#include "../Main/Exception.h"
#include "../Helper/Strings.h"
#include "../Wrapper/URI.h"
#include "../Wrapper/URIQueryList.h"

#include <uriparser/Uri.h>

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace crawlservpp::Parsing {
	class URI final {
	public:
		URI();
		virtual ~URI();

		// setters
		void setCurrentDomain(const std::string& currentDomain);
		void setCurrentSubUrl(const std::string& currentSubUrl);

		// getters
		bool isSameDomain() const;
		std::string getSubUrl(const std::vector<std::string>& args, bool whiteList) const;

		// parser function
		bool parseLink(const std::string& linkToParse);

		// escape functions
		static std::string escape(const std::string& string, bool plusSpace);
		static std::string unescape(const std::string& string, bool plusSpace);
		static std::string escapeUrl(const std::string& urlToEscape);

		// sub-class for URI exceptions
		class Exception : public crawlservpp::Main::Exception {
		public:
			Exception(const std::string& description) : crawlservpp::Main::Exception(description) {}
			virtual ~Exception() {}
		};

		// not moveable, not copyable
		URI(URI&) = delete;
		URI(URI&&) = delete;
		URI& operator=(URI) = delete;
		URI& operator=(URI&&) = delete;

	private:
		std::string domain;
		std::string subUrl;
		std::string current;
		std::string link;

		UriParserStateA state;

		crawlservpp::Wrapper::URI base, uri;

		static std::string textRangeToString(const UriTextRangeA& range);
		static std::string toString(const crawlservpp::Wrapper::URI& uri);
	};
}

#endif /* PARSING_URI_H_ */
