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
	// constructors
	CurlList();
	CurlList(CurlList&& other);

	// destructor
	virtual ~CurlList();

	// getters
	struct curl_slist * get();
	const struct curl_slist * get() const;

	// control functions
	void append(const std::string& newElement);
	void reset();

	// operators
	operator bool() const;
	bool operator!() const;
	CurlList& operator=(CurlList&& other);

	// not copyable
	CurlList(CurlList&) = delete;
	CurlList& operator=(CurlList&) = delete;

private:
	struct curl_slist * ptr;
};

}

#endif /* WRAPPER_CURLLIST_H_ */
