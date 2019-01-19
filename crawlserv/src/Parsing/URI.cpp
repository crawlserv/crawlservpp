/*
 * URI.cpp
 *
 * Encapsulation of the RFC 3986 URI parsing library to parse URLs, check whether their domain is identical with the current domain
 *  and get the sub-URL for the current domain.
 *
 *  Created on: Oct 18, 2018
 *      Author: ans
 */

#include "URI.h"

namespace crawlservpp::Parsing {

// constructor
URI::URI() {
	this->base = NULL;
	this->uri = NULL;
}

// destructor
URI::~URI() {
	if(this->base) {
		uriFreeUriMembersA(this->base);
		delete this->base;
		this->base = NULL;
	}
	if(this->uri) {
		uriFreeUriMembersA(this->uri);
		delete this->uri;
		this->uri = NULL;
	}
}

// set current host
bool URI::setCurrentDomain(const std::string& currentHost) {
	if(currentHost.empty()) {
		this->errorMessage = "URI Parser error: Empty domain.";
		return false;
	}
	this->domain = URI::escapeUrl(currentHost);
	return true;
}

// set current sub-URL (beginning with slash)
bool URI::setCurrentSubUrl(const std::string& currentSubUrl) {
	// error checking
	if(this->domain.empty()) {
		this->errorMessage = "URI Parser error: No current domain specified.";
		return false;
	}
	if(currentSubUrl.empty()) {
		this->errorMessage = "URI Parser error: Empty sub-URL.";
		return false;
	}
	if(currentSubUrl.at(0) != '/') {
		this->errorMessage = "URI Parser error: Sub-URL does not start with slash (\'/\').";
		return false;
	}
	this->subUrl = URI::escapeUrl(currentSubUrl);

	// create current URI string
	this->current = "https://" + this->domain + this->subUrl;

	// delete old base URI if necessary
	if(this->base) {
		uriFreeUriMembersA(this->base);
		delete this->base;
		this->base = NULL;
	}

	// create new base URI
	this->base = new UriUriA;

	// parse (current) base URI
	this->state.uri = this->base;
	if(uriParseUriA(&(this->state), current.c_str()) != URI_SUCCESS) {
		std::ostringstream errStrStr;
		std::string end(this->state.errorPos);
		errStrStr << "URI Parser error #" << this->state.errorCode << ": \'";
		if(end.length() < current.length()) errStrStr << current.substr(0, current.length() - end.length()) << "[!!!]" << end;
		else errStrStr << current << "[!!!]";
		errStrStr << "\' in URIParser::setCurrentSubUrl().";
		this->errorMessage = errStrStr.str();
		return false;
	}
	return true;
}

// parse link to sub-URL (beginning with slash)
bool URI::parseLink(const std::string& linkToParse) {
	// error checking
	if(this->domain.empty()) {
		this->errorMessage = "URI Parser error: No current domain specified.";
		return false;
	}
	if(this->subUrl.empty()) {
		this->errorMessage = "URI Parser error: No current sub-URL specified.";
		return false;
	}

	// copy URL
	std::string linkCopy = linkToParse;

	// remove anchor if necessary
	unsigned long end = linkCopy.find('#');
	if(end != std::string::npos && linkCopy.length() > end) {
		if(end) linkCopy = linkCopy.substr(0, end);
		else linkCopy = "";
	}

	// trim URL
	crawlservpp::Helper::Strings::trim(linkCopy);
	linkCopy = URI::escapeUrl(linkCopy);

	// delete old URI if necessary
	if(this->uri) {
		uriFreeUriMembersA(this->uri);
		delete this->uri;
		this->uri = NULL;
	}

	if(linkCopy.empty()) {
		this->errorMessage = "";
		return false;
	}

	// create new URI
	this->uri = new UriUriA;

	// create temporary URI for relative link
	UriUriA relativeSource;

	// URL needs to be stored in class BEFORE parsing for long-term access by the parsing library
	// (otherwise the URL would be out of scope for the library after leaving this member function)
	this->link.swap(linkCopy);

	// parse relative link
	this->state.uri = &relativeSource;
	if(uriParseUriA(&(this->state), this->link.c_str()) != URI_SUCCESS) {
		std::ostringstream errStrStr;
		std::string end(this->state.errorPos);
		errStrStr << "URI Parser error #" << this->state.errorCode << ": \'";
		if(end.length() < this->link.length())
			errStrStr << this->link.substr(0, this->link.length() - end.length())	<< "[!!!]" << end;
		else
			errStrStr << this->link << "[!!!]";
		errStrStr << "\' in URIParser::parseLink().";
		this->errorMessage = errStrStr.str();
		uriFreeUriMembersA(&relativeSource);
		delete this->uri;
		this->uri = NULL;
		return false;
	}

	// resolve reference
	if(uriAddBaseUriExA(this->uri, &relativeSource, this->base, URI_RESOLVE_IDENTICAL_SCHEME_COMPAT) != URI_SUCCESS) {
		this->errorMessage = "URI Parser error: Reference resolving failed for \'" + this->toString(&relativeSource) + "\'.";
		uriFreeUriMembersA(&relativeSource);
		delete this->uri;
		this->uri = NULL;
		return false;
	}

	// normalize URI
	const unsigned int dirtyParts = uriNormalizeSyntaxMaskRequiredA(this->uri);
	if(uriNormalizeSyntaxExA(this->uri, dirtyParts) != URI_SUCCESS) {
		uriFreeUriMembersA(&relativeSource);
		this->errorMessage = "URI Parser error: Normalizing failed for \'" + this->toString(this->uri) + "\'.";
		uriFreeUriMembersA(this->uri);
		delete this->uri;
		this->uri = NULL;
		return false;
	}

	// free memory for temporary URI
	uriFreeUriMembersA(&relativeSource);
	return true;
}

// check whether parsed link links to current domain
bool URI::isSameDomain() const {
	if(this->domain.empty()) throw std::runtime_error("URI Parser error - No current domain specified");
	if(!(this->uri)) throw std::runtime_error("URI Parser error - No link parsed");
	return URI::textRangeToString(&(this->uri->hostText)) == this->domain;
}

// get sub-URL for link
std::string URI::getSubUrl(const std::vector<std::string>& args, bool whiteList) const {
	if(!(this->uri)) throw std::runtime_error("URI Parser error - No link parsed");
	UriQueryListA * queryList = NULL;
	UriQueryListA * queryNext = NULL;
	int queryCount = 0;
	std::string queries;

	// get query string
	if(this->uri->query.first != this->uri->query.afterLast) {
		if(uriDissectQueryMallocA(&queryList, &queryCount, this->uri->query.first, this->uri->query.afterLast) == URI_SUCCESS) {
			queryNext = queryList;
			while(queryNext) {
				if((whiteList && std::find(args.begin(), args.end(), queryNext->key) != args.end()) ||
						(!whiteList && std::find(args.begin(), args.end(), queryNext->key) == args.end())) {
					queries += queryNext->key;
					if(queryNext->value) queries += "=" + std::string(queryNext->value);
					queries += "&";
				}
				queryNext = queryNext->next;
			}
			uriFreeQueryListA(queryList);
		}
	}

	if(!queries.empty()) queries.pop_back();

	// construct sub-URL (starting with slash)
	UriPathSegmentStructA * nextSegment = this->uri->pathHead;
	std::string result;
	while(nextSegment) {
		result += "/" + URI::unescape(URI::textRangeToString(&(nextSegment->text)), false);
		nextSegment = nextSegment->next;
	}

	// add queries
	if(!queries.empty()) result += "?" + queries;

	return result;
}

// get error string
std::string URI::getErrorMessage() const {
	return this->errorMessage;
}

// public static helper function: escape string
std::string URI::escape(const std::string& string, bool plusSpace) {
	char * cString = new char[string.length() * 6 + 1];
	uriEscapeExA(string.c_str(), string.c_str() + string.length(), cString, plusSpace, false);
	std::string result = cString;
	delete[] cString;
	return result;
}

// public static helper function: unescape string
std::string URI::unescape(const std::string& string, bool plusSpace) {
	if(string.empty()) return "";
	char * cString = new char[string.length() + 1];
	for(unsigned long n = 0; n < string.length(); n++) cString[n] = string.at(n);
	cString[string.length()] = '\0';
	uriUnescapeInPlaceExA(cString, plusSpace, URI_BR_DONT_TOUCH);
	std::string result = cString;
	delete[] cString;
	return result;
}

// escape an URL but leave reserved characters (; / ? : @ = & # %) intact
std::string URI::escapeUrl(const std::string& urlToEscape) {
	unsigned long pos = 0;
	std::string result;

	pos = 0;
	while(pos < urlToEscape.length()) {
		unsigned long end = urlToEscape.find_first_of(";/?:@=&#%", pos);
		if(end == std::string::npos) end = urlToEscape.length();
		if(end - pos) {
			std::string part = urlToEscape.substr(pos, end - pos);
			result += URI::escape(part, false);
		}
		if(end < urlToEscape.length()) result += urlToEscape.at(end);
		pos = end + 1;
	}
	return result;
}

// static helper function: convert URITextRangeA to string
std::string URI::textRangeToString(const UriTextRangeA * range) {
	if(!range || !(range->first) || range->first == range->afterLast) return "";
	std::string result = std::string(range->first, 0, range->afterLast - range->first);
	return result;
}

// static helper function: convert URI to string
std::string URI::toString(const UriUriA * uri) {
	char * uriCString = NULL;
	int charsRequired = 0;
	std::string result;

	if(!uri) return "";

	if(uriToStringCharsRequiredA(uri, &charsRequired) != URI_SUCCESS)
	    throw std::runtime_error("URI Parser error - Could not convert URI to string because uriToStringCharsRequiredA(...) failed");

	uriCString = new char[charsRequired + 1];

	if(uriToStringA(uriCString, uri, charsRequired + 1, NULL) != URI_SUCCESS) {
		delete[] uriCString;
		throw std::runtime_error("URI Parser error - Could not convert URI to string because uriToStringA(...) failed");
	}

	result = uriCString;
	delete[] uriCString;
	return result;
}

}
