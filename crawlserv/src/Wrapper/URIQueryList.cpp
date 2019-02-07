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
URIQueryList::URIQueryList() {
	this->ptr = NULL;
}

// move constructor
URIQueryList::URIQueryList(URIQueryList&& other) {
	this->ptr = other.ptr;
	other.ptr = NULL; }

// destructor: free query list if necessary
URIQueryList::~URIQueryList() {
	if(this->ptr) uriFreeQueryListA(this->ptr);
}

// get pointer to URI query list
UriQueryListA * URIQueryList::get() {
	return this->ptr;
}

// get const pointer to URI query list
const UriQueryListA * URIQueryList::get() const {
	return this->ptr;
}

// get pointer to pointer to URI query list
UriQueryListA ** URIQueryList::getPtr() {
	return &(this->ptr);
}

// bool operator
URIQueryList::operator bool() const {
	return this->ptr != NULL;
}

// not operator
bool URIQueryList::operator!() const {
	return this->ptr == NULL;
}

// move operator
URIQueryList& URIQueryList::operator=(URIQueryList&& other) {
	if(&other != this) {
		this->ptr = other.ptr;
		other.ptr = NULL;
	}
	return *this;
}

}
