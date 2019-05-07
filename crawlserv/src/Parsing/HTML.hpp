/*
 * HTML.hpp
 *
 * Parse HTML, tidy it up and convert it to XML using libtidy.
 *
 *  Created on: Feb 1, 2019
 *      Author: ans
 */

#ifndef PARSING_HTML_HPP_
#define PARSING_HTML_HPP_

#include "../Main/Exception.hpp"
#include "../Wrapper/TidyDoc.hpp"

#include <queue>
#include <string>

namespace crawlservpp::Parsing {

	class HTML final {
	private:
		// for convenience
		typedef Wrapper::TidyDoc TidyDoc;

	public:
		// constructor and destructor
		HTML() {}
		virtual ~HTML() {}

		// member function
		void tidyAndConvert(std::string& inOut, std::queue<std::string>& warningsTo);

		// sub-class for HTML exceptions
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
			virtual ~Exception() {}
		};

		// not moveable, not copyable
		HTML(HTML&) = delete;
		HTML(HTML&&) = delete;
		HTML& operator=(HTML&) = delete;
		HTML& operator=(HTML&&) = delete;

	private:
		TidyDoc doc;
	};

} /* crawlservpp::Parsing */

#endif /* PARSING_HTML_HPP_ */
