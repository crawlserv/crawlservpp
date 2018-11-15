/*
 * ServerCommandArgument.h
 *
 * The [name,value] pair of a server command argument.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef STRUCTS_SERVERCOMMANDARGUMENT_H_
#define STRUCTS_SERVERCOMMANDARGUMENT_H_

#include <string>

struct ServerCommandArgument {
	std::string name; // name of command argument
	std::string value; // value of command argument

	ServerCommandArgument(const std::string& setName, const std::string& setValue) {
		this->name = setName;
		this->value = setValue;
	}

	bool operator==(const ServerCommandArgument& argument) const {
		return this->name == argument.name && this->value == argument.value;
	}

	bool operator==(const std::string& argumentName) const {
		return this->name == argumentName;
	}
};

#endif /* STRUCTS_SERVERCOMMANDARGUMENT_H_ */
