/*
 * TargetTableProperties.h
 *
 * Target table properties (type, website, URL list, table name and full name, columns, compression).
 *
 * NOTE: module will be ignored on update!
 *
 *  Created on: Mar 4, 2019
 *      Author: ans
 */


#ifndef STRUCT_CUSTOMTABLEPROPERTIES_HPP_
#define STRUCT_CUSTOMTABLEPROPERTIES_HPP_

#include <string>
#include <vector>
#include "TableColumn.hpp"

namespace crawlservpp::Struct {

	struct TargetTableProperties {
		std::string type;
		unsigned long website;
		unsigned long urlList;
		std::string name;
		std::string fullName;
		std::vector<TableColumn> columns;
		bool compressed;

		// constructors
		TargetTableProperties() : website(0), urlList(0), compressed(false) {}
		TargetTableProperties(const std::string& setType, unsigned long setWebsite, unsigned long setUrlList,
				const std::string& setName, const std::string& setFullName, bool setCompressed)
				: type(setType), website(setWebsite), urlList(setUrlList), name(setName), fullName(setFullName),
				  compressed(setCompressed) {}
		TargetTableProperties(const std::string& setType, unsigned long setWebsite, unsigned long setUrlList,
				const std::string& setName,	const std::string& setFullName, const std::vector<TableColumn>& setColumns,
				bool setCompressed) : type(setType), website(setWebsite), urlList(setUrlList), name(setName), fullName(setFullName),
						  columns(setColumns), compressed(setCompressed) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_CUSTOMTABLEPROPERTIES_HPP_ */

