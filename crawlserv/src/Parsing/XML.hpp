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
 * XML.hpp
 *
 * Parse HTML markup into clean XML.
 *
 *  Created on: Oct 18, 2018
 *      Author: ans
 */

#ifndef PARSING_XML_HPP_
#define PARSING_XML_HPP_

#define PARSING_XML_

#include "HTML.hpp"

#include "../Helper/Memory.hpp"
#include "../Main/Exception.hpp"

#include <pugixml.hpp>

#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint32_t
#include <memory>		// std::make_unique, std::unique_ptr
#include <queue>		// std::queue
#include <sstream>		// std::ostringstream
#include <string>		// std::string, std::to_string
#include <string_view>	// std::string_view, , std::string_view_literals

namespace crawlservpp::Query {

	class XPath;

} /* namespace crawlservpp::Query */

namespace crawlservpp::Parsing {

	/*
	 * CONSTANTS
	 */

	using std::string_view_literals::operator""sv;

	///@name Constants
	///@{

	//! The beginning of XML markup
	inline constexpr auto xmlBegin{"<?xml "sv};

	//! Array containing additional XML markup tags to be removed.
	inline constexpr std::array xmlTags{"<?i>"sv};

	//! The beginning of a @c CDATA element.
	inline constexpr auto cDataBegin{"<![CDATA["sv};

	//! The end of a @c CDATA element.
	inline constexpr auto cDataEnd{"]]>"sv};

	//! The beginning of a conditional comment
	inline constexpr auto conditionalBegin{"<![if "sv};

	//! The end of a conditional comment
	inline constexpr auto conditionalEnd{"<![endif]>"sv};

	//! Characters to be inserted/replaced to make conditional comments valid
	inline constexpr auto conditionalInsert{"--"sv};

	//! Offset at which to insert at the beginning to make conditional comments valid
	inline constexpr auto conditionalInsertOffsetBegin{2};

	//! Offset at which to insert at the end to make conditional comments valid
	inline constexpr auto conditionalInsertOffsetEnd{9};

	//! Offset at which to insert into stray end tag left from conditional comment
	inline constexpr auto conditionalInsertOffsetStrayEnd{2};

	//! Characters to be replaced inside comments
	inline constexpr auto commentCharsToReplace{"--"sv};

	//! Characters used as replacement inside comments
	inline constexpr auto commentCharsReplaceBy{"=="sv};

	//! The beginning of an invalid comment
	inline constexpr auto invalidBegin{"<? "sv};

	//! The end of an invalid comment
	inline constexpr auto invalidEnd{" ?>"sv};

	//! Characters to be inserted at the beginning to make invalid comments valid
	inline constexpr auto invalidInsertBegin{"!--"sv};

	//! Characters to be inserted at the end to make invalid comments valid
	inline constexpr auto invalidInsertEnd{"--"sv};

	//! Offset at which to insert at the beginning to make invalid comments valid
	inline constexpr auto invalidInsertOffsetBegin{1};

	//! Offset at which to insert at the end to make invalid comments valid
	inline constexpr auto invalidInsertOffsetEnd{2};

	//! The maximum number of characters to be shown in error messages.
	inline constexpr auto numDebugCharacters{50};

	//! The beginning of a XML processing instruction.
	inline constexpr auto xmlInstructionBegin{"<?xml:"sv};

	//! The end of a XML processing instruction.
	inline constexpr auto xmlInstructionEnd{">"sv};

	///@}

	/*
	 * DECLARATION
	 */

	//! Parses %HTML markup into clean %XML.
	/*!
	 * Uses the @c tidy-html5 via Parsing::HTML
	 *  and the @c pugixml library to parse,
	 *  tidy up and clean the given %HTML markup
	 *  and to convert it into clean %XML markup.
	 *
	 * For more information about pugixml, see its
	 *  <a href="https://github.com/zeux/pugixml">
	 *  GitHub repository</a>.
	 *
	 */
	class XML {
		friend class Query::XPath;

	public:
		///@name Construction and Destruction
		///@{

		//! Default constructor.
		XML() = default;

		explicit XML(const pugi::xml_node& node);
		virtual ~XML();

		///@}
		///@name Getters
		///@{

		[[nodiscard]] bool valid() const;
		void getContent(std::string& resultTo) const;

		///@}
		///@name Setter
		///@{

		void setOptions(bool showWarnings, std::uint32_t numOfErrors) noexcept;

		///@}
		///@name Parsing
		///@{

		void parse(
				std::string_view content,
				bool repairCData,
				bool repairComments,
				bool removeXmlInstructions,
				std::queue<std::string>& warningsTo
		);

		///@}
		///@name Cleanup
		///@{

		void clear();

		///@}

		//! Class for %XML exceptions.
		/*!
		 * This exception is being thrown when
		 * - a HTML::Exception occured while parsing
		 *    %HTML markup
		 * - an error occured while parsing the %XML
		 *    markup returned by Parsing::HTML
		 * - content has been requested but none has
		 *    been generated
		 *
		 * \sa parse, getContent
		 */
		MAIN_EXCEPTION_CLASS();

		/**@name Copy and Move
		 * The class is not copyable, only moveable.
		 */
		///@{

		//! Deleted copy constructor.
		XML(const XML&) = delete;

		//! Default move constructor.
		XML(XML&&) noexcept = default;

		//! Deleted copy assignment operator.
		XML& operator=(const XML&) = delete;

		//! Default move assignment operator.
		XML& operator=(XML&&) noexcept = default;

		///@}

	private:
		// unique pointer to (pugi)XML document
		std::unique_ptr<pugi::xml_document> doc;

		// options
		bool warnings{false};
		std::uint32_t errors{};

		// internal static helper functions
		static void cDataRepair(std::string& content);
		static void replaceInvalidConditionalComments(std::string& content);
		static void replaceInvalidComments(std::string& content);
		static void removeXmlProcessingInstructions(std::string& content);
		static void checkResult(pugi::xml_parse_result result, const std::string& content);
	};

	/*
	 * IMPLEMENTATION
	 */

	/*
	 * CONSTRUCTION AND DESTRUCTION
	 */

	//! Constructor creating a new %XML document from an existing %XML node.
	/*!
	 * \param node The node which should be added as
	 *   root node to the new %XML document.
	 */
	inline XML::XML(const pugi::xml_node& node) {
		// create XML document
		this->doc = std::make_unique<pugi::xml_document>();

		this->doc->append_copy(node);
	}

	//! Destructor clearing the underlying %XML document.
	inline XML::~XML() {
		this->clear();
	}

	/*
	 * GETTERS
	 */

	//! Returns whether the underlying document is valid.
	/*!
	 * \returns True, if the underlying document is
	 *   valid, i.e. %XML content has been
	 *   sucessfully parsed. False otherwise.
	 */
	inline bool XML::valid() const {
		return this->doc.operator bool();
	}

	//! Gets the stringified content inside the underlying document.
	/*!
	 * The result will be intended with tabs (\\t).
	 *
	 * The output string will be overwritten, if no
	 *  exception is thrown.
	 *
	 * \warning Should only be called if %XML markup
	 *   has been successfully parsed.
	 *
	 * \param resultTo A reference to the string that
	 *   will be replaced with the content from the
	 *   underlying document.
	 *
	 * \throws XML::Exception if no content is
	 *   available.
	 */
	inline void XML::getContent(std::string& resultTo) const {
		if(!(this->doc)) {
			throw XML::Exception("No content has been parsed.");
		}

		std::ostringstream out;

		Helper::Memory::freeIf(!resultTo.empty(), resultTo);

		this->doc->print(out);

		resultTo += out.str();
	}

	/*
	 * SETTER
	 */

	//! Sets logging options.
	/*!
	 * Forwards the given values to the underlying Parsing::HTML document.
	 *
	 * \param showWarnings Specify whether to report
	 *   simple warnings. The default is false.
	 *
	 * \param numOfErrors Set the number of errors
	 *   to be reported. The default is zero.
	 *
	 * \sa Parsing::HTML::tidyAndConvert
	 */
	inline void XML::setOptions(bool showWarnings, std::uint32_t numOfErrors) noexcept {
		this->warnings = showWarnings;
		this->errors = numOfErrors;
	}

	/*
	 * PARSING
	 */

	//! Parses the given %HTML markup into the underlying %XML document.
	/*!
	 * A copy of the given markup will be created
	 *  and ASCII whitespaces at the beginning of
	 *  the input will be removed.
	 *
	 * \param content A view into the %HTML markup
	 *   to be parsed.
	 * \param repairCData Specifies whether the
	 *   class should try to repair broken @c CDATA
	 *   elements in the input.
	 * \param repairComments Specifies whether the
	 *   class should try to replace broken comments
	 *   in the input.
	 * \param removeXmlInstructions Specifies whether
	 *   the class should remove XML processing
	 *   instructions before parsing HTML content.
	 * \param warningsTo A reference to a queue of
	 *   strings to which warnings and errors will
	 *   be added according to the specified options.
	 *
	 * \throws XML::Exception if a HTML::Exception
	 *   has been thrown.
	 *
	 *  \sa setOptions, getContent
	 */
	inline void XML::parse(
			std::string_view content,
			bool repairCData,
			bool repairComments,
			bool removeXmlInstructions,
			std::queue<std::string>& warningsTo
	) {
		// remove whitespaces at the beginning and null characters
		std::size_t begin{};

		while(content.length() > begin && std::isspace(content.at(begin)) != 0) {
			++begin;
		}

		std::string xml;

		xml.reserve(content.size() - begin);

		for(std::size_t i{begin}; i < content.length(); ++i) {
			if(content[i] != '\0') {
				xml.push_back(content[i]);
			}
		}

		// if necessary, try to tidy HTML and convert it to XML
		if(
				xml.size() < xmlBegin.length()
				|| xml.substr(0, xmlBegin.length()) != xmlBegin
		) {
			// remove XML processing instructions
			if(removeXmlInstructions) {
				removeXmlProcessingInstructions(xml);
			}

			HTML tidy;

			try {
				tidy.tidyAndConvert(xml, this->warnings, this->errors, warningsTo);
			}
			catch(const HTML::Exception& e) {
				throw XML::Exception(
						"tidy-html5 error: "
						+ std::string(e.view())
				);
			}
		}

		// try to repair CDATA
		if(repairCData) {
			cDataRepair(xml);
		}

		// replace invalid comments
		if(repairComments) {
			replaceInvalidConditionalComments(xml);
			replaceInvalidComments(xml);
		}

		// create XML document
		this->doc = std::make_unique<pugi::xml_document>();

		// parse XHTML with pugixml		
		XML::checkResult(this->doc->load_buffer(xml.c_str(), xml.size(), pugi::parse_full), xml);
	}

	/*
	 * CLEANUP
	 */

	//! Clears the content of the underlying %XML document.
	/*!
	 * Does not have any effect if no content has
	 *  been parsed.
	 */
	inline void XML::clear() {
		if(this->doc) {
			this->doc.reset();
		}
	}

	/*
	 * INTERNAL STATIC HELPER FUNCTIONS (private)
	 */

	// internal static helper function: try to fix CDATA error (invalid ']]>' inside CDATA tag)
	inline void XML::cDataRepair(std::string& content) {
		auto pos{content.find(cDataBegin)};

		if(pos == std::string::npos) {
			return;
		}

		pos += cDataBegin.length();

		while(pos < content.size()) {
			const auto next{content.find(cDataBegin, pos)};

			if(next == std::string::npos) {
				break;
			}

			auto last{content.rfind(cDataEnd, next - cDataEnd.length())};

			if(last != std::string::npos && last > pos) {
				while(true) {
					pos = content.find(cDataEnd, pos);

					if(pos < last) {
						content.insert(pos + cDataEnd.length() - 1, 1, ' ');

						pos += cDataEnd.length() + 1;
					}
					else {
						break;
					}
				}
			}

			pos = next + cDataBegin.length();
		}
	}

	// internal static helper function: replace invalid conditional comments (e.g. created by MS Excel)
	inline void XML::replaceInvalidConditionalComments(std::string& content) {
		std::size_t pos{};

		while(pos < content.length()) {
			// find next invalid conditional comment
			pos = content.find(conditionalBegin, pos);

			if(pos == std::string::npos) {
				break;
			}

			// find end of invalid conditional comment
			const auto end{
				content.find(
						conditionalEnd,
						pos + conditionalBegin.length()
				)
			};

			if(end == std::string::npos) {
				break;
			}

			// insert commenting to make conditional comment valid (X)HTML
			content.insert(pos + conditionalInsertOffsetBegin, conditionalInsert);
			content.insert(
					// (consider that "--" has already been added)
					end + conditionalInsertOffsetEnd + conditionalInsert.length(),
					conditionalInsert
			);

			// replace "--" inside new comment with "=="
			auto subPos{pos + conditionalBegin.length() + conditionalInsert.length()};

			while(subPos < end) {
				subPos = content.find(commentCharsToReplace, subPos);

				if(subPos > end) {
					break;
				}

				content.replace(subPos, commentCharsToReplace.length(), commentCharsReplaceBy);

				subPos += commentCharsReplaceBy.length();
			}

			// jump to the end of the changed conditional comment
			pos = end + conditionalEnd.length() + 2 * conditionalInsert.length();
		}

		// replace remaining invalid end tags
		pos = 0;

		while(pos < content.length()) {
			pos = content.find(conditionalEnd, pos);

			if(pos == std::string::npos) {
				break;
			}

			content.insert(pos + conditionalInsertOffsetStrayEnd, conditionalInsert);
			content.insert(
					pos + conditionalInsertOffsetEnd + conditionalInsert.length(),
					conditionalInsert
			);

			pos += conditionalEnd.length() + 2 * conditionalInsert.length();
		}
	}

	// internal static helper function: replace invalid comments (<? ... ?>)
	inline void XML::replaceInvalidComments(std::string& content) {
		std::size_t pos{};

		while(pos < content.length()) {
			// find next invalid comment
			pos = content.find(invalidBegin, pos);

			if(pos == std::string::npos) {
				break;
			}

			// find end of invalid comment
			const auto end{content.find(invalidEnd, pos + invalidBegin.length())};

			if(end == std::string::npos) {
				break;
			}

			// insert commenting to make comment valid (X)HTML
			content.insert(pos + invalidInsertOffsetBegin, invalidInsertBegin);
			content.insert(
					// consider that "!--" has already been added
					end + invalidInsertOffsetBegin + invalidInsertBegin.length(),
					invalidInsertEnd
			);

			// replace "--" inside new comment with "=="
			auto subPos{pos + invalidBegin.length() + invalidInsertBegin.length()};

			while(subPos < end) {
				subPos = content.find(commentCharsToReplace, subPos);

				if(subPos > end) {
					break;
				}

				content.replace(
						subPos,
						commentCharsToReplace.length(),
						commentCharsReplaceBy
				);

				subPos += commentCharsToReplace.length();
			}

			// jump to the end of the changed comment
			pos = end
					+ invalidEnd.length()
					+ invalidInsertBegin.length()
					+ invalidInsertEnd.length();
		}
	}

	// internal static helper function: remove XML processing instructions (<?xml:...>)
	inline void XML::removeXmlProcessingInstructions(std::string& content) {
		std::size_t pos{};

		while(pos < content.length()) {
			pos = content.find(xmlInstructionBegin, pos);

			if(pos == std::string::npos) {
				break;
			}

			const auto end{
				content.find(
						xmlInstructionEnd,
						pos + xmlInstructionBegin.length()
				)
			};

			if(end == std::string::npos) {
				return;
			}

			content.erase(pos, end - pos + xmlInstructionEnd.length());
		}

		for(const auto& tag : xmlTags) {
			pos = 0;

			while(pos < content.length()) {
				pos = content.find(tag, pos);

				if(pos == std::string::npos) {
					break;
				}

				content.erase(pos, tag.length());
			}
		}
	}
	
	// internal static helper function: check parsing result
	inline void XML::checkResult(pugi::xml_parse_result result, const std::string& content) {
		if(result) {
			return;
		}
		
		// parsing error
		std::string errorString{"XML parsing error: "};

		errorString += result.description();
		errorString += " at #";
		errorString += std::to_string(result.offset);
		errorString += " (";

		if(result.offset > 0) {
			errorString += "'[...]";

			if(result.offset > numDebugCharacters) {
				errorString += content.substr(
						result.offset - numDebugCharacters,
						numDebugCharacters
				);
			}
			else {
				errorString += content.substr(0, result.offset);
			}

			errorString += "[!!!]";

			if(content.size() > static_cast<std::size_t>(result.offset + numDebugCharacters)) {
				errorString += "'[...]";
				errorString += content.substr(result.offset, numDebugCharacters);
				errorString += "[...]";
			}
			else {
				errorString += "'[...]";
				errorString += content.substr(result.offset);
			}

			errorString += "').";
		}

		throw XML::Exception(errorString);
	}

} /* namespace crawlservpp::Parsing */

#endif /* PARSING_XML_HPP_ */
