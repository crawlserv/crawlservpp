/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
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

#include <queue>	// std::queue
#include <string>	// std::string

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
		void tidyAndConvert(
				std::string& inOut,
				bool warnings,
				unsigned int numOfErrors,
				std::queue<std::string>& warningsTo
		);

		// sub-class for HTML exceptions
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
			virtual ~Exception() {}
		};

		// only moveable (using default), not copyable
		HTML(HTML&) = delete;
		HTML(HTML&&) = default;
		HTML& operator=(HTML&) = delete;
		HTML& operator=(HTML&&) = default;

	private:
		TidyDoc doc;
	};

} /* crawlservpp::Parsing */

#endif /* PARSING_HTML_HPP_ */
