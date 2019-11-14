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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * RegEx.hpp
 *
 * Using the PCRE2 library to implement a Perl-Compatible Regular Expressions query with boolean, single and/or multiple results.
 *  Expression is only created when needed.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef QUERY_REGEX_HPP_
#define QUERY_REGEX_HPP_

#define PCRE2_ERROR_BUFFER_LENGTH 1024

#include "../Main/Exception.hpp"
#include "../Wrapper/PCRE.hpp"
#include "../Wrapper/PCREMatch.hpp"

#include <pcre2.h>

#include <string>	// std::string, std::to_string
#include <vector>	// std::vector

namespace crawlservpp::Query {

	class RegEx {
	public:
		// constructors and destructor
		RegEx(const std::string& expression, bool single, bool multi);
		virtual ~RegEx();

		// getters
		bool getBool(const std::string& text) const;
		void getFirst(const std::string& text, std::string& resultTo) const;
		void getAll(const std::string& text, std::vector<std::string>& resultTo) const;

		// operators
		explicit operator bool() const noexcept;
		bool operator!() const noexcept;

		// class for RegEx exceptions
		MAIN_EXCEPTION_CLASS();

		// only moveable (using default), not copyable
		RegEx(const RegEx&) = delete;
		RegEx(RegEx&& other) = default;
		RegEx& operator=(const RegEx&) = delete;
		RegEx& operator=(RegEx&&) = default;

	private:
		Wrapper::PCRE expressionSingle, expressionMulti;

#ifndef NDEBUG
		const std::string queryString;
#endif
	};

} /* crawlservpp::Query */

#endif /* QUERY_REGEX_HPP_ */
