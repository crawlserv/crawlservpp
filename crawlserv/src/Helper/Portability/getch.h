/*
 * getch.hpp
 *
 * Plattform-independent getch()
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef HELPER_PORTABILITY_GETCH_HPP_
#define HELPER_PORTABILITY_GETCH_HPP_

#include <cctype>

#ifdef __unix
	#include <termios.h>
	#include <stdio.h>
#else
	#include <conio.h>
#endif /* __unix */

namespace crawlservpp::Helper::Portability {

	static struct termios oldT, newT;

	/*
	 * IMPLEMENTATION
	 */

	inline char getch() {
#ifdef __unix
		char ch = 0;
		tcgetattr(0, &oldT);
		newT = oldT;
		newT.c_lflag &= ~ICANON;
		newT.c_lflag &= ~ECHO;
		tcsetattr(0, TCSANOW, &newT);
		ch = getchar();
		tcsetattr(0, TCSANOW, &oldT);
		return ch;
#else
		return ::getch();
#endif /* __ unix */
	}

} /* crawlservpp::Helper::Portability */

#endif /* HELPER_PORTABILITY_GETCH_HPP_ */
