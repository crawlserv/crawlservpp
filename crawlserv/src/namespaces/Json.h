/*
 * Json.h
 *
 * Namespace with global JSON helper functions.
 *
 *  Created on: Jan 9, 2019
 *      Author: ans
 */

#ifndef SRC_NAMESPACES_JSON_H_
#define SRC_NAMESPACES_JSON_H_

#include "../external/rapidjson/document.h"
#include "../external/rapidjson/stringbuffer.h"
#include "../external/rapidjson/writer.h"

#include <string>
#include <vector>

namespace Json {
	std::string stringify(const std::vector<std::string>& vectorToStringify);
	std::string stringify(const std::string& stringToStringify);
}

#endif /* SRC_NAMESPACES_JSON_H_ */
