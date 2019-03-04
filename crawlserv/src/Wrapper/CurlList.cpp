/*
 * CurlList.cpp
 *
 * RAII wrapper for pointer to cURL list.
 *  Does NOT have ownership of the pointer!
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#include "CurlList.h"

namespace crawlservpp::Wrapper {

// constructor: set pointer to NULL
CurlList::CurlList() noexcept : ptr(NULL) {}

// move constructor
CurlList::CurlList(CurlList&& other) noexcept : ptr(other.ptr) {
	other.ptr = NULL;
}

// destructor: reset cURL list if necessary
CurlList::~CurlList() {
	this->reset();
}

// get pointer to cURL list
struct curl_slist * CurlList::get() {
	return this->ptr;
}

// get const pointer to cURL list
const struct curl_slist * CurlList::get() const {
	return this->ptr;
}

// append newElement to cURL list
void CurlList::append(const std::string& newElement) {
	this->ptr = curl_slist_append(this->ptr, newElement.c_str());
}

// reset cURL list
void CurlList::reset() { if(this->ptr) curl_slist_free_all(this->ptr); this->ptr = NULL; }

// bool operator
CurlList::operator bool() const {
	return this->ptr != NULL;
}

// not operator
bool CurlList::operator!() const {
	return this->ptr == NULL;
}

// move assignment operator
CurlList& CurlList::operator=(CurlList&& other) noexcept {
	if(&other != this) {
		this->ptr = other.ptr;
		other.ptr = NULL;
	}
	return *this;
}

}
