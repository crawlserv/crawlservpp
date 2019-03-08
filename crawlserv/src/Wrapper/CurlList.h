/*
 * CurlList.h
 *
 * RAII wrapper for pointer to cURL list.
 *  Does NOT have ownership of the pointer!
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_CURLLIST_H_
#define WRAPPER_CURLLIST_H_

#include <curl/curl.h>

#include <string>

namespace crawlservpp::Wrapper {

class CurlList {
public:
	// constructor: set pointer to NULL
	CurlList() noexcept : ptr(NULL) {}

	// move constructor
	CurlList(CurlList&& other) noexcept : ptr(other.ptr) {
		other.ptr = NULL;
	}

	// destructor: reset cURL list if necessary
	~CurlList() {
		this->reset();
	}

	// get pointer to cURL list
	struct curl_slist * get() {
		return this->ptr;
	}

	// get const pointer to cURL list
	const struct curl_slist * get() const {
		return this->ptr;
	}

	// append newElement to cURL list
	void append(const std::string& newElement) {
		this->ptr = curl_slist_append(this->ptr, newElement.c_str());
	}

	// reset cURL list
	void reset() { if(this->ptr) curl_slist_free_all(this->ptr); this->ptr = NULL; }

	// bool operator
	operator bool() const {
		return this->ptr != NULL;
	}

	// not operator
	bool operator!() const {
		return this->ptr == NULL;
	}

	// move assignment operator
	CurlList& operator=(CurlList&& other) noexcept {
		if(&other != this) {
			this->ptr = other.ptr;
			other.ptr = NULL;
		}
		return *this;
	}

	// not copyable
	CurlList(CurlList&) = delete;
	CurlList& operator=(CurlList&) = delete;

private:
	struct curl_slist * ptr;
};

}

#endif /* WRAPPER_CURLLIST_H_ */
