/*
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

		// escape functions
		static std::string escape(const std::string& string, bool plusSpace);
		static std::string unescape(const std::string& string, bool plusSpace);
		static std::string escapeUrl(const std::string& urlToEscape);

		// sub-class for URI exceptions
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
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

		bool crossDomain;

		UriParserStateA state;

		Wrapper::URI base, uri;

		static std::string textRangeToString(const UriTextRangeA& range);
		static std::string toString(const Wrapper::URI& uri);
	};

} /* crawlservpp::Parsing */

#endif /* PARSING_URI_HPP_ */
