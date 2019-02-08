/*
 * URI.cpp
 *
 * Wrapper for pointer to URI structure.
 *  Structure is only created when needed.
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#include "URI.h"

namespace crawlservpp::Wrapper {

// constructor stub
URI::URI() noexcept {}

// move constructor
URI::URI(URI&& other) noexcept {
	this->ptr = std::move(other.ptr);
}

// destructor: free and reset URI structure if necessary
URI::~URI() {
	this->reset();
}

// get pointer to URI structure
UriUriA * URI::get() noexcept {
	return this->ptr.get();
}

// get const pointer to URI structure
const UriUriA * URI::get() const noexcept {
	return this->ptr.get();
}

// create URI structure, free old structure if necessary
void URI::create() {
	this->reset();
	this->ptr = std::make_unique<UriUriA>();
}

// free and reset URI structure if necessary
void URI::reset() {
	if(this->ptr) {
		uriFreeUriMembersA(this->ptr.get());
		this->ptr.reset();
	}
}

// bool operator
URI::operator bool() const noexcept {
	return this->ptr.operator bool();
}

// not operator
bool URI::operator!() const noexcept {
	return !(this->ptr);
}

// move assignment operator
URI& URI::operator=(URI&& other) noexcept {
	if(&other != this) this->ptr = std::move(other.ptr);
	return *this;
}

}
