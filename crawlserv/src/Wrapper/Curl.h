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
	// constructors
	Curl();
	Curl(Curl&& other);

	// destructor
	virtual ~Curl();

	// getters
	const CURL * get() const;
	CURL * get();
	CURL ** getPtr();

	// control functions
	void init();
	void reset();

	// operators
	operator bool() const;
	bool operator!() const;
	Curl& operator=(Curl&& other);

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
