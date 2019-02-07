/*
 * RegEx.h
 *
 * Using the PCRE2 library to implement a Perl-Compatible Regular Expressions query with boolean, single and/or multiple results.
 *  Expression is only created when needed.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef QUERY_REGEX_H_
#define QUERY_REGEX_H_

#define PCRE2_ERROR_BUFFER_LENGTH 1024

#include "../Main/Exception.h"
#include "../Wrapper/PCRE.h"
#include "../Wrapper/PCREMatch.h"

#include <pcre2.h>

#include <sstream>
#include <string>
#include <vector>

namespace crawlservpp::Query {
	class RegEx {
	public:
		// constructors
		RegEx(const std::string& expression, bool single, bool multi);
		RegEx(RegEx&& other);

		// destructor
		virtual ~RegEx();

		// getters
		bool getBool(const std::string& text) const;
		void getFirst(const std::string& text, std::string& resultTo) const;
		void getAll(const std::string& text, std::vector<std::string>& resultTo) const;

		// operators
		operator bool() const;
		RegEx& operator=(RegEx&& other);

		// not copyable
		RegEx(RegEx&) = delete;
		RegEx& operator=(RegEx&) = delete;

		// sub-class for RegEx exceptions
		class Exception : public crawlservpp::Main::Exception {
		public:
			Exception(const std::string& description) : crawlservpp::Main::Exception(description) {}
			virtual ~Exception() {}
		};

	private:
		crawlservpp::Wrapper::PCRE expressionSingle, expressionMulti;
	};
}

#endif /* QUERY_REGEX_H_ */
