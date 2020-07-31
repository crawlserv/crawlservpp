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
 * TidyDoc.hpp
 *
 * RAII wrapper for tidy-html5 documents.
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_TIDYDOC_HPP_
#define WRAPPER_TIDYDOC_HPP_

#include "TidyBuffer.hpp"

#include "../Helper/Strings.hpp"
#include "../Main/Exception.hpp"

#include <tidy.h>

#include <queue>	// std::queue
#include <string>	// std::string, std::to_string

namespace crawlservpp::Wrapper {

	/*
	 * DECLARATION
	 */

	//! RAII wrapper for documents used by the tidy-html5 API.
	/*!
	 * Creates a Tidy document on construction
	 *  and automatically releases it on
	 *  destruction, avoiding memory leaks.
	 *
	 * The class encapsulates functionality to
	 *  configure the API, to parse, clean and
	 *  repair markup and to retrieve a
	 *  stringified copy of the resulting tree
	 *  inside the underlying document.
	 *
	 * At the moment, this class is used
	 *  exclusively by
	 *  Parsing::HTML::tidyAndConvert().
	 *
	 * For more information about the tidy-html5
	 *  API, see its
	 *  <a href="https://github.com/htacg/tidy-html5">
	 *  GitHub repository</a>.
	 */
	class TidyDoc {
	public:
		///@name Construction and Destruction
		///@{
		TidyDoc();
		virtual ~TidyDoc();

		///@}
		///@name Getter
		///@{

		[[nodiscard]] std::string getOutput(std::queue<std::string>& warningsTo);

		///@}
		///@name Setters
		///@{

		void setOption(TidyOptionId option, bool value);
		void setOption(TidyOptionId option, int value);
		void setOption(TidyOptionId option, ulong value);
		void setOption(TidyOptionId option, const std::string& value);

		///@}
		///@name Parsing and Cleanup
		///@{

		void parse(const std::string& in, std::queue<std::string>& warningsTo);
		void cleanAndRepair(std::queue<std::string>& warningsTo);

		///@}

		//! Class for tidy-html5 document exceptions.
		/*!
		 * This exception is being thrown when
		 * - the error buffer could not be set on
		 *    construction
		 * - given input could not be parsed
		 *    successfully
		 * - given input could not be cleaned and
		 *    repaired
		 * - the output could not be written to a
		 *    TidyBuffer
		 * - any option could not be set by
		 *    setOption()
		 *
		 * \sa parse, cleanAndRepair, getOutput
		 */
		MAIN_EXCEPTION_CLASS();

		/**@name Copy and Move
		 * The class neither copyable, nor moveable.
		 */
		///@{

		//! Deleted copy constructor.
		TidyDoc(TidyDoc&) = delete;

		//! Deleted copy assignment operator.
		TidyDoc& operator=(TidyDoc&) = delete;

		//! Deleted move constructor.
		TidyDoc(TidyDoc&&) = delete;

		//! Deleted move assignment operator.
		TidyDoc& operator=(TidyDoc&&) = delete;

		///@}

	private:
		::TidyDoc doc;

		TidyBuffer errors;
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Constructor creating an empty tidy-html5 document.
	/*!
	 * Also sets the internal error buffer of
	 *  the newly created document.
	 *
	 * \throws TidyDoc::Exception if the error
	 *   buffer could not be set.
	 */
	inline TidyDoc::TidyDoc() : doc(tidyCreate()) {
		// set error buffer
		if(tidySetErrorBuffer(this->doc, this->errors.get()) != 0) {
			throw Exception("Could not set error buffer");
		}
	}

	//! Destructor releasing the underlying tidy-html5 document.
	inline TidyDoc::~TidyDoc() {
		tidyRelease(this->doc);
	}

	//! Gets the processed text from the tidy-html5 document.
	/*!
	 * If the buffer received from the underlying
	 *  document is invalid (or empty), an empty
	 *  string will be returned.
	 *
	 * An exception will only be thrown when a
	 *  fatal error occured.
	 *
	 * \note All warnings and errors will be
	 *   discarded once they have been received
	 *   by calling this function.
	 *
	 * \param warningsTo The reference to a queue
	 *   of strings into which to push errors and
	 *   warnings that occured while saving the
	 *   output.
	 *
	 * \returns A copy of the text inside the
	 *   underlying document.
	 *
	 * \throws TidyDoc::Exception if writing to
	 *   the output buffer failed.
	 *
	 * \sa parse, cleanAndRepair
	 */
	inline std::string TidyDoc::getOutput(std::queue<std::string>& warningsTo) {
		TidyBuffer buffer;

		switch(tidySaveBuffer(this->doc, buffer.get())) {
		case 0:
			// everything went fine
			break;

		case 1:
		case 2:
			// warnings or errors occured
			if(this->errors.valid()) {
				std::queue<std::string> warnings(
						Helper::Strings::splitToQueue(
								this->errors.getString(),
								'\n',
								true
						)
				);

				while(!warnings.empty()) {
					warningsTo.emplace(warnings.front());

					warnings.pop();
				}

				this->errors.clear();
			}

			break;

		default:
			// fatal errors occured
			if(this->errors.valid() && !(this->errors.empty())) {
				throw Exception("Could not write to buffer: " + errors.getString());
			}

			throw Exception("Could not write to buffer");
		}

		if(buffer.valid() && !buffer.empty()) {
			return buffer.getString();
		}

		return std::string();
	}

	//! Sets a boolean option.
	/*!
	 * Once successfully set, it will take effect
	 *  for all subsequent parsing, cleaning and
	 *  repairing of any given input.
	 *
	 * \warning An invalid option or the wrong
	 *  type will lead to a failing assertion
	 *  inside the API, which cannot be catched
	 *  via exception.
	 *
	 * \param option The ID of the option to be
	 *   set.
	 *
	 * \param value The value which the option
	 *   will be set to.
	 *
	 * \throws TidyDoc::Exception if the option
	 *   could not be set.
	 *
	 * \sa parse, cleanAndRepair,
	 *   <a href="https://api.html-tidy.org/tidy/tidylib_api_5.6.0/tidy_quickref.html">
	 *   Options Quick Reference</a>
	 */
	inline void TidyDoc::setOption(TidyOptionId option, bool value) {
		if(tidyOptSetBool(this->doc, option, value ? yes : no) == 0) {
			throw Exception(
					"Could not set tidy option #"
					+ std::to_string(option)
					+ " to boolean "
					+ (value ? "yes" : "no")
			);
		}
	}

	//! Sets an integer option.
	/*!
	 * Once successfully set, it will take effect
	 *  for all subsequent parsing, cleaning and
	 *  repairing of any given input.
	 *
	 * \warning An invalid option or the wrong type
	 *   will lead to a failing assertion inside the
	 *   API, which cannot be catched via exception.
	 *
	 * \param option The ID of the option to be set.
	 *
	 * \param value The value which the option will
	 *   be set to.
	 *
	 * \throws TidyDoc::Exception if the option could
	 *   not be set.
	 *
	 * \sa parse, cleanAndRepair,
	 *   <a href="https://api.html-tidy.org/tidy/tidylib_api_5.6.0/tidy_quickref.html">
	 *   Options Quick Reference</a>
	 */
	inline void TidyDoc::setOption(TidyOptionId option, int value) {
		if(tidyOptSetInt(this->doc, option, value) == 0) {
			throw Exception(
					"Could not set tidy option #"
					+ std::to_string(option)
					+ " to integer "
					+ std::to_string(value)
			);
		}
	}

	//! Sets a ulong option.
	/*!
	 * Once successfully set, it will take effect
	 *  for all subsequent parsing, cleaning and
	 *  repairing of any given input.
	 *
	 * \warning An invalid option or the wrong type
	 *  will lead to a failing assertion inside the
	 *  API, which cannot be catched via exception.
	 *
	 * \param option The ID of the option to be set.
	 *
	 * \param value The value which the option will
	 *   be set to.
	 *
	 * \throws TidyDoc::Exception if the option could
	 *   not be set.
	 *
	 * \sa parse, cleanAndRepair,
	 *   <a href="https://api.html-tidy.org/tidy/tidylib_api_5.6.0/tidy_quickref.html">
	 *   Options Quick Reference</a>
	 */
	inline void TidyDoc::setOption(TidyOptionId option, ulong value) {
		if(tidyOptSetInt(this->doc, option, value) == 0) {
			throw Exception(
					"Could not set tidy option #"
					+ std::to_string(option)
					+ " to unsigned integer "
					+ std::to_string(value)
			);
		}
	}

	//! Sets a string option.
	/*!
	 * Once successfully set, it will take effect
	 *  for all subsequent parsing, cleaning and
	 *  repairing of any given input.
	 *
	 * \warning An invalid option or the wrong type
	 *  will lead to a failing assertion inside the
	 *  API, which cannot be catched via exception.
	 *
	 * \param option The ID of the option to be set.
	 *
	 * \param value The value which the option will
	 *   be set to.
	 *
	 * \throws TidyDoc::Exception if the option could
	 *   not be set.
	 *
	 * \sa parse, cleanAndRepair,
	 *   <a href="https://api.html-tidy.org/tidy/tidylib_api_5.6.0/tidy_quickref.html">
	 *   Options Quick Reference</a>
	 */
	inline void TidyDoc::setOption(TidyOptionId option, const std::string& value) {
		if(tidyOptSetValue(this->doc, option, value.c_str()) == 0) {
			throw Exception(
					"Could not set tidy option #"
					+ std::to_string(option)
					+ " to string \""
					+ value
					+ "\""
			);
		}
	}

	//! Parses the given markup.
	/*!
	 * The given markup will be parsed according to
	 *  the options that have previously been set by
	 *  calls to the different setOption() functions.
	 *
	 * The underlying API will correct syntax errors
	 *  while parsing.
	 *
	 * The result will be stored in the underlying
	 *  document for possible further processing and
	 *  can be accessed via the getOutput() function.
	 *  Any previous output will be overwritten.
	 *
	 * An exception will only be thrown when a fatal
	 *  error occured.
	 *
	 * \note A string view cannot be used, because the
	 *  underlying API required a null-terminated string.
	 *
	 * \param in A const reference to the string
	 *   containing the markup to be parsed.
	 *
	 * \param warningsTo The reference to a queue of
	 *   strings into which to push errors and warnings
	 *   that occured while parsing the given input.
	 *
	 * \throws TidyDoc::Exception if the given markup
	 *   could not be parsed.
	 */
	inline void TidyDoc::parse(const std::string& in, std::queue<std::string>& warningsTo) {
		switch(tidyParseString(this->doc, in.c_str())) {
		case 0:
			// everything went fine
			break;

		case 1:
		case 2:
			// warnings or errors occured
			if(this->errors.valid()) {
				std::queue<std::string> warnings(
						Helper::Strings::splitToQueue(
								this->errors.getString(),
								'\n',
								true
						)
				);

				while(!warnings.empty()) {
					warningsTo.emplace(warnings.front());

					warnings.pop();
				}

				this->errors.clear();
			}

			break;

		default:
			// errors occured
			if(this->errors.valid() && !(this->errors.empty())) {
				throw Exception("Could not parse HTML: " + errors.getString());
			}

			throw Exception("Could not parse HTML");
		}
	}

	//! Cleans and repairs the previously parsed content of the underlying tidy-html5 document.
	/*!
	 * The parsed content will be cleaned and repaired
	 *  according to the options that have previously been
	 *  set by calls to the different setOption()
	 *  functions.
	 *
	 * The result will be stored in the underlying document
	 *  and can be accessed via the getOutput() function.
	 *  The raw output from the parsing process will be
	 *  overwritten.
	 *
	 * An exception will only be thrown when a fatal error
	 *  occured.
	 *
	 * \note If no input has been parsed, the function will
	 *   have no effect.
	 *
	 * \param warningsTo The reference to a queue of strings
	 *   into which to push errors and warnings that occured
	 *   while cleaning and repairing.
	 *
	 * \throws TidyDoc::Exception if the parsed markup could
	 *   not be cleaned and repaired.
	 */
	inline void TidyDoc::cleanAndRepair(std::queue<std::string>& warningsTo) {
		switch(tidyCleanAndRepair(this->doc)) {
		case 0:
			// everything went fine
			break;

		case 1:
		case 2:
			// warnings or errors occured
			if(this->errors.valid()) {
				std::queue<std::string> warnings(
						Helper::Strings::splitToQueue(
								this->errors.getString(),
								'\n',
								true
						)
				);

				while(!warnings.empty()) {
					warningsTo.emplace(warnings.front());

					warnings.pop();
				}

				this->errors.clear();
			}

			break;

		default:
			// fatal errors occured
			if(this->errors.valid() && !(this->errors.empty())) {
				throw Exception("Could not clean and repair HTML: " + errors.getString());
			}

			throw Exception("Could not clean and repair HTML");
		}
	}

} /* namespace crawlservpp::Wrapper */

#endif /* WRAPPER_TIDYDOC_HPP_ */
