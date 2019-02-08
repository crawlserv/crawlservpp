/*
 * Curl.cpp
 *
 * RAII wrapper for pointer to cURL instance, also handles global instance if necessary.
 * 	Does NOT have ownership of the pointer.
 * 	The first instance has to be destructed last.
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#include "Curl.h"

namespace crawlservpp::Wrapper {

bool Curl::globalInit = false;

// constructor: set pointer to NULL
Curl::Curl() {
	// initialize global instance if necessary
	if(Curl::globalInit) this->localInit = false;
	else {
		Curl::globalInit = true;
		this->localInit = true;
		curl_global_init(CURL_GLOBAL_ALL);
	}

	// initialize cURL
	this->ptr = NULL;
	this->init();
}

// move constructor
Curl::Curl(Curl&& other) noexcept {
	this->ptr = other.ptr;
	other.ptr = NULL;
	this->localInit = other.localInit;
	other.localInit = false;
}

// destructor: cleanup cURL if necessary
Curl::~Curl() {
	this->reset();

	// cleanup global instance if necessary
	if(Curl::globalInit && this->localInit) {
		curl_global_cleanup();
		Curl::globalInit = false;
		this->localInit = false;
	}
}

// get const pointer to query list
const CURL * Curl::get() const {
	return this->ptr;
}

// get non-const pointer to query list
CURL * Curl::get() {
	return this->ptr;
}

// get non-const pointer to pointer to query list
CURL ** Curl::getPtr() {
	return &(this->ptr);
}

// initialize cURL pointer
void Curl::init() {
	this->ptr = curl_easy_init();
}

// reset cURL pointer
void Curl::reset() {
	if(this->ptr) {
		curl_easy_cleanup(this->ptr);
		this->ptr = NULL;
	}
}

// bool operator
Curl::operator bool() const {
	return this->ptr != NULL;
}

// not operator
bool Curl::operator!() const {
	return this->ptr == NULL;
}

// move assignment operator
Curl& Curl::operator=(Curl&& other) noexcept {
	if(&other != this) {
		this->ptr = other.ptr;
		other.ptr = NULL;
		this->localInit = other.localInit;
		other.localInit = false;
	}
	return *this;
}

}
