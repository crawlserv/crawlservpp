/*
 * RegEx.h
 *
 * Using the PCRE2 library to implement a Perl-Compatible Regular Expressions query with boolean, single and/or multiple results.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef QUERY_REGEX_H_
#define QUERY_REGEX_H_

#define PCRE2_CODE_UNIT_WIDTH 8
#define PCRE2_ERROR_BUFFER_LENGTH 1024

#include "../Main/Exception.h"

#include <pcre2.h>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace crawlservpp::Query {
	class RegEx {
	public:
		RegEx();
		virtual ~RegEx();

		void compile(const std::string& expression, bool single, bool multi);
		bool getBool(const std::string& text) const;
		void getFirst(const std::string& text, std::string& resultTo) const;
		void getAll(const std::string& text, std::vector<std::string>& resultTo) const;

		// sub-class for RegEx exceptions
		class Exception : public crawlservpp::Main::Exception {
		public:
			Exception(const std::string& description) : crawlservpp::Main::Exception(description) {}
			virtual ~Exception() {}
		};

		// only moveable, not copyable
		RegEx(RegEx&) = delete;
		RegEx(RegEx&& other) {
			this->expressionSingle = std::move(other.expressionSingle);
			this->expressionMulti = std::move(other.expressionMulti);
		}
		RegEx& operator=(RegEx&) = delete;
		RegEx& operator=(RegEx&& other) {
			if(&other != this) {
				this->expressionSingle = std::move(other.expressionSingle);
				this->expressionMulti = std::move(other.expressionMulti);
			}
			return *this;
		}

	private:
		// RAII wrapper sub-class for PCRE pointers
		//  NOTE: Does NOT have ownership of the pointer!
		class PCREWrapper {
		public:
			// constructor: set pointer to NULL
			PCREWrapper() { this->ptr = NULL; }

			// destructor: free PCRE if necessary
			virtual ~PCREWrapper() { this->reset(); }

			// get pointer to PCRE
			const pcre2_code * get() const { return this->ptr; }
			pcre2_code * get() { return this->ptr; }
			operator bool() const { return this->ptr != NULL; }
			bool operator!() const { return this->ptr == NULL; }

			// reset functions
			void reset() { if(this->ptr) { pcre2_code_free(this->ptr); this->ptr = NULL; } }
			void reset(pcre2_code * other) { if(this->ptr) { pcre2_code_free(this->ptr); } this->ptr = other; }

			// only moveable, not copyable
			PCREWrapper(PCREWrapper&) = delete;
			PCREWrapper(PCREWrapper&& other) { this->ptr = other.ptr; other.ptr = NULL; }
			PCREWrapper& operator=(PCREWrapper&) = delete;
			PCREWrapper& operator=(PCREWrapper&& other) {
				if(&other != this) {
					this->ptr = other.ptr;
					other.ptr = NULL;
				}
				return *this;
			}

		private:
			pcre2_code * ptr;
		} expressionSingle, expressionMulti;

		// RAII wrapper sub-class for PCRE matches
		//  NOTE: Does NOT have ownership of the pointer!
		class PCREMatchWrapper {
		public:
			// constructor: set pointer to NULL
			PCREMatchWrapper(pcre2_match_data * setPtr) { this->ptr = setPtr; }

			// destructor: free PCRE match if necessary
			virtual ~PCREMatchWrapper() { if(this->ptr) { pcre2_match_data_free(this->ptr); } }

			// get pointer to PCRE match
			const pcre2_match_data * get() const { return this->ptr; }
			pcre2_match_data * get() { return this->ptr; }
			operator bool() const { return this->ptr != NULL; }
			bool operator!() const { return this->ptr == NULL; }

			// only moveable, not copyable
			PCREMatchWrapper(PCREMatchWrapper&) = delete;
			PCREMatchWrapper(PCREMatchWrapper&& other) { this->ptr = other.ptr; other.ptr = NULL; }
			PCREMatchWrapper& operator=(PCREMatchWrapper&) = delete;
			PCREMatchWrapper& operator=(PCREMatchWrapper&& other) {
				if(&other != this) {
					this->ptr = other.ptr;
					other.ptr = NULL;
				}
				return *this;
			}

		private:
			pcre2_match_data * ptr;
		};
	};
}

#endif /* QUERY_REGEX_H_ */
