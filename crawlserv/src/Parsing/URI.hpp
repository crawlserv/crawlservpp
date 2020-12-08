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
 * Parser for RFC 3986 URIs that can also analyze their relationships with each other.
 *
 *  Created on: Oct 18, 2018
 *      Author: ans
 */

#ifndef PARSING_URI_HPP_
#define PARSING_URI_HPP_

#include "../Helper/Strings.hpp"
#include "../Main/Exception.hpp"
#include "../Wrapper/URI.hpp"
#include "../Wrapper/URIQueryList.hpp"

#include <uriparser/Uri.h>

#include <algorithm>	// std::find
#include <cstddef>		// std::size_t
#include <memory>		// std::make_unique
#include <string>		// std::string, std::to_string
#include <string_view>	// std::string_view
#include <vector>		// std::vector

namespace crawlservpp::Parsing {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! Maximum length of a URL-escaped character.
	inline constexpr auto maxEscapedCharLength{6};

	///@}

	/*
	 * DECLARATION
	 */

	//! Parser for RFC 3986 URIs that can also analyze their relationships with each other
	/*!
	 * Parses URIs, analyzes their relationship to other URIs and provides
	 * 	encoding (escaping) functionality.
	 */
	class URI {
	public:
		///@name Getters
		///@{

		[[nodiscard]] bool isSameDomain() const;
		[[nodiscard]] std::string getSubUri() const;
		[[nodiscard]] std::string getSubUri(const std::vector<std::string>& args, bool whiteList) const;

		///@}
		///@name Setters
		///@{

		void setCurrentDomain(std::string_view currentDomain);
		void setCurrentOrigin(std::string_view baseUri);

		///@}
		///@name Parsing
		///@{

		bool parseLink(std::string_view uriToParse);

		///@}
		///@name Static Helpers
		///@{

		static std::string escape(std::string_view string, bool plusSpace);
		static std::string unescape(std::string_view string, bool plusSpace);
		static std::string escapeUri(std::string_view uriToEscape);
		static void makeAbsolute(std::string_view uriBase, std::vector<std::string>& uris);

		///@}

		//! Class for %URI exceptions.
		/*!
		 * This exception will be thrown when
		 * - no %URI has been parsed while trying
		 *    to retrieve data about it.
		 * - no domain has been specified or
		 *    parsed, either while trying to
		 *    retrieve a sub-URI or after an
		 *    %URI has been parsed
		 * - a parsed sub-URI is empty or
		 *    does not start with a slash
		 * - no sub-URI has been parsed while
		 *    trying to parse a %URI
		 * - a reference could not be resolved
		 *    while parsing a %URI
		 * - URI normalization failed while
		 *    parsing a %URI
		 * - another error occured while parsing
		 *    a %URI
		 * - the conversion of a %URI to a string
		 *    failed
		 */
		MAIN_EXCEPTION_CLASS();

	private:
		// internal strings
		std::string domain;
		std::string subUri;
		std::string current;
		std::string link;

		// specifies whether the current website is cross-domain
		bool crossDomain{false};

		// base and current URI
		Wrapper::URI base;
		Wrapper::URI uri;

		// private static helper functions
		static std::string textRangeToString(const UriTextRangeA& range);
		static std::string toString(const Wrapper::URI& src);
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Checks whether the parsed %URI links to the current domain.
	/*!
	 * \returns True, if the parsed %URI links
	 *   to the current domain or the current
	 *   website is cross-domain. False otherwise.
	 *
	 * \throws URI::Exception if no %URI has
	 *   been parsed.
	 */
	inline bool URI::isSameDomain() const {
		if(this->crossDomain) {
			return true;
		}

		if(!(this->uri.valid())) {
			throw URI::Exception(
					"Parsing::URI::isSameDomain():"
					" No URI has been parsed"
			);
		}

		return URI::textRangeToString(this->uri.getc()->hostText) == this->domain;
	}

	//! Gets the sub-URI for the current %URI.
	/*!
	 * The current domain and origin need to
	 *  be set before getting the sub-URI.
	 *  A %URI needs to be parsed as well.
	 *
	 * \returns A copy of the sub-URI, including
	 *   the domain if the current website is
	 *   cross-domain.
	 *
	 * \throws URI::Exception if no %URI has
	 *   been parsed or no domain has been
	 *   either specified or parsed.
	 *
	 * \sa setCurrentDomain, setCurrentOrigin,
	 *   parse
	 */
	inline std::string URI::getSubUri() const {
		return this->getSubUri(std::vector<std::string>(), false);
	}

	//! Gets the sub-URI for the current %URI, filtering its query list.
	/*!
	 * The current domain and origin need to
	 *  be set before getting the sub-URI.
	 *  A %URI needs to be parsed as well.
	 *
	 * \param args Vector containing the names
	 *   of query list parameters to either
	 *   ignore (if whiteList is false) or
	 *   keep (if whiteList is true).
	 * \param whiteList Specifies whether args
	 *   is a white list or a black list of
	 *   query list names.
	 *
	 * \returns A copy of the resulting sub-URI,
	 *   including the domain if the current
	 *   website is cross-domain.
	 *
	 * \throws URI::Exception if no domain
	 *   has been specified or parse, or no
	 *   %URI has been parsed.
	 *
	 * \sa setCurrentDomain, setCurrentOrigin,
	 *   parse
	 */
	inline std::string URI::getSubUri(const std::vector<std::string>& args, bool whiteList) const {
		if(this->domain.empty()) {
			throw URI::Exception(
					"Parsing::URI::getSubUri():"
					" No domain has been specified or parsed"
			);
		}

		if(!(this->uri.valid())) {
			throw URI::Exception(
					"Parsing::URI::getSubUri():"
					" No URI has been parsed"
			);
		}

		Wrapper::URIQueryList queryList;
		const UriQueryListA * queryNext{nullptr};
		int queryCount{0};
		std::string queries;

		// get query string
		if(this->uri.getc()->query.first != this->uri.getc()->query.afterLast) {
			if(
					uriDissectQueryMallocA(
							queryList.getPtr(),
							&queryCount,
							this->uri.getc()->query.first,
							this->uri.getc()->query.afterLast
					)
					== URI_SUCCESS
			) {
				queryNext = queryList.getc();

				while(queryNext != nullptr) {
					if(
							(
									whiteList
									&& std::find(
											args.cbegin(),
											args.cend(),
											queryNext->key
									) != args.cend()
							) || (
									!whiteList
									&& std::find(
											args.cbegin(),
											args.cend(),
											queryNext->key
									) == args.cend()
							)
					) {
						queries += queryNext->key;

						if(queryNext->value != nullptr) {
							queries += "=" + std::string(queryNext->value);
						}

						queries += "&";
					}

					queryNext = queryNext->next;
				}
			}
		}

		if(!queries.empty()) {
			queries.pop_back();
		}

		// construct URI (starting with slash if sub-URI or with domain if website is cross-domain)
		UriPathSegmentStructA * nextSegment = this->uri.getc()->pathHead;

		std::string result;

		if(this->crossDomain) {
			result = URI::textRangeToString(this->uri.getc()->hostText);

			if(result.empty()) {
				return "";
			}
		}

		while(nextSegment != nullptr) {
			result += "/" + URI::unescape(URI::textRangeToString(nextSegment->text), false);

			nextSegment = nextSegment->next;
		}

		// add queries
		if(!queries.empty()) {
			result += "?" + queries;
		}

		return result;
	}

	//! Sets the current domain.
	/*!
	 * \param currentDomain View of the domain
	 *   currently being used or of an empty
	 *   string if the current website is
	 *   cross-domain.
	 */
	inline void URI::setCurrentDomain(std::string_view currentDomain) {
		if(currentDomain.empty()) {
			this->domain.clear();

			this->crossDomain = true; // domain needs to be parsed from current URI
		}
		else {
			this->domain = URI::escapeUri(currentDomain);
		}
	}

	//! Sets the current origin.
	/*!
	 * Links will be parsed originating
	 *  from this URI. A domain needs to
	 *  be set before setting the %URI.
	 *
	 * \param baseUri View of the %URI
	 *   to be used as current origin.
	 *   It should begin with a slash
	 *   if it is a sub-URI or with
	 *   the domain if the current
	 *   website is cross-domain.
	 *
	 * \throws URI::Exception if no domain
	 *   has been specified or parsed, the
	 *   sub-URI is empty, the sub-URI does
	 *   not start with a slash, or an error
	 *   occured during %URI parsing.
	 *
	 * \sa setCurrentDomain
	 */
	inline void URI::setCurrentOrigin(std::string_view baseUri) {
		std::string parsedSubUri;

		// if website is cross-domain, get the current domain from the URI
		if(this->crossDomain) {
			const auto domainEnd{baseUri.find('/')};

			if(domainEnd == std::string::npos) {
				this->setCurrentDomain(baseUri);

				parsedSubUri = "/";
			}
			else {
				this->setCurrentDomain(baseUri.substr(0, domainEnd));

				parsedSubUri = baseUri.substr(domainEnd);
			}
		}
		else {
			parsedSubUri = baseUri;
		}

		// error checking
		if(this->domain.empty()) {
			throw URI::Exception(
					"Parsing::URI::setCurrentOrigin():"
					" No domain has been specified or parsed"
			);
		}

		if(parsedSubUri.empty()) {
			throw URI::Exception(
					"Parsing::URI::setCurrentOrigin():"
					" Parsed sub-URI is empty"
			);
		}

		if(parsedSubUri.at(0) != '/') {
			throw URI::Exception(
					"Parsing::URI::setCurrentOrigin():"
					" Parsed sub-URI does not start with slash ('/')"
			);
		}

		// escape and set current sub-URI
		this->subUri = URI::escapeUri(parsedSubUri);

		// create current URI string
		this->current = "https://" + this->domain + this->subUri;

		// create new base URI
		this->base.create();

		// parse (current) base URI
		const char * errorPos{nullptr};
		const auto errorCode{
			uriParseSingleUriA(
					this->base.get(),
					this->current.c_str(),
					&errorPos
			)
		};

		if(errorCode != URI_SUCCESS) {
			const std::string end(
					errorPos,
					baseUri.size() - (errorPos - baseUri.data())
			);

			std::string errorString{
				"Parsing::URI::setCurrentOrigin():"
				" URI Parser error #"
			};

			errorString += std::to_string(errorCode);
			errorString += ": '";

			if(end.size() < this->current.size()) {
				errorString += this->current.substr(0, this->current.size() - end.size());
				errorString += "[!!!]";
				errorString += end;
			}
			else {
				errorString += this->current + "[!!!]";
			}

			errorString += "'";

			throw URI::Exception(errorString);
		}
	}

	//! Parses a link, either abolute or into a sub-URI.
	/*!
	 * Both domain and current origin
	 *  need to be set before parsing
	 *  a link.
	 *
	 * The new sub-URI will be saved
	 *  in-class.
	 *
	 * \param uriToParse View of the %URI
	 *   to parse into a sub-URI (beginning
	 *   with a slash).
	 *
	 * \returns True, if the parsing
	 *   was successful. False, if the
	 *   given string is empty.
	 *
	 * \throws URI::Exception if no domain
	 *   has been specified or parsed, no
	 *   sub-URI has been previously parsed,
	 *   an error occured during parsing the
	 *   %URI, reference resolving failed, or
	 *   the normalization of the URI failed.
	 *
	 * \sa setCurrentDomain, setCurrentOrigin
	 *
	 */
	inline bool URI::parseLink(std::string_view uriToParse) {
		// reset old URI if necessary
		this->uri.clear();

		// error checking
		if(this->domain.empty()) {
			throw URI::Exception(
					"Parsing::URI::parseLink():"
					" No domain has been specified or parsed"
			);
		}

		if(this->subUri.empty()) {
			throw URI::Exception(
					"Parsing::URI::parseLink():"
					" No sub-URI has been parsed"
			);
		}

		// copy URI
		std::string linkCopy(uriToParse);

		// remove anchor if necessary
		const auto end{linkCopy.find('#')};

		if(end != std::string::npos && linkCopy.size() > end) {
			if(end > 0) {
				linkCopy = linkCopy.substr(0, end);
			}
			else {
				linkCopy = "";
			}
		}

		// trim and escape URI
		Helper::Strings::trim(linkCopy);

		linkCopy = URI::escapeUri(linkCopy);

		// check for empty link
		if(linkCopy.empty()) {
			return false;
		}

		// create new URI
		this->uri.create();

		// create temporary URI for relative link
		Wrapper::URI relativeSource;

		relativeSource.create();

		// NOTE: URI needs to be stored in-class BEFORE PARSING to provide long-term access by the parsing library
		//  (otherwise the URI would be out of scope for the library after leaving this member function)
		this->link.swap(linkCopy);

		// parse relative link
		const char * errorPos{nullptr};
		const auto errorCode{
			uriParseSingleUriA(
					relativeSource.get(),
					this->link.c_str(),
					&errorPos
			)
		};

		if(errorCode != URI_SUCCESS) {
			const std::string rest(errorPos);

			std::string errorString{
					"Parsing::URI::parseLink():"
					" URI Parser error #"
			};

			errorString += std::to_string(errorCode);
			errorString += ": '";

			if(rest.size() < this->link.size()) {
				errorString	+= this->link.substr(0, this->link.size() - rest.size());
				errorString += "[!!!]";
				errorString += rest;
			}
			else {
				errorString += this->link;
				errorString += "[!!!]";
			}

			errorString += "'";

			throw URI::Exception(errorString);
		}

		// resolve reference
		if(
				uriAddBaseUriExA(
						this->uri.get(),
						relativeSource.getc(),
						this->base.getc(),
						URI_RESOLVE_IDENTICAL_SCHEME_COMPAT
				) != URI_SUCCESS
		) {
			throw URI::Exception(
					"Parsing::URI::parseLink():"
					" Reference resolving failed for '"
					+ URI::toString(relativeSource)
					+ "'"
			);
		}

		// normalize URI
		const auto dirtyParts{
			uriNormalizeSyntaxMaskRequiredA(
					this->uri.getc()
			)
		};

		if(
				dirtyParts != URI_NORMALIZED
				&& uriNormalizeSyntaxExA(
						this->uri.get(),
						dirtyParts
				) != URI_SUCCESS
		) {
			throw URI::Exception(
					"Parsing::URI::parseLink():"
					" Normalizing failed for '"
					+ URI::toString(this->uri)
					+ "'"
			);
		}

		return true;
	}

	//! Public static helper function URI-escaping a string.
	/*!
	 * \param string View of the string to
	 *   escape.
	 * \param plusSpace Specifies whether
	 *   spaces should be escaped as plusses.
	 *
	 * \returns A copy of the escaped string.
	 */
	inline std::string URI::escape(std::string_view string, bool plusSpace) {
		auto cString{
			//NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays, hicpp-avoid-c-arrays, modernize-avoid-c-arrays)
			std::make_unique<char[]>(
					string.size() * maxEscapedCharLength + 1
			)
		};

		uriEscapeExA(
				string.data(),
				string.data() + string.size(),
				cString.get(),
				static_cast<UriBool>(plusSpace),
				0
		);

		return std::string(cString.get());
	}

	//! Public static helper function URI-unescaping a string.
	/*!
	 * \param string View of the string to
	 *   unescape.
	 * \param plusSpace Specifies whether
	 *   plusses should be unescaped to
	 *   spaces.
	 *
	 * \returns A copy of the unescaped string.
	 */
	inline std::string URI::unescape(std::string_view string, bool plusSpace) {
		if(string.empty()) {
			return std::string();
		}

		auto cString{
			//NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays, hicpp-avoid-c-arrays, modernize-avoid-c-arrays)
			std::make_unique<char[]>(
					string.size() + 1
			)
		};

		for(std::size_t n{0}; n < string.size(); ++n) {
			cString[n] = string.at(n);
		}

		cString[string.size()] = '\0';

		uriUnescapeInPlaceExA(
				cString.get(),
				static_cast<UriBool>(plusSpace),
				URI_BR_DONT_TOUCH
		);

		return std::string(cString.get());
	}

	//! Public static helper function escaping a %URI, but leacing reserved characters intact.
	/*!
	 * The following characters will be
	 *  left intact: @c ; @c / @c ? @c :
	 *  @c @ @c = @c & @c # @c %
	 *
	 * \param uriToEscape View of the %URI
	 *   to be escaped.
	 *
	 * \returns A copy of the escaped %URI.
	 */
	inline std::string URI::escapeUri(std::string_view uriToEscape) {
		std::string result;
		std::size_t pos{0};

		while(pos < uriToEscape.size()) {
			auto end{uriToEscape.find_first_of(";/?:@=&#%", pos)};

			if(end == std::string::npos) {
				end = uriToEscape.size();
			}

			if(end - pos > 0) {
				const std::string part(uriToEscape, pos, end - pos);

				result += URI::escape(part, false);
			}

			if(end < uriToEscape.size()) {
				result += uriToEscape.at(end);
			}

			pos = end + 1;
		}

		// replace % with %25 if not followed by a two-digit hexadecimal number
		Helper::Strings::encodePercentage(result);

		return result;
	}

	//! Public static helper function making a set of (possibly) relative URIs absolute.
	/*!
	 * \note Errors for single URIs will
	 *   be ignored and those URIs will
	 *   be quietly removed.
	 *
	 * \param uriBase View of the base %URI.
	 *   Only its host name will be used.
	 * \param uris Reference to a vector
	 *   containing the (possibly) relative
	 *   URIs to be made absolute in-situ.
	 *
	 * \throws URI::Exception if the given
	 *   base %URI could not be parsed.
	 */
	inline void URI::makeAbsolute(std::string_view uriBase, std::vector<std::string>& uris) {
		// create base URI
		Wrapper::URI baseUri;

		baseUri.create();

		// parse base URI
		const char * errorPos{nullptr};
		const auto errorCode{
			uriParseSingleUriExA(
					baseUri.get(),
					uriBase.data(),
					uriBase.data() + uriBase.size(),
					&errorPos
			)
		};

		if(errorCode != URI_SUCCESS) {
			const std::string end(
					errorPos,
					uriBase.size() - (errorPos - uriBase.data())
			);

			std::string errorString{
				"Parsing::URI::makeAbsolute():"
				" URI Parser error #"
			};

			errorString += std::to_string(errorCode);
			errorString += ": '";

			if(end.size() < uriBase.size()) {
				errorString += uriBase.substr(0, uriBase.size() - end.size());
				errorString += "[!!!]";
				errorString += end;
			}
			else {
				errorString += uriBase;
				errorString += "[!!!]";
			}

			errorString += "'";

			throw URI::Exception(errorString);
		}

		// go through (possibly) relative URIs
		std::vector<std::string> result;

		for(const auto& relUri : uris) {
			// skip empty URIs
			if(relUri.empty()) {
				continue;
			}

			// create relative URI
			Wrapper::URI relativeSource;

			relativeSource.create();

			// create absolute URI
			Wrapper::URI absoluteDest;

			absoluteDest.create();

			// parse relative URI
			if(uriParseSingleUriA(
					relativeSource.get(),
					relUri.c_str(),
					nullptr
			) != URI_SUCCESS) {
				// ignore single URIs that cannot be parsed
				continue;
			}

			// resolve reference
			if(uriAddBaseUriExA(
					absoluteDest.get(),
					relativeSource.getc(),
					baseUri.getc(),
					URI_RESOLVE_IDENTICAL_SCHEME_COMPAT
			) != URI_SUCCESS) {
				// ignore single URIs that cannot be resolved
				continue;
			}

			// normalize absolute URI
			const auto dirtyParts{
				uriNormalizeSyntaxMaskRequiredA(
						absoluteDest.getc()
				)
			};

			if(
					dirtyParts != URI_NORMALIZED
					&& uriNormalizeSyntaxExA(
							absoluteDest.get(),
							dirtyParts
					) != URI_SUCCESS
			) {
				// ignore single URIs that cannot be normalized
				continue;
			}

			// add normalized absolute URI
			result.emplace_back(URI::toString(absoluteDest));
		}

		// swap (possibly) relative with absolute URIs
		result.swap(uris);
	}

	// private static helper function: convert URITextRangeA to std::string
	inline std::string URI::textRangeToString(const UriTextRangeA& range) {
		if(
				range.first == nullptr
				|| *(range.first) == 0
				|| range.afterLast == nullptr
				|| range.afterLast <= range.first
		) {
			return std::string();
		}

		return std::string(range.first, range.afterLast - range.first);
	}

	// private static helper function: convert URI to string
	inline std::string URI::toString(const Wrapper::URI& src) {
		if(!src.valid()) {
			return std::string();
		}

		int charsRequired{0};

		if(
				uriToStringCharsRequiredA(
						src.getc(),
						&charsRequired
				) != URI_SUCCESS
		) {
			throw URI::Exception(
					"Parsing::URI::toString():"
					" Could not convert URI to string,"
					" because uriToStringCharsRequiredA(...)"
					" failed"
			);
		}

		auto uriCString{
			//NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays, hicpp-avoid-c-arrays, modernize-avoid-c-arrays)
			std::make_unique<char[]>(
					charsRequired + 1
			)
		};

		if(
				uriToStringA(
						uriCString.get(),
						src.getc(),
						charsRequired + 1,
						nullptr
				) != URI_SUCCESS
		) {
			throw URI::Exception(
					"Parsing::URI::toString():"
					" Could not convert URI to string,"
					" because uriToStringA(...) failed"
			);
		}

		return std::string(uriCString.get());
	}

} /* namespace crawlservpp::Parsing */

#endif /* PARSING_URI_HPP_ */
