/*
 *
 * ---
 *
 *  Copyright (C) 2021–2023 Anselm Schmidt (ans[ät]ohai.su)
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
 * Using the pugixml parser library to implement a XPath query with boolean,
 *  single and/or multiple results. A query is only created when needed.
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
#include <string_view>	// std::string_view
#include <vector>		// std::vector

namespace crawlservpp::Query {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! The beginning of a CDATA tag.
	inline constexpr std::string_view cDataHead{"<![CDATA["};

	//! The end of a CDATA tag.
	inline constexpr std::string_view cDataTail{"]]>"};

	///@}

	/*
	 * DECLARATION
	 */

	//! Implements a %XPath query using the pugixml library.
	/*!
	 * For more information about the pugixml library, see
	 * its <a href="https://github.com/zeux/pugixml">
	 * GitHub repository</a>
	 */
	class XPath {
	public:
		///@name Construction
		///@{

		XPath(const std::string& xpath, bool textOnly);

		///@}
		///@name Getters
		///@{

		[[nodiscard]] bool getBool(const Parsing::XML& doc) const;
		void getFirst(const Parsing::XML& doc, std::string& resultTo) const;
		void getAll(const Parsing::XML& doc, std::vector<std::string>& resultTo) const;
		void getSubSets(const Parsing::XML& doc, std::vector<Parsing::XML>& resultTo) const;

		///@}

		//! Class for XPath exceptions.
		/*!
		 * Throws an exception when
		 * - an error occurs during compilation
		 *    of the XPath expression.
		 * - no XPath expression has been compiled
		 *    prior to the execution of the query
		 * - the given %XML document has not been
		 *    parsed
		 * - an error occurs during execution
		 *    of the query.
		 */
		MAIN_EXCEPTION_CLASS();

	private:
		pugi::xpath_query query;
		bool compiled;
		bool isTextOnly;

		// static helper function
		[[nodiscard]] static std::string nodeToString(const pugi::xpath_node& node, bool textOnly);

		// sub-class for text-only conversion walker
		class TextOnlyWalker : public pugi::xml_tree_walker {
		public:
			bool for_each(pugi::xml_node& node) override;
			[[nodiscard]] std::string getResult() const;

		protected:
			std::string result;
		};
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Constructor setting a XPath string and whether the result should be text-only.
	/*!
	 * \param xpath Const reference to a string
	 *   containing the XPath expression.
	 * \param textOnly Set whether the query
	 *   should result in raw text only.
	 *
	 * \throws XPath::Exception if an error
	 *   occurs during the compilation of the
	 *   XPath expression.
	 */
	inline XPath::XPath(const std::string& xpath, bool textOnly) : compiled(false), isTextOnly(false) {
		// create new XPath object
		try {
			this->query = pugi::xpath_query(xpath.c_str());
			this->compiled = true;
		}
		catch(const pugi::xpath_exception& e) {
			throw XPath::Exception(e.what());
		}

		// save XPath option
		this->isTextOnly = textOnly;
	}

	//! Gets a boolean result from performing the query on a parsed %XML document.
	/*!
	 * \param doc Const reference to a %XML
	 *   document parsed by @c tidy-html5.
	 *
	 * \returns True, if there is at least one
	 *   match after performing the query on the
	 *   document. False otherwise.
	 *
	 * \throws XPath::Exception if no XPath
	 *   query has been compiled, no %XML
	 *   document has been parsed, or an error
	 *   occurs during the execution of the query.
	 */
	inline bool XPath::getBool(const Parsing::XML& doc) const {
		// check query and content
		if(!(this->compiled)) {
			throw XPath::Exception("No query compiled");
		}

		if(!(doc.doc)) {
			throw XPath::Exception("No content parsed");
		}

		// evaluate query with boolean result
		try {
			return this->query.evaluate_boolean(*(doc.doc));
		}
		catch(const std::exception& e) {
			throw XPath::Exception(e.what());
		}
	}

	//! Gets the first match from performing the query on a parsed JSON document.
	/*!
	 * \param doc Const reference to a %XML
	 *   document parsed by @c tidy-html5.
	 * \param resultTo Reference to a string
	 *   to which the result will be written.
	 *   The string will be cleared even if
	 *   an error occurs during execution of
	 *   the query.
	 *
	 * \throws XPath::Exception if no XPath
	 *   query has been compiled, no %XML
	 *   document has been parsed, or an error
	 *   occurs during the execution of the query.
	 */
	inline void XPath::getFirst(const Parsing::XML& doc, std::string& resultTo) const {
		// empty result
		resultTo.clear();

		// check query and content
		if(!(this->compiled)) {
			throw XPath::Exception("No query compiled");
		}

		if(!(doc.doc)) {
			throw XPath::Exception("No content parsed");
		}

		// evaluate query with string result
		try {
			if(this->query.return_type() == pugi::xpath_type_node_set) {
				const auto nodeSet{this->query.evaluate_node_set(*(doc.doc))};

				if(nodeSet.empty()) {
					resultTo = "";
				}
				else {
					resultTo = XPath::nodeToString(nodeSet[0], this->isTextOnly);
				}
			}
			else {
				setlocale(LC_ALL, "C"); // see https://github.com/crawlserv/crawlservpp/issues/164
				
				resultTo = this->query.evaluate_string(*(doc.doc));
			}
		}
		catch(const std::exception& e) {
			throw XPath::Exception(e.what());
		}
	}

	//! Gets all matches from performing the query on a parsed JSON document.
	/*!
	 * \param doc Const reference to a %XML
	 *   document parsed by @c tidy-html5.
	 * \param resultTo Reference to a vector
	 *   to which the results will be written.
	 *   The vector will be cleared even if
	 *   an error occurs during execution of
	 *   the query.
	 *
	 * \throws XPath::Exception if no XPath
	 *   query has been compiled, no %XML
	 *   document has been parsed, or an error
	 *   occurs during the execution of the query.
	 */
	inline void XPath::getAll(const Parsing::XML& doc, std::vector<std::string>& resultTo) const {
		// empty result
		resultTo.clear();

		// check query and content
		if(!(this->compiled)) {
			throw XPath::Exception("No query compiled");
		}

		if(!(doc.doc)) {
			throw XPath::Exception("No content parsed");
		}

		// evaluate query with multiple string results
		try {
			if(this->query.return_type() == pugi::xpath_type_node_set) {
				const auto nodeSet{this->query.evaluate_node_set(*(doc.doc))};

				resultTo.reserve(nodeSet.size());

				for(const auto& node : nodeSet) {
					const auto result{XPath::nodeToString(node, this->isTextOnly)};

					if(!result.empty()) {
						resultTo.emplace_back(result);
					}
				}
			}
			else {
				setlocale(LC_ALL, "C"); // see https://github.com/crawlserv/crawlservpp/issues/164
				
				const auto result{this->query.evaluate_string(*(doc.doc))};

				if(!result.empty()) {
					resultTo.emplace_back(result);
				}
			}
		}
		catch(const std::exception& e) {
			throw XPath::Exception(e.what());
		}
	}

	//! Gets all matching subsets from performing the query on a parsed JSON document.
	/*!
	 * The subsets will be saved as JSON documents
	 *  as defined by the jsoncons library.
	 *
	 * \param doc Const reference to a %XML
	 *   document parsed by @c tidy-html5.
	 * \param resultTo Reference to a vector
	 *   to which the results will be written.
	 *   The vector will be cleared even if
	 *   an error occurs during execution of
	 *   the query.
	 *
	 * \throws XPath::Exception if no XPath
	 *   query has been compiled, no %XML
	 *   document has been parsed, or an error
	 *   occurs during the execution of the query.
	 */
	inline void XPath::getSubSets(const Parsing::XML& doc, std::vector<Parsing::XML>& resultTo) const {
		// empty result
		resultTo.clear();

		// check query and content
		if(!(this->compiled)) {
			throw XPath::Exception("No query compiled");
		}

		if(!(doc.doc)) {
			throw XPath::Exception("No content parsed");
		}

		// evaluate query with multiple results
		try {
			if(this->query.return_type() == pugi::xpath_type_node_set) {
				const auto nodeSet{this->query.evaluate_node_set(*(doc.doc))};

				resultTo.reserve(nodeSet.size());

				for(const auto& node : nodeSet) {
					if(node != nullptr) {
						resultTo.emplace_back(node.node());
					}
				}
			}
			else {
				throw XPath::Exception(
						"Could not create subset, because the result of the query is no node set"
				);
			}
		}
		catch(const std::exception& e) {
			throw XPath::Exception(e.what());
		}
	}

	// static helper function: convert node to string
	inline std::string XPath::nodeToString(const pugi::xpath_node& node, bool textOnly) {
		std::string result;

		if(node.attribute() != nullptr) {
			result = node.attribute().as_string();
		}
		else if(node.node() != nullptr) {
			if(textOnly) {
				XPath::TextOnlyWalker walker;

				node.node().traverse(walker);

				result = walker.getResult();

				if(!result.empty()) {
					result.pop_back();
				}
			}
			else {
				for(const auto& child : node.node().children()) {
					std::ostringstream outStrStr;
					std::string out;

					child.print(outStrStr, "", 0);

					out = outStrStr.str();

					// parse CDATA
					if(
							out.length() > cDataHead.length() + cDataTail.length()
							&& out.substr(0, cDataHead.length()) == cDataHead
							&& out.substr(out.length() - cDataTail.length()) == cDataTail
					) {
						out = out.substr(
								cDataHead.length(),
								out.length() - cDataHead.length() - cDataTail.length()
						);
					}

					result += out;
				}
			}
		}

		return result;
	}

	// XML walker for text-only conversion helper functions
	inline bool XPath::TextOnlyWalker::for_each(pugi::xml_node& node) {
		if(node.type() == pugi::node_pcdata) {
			std::string nodeText(node.text().as_string());

			Helper::Strings::trim(nodeText);

			this->result += nodeText;

			this->result.push_back(' ');
		}

		return true;
	}

	// get result from XML walker
	inline std::string XPath::TextOnlyWalker::getResult() const {
		return this->result;
	}

} /* namespace crawlservpp::Query */

#endif /* QUERY_XPATH_HPP_ */
