/*
 * DatabaseSettings.h
 *
 * Database settings.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef DATABASESETTINGS_H_
#define DATABASESETTINGS_H_

#include <string>

struct DatabaseSettings {
	std::string host; // host name of database server
	unsigned short port; // port of database server
	std::string user; // user name for database
	std::string password; // user password for database
	std::string name; // name of database
};

#endif /* DATABASESETTINGS_H_ */
