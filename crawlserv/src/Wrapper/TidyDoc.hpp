/*
 * TidyDoc.hpp
 *
 * RAII wrapper for tidyhtml documents.
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_TIDYDOC_HPP_
#define WRAPPER_TIDYDOC_HPP_

#include "TidyBuffer.hpp"

#include "../Main/Exception.hpp"

#include <tidy.h>

#include <queue>
#include <string>

namespace crawlservpp::Wrapper {

	class TidyDoc {
	public:
		// constructor: create document, throws TidyDoc::Exception
		TidyDoc() : doc(tidyCreate()) {
			// set error buffer
			if(tidySetErrorBuffer(this->doc, this->errors.get()) != 0)
				throw Exception("Could not set error buffer");
		}

		// destructor: release document
		~TidyDoc() {
			tidyRelease(this->doc);
		}

		// get reference to document
		::TidyDoc& get() {
			return this->doc;
		}

		// get const reference to document
		const ::TidyDoc& get() const {
			return this->doc;
		}

		// set boolean option, throws TidyDoc::Exception
		void setOption(TidyOptionId option, bool value) {
			if(!tidyOptSetBool(this->doc, option, value ? yes : no))
				throw Exception("Could not set tidy option");
		}

		// set int option, throws TidyDoc::Exception
		void setOption(TidyOptionId option, int value) {
			if(!tidyOptSetInt(this->doc, option, value))
				throw Exception("Could not set tidy option");
		}

		// set unsigned int option, throws TidyDoc::Exception
		void setOption(TidyOptionId option, unsigned int value) {
			if(!tidyOptSetInt(this->doc, option, value))
				throw Exception("Could not set tidy option");
		}

		// set string option, throws TidyDoc::Exception
		void setOption(TidyOptionId option, const std::string& value) {
			if(!tidyOptSetValue(this->doc, option, value.c_str()))
				throw Exception("Could not set tidy option");
		}

		// set string option, throws TidyDoc::Exception
		void setOption(TidyOptionId option, const char * value) {
			if(!tidyOptSetValue(this->doc, option, value))
				throw Exception("Could not set tidy option");
		}

		// parse string, throws TidyDoc::Exception
		void parseString(const std::string& in, std::queue<std::string>& warningsTo) {
			switch(tidyParseString(this->doc, in.c_str())) { // TODO: MEMORY LEAK !!!
			case 0:
				// everything went fine
				break;

			case 1:
			case 2:
				// warnings or errors occured
				if(this->errors) {
					warningsTo.emplace(this->errors.getString());

					this->errors.clear();
				}

				break;

			default:
				// errors occured
				if(this->errors)
					throw Exception("Could not parse HTML: " + errors.getString());

				throw Exception("Could not parse HTML");
			}
		}

		// clean and repair parsed content, throws TidyDoc::Exception
		void cleanAndRepair(std::queue<std::string>& warningsTo) {
			switch(tidyCleanAndRepair(this->doc)) {
			case 0:
				// everything went fine
				break;

			case 1:
			case 2:
				// warnings or errors occured
				if(this->errors) {
					warningsTo.emplace(this->errors.getString());

					this->errors.clear();
				}

				break;

			default:
				// fatal errors occured
				if(this->errors)
					throw Exception("Could not clean and repair HTML: " + errors.getString());

				throw Exception("Could not clean and repair HTML");
			}
		}

		// get output string, throws TidyDoc::Exception
		std::string getOutput(std::queue<std::string>& warningsTo) {
			TidyBuffer buffer;

			switch(tidySaveBuffer(this->doc, buffer.get())) {
			case 0:
				// everything went fine
				break;

			case 1:
			case 2:
				// warnings or errors occured
				if(this->errors) {
					warningsTo.emplace(this->errors.getString());

					this->errors.clear();
				}

				break;

			default:
				// fatal errors occured
				if(this->errors)
					throw Exception("Could not write to buffer: " + errors.getString());

				throw Exception("Could not write to buffer");
			}

			return buffer.getString();
		}

		// sub-class for TidyDoc exceptions
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
			virtual ~Exception() {}
		};

		// not moveable, not copyable
		TidyDoc(TidyDoc&) = delete;
		TidyDoc(TidyDoc&&) = delete;
		TidyDoc& operator=(TidyDoc&) = delete;
		TidyDoc& operator=(TidyDoc&&) = delete;

	private:
		::TidyDoc doc;

		TidyBuffer errors;
	};

	} /* crawlservpp::Wrapper */

#endif /* WRAPPER_TIDYDOC_HPP_ */
