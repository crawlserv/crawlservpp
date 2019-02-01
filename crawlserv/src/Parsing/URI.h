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
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace crawlservpp::Parsing {
	class URI final {
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

	private:
		std::string domain;
		std::string subUrl;
		std::string current;
		std::string link;

		UriParserStateA state;

		// RAII wrapper sub-class for URI structures
		class URIWrapper {
		public:
			// constructor stub
			URIWrapper() {}

			// destructor: free and reset structure
			virtual ~URIWrapper() { this->reset(); }

			// create structure, free old structure if necessary
			void create() { if(this->ptr) { uriFreeUriMembersA(this->ptr.get()); } this->ptr = std::make_unique<UriUriA>(); }

			// free and reset structure if necessary
			void reset() { if(this->ptr) { uriFreeUriMembersA(this->ptr.get()); this->ptr.reset(); } }

			// get const pointer to structure
			const UriUriA * get() const { return this->ptr.get(); }

			// get non-const pointer to structure
			UriUriA * get() { return this->ptr.get(); }

			// check pointer for NULL
			operator bool() const { return this->ptr.operator bool(); }

			// rule of five
			URIWrapper(URIWrapper const&) = delete;
			URIWrapper(URIWrapper&&) = delete;
			URIWrapper& operator=(URIWrapper const&) = delete;
			URIWrapper& operator=(URIWrapper&&) = delete;

		private:
			std::unique_ptr<UriUriA> ptr;
		} base, uri;

		// RAII wrapper sub-class for URI query lists (does NOT have ownership of the pointer!)
		class URIQueryListWrapper {
		public:
			// constructor: set pointer to NULL
			URIQueryListWrapper() { this->ptr = NULL; }

			// destructor: free query list if necessary
			virtual ~URIQueryListWrapper() { if(this->ptr) uriFreeQueryListA(this->ptr); }

			// get const pointer to query list
			const UriQueryListA * get() const { return this->ptr; }

			// get non-const pointer to query list
			UriQueryListA * get() { return this->ptr; }

			// get non-const pointer to pointer to query list
			UriQueryListA ** getPtr() { return &(this->ptr); }

			// rule of five
			URIQueryListWrapper(URIQueryListWrapper const&) = delete;
			URIQueryListWrapper(URIQueryListWrapper&&) = delete;
			URIQueryListWrapper& operator=(URIQueryListWrapper const&) = delete;
			URIQueryListWrapper& operator=(URIQueryListWrapper&&) = delete;

		private:
			UriQueryListA * ptr;
		};

		mutable std::string errorMessage;

		static std::string textRangeToString(const UriTextRangeA * range);
		static std::string toString(const URI::URIWrapper& uri);
	};
}

#endif /* PARSING_URI_H_ */
