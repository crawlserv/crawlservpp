/*
 * Json.h
 *
 * Namespace with global JSON helper functions.
 *
 *  Created on: Jan 9, 2019
 *      Author: ans
 */

#ifndef HELPER_JSON_H_
#define HELPER_JSON_H_

#include "../_extern/rapidjson/document.h"
#include "../_extern/rapidjson/stringbuffer.h"
#include "../_extern/rapidjson/writer.h"

#include <string>
#include <vector>

namespace crawlservpp::Helper::Json {
	std::string stringify(const std::vector<std::string>& vectorToStringify);
	std::string stringify(const std::string& stringToStringify);
}

#endif /* HELPER_JSON_H_ */
