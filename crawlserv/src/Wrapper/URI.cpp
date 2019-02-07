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
URI::URI() {}

// move constructor
URI::URI(URI&& other) {
	this->ptr = std::move(other.ptr);
}

// destructor: free and reset URI structure if necessary
URI::~URI() {
	this->reset();
}

// get pointer to URI structure
UriUriA * URI::get() {
	return this->ptr.get();
}

// get const pointer to URI structure
const UriUriA * URI::get() const {
	return this->ptr.get();
}

// create URI structure, free old structure if necessary
void URI::create() {
	if(this->ptr) {
		uriFreeUriMembersA(this->ptr.get());
	}
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
URI::operator bool() const {
	return this->ptr.operator bool();
}

// not operator
bool URI::operator!() const {
	return !(this->ptr);
}

// move assignment operator
URI& URI::operator=(URI&& other) {
	if(&other != this) this->ptr = std::move(other.ptr);
	return *this;
}

}
