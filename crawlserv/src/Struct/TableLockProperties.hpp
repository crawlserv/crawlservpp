/*
 * TableLockProperties.h
 *
 * Structure for table locks (table name, alias name, number of aliases).
 *
 *  Created on: Mar 9, 2019
 *      Author: ans
 */

#ifndef STRUCT_TABLELOCKPROPERTIES_HPP_
#define STRUCT_TABLELOCKPROPERTIES_HPP_

#include <string>

namespace crawlservpp::Struct {

	struct TableLockProperties {
		std::string name;				// name of the table
		std::string alias;				// name of the alias (for reading only), will be appended by '1', '2',...
		unsigned short numberOfAliases; // number of aliases

		TableLockProperties(const std::string& setName) : name(setName), numberOfAliases(0) {}
		TableLockProperties(const std::string& setName, const std::string& setAlias, unsigned short setNumberOfAliases)
			: name(setName), alias(setAlias), numberOfAliases(setNumberOfAliases) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_TABLELOCKPROPERTIES_HPP_ */
