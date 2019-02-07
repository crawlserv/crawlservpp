/*
 * PCRE.h
 *
 * Wrapper for pointer to Perl-Compatible Regular Expression.
 *  Does NOT have ownership of the pointer!
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_PCRE_H_
#define WRAPPER_PCRE_H_

#define PCRE2_CODE_UNIT_WIDTH 8

#include <pcre2.h>
#include <stddef.h>

namespace crawlservpp::Wrapper {

class PCRE {
public:
	// constructors
	PCRE();
	PCRE(PCRE&& other);

	// destructor
	virtual ~PCRE();

	// getters
	pcre2_code * get();
	const pcre2_code * get() const;

	// control functions
	void reset();
	void reset(pcre2_code * other);

	// operators
	operator bool() const;
	bool operator!() const;
	PCRE& operator=(PCRE&& other);

	// not copyable
	PCRE(PCRE&) = delete;
	PCRE& operator=(PCRE&) = delete;


private:
	pcre2_code * ptr;
};

} /* namespace crawlservpp */

#endif /* WRAPPER_PCRE_H_ */
