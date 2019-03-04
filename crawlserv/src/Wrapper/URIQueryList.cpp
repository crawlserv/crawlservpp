/*
 * URIQueryList.cpp
 *
 * RAII wrapper for pointer to URI query list.
 *  Does NOT have ownership of the pointer!
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#include "URIQueryList.h"

namespace crawlservpp::Wrapper {

// constructor: set pointer to NULL
URIQueryList::URIQueryList() noexcept : ptr(NULL) {}

// move constructor
URIQueryList::URIQueryList(URIQueryList&& other) noexcept : ptr(other.ptr) {
	other.ptr = NULL;
}

// destructor: free query list if necessary
URIQueryList::~URIQueryList() {
	if(this->ptr) uriFreeQueryListA(this->ptr);
}

// get pointer to URI query list
UriQueryListA * URIQueryList::get() noexcept {
	return this->ptr;
}

// get const pointer to URI query list
const UriQueryListA * URIQueryList::get() const noexcept {
	return this->ptr;
}

// get pointer to pointer to URI query list
UriQueryListA ** URIQueryList::getPtr() noexcept {
	return &(this->ptr);
}

// bool operator
URIQueryList::operator bool() const noexcept {
	return this->ptr != NULL;
}

// not operator
bool URIQueryList::operator!() const noexcept {
	return this->ptr == NULL;
}

// move operator
URIQueryList& URIQueryList::operator=(URIQueryList&& other) noexcept {
	if(&other != this) {
		this->ptr = other.ptr;
		other.ptr = NULL;
	}
	return *this;
}

}
