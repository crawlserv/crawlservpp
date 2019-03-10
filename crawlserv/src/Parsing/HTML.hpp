/*
 * HTML.h
 *
 * Parse HTML, tidy it up and convert it to XML using libtidy.
 *
 *  Created on: Feb 1, 2019
 *      Author: ans
 */

#ifndef PARSING_HTML_HPP_
#define PARSING_HTML_HPP_

#include "../Main/Exception.hpp"

#include "tidy.h"
#include "tidybuffio.h"

#include <string>

namespace crawlservpp::Parsing {

	class HTML final {
	public:
		HTML();
		virtual ~HTML();

		void tidyAndConvert(std::string& content);

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
		TidyBuffer buffer;
	};

} /* crawlservpp::Parsing */

#endif /* PARSING_HTML_HPP_ */
