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
 * HTML.hpp
 *
 * Parse HTML, tidy it up and convert it to %XML using tidy5-html.
 *
 *  Created on: Feb 1, 2019
 *      Author: ans
 */

#ifndef PARSING_HTML_HPP_
#define PARSING_HTML_HPP_

#include "../Main/Exception.hpp"
#include "../Wrapper/TidyDoc.hpp"

#include <queue>		// std::queue
#include <string>		// std::string
#include <string_view>	// std::string_view

//! Namespace for classes parsing %HTML, URIs, and %XML.
namespace crawlservpp::Parsing {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! The character encoding used by the tidy-html5 API.
	inline constexpr std::string_view tidyEncoding{"utf8"};

	///@}

	/*
	 * DECLARATION
	 */

	//! Parses and cleans %HTML markup.
	/*!
	 * Parses the provided %HTML markup, tidies it up and converts it
	 *  into %XML using the tidy5-html via Wrapper::TidyDoc.
	 *
	 * At the moment, this class is used exclusively by
	 *  Parsing::XML::parse().
	 *
	 * For more information about the tidy-html5 API, see its
	 *  <a href="https://github.com/htacg/tidy-html5">GitHub repository</a>.
	 */
	class HTML {
	private:
		// for convenience
		using TidyDoc = Wrapper::TidyDoc;

	public:
		///@name Construction and Destruction
		///@{

		//! Default constructor.
		HTML() = default;

		//! Default destructor.
		virtual ~HTML() = default;

		///@}
		///@name Functionality
		///@{

		void tidyAndConvert(
				std::string& inOut,
				bool warnings,
				ulong numOfErrors,
				std::queue<std::string>& warningsTo
		);

		///@}

		//! Class for %HTML exceptions.
		/*!
		 * This exception is being thrown when
		 * - a TidyDoc::Exception occurs while parsing,
		 *    tidying and converting %HTML markup to %XML
		 *
		 * \sa tidyAndConvert
		 */
		MAIN_EXCEPTION_CLASS();

		/**@name Copy and Move
		 * The class is not copyable and not moveable.
		 */
		///@{

		//! Deleted copy constructor.
		HTML(HTML&) = delete;

		//! Deleted move constructor.
		HTML(HTML&&) = delete;

		//! Deleted copy operator.
		HTML& operator=(HTML&) = delete;

		//! Deleted move operator.
		HTML& operator=(HTML&&) = delete;

		///@}

	private:
		TidyDoc doc;
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Parse and tidy the given %HTML markup and convert the result to %XML.
	/*!
	 * The markup will be parsed, cleaned and repaired by tidy-html5 with
	 *  the following options set:
	 *  - output-xml=yes
	 *  - quiet=yes
	 *  - numeric-entities=yes
	 *  - tidy-mark=no
	 *  - force-output=yes
	 *  - drop-empty-elements=no
	 *  - output-encoding=utf8 [default]
	 *
	 * Additionally, show-warnings and show-errors will be set according to the
	 *  arguments passed to the function.
	 *
	 * \note If the output returned from the underlying TidyDoc is empty,
	 *   the given markup will not be changed.
	 *
	 * \param inOut Reference to a string containing the %HTML markup to parse,
	 *   tidy up and convert. The string will be replaced with the resulting
	 *   %XML output, unless the result would be an empty string.
	 *
	 * \param warnings Specifies whether to add minor warnings to the given queue.
	 *
	 * \param numOfErrors Specifies the number used "to determine if further errors
	 *   should be added" to the queue. If set to zero, no errors will be added.
	 *
	 * \param warningsTo Reference to a queue of strings to which the reported
	 *   warnings and errors will be added.
	 *
	 * \throws HTML::Exception if a TidyDoc::Exception has been thrown.
	 *
	 * \sa
	 *   <a href="https://api.html-tidy.org/tidy/tidylib_api_5.6.0/tidy_quickref.html">Options Quick Reference</a>
	 */
	inline void HTML::tidyAndConvert(
			std::string& inOut,
			bool warnings,
			ulong numOfErrors,
			std::queue<std::string>& warningsTo
	) {
		// set options
		try {
			this->doc.setOption(TidyXmlOut, true);
			this->doc.setOption(TidyQuiet, true);
			this->doc.setOption(TidyNumEntities, true);
			this->doc.setOption(TidyMark, false);
			this->doc.setOption(TidyShowWarnings, warnings);
			this->doc.setOption(TidyForceOutput, true);
			this->doc.setOption(TidyDropEmptyElems, false);
			this->doc.setOption(TidyShowErrors, numOfErrors);
			this->doc.setOption(TidyOutCharEncoding, std::string(tidyEncoding));

			this->doc.parse(inOut, warningsTo);
			this->doc.cleanAndRepair(warningsTo);

			const auto output(this->doc.getOutput(warningsTo));

			if(output.empty()) {
				return;
			}

			inOut = output;
		}
		catch(const TidyDoc::Exception& e) {
			// re-throw as HTML::Exception
			throw HTML::Exception(e.what());
		}
	}

} /* namespace crawlservpp::Parsing */

#endif /* PARSING_HTML_HPP_ */
