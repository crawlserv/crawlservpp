/*
 * URI.h
 *
 * Wrapper for pointer to URI structure.
 *  Structure is only created when needed.
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_URI_H_
#define WRAPPER_URI_H_

#include <uriparser/Uri.h>

#include <memory>
#include <utility>

namespace crawlservpp::Wrapper {

class URI {
public:
	// constructors
	URI() noexcept;
	URI(URI&& other) noexcept;

	// destructor
	virtual ~URI();

	// control functions
	void create();
	void reset();

	// getters
	const UriUriA * get() const noexcept;
	UriUriA * get() noexcept;

	// operators
	operator bool() const noexcept;
	bool operator!() const noexcept;
	URI& operator=(URI&& other) noexcept;

	// not copyable
	URI(URI&) = delete;
	URI& operator=(URI&) = delete;

private:
	std::unique_ptr<UriUriA> ptr;
};

}

#endif /* WRAPPER_URI_H_ */
