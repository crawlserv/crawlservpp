/*
 * URI.cpp
 *
 * Encapsulation of the RFC 3986 URI parsing library to parse URLs, check whether their domain is identical with the current domain
 *  and get the sub-URL for the current domain.
 *
 *  Created on: Oct 18, 2018
 *      Author: ans
 */

#include "URI.hpp"

namespace crawlservpp::Parsing {

	// constructor stub
	URI::URI() : crossDomain(false) {}

	// destructor stub
	URI::~URI() {}

	// set current domain
	void URI::setCurrentDomain(const std::string& currentHost) {
		if(currentHost.empty()) {
			this->domain.clear();

			this->crossDomain = true; // domain needs to be parsed from current URL
		}
		else
			this->domain = URI::escapeUrl(currentHost);
	}

	// set current URL (beginning with slash if sub-URL or domain if the website is cross-domain), throws URI::Exception
	void URI::setCurrentUrl(const std::string& currentUrl) {
		std::string parsedSubUrl;

		// if website is cross-domain, get the current domain from the URL
		if(this->crossDomain) {
			const size_t domainEnd = currentUrl.find('/');

			if(domainEnd == std::string::npos) {
				this->setCurrentDomain(currentUrl);

				parsedSubUrl = "/";
			}
			else {
				this->setCurrentDomain(currentUrl.substr(0, domainEnd));

				parsedSubUrl = currentUrl.substr(domainEnd);
			}
		}
		else
			parsedSubUrl = currentUrl;

		// error checking
		if(this->domain.empty())
			throw URI::Exception("No domain specified or parsed");

		if(parsedSubUrl.empty())
			throw URI::Exception("Empty sub-URL");

		if(parsedSubUrl.at(0) != '/')
			throw URI::Exception("Sub-URL does not start with slash (\'/\')");

		// escape and set current sub-URL
		this->subUrl = URI::escapeUrl(parsedSubUrl);

		// create current URI string
		this->current = "https://" + this->domain + this->subUrl;

		// create new base URI
		this->base.create();

		// parse (current) base URI
		this->state.uri = this->base.get();

		if(uriParseUriA(&(this->state), current.c_str()) != URI_SUCCESS) {
			const std::string end(this->state.errorPos);
			std::ostringstream errStrStr;

			errStrStr << "URI Parser error #" << this->state.errorCode << ": \'";

			if(end.length() < current.length())
				errStrStr << current.substr(0, current.length() - end.length()) << "[!!!]" << end;
			else
				errStrStr << current << "[!!!]";

			errStrStr << "\' in URI::setCurrentSubUrl()";

			throw URI::Exception(errStrStr.str());
		}
	}

	// check whether parsed link links to current domain, return always true if website is cross-domain
	bool URI::isSameDomain() const {
		if(this->crossDomain)
			return true;

		if(!(this->uri))
			throw URI::Exception("No link parsed");

		return URI::textRangeToString(this->uri.get()->hostText) == this->domain;
	}

	// get sub-URL for link (including domain if website is cross-domain)
	//  while keeping all query arguments
	std::string URI::getSubUrl() {
		return this->getSubUrl(std::vector<std::string>(), false);
	}

	// get sub-URL for link (including domain if website is cross-domain)
	//  while filtering query arguments according to white or black list
	std::string URI::getSubUrl(const std::vector<std::string>& args, bool whiteList) const {
		if(!(this->uri))
			throw URI::Exception("No link parsed");

		if(this->domain.empty())
			throw URI::Exception("No domain specified");

		Wrapper::URIQueryList queryList;
		UriQueryListA * queryNext = nullptr;
		int queryCount = 0;
		std::string queries;

		// get query string
		if(this->uri.get()->query.first != this->uri.get()->query.afterLast) {
			if(
					uriDissectQueryMallocA(
							queryList.getPtr(),
							&queryCount,
							this->uri.get()->query.first,
							this->uri.get()->query.afterLast
					)
					== URI_SUCCESS
			) {
				queryNext = queryList.get();

				while(queryNext) {
					if(
							(
									whiteList && std::find(args.begin(), args.end(), queryNext->key) != args.end()
							) || (
									!whiteList && std::find(args.begin(), args.end(), queryNext->key) == args.end()
							)
					) {
						queries += queryNext->key;

						if(queryNext->value)
							queries += "=" + std::string(queryNext->value);

						queries += "&";
					}

					queryNext = queryNext->next;
				}
			}
		}

		if(!queries.empty())
			queries.pop_back();

		// construct URL (starting with slash if sub-URL or with domain if website is cross-domain)
		UriPathSegmentStructA * nextSegment = this->uri.get()->pathHead;

		std::string result;

		if(this->crossDomain) {
			result = URI::textRangeToString(this->uri.get()->hostText);

			if(result.empty())
				return "";
		}

		while(nextSegment) {
			result += "/" + URI::unescape(URI::textRangeToString(nextSegment->text), false);

			nextSegment = nextSegment->next;
		}

		// add queries
		if(!queries.empty())
			result += "?" + queries;

		return result;
	}

	// parse link to sub-URL (beginning with slash), return false if link is empty, throws URI::Exception
	bool URI::parseLink(const std::string& linkToParse) {
		// reset old URI if necessary
		this->uri.reset();

		// error checking
		if(this->domain.empty())
			throw URI::Exception("No domain specified or parsed");

		if(this->subUrl.empty())
			throw URI::Exception("No current sub-URL specified");

		// copy URL
		std::string linkCopy(linkToParse);

		// remove anchor if necessary
		const unsigned long end = linkCopy.find('#');

		if(end != std::string::npos && linkCopy.length() > end) {
			if(end)
				linkCopy = linkCopy.substr(0, end);
			else
				linkCopy = "";
		}

		// trim and escape URL
		Helper::Strings::trim(linkCopy);

		linkCopy = URI::escapeUrl(linkCopy);

		// check for empty link
		if(linkCopy.empty())
			return false;

		// create new URI
		this->uri.create();

		// create temporary URI for relative link
		Wrapper::URI relativeSource;

		relativeSource.create();

		// NOTE: URL needs to be stored in class BEFORE PARSING to provide long-term access by the parsing library
		//  (otherwise the URL would be out of scope for the library after leaving this member function)
		this->link.swap(linkCopy);

		// parse relative link
		this->state.uri = relativeSource.get();

		if(uriParseUriA(&(this->state), this->link.c_str()) != URI_SUCCESS) {
			const std::string end(this->state.errorPos);
			std::ostringstream errStrStr;

			errStrStr << "URI Parser error #" << this->state.errorCode << ": \'";

			if(end.length() < this->link.length())
				errStrStr << this->link.substr(0, this->link.length() - end.length())	<< "[!!!]" << end;
			else
				errStrStr << this->link << "[!!!]";

			errStrStr << "\' in URIParser::parseLink()";

			throw URI::Exception(errStrStr.str());
		}

		// resolve reference
		if(
				uriAddBaseUriExA(
						this->uri.get(),
						relativeSource.get(),
						this->base.get(),
						URI_RESOLVE_IDENTICAL_SCHEME_COMPAT
				) != URI_SUCCESS
		)
			throw URI::Exception("Reference resolving failed for \'" + this->toString(relativeSource) + "\'");

		// normalize URI
		const unsigned int dirtyParts = uriNormalizeSyntaxMaskRequiredA(this->uri.get());

		if(dirtyParts != URI_NORMALIZED && uriNormalizeSyntaxExA(this->uri.get(), dirtyParts) != URI_SUCCESS)
			throw URI::Exception("Normalizing failed for \'" + this->toString(this->uri) + "\'");

		return true;
	}

	// public static helper function: escape string
	std::string URI::escape(const std::string& string, bool plusSpace) {
		std::unique_ptr<char[]> cString(std::make_unique<char[]>(string.size() * 6 + 1));

		uriEscapeExA(string.c_str(), string.c_str() + string.size(), cString.get(), plusSpace, false);

		return std::string(cString.get());
	}

	// public static helper function: unescape string
	std::string URI::unescape(const std::string& string, bool plusSpace) {
		if(string.empty())
			return "";

		std::unique_ptr<char[]> cString(std::make_unique<char[]>(string.size() + 1));

		for(unsigned long n = 0; n < string.length(); ++n)
			cString[n] = string.at(n);

		cString[string.length()] = '\0';

		uriUnescapeInPlaceExA(cString.get(), plusSpace, URI_BR_DONT_TOUCH);

		return std::string(cString.get());
	}

	// escape an URL but leave reserved characters (; / ? : @ = & # %) intact
	std::string URI::escapeUrl(const std::string& urlToEscape) {
		std::string result;
		unsigned long pos = 0;

		while(pos < urlToEscape.length()) {
			unsigned long end = urlToEscape.find_first_of(";/?:@=&#%", pos);

			if(end == std::string::npos)
				end = urlToEscape.length();

			if(end - pos) {
				const std::string part(urlToEscape, pos, end - pos);

				result += URI::escape(part, false);
			}

			if(end < urlToEscape.length())
				result += urlToEscape.at(end);

			pos = end + 1;
		}

		return result;
	}

	// static helper function: convert URITextRangeA to std::string
	std::string URI::textRangeToString(const UriTextRangeA& range) {
		if(!(range.first) || *(range.first) == 0 || !(range.afterLast) || range.afterLast <= range.first)
			return "";

		return std::string(range.first, range.afterLast - range.first);
	}

	// static helper function: convert URI to string
	std::string URI::toString(const Wrapper::URI& uri) {
		if(!uri)
			return "";

		int charsRequired = 0;

		if(uriToStringCharsRequiredA(uri.get(), &charsRequired) != URI_SUCCESS)
			throw URI::Exception("Could not convert URI to string because uriToStringCharsRequiredA(...) failed");

		std::unique_ptr<char[]> uriCString(std::make_unique<char[]>(charsRequired + 1));

		if(uriToStringA(uriCString.get(), uri.get(), charsRequired + 1, nullptr) != URI_SUCCESS)
			throw URI::Exception("Could not convert URI to string because uriToStringA(...) failed");

		return std::string(uriCString.get());
	}

} /* crawlservpp::Parsing */
