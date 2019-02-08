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
	PCREMatch(pcre2_match_data * setPtr) noexcept;
	PCREMatch(PCREMatch&& other) noexcept;

	// destructor
	virtual ~PCREMatch();

	// getters
	pcre2_match_data * get() noexcept;
	const pcre2_match_data * get() const noexcept;

	// operators
	operator bool() const noexcept;
	bool operator!() const noexcept;
	PCREMatch& operator=(PCREMatch&& other) noexcept;

	// not copyable
	PCREMatch(PCREMatch&) = delete;
	PCREMatch& operator=(PCREMatch&) = delete;

private:
	pcre2_match_data * ptr;
};

} /* namespace crawlservpp */

#endif /* WRAPPER_PCREMATCH_H_ */
