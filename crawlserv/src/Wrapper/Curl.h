/*
 * Curl.h
 *
 * RAII wrapper for pointer to cURL instance, also handles global instance if necessary.
 * 	Does NOT have ownership of the pointer.
 * 	The first instance has to be destructed last.
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_CURL_H_
#define WRAPPER_CURL_H_

#include <curl/curl.h>

namespace crawlservpp::Wrapper {

class Curl {
public:
	// constructor: set pointer to NULL
	Curl() : ptr(NULL) {
		// initialize global instance if necessary
		if(globalInit) this->localInit = false;
		else {
			globalInit = true;
			this->localInit = true;
			curl_global_init(CURL_GLOBAL_ALL);
		}

		// initialize cURL
		this->init();
	}

	// move constructor
	Curl(Curl&& other) noexcept {
		this->ptr = other.ptr;
		other.ptr = NULL;
		this->localInit = other.localInit;
		other.localInit = false;
	}

	// destructor: cleanup cURL if necessary
	~Curl() {
		this->reset();

		// cleanup global instance if necessary
		if(globalInit && this->localInit) {
			curl_global_cleanup();
			globalInit = false;
			this->localInit = false;
		}
	}

	// get const pointer to query list
	const CURL * get() const {
		return this->ptr;
	}

	// get non-const pointer to query list
	CURL * get() {
		return this->ptr;
	}

	// get non-const pointer to pointer to query list
	CURL ** getPtr() {
		return &(this->ptr);
	}

	// initialize cURL pointer
	void init() {
		this->ptr = curl_easy_init();
	}

	// reset cURL pointer
	void reset() {
		if(this->ptr) {
			curl_easy_cleanup(this->ptr);
			this->ptr = NULL;
		}
	}

	// bool operator
	operator bool() const {
		return this->ptr != NULL;
	}

	// not operator
	bool operator!() const {
		return this->ptr == NULL;
	}

	// move assignment operator
	Curl& operator=(Curl&& other) noexcept {
		if(&other != this) {
			this->ptr = other.ptr;
			other.ptr = NULL;
			this->localInit = other.localInit;
			other.localInit = false;
		}
		return *this;
	}

	// not copyable
	Curl(Curl&) = delete;
	Curl& operator=(Curl&) = delete;

private:
	CURL * ptr;
	bool localInit;
	static bool globalInit;
};

}

#endif /* SRC_WRAPPER_CURL_H_ */
