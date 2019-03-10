/*
 * RegEx.h
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

#include <sstream>
#include <string>
#include <vector>

namespace crawlservpp::Query {

	class RegEx {
	public:
		// constructors
		RegEx(const std::string& expression, bool single, bool multi);
		RegEx(RegEx&& other) noexcept;

		// getters
		bool getBool(const std::string& text) const;
		void getFirst(const std::string& text, std::string& resultTo) const;
		void getAll(const std::string& text, std::vector<std::string>& resultTo) const;

		// operator
		operator bool() const noexcept;

		// sub-class for RegEx exceptions
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
			virtual ~Exception() {}
		};

	private:
		Wrapper::PCRE expressionSingle, expressionMulti;
	};

} /* crawlservpp::Query */

#endif /* QUERY_REGEX_HPP_ */
