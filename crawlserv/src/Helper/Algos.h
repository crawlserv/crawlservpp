/*
 * Algos.h
 *
 * Namespace with global helper functions for algorithms.
 *
 *  Created on: Jan 10, 2019
 *      Author: ans
 */

#ifndef HELPER_ALGOS_H_
#define HELPER_ALGOS_H_

#include <algorithm>

namespace crawlservpp::Helper::Algos {
	// return the lowest of the three values
	template<class T> const T& min(const T& a, const T& b, const T& c) { return std::min(a, std::min(b, c)); }

	// return the lowest of the four values
	template<class T> const T& min(const T& a, const T& b, const T& c, const T& d) { return std::min(a, std::min(b, std::min(c, d))); }
}

#endif /* HELPER_ALGOS_H_ */
