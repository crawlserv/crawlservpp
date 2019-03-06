/*
 * Portability.h
 *
 * Namespace with global portability helper functions.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef HELPER_PORTABILITY_H_
#define HELPER_PORTABILITY_H_

#include <cctype>

#ifdef __unix
	#include <termios.h>
	#include <stdio.h>
#elif
	#include <conio.h>
#endif

namespace Helper::Portability {
#ifdef __unix
	// portable getch
	char getch(void);
#endif
}

#endif /* HELPER_PORTABILITY_H_ */
