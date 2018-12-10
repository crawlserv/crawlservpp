/*
 * Portability.h
 *
 * Namespace with global portability helper functions.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef NAMESPACES_HELPERS_H_
#define NAMESPACES_HELPERS_H_

#include <cctype>

#ifdef __unix
	#include <termios.h>
	#include <stdio.h>
#elif
	#include <conio.h>
#endif

namespace Portability {
#ifdef __unix
	// portable getch
	char getch(void);
#endif
}


#endif /* NAMESPACES_HELPERS_H_ */
