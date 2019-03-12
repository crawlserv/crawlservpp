/*
 * PCRE.hpp
 *
 * Wrapper for pointer to Perl-Compatible Regular Expression.
 *  Does NOT have ownership of the pointer!
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_PCRE_HPP_
#define WRAPPER_PCRE_HPP_

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include <stddef.h>

namespace crawlservpp::Wrapper {

	class PCRE {
	public:
		// constructor: set pointer to nullptr
		PCRE() noexcept : ptr(nullptr) {}

		// move constructor
		PCRE(PCRE&& other) noexcept : ptr(other.ptr) {
			other.ptr = nullptr;
		}

		// destructor: free Perl-Compatible Regular Expression if necessary
		~PCRE() noexcept {
			this->reset();
		}

		// get pointer to Perl-Compatible Regular Expression
		pcre2_code * get() noexcept {
			return this->ptr;
		}

		// get const pointer to Perl-Compatible Regular Expression
		const pcre2_code * get() const noexcept {
			return this->ptr;
		}

		// reset Perl-Compatible Regular Expression
		void reset() {
			if(this->ptr) {
				pcre2_code_free(this->ptr);
				this->ptr = nullptr;
			}
		}

		// replace Perl-Compatible Regular Expression
		void reset(pcre2_code * other) {
			if(this->ptr) pcre2_code_free(this->ptr);
			this->ptr = other;
		}

		// bool operator
		operator bool() const noexcept {
			return this->ptr != nullptr;
		}

		// not operator
		bool operator!() const noexcept {
			return this->ptr == nullptr;
		}

		// move operator
		PCRE& operator=(PCRE&& other) noexcept {
			if(&other != this) {
				this->ptr = other.ptr;
				other.ptr = nullptr;
			}
			return *this;
		}

		// not copyable
		PCRE(PCRE&) = delete;
		PCRE& operator=(PCRE&) = delete;


	private:
		pcre2_code * ptr;
	};

} /* crawlservpp::Wrapper */

#endif /* WRAPPER_PCRE_HPP_ */
