/*
 * PCREMatch.cpp
 *
 * RAII wrapper for pointer to Perl-Compatible Regular Expression match.
 *  Does NOT have ownership of the pointer!
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#include "PCREMatch.h"

namespace crawlservpp::Wrapper {

// constructor: set pointer to NULL
PCREMatch::PCREMatch(pcre2_match_data * setPtr) noexcept {
	this->ptr = setPtr;
}

// move constructor
PCREMatch::PCREMatch(PCREMatch&& other) noexcept {
	this->ptr = other.ptr;
	other.ptr = NULL;
}

// destructor: free Perl-Compatible Regular Expression match if necessary
PCREMatch::~PCREMatch() {
	if(this->ptr) pcre2_match_data_free(this->ptr);
}

// get pointer to Perl-Compatible Regular Expression match
pcre2_match_data * PCREMatch::get() noexcept {
	return this->ptr;
}

// get const pointer to Perl-Compatible Regular Expression match
const pcre2_match_data * PCREMatch::get() const noexcept {
	return this->ptr;
}

// bool operator
PCREMatch::operator bool() const noexcept {
	return this->ptr != NULL;
}

// not operator
bool PCREMatch::operator!() const noexcept {
	return this->ptr == NULL;
}

// move operator
PCREMatch& PCREMatch::operator=(PCREMatch&& other) noexcept {
	if(&other != this) {
		this->ptr = other.ptr;
		other.ptr = NULL;
	}
	return *this;
}

}
