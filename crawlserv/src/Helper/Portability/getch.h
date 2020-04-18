/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * getch.h
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
