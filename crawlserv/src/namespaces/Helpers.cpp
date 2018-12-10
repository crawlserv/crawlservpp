/*
 * Helpers.cpp
 *
 * Global helper functions encapsulated into one namespace.
 *
 *  Created on: Oct 12, 2018
 *      Author: ans
 */

#include "Helpers.h"

// portable getch
static struct termios oldT, newT;

// portable getch
char Helpers::getch() {
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
#elif
	return ::getch();
#endif
}
