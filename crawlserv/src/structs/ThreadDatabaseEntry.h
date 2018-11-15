/*
 * ThreadDatabaseEntry.h
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef STRUCTS_THREADDATABASEENTRY_H_
#define STRUCTS_THREADDATABASEENTRY_H_

#include "ThreadOptions.h"

#include <string>

struct ThreadDatabaseEntry {
	unsigned long id; // thread id
	std::string module; // thread module
	std::string status; // thread status
	bool paused; // is thread paused?
	ThreadOptions options; // options for thread
	unsigned long last; // last id
};

#endif /* STRUCTS_THREADDATABASEENTRY_H_ */
