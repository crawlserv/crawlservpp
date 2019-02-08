/*
 * PCRE.cpp
 *
 * Wrapper for pointer to Perl-Compatible Regular Expression.
 *  Does NOT have ownership of the pointer!
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#include "PCRE.h"

namespace crawlservpp::Wrapper {

// constructor: set pointer to NULL
PCRE::PCRE() noexcept {
	this->ptr = NULL;
}

// move constructor
PCRE::PCRE(PCRE&& other) noexcept {
	this->ptr = other.ptr;
	other.ptr = NULL;
}

// destructor: free Perl-Compatible Regular Expression if necessary
PCRE::~PCRE() noexcept { this->reset(); }

// get pointer to Perl-Compatible Regular Expression
pcre2_code * PCRE::get() noexcept {
	return this->ptr;
}

// get const pointer to Perl-Compatible Regular Expression
const pcre2_code * PCRE::get() const noexcept {
	return this->ptr;
}

// reset Perl-Compatible Regular Expression
void PCRE::reset() {
	if(this->ptr) {
		pcre2_code_free(this->ptr);
		this->ptr = NULL;
	}
}

// replace Perl-Compatible Regular Expression
void PCRE::reset(pcre2_code * other) {
	if(this->ptr) pcre2_code_free(this->ptr);
	this->ptr = other;
}

// bool operator
PCRE::operator bool() const noexcept {
	return this->ptr != NULL;
}

// not operator
bool PCRE::operator!() const noexcept {
	return this->ptr == NULL;
}

// move operator
PCRE& PCRE::operator=(PCRE&& other) noexcept {
	if(&other != this) {
		this->ptr = other.ptr;
		other.ptr = NULL;
	}
	return *this;
}

}
