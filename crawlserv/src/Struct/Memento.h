/*
 * Memento.h
 *
 * URL and timestamp of a memento (archived website).
 *
 *  Created on: Oct 28, 2018
 *      Author: ans
 */

#ifndef STRUCT_MEMENTO_H_
#define STRUCT_MEMENTO_H_

#include <string>

namespace crawlservpp::Struct {
	struct Memento {
		std::string url;
		std::string timeStamp;
	};
}

#endif /* STRUCT_MEMENTO_H_ */
