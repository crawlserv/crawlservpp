/*
 * PCREMatch.h
 *
 * RAII wrapper for pointer to Perl-Compatible Regular Expression match.
 *  Does NOT have ownership of the pointer!
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_PCREMATCH_H_
#define WRAPPER_PCREMATCH_H_

#define PCRE2_CODE_UNIT_WIDTH 8

#include <pcre2.h>
#include <stddef.h>

namespace crawlservpp::Wrapper {

class PCREMatch {
public:
	// constructors
	PCREMatch(pcre2_match_data * setPtr);
	PCREMatch(PCREMatch&& other);

	// destructor
	virtual ~PCREMatch();

	// getters
	pcre2_match_data * get();
	const pcre2_match_data * get() const;

	// operators
	operator bool() const;
	bool operator!() const;
	PCREMatch& operator=(PCREMatch&& other);

	// not copyable
	PCREMatch(PCREMatch&) = delete;
	PCREMatch& operator=(PCREMatch&) = delete;

private:
	pcre2_match_data * ptr;
};

} /* namespace crawlservpp */

#endif /* WRAPPER_PCREMATCH_H_ */
