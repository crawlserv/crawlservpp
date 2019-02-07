/*
 * URIQueryList.h
 *
 * RAII wrapper for pointer to URI query list.
 *  Does NOT have ownership of the pointer!
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_URIQUERYLIST_H_
#define WRAPPER_URIQUERYLIST_H_

#include <uriparser/Uri.h>

namespace crawlservpp::Wrapper {

class URIQueryList {
public:
	// constructors
	URIQueryList();
	URIQueryList(URIQueryList&& other);

	// destructor
	virtual ~URIQueryList();

	// getters
	UriQueryListA * get();
	const UriQueryListA * get() const;
	UriQueryListA ** getPtr();

	// operators
	operator bool() const;
	bool operator!() const;
	URIQueryList& operator=(URIQueryList&& other);

	// not copyable
	URIQueryList(URIQueryList&) = delete;
	URIQueryList& operator=(URIQueryList&) = delete;

private:
	UriQueryListA * ptr;
};

}

#endif /* WRAPPER_URIQUERYLIST_H_ */
