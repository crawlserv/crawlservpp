/*
 * CustomTableProperties.h
 *
 * Custom table properties (type, website, URL list, table name and full name, columns, compression).
 *
 * NOTE: module will be ignored on update!
 *
 *  Created on: Mar 4, 2019
 *      Author: ans
 */


#ifndef STRUCT_CUSTOMTABLEPROPERTIES_H_
#define STRUCT_CUSTOMTABLEPROPERTIES_H_

#include "TableColumn.h"

#include <string>
#include <vector>

namespace crawlservpp::Struct {
	struct CustomTableProperties {
		std::string type;
		unsigned long website;
		unsigned long urlList;
		std::string name;
		std::string fullName;
		std::vector<TableColumn> columns;
		bool compressed;

		// constructors
		CustomTableProperties() : website(0), urlList(0), compressed(false) {}
		CustomTableProperties(const std::string& setType, unsigned long setWebsite, unsigned long setUrlList,
				const std::string& setName, const std::string& setFullName, bool setCompressed)
				: type(setType), website(setWebsite), urlList(setUrlList), name(setName), fullName(setFullName),
				  compressed(setCompressed) {}
		CustomTableProperties(const std::string& setType, unsigned long setWebsite, unsigned long setUrlList,
				const std::string& setName,	const std::string& setFullName, const std::vector<TableColumn>& setColumns,
				bool setCompressed) : type(setType), website(setWebsite), urlList(setUrlList), name(setName), fullName(setFullName),
						  columns(setColumns), compressed(setCompressed) {}
	};
}

#endif /* STRUCT_CUSTOMTABLEPROPERTIES_H_ */

