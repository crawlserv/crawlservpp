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

#include "../_extern/rapidjson/include/rapidjson/document.h"
#include "../_extern/rapidjson/include/rapidjson/stringbuffer.h"
#include "../_extern/rapidjson/include/rapidjson/writer.h"

#include <string>
#include <tuple>
#include <vector>

namespace crawlservpp::Helper::Json {
	// text maps are used to describe certain parts of a text defined by their positions and lengths with certain strings (words, dates etc.)
	typedef std::vector<std::tuple<std::string, unsigned long, unsigned long>> TextMap;

	// stringify different kind of data to a JSON string
	std::string stringify(const std::vector<std::string>& vectorToStringify);
	std::string stringify(const std::string& stringToStringify);
	std::string stringify(const std::vector<std::vector<std::pair<std::string, std::string>>>& vectorToStringify);
	std::string stringify(const TextMap& textMapToStringify);
}

#endif /* HELPER_JSON_H_ */
